#include "gamepad.h"
#include "system_state.h"
#include "bldc_motor.h"  // 引入电机底层控制接口
#include <stdio.h>
#include <string.h>

// 小车最大设计转速 (可以根据实际情况调整)
#define MAX_CHASSIS_RPM 1500.0f

// 全局变量，各种数据变量参数
SystemState_t g_state = {0}; 

static int last_btn_b = 0; 

// ========================================================
// 按键式 (-1, 0, 1) 差速控制核心算法
// ========================================================
static void update_chassis_motion(int dx, int dy)
{
    float left_target = 0.0f;
    float right_target = 0.0f;
    
    // 基础转速设定 (全速与半速)
    const float SPEED_NORMAL = MAX_CHASSIS_RPM;       
    const float SPEED_SLOW   = MAX_CHASSIS_RPM * 0.5f; // 边走边转时内侧轮的速度

    if (dy == 1) {
        // ========== 前进系列 ==========
        if (dx == 0) {
            // 直行前进
            left_target = SPEED_NORMAL;
            right_target = SPEED_NORMAL;
        } 
        else if (dx == -1) {
            // 左前转 (右轮全速，左轮半速)
            left_target = SPEED_SLOW; 
            right_target = SPEED_NORMAL;
        } 
        else if (dx == 1) {
            // 右前转 (左轮全速，右轮半速)
            left_target = SPEED_NORMAL;
            right_target = SPEED_SLOW;
        }
    } 
    else if (dy == 0) {
        // ========== 原地转向系列 ==========
        if (dx == -1) {
            // 原地左转 (右轮正转，左轮停止)
            left_target = 0; 
            right_target = SPEED_NORMAL;
        } 
        else if (dx == 1) {
            // 原地右转 (左轮正转，右轮停止)
            left_target = SPEED_NORMAL;
            right_target = 0; 
        }
        else {
            // dx=0, dy=0：松开按键，停车
            left_target = 0;
            right_target = 0;
        }
    }
    // 暂未处理 dy == -1 的倒车情况 (需底层库支持后退切换后再加入)

    // 下发转速指令到底层电机
    bldc_motor_set_target_rpm(MOTOR_1, left_target);
    bldc_motor_set_target_rpm(MOTOR_2, right_target);

    // 把算出来的平均速度更新到状态里，供 UDP 遥测回传
    g_state.motor_speed = (int)((left_target + right_target) / 2.0f);
}

// ========================================================
// 解析 UDP 收到的字符串，更新系统状态，并执行硬件控制
// ========================================================
void parse_gamepad_data(const char* rx_str)
{
    int dx = 0, dy = 0, a = 0, b = 0;
    
    // 1. 提取数据
    int parsed_count = sscanf(rx_str, "DX:%d,DY:%d,A:%d,B:%d", &dx, &dy, &a, &b);

    if (parsed_count == 4) {
        g_state.d_x = dx;
        g_state.d_y = dy;
        g_state.btn_a = a;
        g_state.btn_b = b;

        // 2. 执行逻辑：模式切换 (按键 B)
        if (g_state.btn_b == 1 && last_btn_b == 0) {
            g_state.current_mode = !g_state.current_mode;
        }
        last_btn_b = g_state.btn_b;

        // 3. 机械爪 (按键 A)
        if (g_state.btn_a == 1) {
            // open_gripper(); 
        }

        // 4. 调用差速控制逻辑
        // 假设 current_mode == 0 是遥控模式
        if (g_state.current_mode == 0) {
             update_chassis_motion(g_state.d_x, g_state.d_y);
        } else {
             // 切换到其他模式(如自动巡线)时，由其他任务接管，此处切断遥控动力
             bldc_motor_set_target_rpm(MOTOR_1, 0.0f);
             bldc_motor_set_target_rpm(MOTOR_2, 0.0f);
             g_state.motor_speed = 0;
        }
        
        // 模拟一些传感器数据（实际开发中，这些会在 ADC / I2C 任务里更新）
        // g_state.battery_voltage = 12.4f;
    }
}

// ========================================================
// 将系统状态打包成字符串，供 UDP 发送
// ========================================================
void get_telemetry_string(char* buffer, size_t max_len)
{
    // 以后增加新传感器，只改这里！
    snprintf(buffer, max_len, 
             "MODE:%d,DX:%d,DY:%d,A:%d,B:%d,BAT:%.1f,SPD:%d,SRV:%d",
             g_state.current_mode, 
             g_state.d_x, g_state.d_y, g_state.btn_a, g_state.btn_b,
             g_state.battery_voltage, g_state.motor_speed,
             g_state.servo_angle);
}