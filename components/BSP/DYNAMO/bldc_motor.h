#ifndef BLDC_MOTOR_H
#define BLDC_MOTOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ================= 电机 1 引脚配置 =================
#define M1_PIN_FG       14   // 测速
#define M1_PIN_DIR      13   // 方向
#define M1_PIN_PWM      12   // PWM
#define M1_PIN_BRAKE    11   // 刹车

// ================= 电机 2 引脚配置 =================
#define M2_PIN_FG       19   // 测速 (注意: S3默认USB_D-)
#define M2_PIN_DIR      20   // 方向 (注意: S3默认USB_D+)
#define M2_PIN_PWM      21   // PWM
#define M2_PIN_BRAKE    47   // 刹车

// ================= 电机参数配置 =================
#define BLDC_PWM_FREQ_HZ      25000 // PWM 频率 25kHz
#define BLDC_PULSES_PER_ROUND 6     // 每圈 FG 脉冲数
#define BLDC_CTRL_PERIOD_MS   200    // PID 控制周期 50ms

// 电机 ID 枚举
typedef enum {
    MOTOR_1 = 0,
    MOTOR_2 = 1,
    MOTOR_MAX
} bldc_motor_id_t;

// 方向枚举
typedef enum {
    BLDC_DIR_CW = 0, // 正转 (低电平)
    BLDC_DIR_CCW = 1 // 反转 (高电平)
} bldc_dir_t;

/**
 * @brief 初始化所有无刷电机硬件及 PID 任务
 */
void bldc_motors_init(void);

/**
 * @brief 设置指定电机的目标转速 (RPM)
 * @param motor_id MOTOR_1 或 MOTOR_2
 * @param rpm 目标转速
 */
void bldc_motor_set_target_rpm(bldc_motor_id_t motor_id, float rpm);

/**
 * @brief 设置指定电机方向 (必须在停机时调用)
 */
void bldc_motor_set_direction(bldc_motor_id_t motor_id, bldc_dir_t dir);

/**
 * @brief 指定电机紧急停止
 */
void bldc_motor_estop(bldc_motor_id_t motor_id);

/**
 * @brief 所有电机紧急停止
 */
void bldc_motor_estop_all(void);

#ifdef __cplusplus
}
#endif

#endif // BLDC_MOTOR_H