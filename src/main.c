#include <stdio.h>
#include <string.h>

#include "driver/uart.h"

#include "esp_log.h"
// ---------------------

#include "isr.h"
#include "cnc.h"
#include "controls.h"
#include "screen.h"

#define LOG_BUFFER_LEN 512

void app_main() {
    esp_log_level_set("*", ESP_LOG_VERBOSE);        // set all components to ERROR level

    init_cnc();
    init_isr();
    init_controls();

    init_screen();
}

int __log(const char *format, va_list args) {
    static char buf[LOG_BUFFER_LEN];
    vsnprintf(buf, LOG_BUFFER_LEN-1, format, args);

    uart_write_bytes(UART_NUM_0, buf, strnlen(buf, LOG_BUFFER_LEN));

	return 0;
}

