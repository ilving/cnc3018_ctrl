#include "freertos/FreeRTOS.h"

#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/gpio.h"


#include "isr.h"

static xQueueHandle gpio_queue = NULL;

xQueueHandle get_gpio_queue() { return gpio_queue; }

static void IRAM_ATTR isrGPIO(void* arg) {
	uint32_t regval = REG_READ(GPIO_IN1_REG) & 0xFF;

	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	BaseType_t xResult = pdFALSE;

	uint32_t gpio_intr_status = READ_PERI_REG(GPIO_STATUS_REG);
	SET_PERI_REG_MASK(GPIO_STATUS_W1TC_REG, gpio_intr_status);      // Clear interrupt status for GPIO0-31

	uint32_t gpio1_intr_status = READ_PERI_REG(GPIO_STATUS1_REG);
	SET_PERI_REG_MASK(GPIO_STATUS1_W1TC_REG, gpio1_intr_status);   // Clear interrupt status for GPIO32-39

	// Определяем номер порта, с которого поступило событие
	uint32_t gpio_num = 0;
	do {
		if (gpio_num < 32) {
			if (gpio_intr_status & BIT(gpio_num)) { // Отправляем в очередь номер gpio с прерыванием
				// blank for now
			}
		} else {
			if (gpio1_intr_status & BIT(gpio_num - 32)) { // Отправляем в очередь задачи номер нажатой кнопки
				switch(gpio_num) {
					case ENC_Z_A:
					case ENC_Z_B:
						regval |= ENC_Z_FLAG;
				}
			}
		};
	} while (++gpio_num < GPIO_PIN_COUNT);

	xResult = xQueueSendFromISR(gpio_queue, &regval, &xHigherPriorityTaskWoken);
	if (xResult == pdPASS) portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void init_isr() {
	gpio_queue = xQueueCreate(32, sizeof(uint32_t));

	gpio_pad_select_gpio(ENC_Z_A);
	gpio_set_direction(ENC_Z_A, GPIO_MODE_INPUT);
	gpio_set_pull_mode(ENC_Z_A, GPIO_FLOATING);

	gpio_pad_select_gpio(ENC_Z_B);
	gpio_set_direction(ENC_Z_B, GPIO_MODE_INPUT);
	gpio_set_pull_mode(ENC_Z_B, GPIO_FLOATING);

	gpio_isr_register(isrGPIO, NULL, 0, NULL);

	gpio_set_intr_type(ENC_Z_A, GPIO_INTR_ANYEDGE);	gpio_intr_enable(ENC_Z_A);
	gpio_set_intr_type(ENC_Z_B, GPIO_INTR_ANYEDGE);	gpio_intr_enable(ENC_Z_B);
}