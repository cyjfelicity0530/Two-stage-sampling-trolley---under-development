#include "servo.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/ledc.h"
#include "hal/ledc_types.h"
#include "system_state.h" // 引入全局状态结构体定义


static const char *TAG = "SERVO";

// --- 舵机PWM与定时器参数配置 ---
// ESP32-S3 的 LEDC 使用低速模式即可
#define SERVO_LEDC_MODE         LEDC_LOW_SPEED_MODE 
#define SERVO_LEDC_TIMER        LEDC_TIMER_0        // 共用定时器0
#define SERVO_LEDC_DUTY_RES     LEDC_TIMER_13_BIT   // 13位分辨率 (值范围: 0 ~ 8191)
#define SERVO_LEDC_FREQ_HZ      50                  // PWM频率: 50Hz (周期20ms)

// --- 舵机脉宽参数 (大多数标准180度舵机参数，可根据实际微调) ---
#define SERVO_MIN_PULSEWIDTH_US 500                 // 0度对应的脉宽: 0.5ms
#define SERVO_MAX_PULSEWIDTH_US 2500                // 180度对应的脉宽: 2.5ms
#define SERVO_MAX_DEGREE        180.0f              // 最大角度

/**
 * @brief 根据微秒数计算LEDC占空比值
 */
static uint32_t calculate_duty(uint32_t pulsewidth_us)
{
    // 周期时间 = 1000000us / 50Hz = 20000us
    // 分辨率 = 13位 (2^13 = 8192)
    // 占空比值 = (脉宽us / 20000us) * 8192
    return (pulsewidth_us * (1 << SERVO_LEDC_DUTY_RES)) / (1000000 / SERVO_LEDC_FREQ_HZ);
}

esp_err_t servo_init(int gpio_num, ledc_channel_t channel)
{
    esp_err_t err;

    // 1. 配置 LEDC 定时器 
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = SERVO_LEDC_MODE,
        .timer_num        = SERVO_LEDC_TIMER,
        .duty_resolution  = SERVO_LEDC_DUTY_RES,
        .freq_hz          = SERVO_LEDC_FREQ_HZ,
        .clk_cfg          = LEDC_USE_APB_CLK 
    };
    err = ledc_timer_config(&ledc_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LEDC timer config failed!");
        return err;
    }

    // 2. 配置 LEDC 通道
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = SERVO_LEDC_MODE,
        .channel        = channel,
        .timer_sel      = SERVO_LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = gpio_num,
        .duty           = calculate_duty(SERVO_MIN_PULSEWIDTH_US), // 上电默认设置在0度位置
        .hpoint         = 0
    };
    err = ledc_channel_config(&ledc_channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LEDC channel config failed!");
        return err;
    }

    ESP_LOGI(TAG, "Servo initialized on GPIO %d, Channel %d", gpio_num, channel);
    return ESP_OK;
}

esp_err_t servo_set_angle(ledc_channel_t channel, float angle)
{
    // 限制角度在 0 到 180 度之间
    if (angle < 0.0f) angle = 0.0f;
    if (angle > SERVO_MAX_DEGREE) angle = SERVO_MAX_DEGREE;

    // 线性映射角度到脉宽微秒数 (0~180度 -> 500~2500us)
    uint32_t pulsewidth_us = SERVO_MIN_PULSEWIDTH_US + 
        (uint32_t)(((SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US) * angle) / SERVO_MAX_DEGREE);

    // 计算对应的占空比数值
    uint32_t duty = calculate_duty(pulsewidth_us);

    // 设置并更新占空比
    esp_err_t err = ledc_set_duty(SERVO_LEDC_MODE, channel, duty);
    if (err != ESP_OK) {
        return err;
    }
    g_state.servo_angle = (int)angle; // 更新全局状态中的舵机角度
    return ledc_update_duty(SERVO_LEDC_MODE, channel);
}