#include <string.h>

#include "cnc.h"
#include "screen.h"
#include "controls.h"

SSD1306_t lcd;

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define SCREEN_SYM_WIDTH (SCREEN_WIDTH/8)
#define SCREEN_SYM_HEIGHT (SCREEN_HEIGHT/8)
#define SCREEN_BUF_LEN SCREEN_SYM_WIDTH*SCREEN_SYM_HEIGHT

#define SCREEN_X_STRING 2 * SCREEN_SYM_WIDTH
#define SCREEN_Y_STRING 3 * SCREEN_SYM_WIDTH
#define SCREEN_Z_STRING 4 * SCREEN_SYM_WIDTH

#define SCREEN_STATE_POS 0 * SCREEN_SYM_WIDTH + 0
#define SCREEN_CSYSTEM_POS 0 * SCREEN_SYM_WIDTH + 0x0D

#define SCREEN_ZPROBE_POS 1 * SCREEN_SYM_WIDTH + 0x0F

#define SCREEN_FEED_OVERRIDE_POS 1 * SCREEN_SYM_WIDTH + 0x00
#define SCREEN_SPINDLE_OVERRIDE_POS 1 * SCREEN_SYM_WIDTH + 0x05

#define COORDS_FORMAT "%8.3f"
/*
  0123456789ABCDEF
0 idle.........Gxx
1 F100.S100......P
2 Z: .-111.000 x10
3 X: ......... x10
4 Y: ......... x10
*/
static void screenTask(void* args) {
	static char buf[SCREEN_BUF_LEN];

	machineStatus_t *state = get_cnc_status();

	for(;;) {
		memset(buf, ' ', SCREEN_BUF_LEN);
		vTaskDelay(100/portTICK_PERIOD_MS);

		if(!state->active)sprintf(&buf[SCREEN_STATE_POS], "no CNC");
		else sprintf(&buf[SCREEN_STATE_POS], "%-7s", get_cnc_state_name(state->state));

		sprintf(&buf[SCREEN_CSYSTEM_POS], "G%02u", 54+state->coord_system);

		if(state->z_probe) buf[SCREEN_ZPROBE_POS] = PINSTATE_Z_PROBE;

		sprintf(&buf[SCREEN_FEED_OVERRIDE_POS], "F%3u", state->feed_override);
		sprintf(&buf[SCREEN_SPINDLE_OVERRIDE_POS], "S%3u", state->spindle_override);

		sprintf(&buf[SCREEN_Z_STRING + 0], "Z:");

		sprintf(&buf[SCREEN_Z_STRING + 3], COORDS_FORMAT, state->wz);

		// Z: move x1 vs x10
		if(gpio_get_level(Z_DIST_PIN) == 0)	sprintf(&buf[SCREEN_Z_STRING + 0x0D], "x 1");
		else sprintf(&buf[SCREEN_Z_STRING + 0x0D], "x10");

		// XY: move x1 vs x10
		sprintf(&buf[SCREEN_X_STRING + 0], "X:");
		sprintf(&buf[SCREEN_X_STRING + 3], COORDS_FORMAT, state->wx);
		if(gpio_get_level(XY_DIST_PIN) == 0)sprintf(&buf[SCREEN_X_STRING + 0x0D], "x 1");
		else sprintf(&buf[SCREEN_X_STRING + 0x0D], "x10");

		sprintf(&buf[SCREEN_Y_STRING + 0], "Y:");
		sprintf(&buf[SCREEN_Y_STRING + 3], COORDS_FORMAT, state->wy);


		// send to display
		for (int i=0; i<SCREEN_SYM_HEIGHT; i++) {
			ssd1306_display_text(&lcd, i, &buf[SCREEN_SYM_WIDTH*i], SCREEN_SYM_WIDTH, false);
		}
	}

	vTaskDelete(NULL);
}

void init_screen(void) {
	i2c_master_init(&lcd, SDA_GPIO, SCL_GPIO, GPIO_NUM_NC);
	ssd1306_init(&lcd, SCREEN_WIDTH, SCREEN_HEIGHT);
	
	ssd1306_clear_screen(&lcd, false);
	ssd1306_contrast(&lcd, 0xff);

	xTaskCreate(screenTask, "screenTask", 2048, NULL, 1, NULL);
}
