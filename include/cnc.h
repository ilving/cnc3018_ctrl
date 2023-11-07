#ifndef __CNC_H
#define __CNC_H

// #include "freertos/FreeRTOS.h"
// #include "freertos/queue.h"
// #include "driver/gpio.h"
#include "driver/uart.h"

#define CMD_STATUS_REPORT '?'
#define CMD_FEED_HOLD '!'
#define CMD_CYCLE_START '~'
#define CMD_RESET 0x18 // ctrl-x.
#define CMD_SAFETY_DOOR '@'

#define CANDLE_UART UART_NUM_0
#define CNC_UART UART_NUM_2

#define UART_BUF_SIZE (127)

typedef struct {
    char cmd[UART_BUF_SIZE];
} qItem;

extern void init_cnc(void);
extern xQueueHandle get_cnctx_queue();
#endif