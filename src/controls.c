#include <inttypes.h>

#include "esp_log.h"

// ------------------

#include "generic.h"

#include "cnc.h"
#include "isr.h"
#include "controls.h"

static xQueueHandle move_queue = NULL;

static void movementTask(void* args) {
	xQueueHandle cncTx = get_cnctx_queue();

	Movement_t move = MOVE_None;
	static qItem cncCmd;
	static uint8_t moveCount = 0;

	float moveX=0, moveY=0, moveZ=0;

	for(;;) {
        BaseType_t qGet = xQueueReceive(move_queue, (void *)&move, 100/portTICK_PERIOD_MS);
		
		if (qGet == pdFALSE || moveCount == ENCODER_PULSES/2) {
			moveCount = 0;

			if (moveX != 0 || moveY != 0) {
				snprintf(cncCmd.cmd, UART_BUF_SIZE, "$J=G21G91X%.3fY%.3fF500\r\n", moveX, moveY);
				xQueueSend(cncTx, &cncCmd, 100/portTICK_PERIOD_MS);

				ESP_LOGI(CONTROLS_TAG, "[%s]", cncCmd.cmd);

				moveX = moveY = 0;
			}
			if (moveZ != 0) {
				snprintf(cncCmd.cmd, UART_BUF_SIZE, "$J=G21G91Z%.3fF500\r\n", moveZ);
				xQueueSend(cncTx, &cncCmd, 100/portTICK_PERIOD_MS);

				ESP_LOGI(CONTROLS_TAG, "[%s]", cncCmd.cmd);

				moveZ = 0;
			}
			
			continue;
		}

		moveCount++;

		float z_shift = gpio_get_level(Z_DIST_PIN) == 0 ? MOVE_1X : MOVE_10X;
		float xy_shift = gpio_get_level(XY_DIST_PIN) == 0 ? MOVE_1X : MOVE_10X;

		switch (move) {
			case MOVE_None: break;
			case MOVE_Z_CW: moveZ += z_shift;	break;
			case MOVE_Z_CCW: moveZ -= z_shift;	break;			
			case MOVE_X_CW: moveX += xy_shift;	break;
			case MOVE_X_CCW: moveX -= xy_shift;	break;			
			case MOVE_Y_CW: moveY += xy_shift;	break;
			case MOVE_Y_CCW: moveY -= xy_shift;	break;			
		}
		
		//ESP_LOGI(CRX_TAG, "move: %u", (uint8_t)move);
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

void init_controls(void) {
	move_queue = xQueueCreate(32, sizeof(Movement_t));
	esp_log_level_set(CONTROLS_TAG, ESP_LOG_ERROR);

	gpio_pad_select_gpio(Z_DIST_PIN);
	gpio_set_direction(Z_DIST_PIN, GPIO_MODE_INPUT);
	gpio_set_pull_mode(Z_DIST_PIN, GPIO_PULLDOWN_ONLY);

	xTaskCreate(encodersTask, "encodersTask", 1024, NULL, configMAX_PRIORITIES, NULL);
	xTaskCreate(movementTask, "movementTask", 2048, NULL, 1, NULL);
}