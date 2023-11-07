#ifndef __CONTROLS_H
#define __CONTROLS_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

#define ENCODER_PULSES 100

typedef enum {
	MOVE_None,
	MOVE_Z_CW, MOVE_Z_CCW,
	MOVE_X_CW, MOVE_X_CCW,
	MOVE_Y_CW, MOVE_Y_CCW,
} Movement_t;

extern void init_controls(void);

#define GPIO_Z_A_BIT 4
#define GPIO_Z_B_BIT 7
#define ENC_Z_CW 0xB4
#define ENC_Z_CCW 0x78

	// Z axis
	// B  A      CW
	// 00010000 
	// 10010000
	// 10000000
	// 00000000

	// B  A      CCW
	// 10000000
	// 10010000
	// 00010000 
	// 00000000

#endif