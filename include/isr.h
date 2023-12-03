#ifndef __ISR_H
#define __ISR_H

#include "freertos/FreeRTOS.h"

#include "freertos/queue.h"
#include "driver/gpio.h"

#define ENC_Z_A GPIO_NUM_39
#define ENC_Z_B GPIO_NUM_36

#define ENC_X_A GPIO_NUM_34
#define ENC_X_B GPIO_NUM_35

#define ENC_Y_A GPIO_NUM_32
#define ENC_Y_B GPIO_NUM_33

#define ENC_Z_FLAG 0
#define ENC_X_FLAG 1
#define ENC_Y_FLAG 2

extern void init_isr(void);

#endif