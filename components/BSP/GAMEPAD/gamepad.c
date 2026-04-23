#include "gamepad.h"
#include "system_state.h"
#include <stdio.h>
#include <string.h>

// 【真正定义】全局变量的地方，只有这里需要赋初值
SystemState_t g_state = {0}; 

// 内部使用的边缘检测变量
static int last_btn_b = 0; 

// ========================================================
// 解析 UDP 收到的字符串，更新系统状态，并执行硬件控制
// ========================================================
void parse_gamepad_data(const char* rx_str)
{
    int dx = 0, dy = 0, a = 0, b = 0;
    
    // 1. 提取数据
    int parsed_count = sscanf(rx_str, "DX:%d,DY:%d,A:%d,B:%d", &dx, &dy, &a, &b);

    if (parsed_count == 4) {
        // 2. 更新到全局结构体中
        g_state.d_x = dx;
        g_state.d_y = dy;
        g_state.btn_a = a;
        g_state.btn_b = b;

        // 3. 执行逻辑：模式切换
        if (g_state.btn_b == 1 && last_btn_b == 0) {
            g_state.current_mode = !g_state.current_mode;
        }
        last_btn_b = g_state.btn_b;

        // 4. 执行逻辑：机械爪与底盘控制
        if (g_state.btn_a == 1) {
            // open_gripper(); 
        }

        // 模拟一些传感器数据（实际开发中，这些会在 ADC / I2C 任务里更新）
        //g_state.battery_voltage = 12.4f;
        //g_state.motor_speed = g_state.d_y * 100;
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