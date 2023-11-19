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

#define CMD_FEED_OV_NORMAL 0x90
#define CMD_FEED_OV_INC_10 0x91
#define CMD_FEED_OV_DEC_10 0x92
#define CMD_FEED_OV_INC_1  0x93
#define CMD_FEED_OV_DEC_1  0x94

#define CMD_DOOR 0x84 // ???
#define CMD_JOG_CANCEL 0x85

#define CMD_RAPID_100 0x95
#define CMD_RAPID_50 0x96
#define CMD_RAPID_25 0x97

#define CMD_SPINDLE_100 0x99
#define CMD_SPINDLE_INC_10 0x9A
#define CMD_SPINDLE_DEC_10 0x9B
#define CMD_SPINDLE_INC_1 0x9C
#define CMD_SPINDLE_DEC_1 0x9D
#define CMD_SPINDLE_STOP 0x9E

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
    bool active;
    
    cncState_t state;
    uint8_t coord_system;

    uint8_t feed_override, rapid_override, spindle_override;

    bool z_probe;

    float mx, my, mz; // Machine coords
    float wo_x, wo_y, wo_z; // Working offset
    float wx, wy, wz; // Working coords
} machineStatus_t;

#define PINSTATE_Z_PROBE 'P'

#define PINSTATE_Z_LIMIT 'Z'
#define PINSTATE_X_LIMIT 'X'
#define PINSTATE_Y_LIMIT 'Y'

extern void init_cnc(void);
extern xQueueHandle get_cnctx_queue();
extern xQueueHandle get_cncinstant_queue();

extern machineStatus_t* get_cnc_status();
extern const char* get_cnc_state_name(cncState_t state);
#endif