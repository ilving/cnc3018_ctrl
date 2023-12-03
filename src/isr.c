#include "freertos/FreeRTOS.h"

#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/pulse_cnt.h"

#include "driver/gpio.h"
#include "esp_log.h"

#include "isr.h"
#include "controls.h"
#include "generic.h"

typedef struct {
	uint8_t flag;
	pcnt_unit_handle_t unit;
	pcnt_channel_handle_t chan_a, chan_b;
	pcnt_chan_config_t chan_a_config, chan_b_config;
	QueueHandle_t queue;
} Enccfg_t;

static bool pcnt_on_reach(pcnt_unit_handle_t unit, const pcnt_watch_event_data_t *edata, void *user_ctx)
{
    BaseType_t high_task_wakeup;
    QueueHandle_t queue = (QueueHandle_t)user_ctx;
    // send event data to queue, from this interrupt callback
	
    xQueueSendFromISR(queue, &(edata->zero_cross_mode), &high_task_wakeup);
    return (high_task_wakeup == pdTRUE);
}

void encHandlerTask(void* arg) {
	Enccfg_t* cfg = (Enccfg_t*)arg;
	pcnt_unit_zero_cross_mode_t event = 0;

	while (1) {
        if (xQueueReceive(cfg->queue, (void*)&event, pdMS_TO_TICKS(1000))) {

			QueueHandle_t move_queue = get_move_queue();
			if (move_queue == NULL) {
				ESP_LOGE(ISR_TAG, "get move_queue failed");
				continue;
			}

			Movement_t move = MOVE_None;

			if(event == PCNT_UNIT_ZERO_CROSS_NEG_ZERO) {
				switch (cfg->flag) {
					case ENC_Z_FLAG: move = MOVE_Z_CW; break;
					case ENC_X_FLAG: move = MOVE_X_CW; break;
					case ENC_Y_FLAG: move = MOVE_Y_CW; break;
				}
			} else if (event == PCNT_UNIT_ZERO_CROSS_POS_ZERO) {
				switch (cfg->flag) {
					case ENC_Z_FLAG: move = MOVE_Z_CCW; break;
					case ENC_X_FLAG: move = MOVE_X_CCW; break;
					case ENC_Y_FLAG: move = MOVE_Y_CCW; break;
				}
			}

			if(move != MOVE_None) xQueueSend(move_queue, &move, pdMS_TO_TICKS(10));
            ESP_LOGD(ISR_TAG, "Watch point event %d, evt: %d, mv %d", cfg->flag, event, move);

        }
	}

	vTaskDelete(NULL);
}

void init_isr() {
	esp_log_level_set(ISR_TAG, ESP_LOG_VERBOSE);

	static Enccfg_t encoders[] = {
		{
			.flag = ENC_Z_FLAG,
			.unit = NULL, .chan_a = NULL, .chan_b = NULL,
			.chan_a_config = {.edge_gpio_num = ENC_Z_A,.level_gpio_num = ENC_Z_B},
			.chan_b_config = {.edge_gpio_num = ENC_Z_B,.level_gpio_num = ENC_Z_A},
			.queue = NULL,
		},
		{
			.flag = ENC_X_FLAG,
			.unit = NULL, .chan_a = NULL, .chan_b = NULL,
			.chan_a_config = {.edge_gpio_num = ENC_X_A,.level_gpio_num = ENC_X_B},
			.chan_b_config = {.edge_gpio_num = ENC_X_B,.level_gpio_num = ENC_X_A},
			.queue = NULL,
		},
		{
			.flag = ENC_Y_FLAG,
			.unit = NULL, .chan_a = NULL, .chan_b = NULL,
			.chan_a_config = {.edge_gpio_num = ENC_Y_A,.level_gpio_num = ENC_Y_B},
			.chan_b_config = {.edge_gpio_num = ENC_Y_B,.level_gpio_num = ENC_Y_A},
			.queue = NULL,
		},
	};

    static pcnt_unit_config_t unit_config = {
        .high_limit = 4,
        .low_limit = -4,
    };
    static pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = 1000,
    };

    static pcnt_event_callbacks_t cbs = {.on_reach = pcnt_on_reach};

	for(int i = 0;i<sizeof(encoders)/sizeof(encoders[0]);i++) {
		ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &encoders[i].unit));
		ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(encoders[i].unit, &filter_config));


		ESP_ERROR_CHECK(pcnt_new_channel(encoders[i].unit, &encoders[i].chan_a_config, &encoders[i].chan_a));
		ESP_ERROR_CHECK(pcnt_new_channel(encoders[i].unit, &encoders[i].chan_b_config, &encoders[i].chan_b));

		ESP_ERROR_CHECK(pcnt_channel_set_edge_action(encoders[i].chan_a, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE));
		ESP_ERROR_CHECK(pcnt_channel_set_level_action(encoders[i].chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
		ESP_ERROR_CHECK(pcnt_channel_set_edge_action(encoders[i].chan_b, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE));
		ESP_ERROR_CHECK(pcnt_channel_set_level_action(encoders[i].chan_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

		ESP_ERROR_CHECK(pcnt_unit_add_watch_point(encoders[i].unit, 0));
		encoders[i].queue = xQueueCreate(10, sizeof(pcnt_unit_zero_cross_mode_t));
		ESP_ERROR_CHECK(pcnt_unit_register_event_callbacks(encoders[i].unit, &cbs, encoders[i].queue));

		ESP_ERROR_CHECK(pcnt_unit_enable(encoders[i].unit));
		ESP_ERROR_CHECK(pcnt_unit_clear_count(encoders[i].unit));
		ESP_ERROR_CHECK(pcnt_unit_start(encoders[i].unit));

		xTaskCreate(encHandlerTask, "encHandlerTask", 2048, &encoders[i], 1, NULL);
	}
}