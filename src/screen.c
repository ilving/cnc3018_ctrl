#include <string.h>

#include "screen.h"
#include "controls.h"

SSD1306_t lcd;

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define SCREEN_SYM_WIDTH SCREEN_WIDTH/8
#define SCREEN_SYM_HEIGHT SCREEN_HEIGHT/8
#define SCREEN_BUF_LEN SCREEN_SYM_WIDTH*SCREEN_SYM_HEIGHT

#define SCREEN_Z_STRING 2 * SCREEN_SYM_WIDTH
#define SCREEN_XY_STRING 3 * SCREEN_SYM_WIDTH
/*
  0123456789ABCDEF
0 St
1 St ?
2 Z : ........ x10
3 XY: ........ x10
*/
static void screenTask(void* args) {
	static char buf[SCREEN_BUF_LEN];

	for(;;) {
		memset(buf, ' ', SCREEN_BUF_LEN);
		//sprintf(&buf[0], "S: ");

		sprintf(&buf[SCREEN_Z_STRING + 0], "Z :");

		// Z: move x1 vs x10
		if(gpio_get_level(Z_DIST_PIN) == 0)	sprintf(&buf[SCREEN_Z_STRING + 0x0D], "x 1");
		else sprintf(&buf[SCREEN_Z_STRING + 0x0D], "x10");

		// XY: move x1 vs x10
		sprintf(&buf[SCREEN_XY_STRING + 0], "XY:");
		if(gpio_get_level(XY_DIST_PIN) == 0)sprintf(&buf[SCREEN_XY_STRING + 0x0D], "x 1");
		else sprintf(&buf[SCREEN_XY_STRING + 0x0D], "x10");


		// send to display
		for (int i=0; i<SCREEN_SYM_HEIGHT; i++) {
			ssd1306_display_text(&lcd, i, &buf[SCREEN_SYM_WIDTH*i], SCREEN_SYM_WIDTH, false);
		}

		vTaskDelay(50/portTICK_PERIOD_MS);
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
