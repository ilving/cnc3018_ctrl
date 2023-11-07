#include <string.h>

#include "driver/gpio.h"
#include "esp_log.h"
// ------------------------

#include "generic.h"
#include "cnc.h"

static QueueHandle_t cnc_tx_q;
xQueueHandle get_cnctx_queue() { return cnc_tx_q; }

static void cncRxTask(void* args) {
    // status report: <Idle|MPos:-0.999,-121.001,-36.979|FS:0,0|Pn:P|Ov:30,100,100>
    // https://github.com/gnea/grbl/wiki/Grbl-v1.1-Interface#real-time-status-reports

    char statusBuf[UART_BUF_SIZE];
    uint8_t bufPos = 0;
    memset((void*)statusBuf, 0, UART_BUF_SIZE);

    char c; // temp one-symbol buffer
    
    for(;;) {
        c = 0;
        int st = uart_read_bytes(CNC_UART, &c, 1, 100 / portTICK_PERIOD_MS);
        if( st < 0) {
            ESP_LOGE(CRX_TAG, "buf: %d", st);
            continue;
        }
        if(st == 0 || c == 0)continue;

        uart_write_bytes(CANDLE_UART, &c, 1);

        if(c == '<') {// status command start
        }
    }

    vTaskDelete(NULL);
}

static void cncTxTask(void* args) {
    static qItem event;
    size_t cmd_len = 0;
    for(;;) {
        if (xQueueReceive(cnc_tx_q, (void *)&event, (TickType_t)portMAX_DELAY)) {
            switch(event.cmd[0]) {
                case CMD_RESET:
                    xQueueReset(cnc_tx_q);
                case CMD_CYCLE_START:
                case CMD_FEED_HOLD:
                case CMD_SAFETY_DOOR:
                case CMD_STATUS_REPORT:
                    uart_write_bytes(CNC_UART, event.cmd, 1);
                    break;

                default:
                    cmd_len = strnlen(event.cmd, UART_BUF_SIZE);
                    uart_write_bytes(CNC_UART, event.cmd, cmd_len);
            }
        }
    }
	
	vTaskDelete(NULL);
}

static void candleRxTask(void* args) {

    static qItem instant;
	memset((void*)&instant.cmd, 0, UART_BUF_SIZE);

    static qItem buf;
    static uint16_t bufPos = 0;
	memset((void*)&buf.cmd, 0, UART_BUF_SIZE);

    char c; // temp one-symbol buffer

    for(;;) {
        c = 0;

        int st = uart_read_bytes(CANDLE_UART, &c, 1, 100 / portTICK_PERIOD_MS);
        if( st < 0) {
            ESP_LOGE(CRX_TAG, "buf: %d", st);
            continue;
        }
        if(st == 0 || c == 0)continue;

        switch(c) {
            case CMD_STATUS_REPORT:
            case CMD_FEED_HOLD:
            case CMD_CYCLE_START:
            case CMD_RESET:
            case CMD_SAFETY_DOOR:
                instant.cmd[0] = c;
                xQueueSend(cnc_tx_q, &instant, portTICK_PERIOD_MS);
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

    esp_log_level_set(CRX_TAG, ESP_LOG_INFO);

	cnc_tx_q = xQueueCreate(20, sizeof(qItem));

	xTaskCreate(candleRxTask, "candleRxTask", 2048, NULL, 1, NULL);
	xTaskCreate(cncTxTask, "cncTxTask", 2048, NULL, 1, NULL);
	xTaskCreate(cncRxTask, "cncRxTask", 2048, NULL, 1, NULL);
}