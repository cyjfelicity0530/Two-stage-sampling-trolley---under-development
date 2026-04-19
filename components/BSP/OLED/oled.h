#ifndef OLED_H
#define OLED_H

#include "esp_err.h"
#include <stdint.h>

// ==================== 【用户配置】 ====================
#define OLED_I2C_SDA_PIN  16
#define OLED_I2C_SCL_PIN  17
#define OLED_RESET_PIN    0   // 无复位引脚填 -1
#define OLED_I2C_ADDR     0x3c // 0x3C/0x3D
#define OLED_WIDTH        128
#define OLED_HEIGHT       64

// ==================== 对外接口 ====================
esp_err_t oled_init(void);
void oled_clear(void);
void oled_update(void);
void oled_draw_pixel(uint8_t x, uint8_t y);
void oled_draw_string(uint8_t x, uint8_t y, const char *str);


#endif // OLED_H