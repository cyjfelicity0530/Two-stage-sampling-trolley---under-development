#ifndef __SERVO_H
#define __SERVO_H

#include "servo.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/ledc.h"
#include "hal/ledc_types.h"
// 舵机初始化配置
// gpio_num: 舵机信号线(PWM)连接的GPIO引脚
// channel: 分配的LEDC通道 (如 LEDC_CHANNEL_0, LEDC_CHANNEL_1 等)
esp_err_t servo_init(int gpio_num, ledc_channel_t channel);

// 设置特定通道上舵机的角度
// channel: 初始化时使用的LEDC通道
// angle: 目标角度 (0 ~ 180度)
esp_err_t servo_set_angle(ledc_channel_t channel, float angle);

#endif // __SERVO_H