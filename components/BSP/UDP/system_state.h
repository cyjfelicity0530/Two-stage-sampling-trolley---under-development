#ifndef _SYSTEM_STATE_H_
#define _SYSTEM_STATE_H_

// ========================================================
// 统一的【系统状态结构体】
// ========================================================
typedef struct {
    // 1. 手柄控制指令
    int d_x;
    int d_y;
    int btn_a;
    int btn_b;
    int current_mode; // 0:正常模式, 1:特殊模式
    
    // 2. 传感器与硬件状态 (后续在这里无限扩展)
    float battery_voltage; 
    int motor_speed; 
    int servo_angle; // 舵机角度      
} SystemState_t;

// 【外部声明】这告诉所有引用这个头文件的文件：
// “有一个叫 g_state 的全局变量在别的地方定义了，大家共享使用它！”
extern SystemState_t g_state;

#endif