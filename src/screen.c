
#include "screen.h"

SSD1306_t lcd;

void init_screen(void) {
	i2c_master_init(&lcd, SDA_GPIO, SCL_GPIO, GPIO_NUM_NC);
	ssd1306_init(&lcd, 128, 64);
	
	ssd1306_clear_screen(&lcd, false);
	ssd1306_contrast(&lcd, 0xff);
	ssd1306_display_text(&lcd, 0, "Hello", 5, false);
}