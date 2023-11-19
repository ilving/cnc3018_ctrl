#include <string.h>

#include "driver/gpio.h"
#include "esp_log.h"
// ------------------------

#include "generic.h"
#include "cnc.h"

static QueueHandle_t cnc_tx_q;
xQueueHandle get_cnctx_queue() { return cnc_tx_q; };

static QueueHandle_t cnc_instant_tx_q;
xQueueHandle get_cncinstant_queue() {return cnc_instant_tx_q; }

static machineStatus_t state;
machineStatus_t* get_cnc_status() { return &state; };

static void cncRxTask(void* args) {
    // status report: <Idle|MPos:-0.999,-121.001,-36.979|FS:0,0|Pn:P|Ov:30,100,100>
    // https://github.com/gnea/grbl/wiki/Grbl-v1.1-Interface#real-time-status-reports

    char buf[UART_BUF_SIZE];
    uint8_t pos = 0;
    memset((void*)buf, 0, UART_BUF_SIZE);

    char c; // temp one-symbol buffer
    
    for(;;) {
        c = 0;
        int st = uart_read_bytes(CNC_UART, &c, 1, 500 / portTICK_PERIOD_MS);
        if( st < 0) {
            ESP_LOGE(CNC_TAG, "buf: %d", st);
            state.active = false;
            continue;
        }
        if(st == 0 || c == 0) {
            state.active = false;
            continue;
        }

        state.active = true;

        uart_write_bytes(CANDLE_UART, &c, 1);

        if(pos < UART_BUF_SIZE) { buf[pos] = c;pos++;};

        if (c == '\r' || c == '\n') {
            buf[UART_BUF_SIZE-1] = 0;
            buf[pos-1] = 0;
            
            if(buf[0] == '<') { // chevron symbol
                // <Idle|MPos:-25.999,-16.000,-11.050|FS:0,0|WCO:-84.000,-90.500,-41.000>
                switch (buf[1]){
                    case 'I': state.state = CNC_STATE_IDLE; break;
                    case 'R': state.state = CNC_STATE_RUN; break;
                    case 'H': state.state = CNC_STATE_HOME;if(buf[3] == 'l') state.state = CNC_STATE_HOLD;break;
                    case 'J': state.state = CNC_STATE_JOG; break;
                    case 'A': state.state = CNC_STATE_ALARM; break;
                    case 'D': state.state = CNC_STATE_DOOR; break;
                    case 'C': state.state = CNC_STATE_CHECK; break;
                    case 'S': state.state = CNC_STATE_SLEEP; break;
                    default: state.state = CNC_STATE_UNDEFINED;
                }

                int bl = strnlen(buf, UART_BUF_SIZE);
                // for(int i=1;i<bl;i++) {
                //     if(buf[i]==',') buf[i] = ' ';
                //     if(buf[i]=='>') buf[i]=' ';
                // }

                state.z_probe = false;

                for(int i=1;i<bl;i++) {
                    if(buf[i-1] == '|') {
                        if(strncmp(&buf[i], "Pn:", 3) == 0) { // Input pin state, Pn:XYZPDHRS
                            for(int j = i+3;j<bl && buf[j] != '|';j++) {
                                switch(buf[j]) {
                                    case PINSTATE_Z_PROBE: state.z_probe = true; break;
                                }
                            }
                        } else if (strncmp(&buf[i], "Ov:", 3) == 0) { // Override Values. Ov:feed%, rapids%, spindle%
                            sscanf(&buf[i+3], "%hhu,%hhu,%hhu", &state.feed_override, &state.rapid_override, &state.spindle_override);
                        } else if (strncmp(&buf[i], "MPos:", 5) == 0) { // machine coords
                            sscanf(&buf[i+5], "%f,%f,%f", &state.mx, &state.my, &state.mz);
                        } else if (strncmp(&buf[i], "WCO:", 4) == 0) { // working offset
                            //ESP_LOGI(CNC_TAG, ">%s<", &buf[i+4]);
                            sscanf(&buf[i+4], "%f,%f,%f", &state.wo_x, &state.wo_y, &state.wo_z);
                        }
                    }
                }
            } else if (strncmp("[GC:", buf, 4) == 0) {
                // [GC:G0 G54 G17 G21 G90 G94 M5 M9 T0 F0.0 S0]
                // https://github.com/gnea/grbl/blob/v1.1f.20170801/grbl/report.c#L285     
                float tval;
                for(int i=4;i<strnlen(buf, UART_BUF_SIZE);i++) {
                    if(buf[i] == 'G') {
                        sscanf(&buf[i+1], "%f", &tval);
                        int iVal = (int)(tval*10);

                        if(iVal >= 540 && iVal <= 590) {// G54..G59
                            state.coord_system = (iVal/10)-54;
                        }
                    }
                }
            }

            state.wx = state.mx - state.wo_x;
            state.wy = state.my - state.wo_y;
            state.wz = state.mz - state.wo_z;
            memset((void*)buf, 0, UART_BUF_SIZE);
            pos = 0;
        }
    }

    vTaskDelete(NULL);
}

static void cncTxTask(void* args) {
    static qItem event;
    size_t cmd_len = 0;
    static char instant = 0;

    for(;;) {
        if (xQueueReceive(cnc_instant_tx_q, (void *)&instant, 10/portTICK_PERIOD_MS)) {
            uart_write_bytes(CNC_UART, &instant, 1);
        }

        if (xQueueReceive(cnc_tx_q, (void *)&event, 10/portTICK_PERIOD_MS)) {
            cmd_len = strnlen(event.cmd, UART_BUF_SIZE);
            uart_write_bytes(CNC_UART, event.cmd, cmd_len);
        }
    }
	
	vTaskDelete(NULL);
}

static void candleRxTask(void* args) {
    static qItem buf;
    static uint16_t bufPos = 0;
	memset((void*)&buf.cmd, 0, UART_BUF_SIZE);

    char c; // temp one-symbol buffer

    for(;;) {
        c = 0;

        int st = uart_read_bytes(CANDLE_UART, &c, 1, 100 / portTICK_PERIOD_MS);
        if( st < 0) {
            ESP_LOGE(CNC_TAG, "buf: %d", st);
            continue;
        }
        if(st == 0 || c == 0)continue;

        switch(c) { // known instant commands
            case CMD_STATUS_REPORT:
            case CMD_FEED_HOLD:
            case CMD_CYCLE_START:
            case CMD_RESET:
            case CMD_SAFETY_DOOR:

            case CMD_DOOR:
            case CMD_JOG_CANCEL:

            case CMD_RAPID_100:
            case CMD_RAPID_50:
            case CMD_RAPID_25:
            
            case CMD_SPINDLE_100:
            case CMD_SPINDLE_STOP:
            case CMD_SPINDLE_DEC_10:
            case CMD_SPINDLE_DEC_1:
            case CMD_SPINDLE_INC_10:
            case CMD_SPINDLE_INC_1:
                xQueueSend(cnc_instant_tx_q, &c, portMAX_DELAY);
                continue;

            case CMD_FEED_OV_NORMAL: // bypassed due to candle behavior
            case CMD_FEED_OV_INC_10:
            case CMD_FEED_OV_INC_1:
            case CMD_FEED_OV_DEC_10:
            case CMD_FEED_OV_DEC_1:
                continue;
        }

        if(bufPos == 0 && (c == '\r' || c == '\n'))continue;

        buf.cmd[bufPos] = c;
        bufPos++;
        
        if(bufPos == UART_BUF_SIZE-1 || c == '\r' || c == '\n') {
            xQueueSend(cnc_tx_q, &buf, portTICK_PERIOD_MS);
            bufPos = 0;
            memset((void*)&buf.cmd, 0, UART_BUF_SIZE);
            continue;
        }
    }

    vTaskDelete(NULL);
}

const char* get_cnc_state_name(cncState_t state) {
    switch(state) {
        case CNC_STATE_IDLE: return "idle";
        case CNC_STATE_RUN: return "run";
        case CNC_STATE_HOLD: return "hold";
        case CNC_STATE_JOG: return "jog";
        case CNC_STATE_ALARM: return "ALARM";
        case CNC_STATE_DOOR: return "door";
        case CNC_STATE_CHECK: return "chk";
        case CNC_STATE_HOME: return "H";
        case CNC_STATE_SLEEP: return "sleep";
        default: return "undef";
    }
}

void init_cnc(void) {
    uart_config_t uartCfg = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_REF_TICK,
    };

    esp_timer_get_time();

    uart_driver_install(CANDLE_UART, UART_BUF_SIZE * 2, UART_BUF_SIZE * 2, 0, NULL, 0);
	uart_param_config(CANDLE_UART, &uartCfg);
	uart_set_pin(CANDLE_UART, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    uart_driver_install(CNC_UART, UART_BUF_SIZE * 2, UART_BUF_SIZE * 2, 0, NULL, 0);
	uart_param_config(CNC_UART, &uartCfg);
	uart_set_pin(CNC_UART, GPIO_NUM_16, GPIO_NUM_17, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    esp_log_level_set(CNC_TAG, ESP_LOG_INFO);

	cnc_tx_q = xQueueCreate(20, sizeof(qItem));
	cnc_instant_tx_q = xQueueCreate(20, sizeof(char));

    state.active = false;

	xTaskCreate(candleRxTask, "candleRxTask", 2048, NULL, 1, NULL);
	xTaskCreate(cncTxTask, "cncTxTask", 2048, NULL, 1, NULL);
	xTaskCreate(cncRxTask, "cncRxTask", 4096, NULL, 1, NULL);
}