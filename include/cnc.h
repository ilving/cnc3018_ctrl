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

typedef enum {
    CNC_STATE_UNDEFINED,
    CNC_STATE_IDLE,
    CNC_STATE_RUN,
    CNC_STATE_HOLD,
    CNC_STATE_JOG,
    CNC_STATE_ALARM,
    CNC_STATE_DOOR,
    CNC_STATE_CHECK,
    CNC_STATE_HOME,
    CNC_STATE_SLEEP,
    CNC_STATE_MAX
} cncState_t;

typedef struct {
    cncState_t state;
    uint8_t coord_system;
    uint8_t feed_override;
    uint8_t spindle_override;
    uint8_t z_probe;
} machineStatus_t;

extern void init_cnc(void);
extern xQueueHandle get_cnctx_queue();

extern machineStatus_t* get_cnc_status();
extern const char* get_cnc_state_name(cncState_t state);
#endif