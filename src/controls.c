#include <inttypes.h>
#include <string.h>

#include <esp_rom_gpio.h>
#include <driver/adc.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "esp_log.h"

// ------------------

#include "generic.h"

#include "cnc.h"
#include "controls.h"

static QueueHandle_t move_queue = NULL;
QueueHandle_t get_move_queue() { return move_queue; }

float z_dist_per_tick() { return gpio_get_level(Z_DIST_PIN) == 0 ? MOVE_1X_STEP : MOVE_10X_STEP; };
float xy_dist_per_tick() { return gpio_get_level(XY_DIST_PIN) == 0 ? MOVE_1X_STEP : MOVE_10X_STEP; };

static void movementTask(void* args) {
	QueueHandle_t cncTx = get_cnctx_queue();

	Movement_t move = MOVE_None;
	static qItem cncCmd;
	static uint8_t moveCount = 0;

	float moveX=0, moveY=0, moveZ=0;

	machineStatus_t *state = get_cnc_status();

	for(;;) {
        BaseType_t qGet = xQueueReceive(move_queue, (void *)&move, pdMS_TO_TICKS(100));
		if(state->state != CNC_STATE_IDLE && state->state != CNC_STATE_JOG) continue;

		if (qGet == pdFALSE || moveCount == ENCODER_PULSES/2) {
			moveCount = 0;

			if (moveX != 0 || moveY != 0) {
				snprintf(cncCmd.cmd, UART_BUF_SIZE, "$J=G21G91X%.3fY%.3fF500\r\n", moveX, moveY);
				xQueueSend(cncTx, &cncCmd, pdMS_TO_TICKS(100));

				ESP_LOGD(CTRL_TAG, "[%s]", cncCmd.cmd);

				moveX = moveY = 0;
			}
			if (moveZ != 0) {
				snprintf(cncCmd.cmd, UART_BUF_SIZE, "$J=G21G91Z%.3fF500\r\n", moveZ);
				xQueueSend(cncTx, &cncCmd, pdMS_TO_TICKS(100));

				ESP_LOGD(CTRL_TAG, "[%s]", cncCmd.cmd);

				moveZ = 0;
			}
			
			continue;
		}

		moveCount++;

		float z_shift = z_dist_per_tick();
		float xy_shift = xy_dist_per_tick();

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

static void federationOverrideTask() {
	machineStatus_t *state = get_cnc_status();
	QueueHandle_t fedChangeQ = get_cncinstant_queue();

	static int federationOverride = 0;

	for(;;) {
		vTaskDelay(250/portTICK_PERIOD_MS);
		if(!state->active) continue;

		adc2_get_raw(FEDERATION_OVERRIDE, ADC_WIDTH_BIT_12, &federationOverride);

		federationOverride >>= 2;
		federationOverride = 10+(189*federationOverride)/(4096>>2);
				

		federationOverride = (int)state->feed_override - federationOverride;
		//ESP_LOGI(CTRL_TAG, "ov: %d", federationOverride);

		char cmd = 0;

		if(federationOverride != 0) {
			if(federationOverride < -1) cmd = CMD_FEED_OV_INC_1;
			if(federationOverride <= -10) cmd = CMD_FEED_OV_INC_10;
			if(federationOverride > 1) cmd = CMD_FEED_OV_DEC_1;
			if(federationOverride >= 10) cmd = CMD_FEED_OV_DEC_10;
			xQueueSend(fedChangeQ, &cmd, portTICK_PERIOD_MS);
		}
	}

	vTaskDelete(NULL);
}

void init_controls(void) {
	move_queue = xQueueCreate(32, sizeof(Movement_t));

	esp_log_level_set(CTRL_TAG, ESP_LOG_VERBOSE);

	esp_rom_gpio_pad_select_gpio(Z_DIST_PIN);
	gpio_set_direction(Z_DIST_PIN, GPIO_MODE_INPUT);
	gpio_set_pull_mode(Z_DIST_PIN, GPIO_PULLDOWN_ONLY);

	esp_rom_gpio_pad_select_gpio(XY_DIST_PIN);
	gpio_set_direction(XY_DIST_PIN, GPIO_MODE_INPUT);
	gpio_set_pull_mode(XY_DIST_PIN, GPIO_PULLDOWN_ONLY);

	adc2_config_channel_atten(FEDERATION_OVERRIDE, ADC_ATTEN_DB_11);
	
	xTaskCreate(movementTask, "movementTask", 2048, NULL, 1, NULL);
	xTaskCreate(federationOverrideTask, "federationOverrideReadTask", 2048, NULL, 1, NULL);
}