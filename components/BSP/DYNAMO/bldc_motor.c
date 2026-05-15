#include "bldc_motor.h"
#include "esp_log.h"
#include "driver/mcpwm_prelude.h"
#include "driver/pulse_cnt.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "BLDC_MOTOR";

// 硬件句柄
static mcpwm_cmpr_handle_t comparator = NULL;
static pcnt_unit_handle_t pcnt_unit = NULL;

// 控制变量
static volatile float target_rpm = 0.0f;
static volatile bool motor_running = false;

// MCPWM 周期滴答数 (1MHz 时钟下，40 tick = 25kHz)
#define MCPWM_PERIOD_TICKS (1000000 / BLDC_PWM_FREQ_HZ)

// ================= PID 闭环任务 =================
static void bldc_pid_control_task(void *arg)
{
    float Kp = 0.05f, Ki = 0.01f, Kd = 0.005f; // 注意：此参数需根据您的实际电机带载情况整定
    float integral = 0.0f, prev_error = 0.0f;
    int pulse_count = 0;
    const float dt = BLDC_CTRL_PERIOD_MS / 1000.0f; 

    while (1) {
        if (!motor_running || target_rpm <= 0.1f) {
            // 停止状态下：清空积分，输出 100% 占空比 (高电平停止)
            mcpwm_comparator_set_compare_value(comparator, 0); 
            integral = 0.0f;
            prev_error = 0.0f;
            vTaskDelay(pdMS_TO_TICKS(BLDC_CTRL_PERIOD_MS));
            continue;
        }

        // 1. 获取过去周期内的脉冲数并清零
        pcnt_unit_get_count(pcnt_unit, &pulse_count);
        pcnt_unit_clear_count(pcnt_unit);

        // 2. 计算当前真实 RPM
        // RPM = (脉冲数 / 时间) * (60秒 / 每圈脉冲数)
        float current_rpm = ((float)pulse_count / dt) * (60.0f / BLDC_PULSES_PER_ROUND);

        // 3. PID 计算 (位置式 PID)
        float error = target_rpm - current_rpm;
        integral += error * dt;
        
        // 积分限幅 (防止积分饱和)
        if (integral > 1000.0f) integral = 1000.0f;
        if (integral < -1000.0f) integral = -1000.0f;

        float derivative = (error - prev_error) / dt;
        float output = (Kp * error) + (Ki * integral) + (Kd * derivative);
        prev_error = error;

        // 4. 将 PID 输出映射到 PWM 比较值 (0 ~ MCPWM_PERIOD_TICKS)
        // 假设 output 代表需要的动力 (0-100%)
        if (output > 100.0f) output = 100.0f;
        if (output < 0.0f) output = 0.0f;

        // 【关键电平反转逻辑】：P5 蓝线低电平启动，高电平停止
        // 动力 100% -> 占空比 0% (全低电平) -> 比较值 = MCPWM_PERIOD_TICKS
        // 动力 0%   -> 占空比 100% (全高电平) -> 比较值 = 0
        float duty_cycle_percent = 100.0f - output; 
        uint32_t compare_val = (uint32_t)((duty_cycle_percent / 100.0f) * MCPWM_PERIOD_TICKS);

        // 安全限制
        if(compare_val > MCPWM_PERIOD_TICKS) compare_val = MCPWM_PERIOD_TICKS;

        mcpwm_comparator_set_compare_value(comparator, compare_val);

        vTaskDelay(pdMS_TO_TICKS(BLDC_CTRL_PERIOD_MS));
    }
}

// ================= 硬件初始化 =================
void bldc_motor_init(void)
{
    ESP_LOGI(TAG, "Initializing BLDC Motor (ESP-IDF v6.0 API)...");

    // 1. 初始化普通 GPIO (方向与刹车)
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BLDC_PIN_DIR) | (1ULL << BLDC_PIN_BRAKE),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    
    // 默认设置：正转，刹车拉低(停止)
    gpio_set_level(BLDC_PIN_DIR, 0); 
    gpio_set_level(BLDC_PIN_BRAKE, 0); 

    // 2. 初始化 PCNT (读取 FG 信号)
    pcnt_unit_config_t unit_config = {
        .high_limit = 10000,
        .low_limit = -10000,
    };
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &pcnt_unit));

    // 添加毛刺滤波器 (防止 PWM 高频噪声干扰 FG 测速)
    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = 1000,
    };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt_unit, &filter_config));

    pcnt_chan_config_t chan_config = {
        .edge_gpio_num = BLDC_PIN_FG,
        .level_gpio_num = -1, // 不使用电平控制方向
    };
    pcnt_channel_handle_t pcnt_chan = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_config, &pcnt_chan));
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_HOLD));

    ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_start(pcnt_unit));

    // 3. 初始化 MCPWM (输出 P5 调速)
    mcpwm_timer_handle_t timer = NULL;
    mcpwm_timer_config_t timer_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = 1000000, // 1MHz 分辨率
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
        .period_ticks = MCPWM_PERIOD_TICKS,
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &timer));

    mcpwm_oper_handle_t oper = NULL;
    mcpwm_operator_config_t oper_config = { .group_id = 0 };
    ESP_ERROR_CHECK(mcpwm_new_operator(&oper_config, &oper));
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper, timer));

    mcpwm_comparator_config_t cmpr_config = { .flags.update_cmp_on_tez = true };
    ESP_ERROR_CHECK(mcpwm_new_comparator(oper, &cmpr_config, &comparator));
    
    // 初始化时设为 0 (对应 100% 高电平，电机停止)
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator, 0));

    mcpwm_gen_handle_t generator = NULL;
    mcpwm_generator_config_t gen_config = { .gen_gpio_num = BLDC_PIN_PWM };
    ESP_ERROR_CHECK(mcpwm_new_generator(oper, &gen_config, &generator));

    // 计数器清零时输出高电平，达到比较值时输出低电平 (配合前面的占空比计算)
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(generator,
        MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(generator,
        MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator, MCPWM_GEN_ACTION_LOW)));

    ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));

    // 4. 创建 PID 控制任务
    xTaskCreate(bldc_pid_control_task, "bldc_pid_task", 4096, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "BLDC Motor Init OK.");
}

// ================= 对外控制接口 =================
void bldc_motor_set_target_rpm(float rpm)
{
    target_rpm = rpm;
    if (rpm > 0.0f && !motor_running) {
        // 启动电机：拉高 BRAKE 引脚
        gpio_set_level(BLDC_PIN_BRAKE, 1);
        motor_running = true;
        ESP_LOGI(TAG, "Motor Started, Target RPM: %.0f", rpm);
    } else if (rpm <= 0.0f && motor_running) {
        bldc_motor_estop();
    }
}

void bldc_motor_set_direction(bldc_dir_t dir)
{
    // 强制安全机制：仅在目标转速为 0 且电机停止时才允许切换方向
    if (target_rpm > 0.0f || motor_running) {
        ESP_LOGW(TAG, "Warning: Attempted to switch direction while running! Command ignored.");
        return;
    }
    gpio_set_level(BLDC_PIN_DIR, (uint32_t)dir);
    ESP_LOGI(TAG, "Direction set to %s", dir == BLDC_DIR_CW ? "CW" : "CCW");
}

void bldc_motor_estop(void)
{
    motor_running = false;
    target_rpm = 0.0f;
    // 1. 硬件立即断电：拉低 BRAKE
    gpio_set_level(BLDC_PIN_BRAKE, 0);
    // 2. PWM 立即输出 100% 高电平
    mcpwm_comparator_set_compare_value(comparator, 0);
    ESP_LOGI(TAG, "Motor Emergency Stopped.");
}