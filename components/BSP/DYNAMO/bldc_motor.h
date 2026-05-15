#ifndef BLDC_MOTOR_H
#define BLDC_MOTOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ================= 引脚定义 (基于 ESP32-S3) =================
#define BLDC_PIN_FG       4   // P2 黄线：FG 测速 (输入)
#define BLDC_PIN_DIR      5   // P3 白线：方向控制 (输出)
#define BLDC_PIN_PWM      6   // P5 蓝线：PWM 调速 (MCPWM 输出)
#define BLDC_PIN_BRAKE    7   // P6 绿线：启停控制 (输出)

// ================= 电机参数配置 =================
#define BLDC_PWM_FREQ_HZ      25000 // PWM 频率 25kHz (推荐 20k~30k)
#define BLDC_PULSES_PER_ROUND 6     // 每圈 FG 脉冲数
#define BLDC_CTRL_PERIOD_MS   50    // PID 控制周期 50ms

// 方向枚举
typedef enum {
    BLDC_DIR_CW = 0, // 正转 (低电平)
    BLDC_DIR_CCW = 1 // 反转 (高电平)
} bldc_dir_t;

/**
 * @brief 初始化无刷电机硬件 (MCPWM, PCNT, GPIO) 及 PID 任务
 */
void bldc_motor_init(void);

/**
 * @brief 设置目标转速 (RPM)
 * @param rpm 目标转速，设为 0 时电机停止
 */
void bldc_motor_set_target_rpm(float rpm);

/**
 * @brief 设置电机方向 (必须在电机停止时调用，内部包含安全保护)
 * @param dir BLDC_DIR_CW 或 BLDC_DIR_CCW
 */
void bldc_motor_set_direction(bldc_dir_t dir);

/**
 * @brief 紧急停止 (拉低 BRAKE 引脚，输出 100% 高电平 PWM)
 */
void bldc_motor_estop(void);

#ifdef __cplusplus
}
#endif

#endif // BLDC_MOTOR_H