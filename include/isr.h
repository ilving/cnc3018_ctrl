#ifndef __ISR_H
#define __ISR_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

#define ENC_Z_A GPIO_NUM_39
#define ENC_Z_B GPIO_NUM_36
#define ENC_Z_FLAG (1<<8)

extern void init_isr(void);
extern xQueueHandle get_gpio_queue();

#endif