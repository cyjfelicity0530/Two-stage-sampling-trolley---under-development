#ifndef _BSP_DYNAMO_H_
#define _BSP_DYNAMO_H_

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// 初始化电机驱动、编码器(PCNT)和PID计算定时器
esp_err_t dynamo_init(void);

// 设置电机目标速度 (开环控制升级为闭环PID控制)
// 左/右目标速度，这里建议为期望的每周期(如50ms)编码器脉冲数。
// 带正负号表示方向。
void dynamo_set_target_speed(float left_target, float right_target);

// 停止电机
void dynamo_stop(void);

#ifdef __cplusplus
}
#endif

#endif // _BSP_DYNAMO_H_
