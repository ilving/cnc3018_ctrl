#ifndef __SCREEN_H
#define __SCREEN_H

#include "driver/i2c.h"
#include "driver/gpio.h"

#include "components/ssd1306/ssd1306.h"

#define SDA_GPIO GPIO_NUM_21
#define SCL_GPIO GPIO_NUM_22

extern void init_screen(void);

#endif