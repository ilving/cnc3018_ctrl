#include <inttypes.h>

#include <driver/adc.h>

#include "esp_log.h"

// ------------------

#include "generic.h"

#include "cnc.h"
#include "isr.h"
#include "controls.h"

static xQueueHandle move_queue = NULL;

float z_dist_per_tick() { return gpio_get_level(Z_DIST_PIN) == 0 ? MOVE_1X_STEP : MOVE_10X_STEP; };

static void movementTask(void* args) {
	xQueueHandle cncTx = get_cnctx_queue();

	Movement_t move = MOVE_None;
	static qItem cncCmd;
	static uint8_t moveCount = 0;

	float moveX=0, moveY=0, moveZ=0;

	machineStatus_t *state = get_cnc_status();

	for(;;) {
        BaseType_t qGet = xQueueReceive(move_queue, (void *)&move, 100/portTICK_PERIOD_MS);
		
		if (qGet == pdFALSE || moveCount == ENCODER_PULSES/2) {
			moveCount = 0;

			if (moveX != 0 || moveY != 0) {
				snprintf(cncCmd.cmd, UART_BUF_SIZE, "$J=G21G91X%.3fY%.3fF500\r\n", moveX, moveY);
				xQueueSend(cncTx, &cncCmd, 100/portTICK_PERIOD_MS);

				ESP_LOGI(CTRL_TAG, "[%s]", cncCmd.cmd);

				moveX = moveY = 0;
			}
			if (moveZ != 0) {
				snprintf(cncCmd.cmd, UART_BUF_SIZE, "$J=G21G91Z%.3fF500\r\n", moveZ);
				xQueueSend(cncTx, &cncCmd, 100/portTICK_PERIOD_MS);

				ESP_LOGI(CTRL_TAG, "[%s]", cncCmd.cmd);

				moveZ = 0;
			}
			
			continue;
		}

		moveCount++;

		float z_shift = z_dist_per_tick();
		float xy_shift = gpio_get_level(XY_DIST_PIN) == 0 ? MOVE_1X_STEP : MOVE_10X_STEP;

		switch (move) {
			case MOVE_None: break;
			case MOVE_Z_CW: moveZ += z_shift;	break;
			case MOVE_Z_CCW: moveZ -= (state->z_probe ? 0 : z_shift);	break;			
			case MOVE_X_CW: moveX += xy_shift;	break;
			case MOVE_X_CCW: moveX -= xy_shift;	break;			
			case MOVE_Y_CW: moveY += xy_shift;	break;
			case MOVE_Y_CCW: moveY -= xy_shift;	break;			
		}
	}

	vTaskDelete(NULL);
}

static void encodersTask(void* args) {
	xQueueHandle isrQueue = get_gpio_queue();

	uint8_t zEncoder = 0; 

	uint32_t gpio_state = 0;
	Movement_t move = MOVE_None;

	for(;;) {
        if (xQueueReceive(isrQueue, (void *)&gpio_state, portMAX_DELAY) == pdFALSE) continue;

		move = MOVE_None;

		if( (gpio_state & ENC_Z_FLAG) != 0) {
			zEncoder = zEncoder << 2 | (GET_BIT(gpio_state, GPIO_Z_A_BIT) << 1) | GET_BIT(gpio_state, GPIO_Z_B_BIT);
			if(zEncoder == ENC_Z_CW) move = MOVE_Z_CW;
			if(zEncoder == ENC_Z_CCW) move = MOVE_Z_CCW;
		}

		if(move != MOVE_None) xQueueSend(move_queue, &move, 1/portTICK_PERIOD_MS);
    }
}

static void federationOverrideTask() {
	machineStatus_t *state = get_cnc_status();
	xQueueHandle fedChangeQ = get_cncinstant_queue();

	static int federationOverride = 0;

	for(;;) {
		vTaskDelay(250/portTICK_PERIOD_MS);
		if(!state->active) continue;

		adc2_get_raw(FEDERATION_OVERRIDE, ADC_WIDTH_12Bit, &federationOverride);

		federationOverride >>= 2;
		federationOverride = 10+(190*federationOverride)/(4096>>2);
				

		federationOverride = (int)state->feed_override - federationOverride;
		ESP_LOGI(CTRL_TAG, "ov: %d", federationOverride);

		char cmd = 0;

		if(federationOverride != 0) {
			if(federationOverride <= -10) cmd = CMD_FEED_OV_INC_10;
			if(federationOverride < -1) cmd = CMD_FEED_OV_INC_1;
			if(federationOverride > 1) cmd = CMD_FEED_OV_DEC_1;
			if(federationOverride >= 10) cmd = CMD_FEED_OV_DEC_10;
			xQueueSend(fedChangeQ, &cmd, portTICK_PERIOD_MS);
		}
	}

	vTaskDelete(NULL);
}

void init_controls(void) {
	move_queue = xQueueCreate(32, sizeof(Movement_t));

	esp_log_level_set(CTRL_TAG, ESP_LOG_WARN);

	gpio_pad_select_gpio(Z_DIST_PIN);
	gpio_set_direction(Z_DIST_PIN, GPIO_MODE_INPUT);
	gpio_set_pull_mode(Z_DIST_PIN, GPIO_PULLDOWN_ONLY);

	gpio_pad_select_gpio(XY_DIST_PIN);
	gpio_set_direction(XY_DIST_PIN, GPIO_MODE_INPUT);
	gpio_set_pull_mode(XY_DIST_PIN, GPIO_PULLDOWN_ONLY);


	adc2_config_channel_atten(FEDERATION_OVERRIDE, ADC_ATTEN_DB_11);

	xTaskCreate(encodersTask, "encodersTask", 1024, NULL, configMAX_PRIORITIES, NULL);
	xTaskCreate(movementTask, "movementTask", 2048, NULL, 1, NULL);

	xTaskCreate(federationOverrideTask, "federationOverrideReadTask", 2048, NULL, 1, NULL);
}