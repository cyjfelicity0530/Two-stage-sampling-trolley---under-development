#include "dynamo.h"
#include "driver/ledc.h"
#include "driver/pulse_cnt.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "BSP_DYNAMO";

// --- 引脚定义配置 (需根据实际 ESP32-P4 接线修改) ---
// 左履带驱动引脚 (PWM)
#define MOTOR_LEFT_RPWM_PIN   8  
#define MOTOR_LEFT_LPWM_PIN   9  
// 右履带驱动引脚 (PWM)
#define MOTOR_RIGHT_RPWM_PIN  10 
#define MOTOR_RIGHT_LPWM_PIN  11 

// 编码器引脚 (待定，先用12, 13, 14, 15占位，您可以随时修改)
#define ENCODER_LEFT_A_PIN    12
#define ENCODER_LEFT_B_PIN    13
#define ENCODER_RIGHT_A_PIN   14
#define ENCODER_RIGHT_B_PIN   15

// LEDC 配置参数
#define LEDC_TIMER            LEDC_TIMER_0
#define LEDC_MODE             LEDC_LOW_SPEED_MODE
#define LEDC_DUTY_RES         LEDC_TIMER_10_BIT // 10位分辨率: 0-1023
#define LEDC_FREQUENCY        20000             // 20kHz

// PCNT (脉冲计数器) 句柄
static pcnt_unit_handle_t pcnt_unit_left = NULL;
static pcnt_unit_handle_t pcnt_unit_right = NULL;

// PID 定时器句柄
static esp_timer_handle_t pid_timer_handle;

// PID 模型结构体
typedef struct {
    float kp, ki, kd;
    float target;
    float current;
    float error_sum;
    float last_error;
    float out;
    float out_max;
} pid_ctrl_t;

// 实例化左右电机的 PID (这里的 Kp,Ki,Kd 需在真机测试时整定)
static pid_ctrl_t pid_left  = { .kp = 2.0f, .ki = 0.05f, .kd = 0.5f, .out_max = 1023.0f };
static pid_ctrl_t pid_right = { .kp = 2.0f, .ki = 0.05f, .kd = 0.5f, .out_max = 1023.0f };

// 底层 PWM 设置函数 (只供 PID 内部调用)
static void dynamo_set_pwm(int left_pwm, int right_pwm) {
    // 限制速度在 -1023 到 1023 之间
    left_pwm = (left_pwm > 1023) ? 1023 : (left_pwm < -1023) ? -1023 : left_pwm;
    right_pwm = (right_pwm > 1023) ? 1023 : (right_pwm < -1023) ? -1023 : right_pwm;

    if (left_pwm >= 0) {
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_0, left_pwm);
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_1, 0);
    } else {
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_0, 0);
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_1, -left_pwm);
    }
    
    if (right_pwm >= 0) {
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_2, right_pwm);
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_3, 0);
    } else {
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_2, 0);
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_3, -right_pwm);
    }
    
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_0);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_1);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_2);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_3);
}

// PID 计算核心
static float pid_compute(pid_ctrl_t *pid) {
    float error = pid->target - pid->current;
    pid->error_sum += error;
    
    // 积分限幅 (抗积分饱和)
    if (pid->error_sum > 2000.0f) pid->error_sum = 2000.0f;
    if (pid->error_sum < -2000.0f) pid->error_sum = -2000.0f;

    float d_error = error - pid->last_error;
    
    pid->out = (pid->kp * error) + (pid->ki * pid->error_sum) + (pid->kd * d_error);
    pid->last_error = error;
    
    // 输出限幅
    if (pid->out > pid->out_max) pid->out = pid->out_max;
    if (pid->out < -pid->out_max) pid->out = -pid->out_max;
    
    return pid->out;
}

// PID 周期控制任务 (每 50ms 触发一次)
static void pid_timer_cb(void* arg) {
    int left_pulse = 0;
    int right_pulse = 0;
    
    // 1. 获取 50ms 内编码器积累的脉冲数，并清零
    if (pcnt_unit_left) {
        pcnt_unit_get_count(pcnt_unit_left, &left_pulse);
        pcnt_unit_clear_count(pcnt_unit_left);
    }
    if (pcnt_unit_right) {
        pcnt_unit_get_count(pcnt_unit_right, &right_pulse);
        pcnt_unit_clear_count(pcnt_unit_right);
    }

    // 2. 更新当前速度反馈指标
    pid_left.current = (float)left_pulse;
    pid_right.current = (float)right_pulse;

    // 3. 计算 PID 占空比输出
    int left_pwm = (int)pid_compute(&pid_left);
    int right_pwm = (int)pid_compute(&pid_right);

    // 4. 给电机下发实际 PWM
    dynamo_set_pwm(left_pwm, right_pwm);
}

// 初始化硬件 PCNT (解码 775 尾部的 AB 相霍尔编码器)
static esp_err_t encoder_init(void) {
    ESP_LOGI(TAG, "初始化硬件霍尔编码器 (PCNT)...");
    
    pcnt_unit_config_t unit_config = {
        .high_limit = 32767,
        .low_limit = -32768,
    };

    // --- 左电机 PCNT ---
    pcnt_new_unit(&unit_config, &pcnt_unit_left);
    pcnt_glitch_filter_config_t filter_config = { .max_glitch_ns = 1000, };
    pcnt_unit_set_glitch_filter(pcnt_unit_left, &filter_config);
    
    pcnt_chan_config_t chan_a_config = { 
        .edge_gpio_num = ENCODER_LEFT_A_PIN, 
        .level_gpio_num = ENCODER_LEFT_B_PIN, 
    };
    pcnt_channel_handle_t pcnt_chan_a = NULL;
    pcnt_new_channel(pcnt_unit_left, &chan_a_config, &pcnt_chan_a);
    pcnt_channel_set_edge_action(pcnt_chan_a, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE);
    pcnt_channel_set_level_action(pcnt_chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);

    pcnt_unit_enable(pcnt_unit_left);
    pcnt_unit_clear_count(pcnt_unit_left);
    pcnt_unit_start(pcnt_unit_left);

    // --- 右电机 PCNT ---
    pcnt_new_unit(&unit_config, &pcnt_unit_right);
    pcnt_unit_set_glitch_filter(pcnt_unit_right, &filter_config);
    
    pcnt_chan_config_t chan_b_config = { 
        .edge_gpio_num = ENCODER_RIGHT_A_PIN, 
        .level_gpio_num = ENCODER_RIGHT_B_PIN, 
    };
    pcnt_channel_handle_t pcnt_chan_b = NULL;
    pcnt_new_channel(pcnt_unit_right, &chan_b_config, &pcnt_chan_b);
    pcnt_channel_set_edge_action(pcnt_chan_b, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE);
    pcnt_channel_set_level_action(pcnt_chan_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);

    pcnt_unit_enable(pcnt_unit_right);
    pcnt_unit_clear_count(pcnt_unit_right);
    pcnt_unit_start(pcnt_unit_right);

    return ESP_OK;
}

esp_err_t dynamo_init(void) {
    ESP_LOGI(TAG, "初始化底盘电机驱动与 PID...");

    // 1. 初始化 PWM (LEDC)
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = LEDC_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    esp_err_t err = ledc_timer_config(&ledc_timer);
    if (err != ESP_OK) return err;

    ledc_channel_config_t ledc_channel[4] = {
        { .channel = LEDC_CHANNEL_0, .duty = 0, .gpio_num = MOTOR_LEFT_RPWM_PIN,  .speed_mode = LEDC_MODE, .hpoint = 0, .timer_sel = LEDC_TIMER },
        { .channel = LEDC_CHANNEL_1, .duty = 0, .gpio_num = MOTOR_LEFT_LPWM_PIN,  .speed_mode = LEDC_MODE, .hpoint = 0, .timer_sel = LEDC_TIMER },
        { .channel = LEDC_CHANNEL_2, .duty = 0, .gpio_num = MOTOR_RIGHT_RPWM_PIN, .speed_mode = LEDC_MODE, .hpoint = 0, .timer_sel = LEDC_TIMER },
        { .channel = LEDC_CHANNEL_3, .duty = 0, .gpio_num = MOTOR_RIGHT_LPWM_PIN, .speed_mode = LEDC_MODE, .hpoint = 0, .timer_sel = LEDC_TIMER },
    };
    
    for (int i = 0; i < 4; i++) {
        err = ledc_channel_config(&ledc_channel[i]);
        if (err != ESP_OK) return err;
    }

    // 2. 初始化 PCNT 硬件编码器
    encoder_init();

    // 3. 启动 PID 定时器 (每 50ms = 50000us 触发一次)
    const esp_timer_create_args_t pid_timer_args = {
        .callback = &pid_timer_cb,
        .name = "pid_timer"
    };
    esp_timer_create(&pid_timer_args, &pid_timer_handle);
    esp_timer_start_periodic(pid_timer_handle, 50000); 

    ESP_LOGI(TAG, "初始化完成。");
    return ESP_OK;
}

// 设置目标速度
void dynamo_set_target_speed(float left_target, float right_target) {
    pid_left.target = left_target;
    pid_right.target = right_target;
}

void dynamo_stop(void) {
    dynamo_set_target_speed(0, 0);
}
