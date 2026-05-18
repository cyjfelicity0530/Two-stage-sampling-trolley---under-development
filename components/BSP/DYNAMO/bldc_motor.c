#include "bldc_motor.h"
#include "esp_log.h"
#include "driver/mcpwm_prelude.h"
#include "driver/pulse_cnt.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "BLDC_MOTORS";

// MCPWM 周期滴答数
#define MCPWM_PERIOD_TICKS (1000000 / BLDC_PWM_FREQ_HZ)

// 电机上下文结构体 (面向对象思想)
typedef struct {
    bldc_motor_id_t id;
    int pin_fg;
    int pin_dir;
    int pin_pwm;
    int pin_brake;
    int mcpwm_group_id; // S3有2个组(0和1)，我们各分配一个防冲突

    // 硬件句柄
    mcpwm_cmpr_handle_t comparator;
    pcnt_unit_handle_t pcnt_unit;

    // 运行状态
    volatile float target_rpm;
    volatile float current_rpm;
    volatile bool motor_running;

    // PID 变量
    float integral;
    float prev_error;
} bldc_motor_ctx_t;

// 实例化两个电机
static bldc_motor_ctx_t motors[MOTOR_MAX] = {
    {
        .id = MOTOR_1, .pin_fg = M1_PIN_FG, .pin_dir = M1_PIN_DIR, 
        .pin_pwm = M1_PIN_PWM, .pin_brake = M1_PIN_BRAKE, .mcpwm_group_id = 0,
        .target_rpm = 0, .current_rpm = 0, .motor_running = false, .integral = 0, .prev_error = 0
    },
    {
        .id = MOTOR_2, .pin_fg = M2_PIN_FG, .pin_dir = M2_PIN_DIR, 
        .pin_pwm = M2_PIN_PWM, .pin_brake = M2_PIN_BRAKE, .mcpwm_group_id = 1,
        .target_rpm = 0, .current_rpm = 0, .motor_running = false, .integral = 0, .prev_error = 0
    }
};

// ================= PID 闭环任务 (双电机版) =================
static void bldc_pid_control_task(void *arg)
{
    // 经测试调优的温柔参数
    const float Kp = 0.002f, Ki = 0.005f, Kd = 0.000f; 
    const float dt = BLDC_CTRL_PERIOD_MS / 1000.0f; 
    static int print_cnt = 0;
    while (1) {
        // 遍历处理每个电机
        for (int i = 0; i < MOTOR_MAX; i++) {
            bldc_motor_ctx_t *m = &motors[i];

            if (!m->motor_running || m->target_rpm <= 0.1f) {
                mcpwm_comparator_set_compare_value(m->comparator, 0); 
                m->integral = 0.0f;
                m->prev_error = 0.0f;
                m->current_rpm = 0.0f;
                continue;
            }

            // 1. 获取脉冲数
            int pulse_count = 0;
            pcnt_unit_get_count(m->pcnt_unit, &pulse_count);
            pcnt_unit_clear_count(m->pcnt_unit);

            // 2. 计算当前真实 RPM
            m->current_rpm = ((float)pulse_count / dt) * (60.0f / BLDC_PULSES_PER_ROUND);

            // 3. PID 计算 (抗积分饱和)
            float error = m->target_rpm - m->current_rpm;
            
            m->integral += error * dt;
            if (m->integral > 20000.0f) m->integral = 20000.0f; 
            if (m->integral < -20000.0f) m->integral = -20000.0f;

            float derivative = (error - m->prev_error) / dt;
            float output = (Kp * error) + (Ki * m->integral) + (Kd * derivative);
            m->prev_error = error;

            // ==========================================
            // 4. 【核心救命修改】：严格限制最大输出动力！
            // 绝对不允许超过 30%，防止触发电机过流抽搐
            // ==========================================
            if (output > 30.0f) output = 30.0f;  // <--- 以前是 100.0f，改小！
            if (output < 0.0f) output = 0.0f;

            // 动力 100% -> 占空比 0% (全低电平)
            float duty_cycle_percent = 100.0f - output; 
            uint32_t compare_val = (uint32_t)((duty_cycle_percent / 100.0f) * MCPWM_PERIOD_TICKS);
            if(compare_val > MCPWM_PERIOD_TICKS) compare_val = MCPWM_PERIOD_TICKS;

            mcpwm_comparator_set_compare_value(m->comparator, compare_val);
        }

        // 5. 打印状态 (交替打印 M1 和 M2，避免日志刷屏过快)
        if (print_cnt++ % 10 == 0) {
            ESP_LOGI(TAG, "M1 | Target: %.0f | RPM: %.1f  ||  M2 | Target: %.0f | RPM: %.1f", 
                     motors[MOTOR_1].target_rpm, motors[MOTOR_1].current_rpm,
                     motors[MOTOR_2].target_rpm, motors[MOTOR_2].current_rpm);
        }

        // 统一延时 (一个周期只需延时一次)
        vTaskDelay(pdMS_TO_TICKS(BLDC_CTRL_PERIOD_MS));
    }
}

// ================= 单个电机底层初始化封装 =================
static void init_single_motor_hardware(bldc_motor_ctx_t *m)
{
    // 1. GPIO 初始化
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << m->pin_dir) | (1ULL << m->pin_brake),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(m->pin_dir, 0); 
    gpio_set_level(m->pin_brake, 0); 

    // 2. PCNT 初始化
    pcnt_unit_config_t unit_config = {
        .high_limit = 10000,
        .low_limit = -10000,
    };
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &m->pcnt_unit));

    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = 5000, 
    };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(m->pcnt_unit, &filter_config));

    pcnt_chan_config_t chan_config = {
        .edge_gpio_num = m->pin_fg,
        .level_gpio_num = -1,
    };
    pcnt_channel_handle_t pcnt_chan = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(m->pcnt_unit, &chan_config, &pcnt_chan));
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_HOLD));

    ESP_ERROR_CHECK(pcnt_unit_enable(m->pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(m->pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_start(m->pcnt_unit));

    // 3. MCPWM 初始化
    mcpwm_timer_handle_t timer = NULL;
    mcpwm_timer_config_t timer_config = {
        .group_id = m->mcpwm_group_id, // 关键：分离不同的组
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = 1000000,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
        .period_ticks = MCPWM_PERIOD_TICKS,
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &timer));

    mcpwm_oper_handle_t oper = NULL;
    mcpwm_operator_config_t oper_config = { .group_id = m->mcpwm_group_id };
    ESP_ERROR_CHECK(mcpwm_new_operator(&oper_config, &oper));
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper, timer));

    mcpwm_comparator_config_t cmpr_config = { .flags.update_cmp_on_tez = true };
    ESP_ERROR_CHECK(mcpwm_new_comparator(oper, &cmpr_config, &m->comparator));
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(m->comparator, 0)); // 默认停止

    mcpwm_gen_handle_t generator = NULL;
    mcpwm_generator_config_t gen_config = { .gen_gpio_num = m->pin_pwm };
    ESP_ERROR_CHECK(mcpwm_new_generator(oper, &gen_config, &generator));

    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(generator,
        MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(generator,
        MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, m->comparator, MCPWM_GEN_ACTION_LOW)));

    ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));
}

// ================= 对外控制接口 =================
void bldc_motors_init(void)
{
    ESP_LOGI(TAG, "Initializing Dual BLDC Motors...");

    for (int i = 0; i < MOTOR_MAX; i++) {
        init_single_motor_hardware(&motors[i]);
    }

    // 启动统一的 PID 控制任务
    xTaskCreate(bldc_pid_control_task, "bldc_pid_task", 4096, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "Dual Motors Init OK.");
}

void bldc_motor_set_target_rpm(bldc_motor_id_t motor_id, float rpm)
{
    if (motor_id >= MOTOR_MAX) return;
    bldc_motor_ctx_t *m = &motors[motor_id];

    m->target_rpm = rpm;
    if (rpm > 0.0f && !m->motor_running) {
        gpio_set_level(m->pin_brake, 1);
        m->motor_running = true;
        ESP_LOGI(TAG, "Motor %d Started, Target RPM: %.0f", motor_id, rpm);
    } else if (rpm <= 0.0f && m->motor_running) {
        bldc_motor_estop(motor_id);
    }
}

void bldc_motor_set_direction(bldc_motor_id_t motor_id, bldc_dir_t dir)
{
    if (motor_id >= MOTOR_MAX) return;
    bldc_motor_ctx_t *m = &motors[motor_id];

    if (m->target_rpm > 0.0f || m->motor_running) {
        ESP_LOGW(TAG, "Motor %d running! Dir switch ignored.", motor_id);
        return;
    }
    gpio_set_level(m->pin_dir, (uint32_t)dir);
}

void bldc_motor_estop(bldc_motor_id_t motor_id)
{
    if (motor_id >= MOTOR_MAX) return;
    bldc_motor_ctx_t *m = &motors[motor_id];

    m->motor_running = false;
    m->target_rpm = 0.0f;
    gpio_set_level(m->pin_brake, 0);
    mcpwm_comparator_set_compare_value(m->comparator, 0);
    ESP_LOGI(TAG, "Motor %d Emergency Stopped.", motor_id);
}

void bldc_motor_estop_all(void)
{
    for (int i = 0; i < MOTOR_MAX; i++) {
        bldc_motor_estop((bldc_motor_id_t)i);
    }
}