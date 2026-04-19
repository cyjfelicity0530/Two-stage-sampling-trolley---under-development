#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "oled.h"

#define PORT 3333 // 我们自己约定的端口号
int d_x = 0, d_y = 0, btn_a = 0, btn_b = 0;
int last_btn_b = 0;

// 这是一个 FreeRTOS 任务，专门在后台死循环收数据
void udp_server_task(void *pvParameters)
{
    
    int current_mode = 0; // 假设 0 是普通模式，1 是特殊模式
    char rx_buffer[128];
    char addr_str[128];
    int addr_family = AF_INET;
    int ip_protocol = IPPROTO_IP;
    struct sockaddr_in dest_addr;

    while (1) {
        dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(PORT);

        // 1. 创建 UDP Socket
        int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
        if (sock < 0) {
            printf("Socket 创建失败\n");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        // 2. 绑定端口
        int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err < 0) {
            printf("Socket 绑定失败\n");
            close(sock);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        printf("UDP 服务器启动成功！正在监听端口 %d...\n", PORT);

        // 3. 死循环接收数据
        struct sockaddr_storage source_addr;
        socklen_t socklen = sizeof(source_addr);

        while (1) {
            // 收到数据前，这行代码会一直等待（阻塞），不占用CPU
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, 
                               (struct sockaddr *)&source_addr, &socklen);

            if (len > 0) 
            {
                rx_buffer[len] = 0; // 添加字符串结束符

                // 按新格式解析："DX:%d,DY:%d,A:%d,B:%d"
                int parsed_count = sscanf(rx_buffer, "DX:%d,DY:%d,A:%d,B:%d", &d_x, &d_y, &btn_a, &btn_b);

                if (parsed_count == 4) {
                    
                    // ==========================================
                    // 逻辑 1：B键 模式切换（下降沿/上升沿检测）
                    // ==========================================
                    // 如果当前收到 B=1，且上一次 B=0，说明按键被“刚刚按下”
                    if (btn_b == 1 && last_btn_b == 0) {
                        current_mode = !current_mode; // 翻转模式 (0变1，1变0)
                        printf(">>> 模式已切换！当前模式: %d <<<\n", current_mode);
                    }
                    last_btn_b = btn_b; // 更新状态，为下一次循环做准备

                    // ==========================================
                    // 逻辑 2：A键 抓取逻辑
                    // ==========================================
                    // 抓取通常只要按住就一直保持抓取，松开就松开（或者也是按一下抓、再按一下松）
                    // 这里演示“按住A键即执行抓取”的逻辑
                    if (btn_a == 1) {
                        // 执行抓取动作 (比如闭合舵机)
                        // close_gripper();
                    } else {
                        // 释放动作 (比如张开舵机)
                        // open_gripper();
                    }

                    // ==========================================
                    // 逻辑 3：十字方向键 运动逻辑
                    // ==========================================
                    // 根据 current_mode 的不同，十字键可以有不同的功能
                    if (current_mode == 0) {
                        // 模式0：控制底盘移动
                        if (d_y == 1)       { /* 前进 */ }
                        else if (d_y == -1) { /* 后退 */ }
                        else if (d_x == -1) { /* 左转 */ }
                        else if (d_x == 1)  { /* 右转 */ }
                        else                { /* 停止移动 */ }
                    } else {
                        // 模式1：控制云台/机械臂上下左右
                        // if (d_y == 1) move_arm_up();
                        // ...
                    }
                    
                    // 为了调试，每秒打印一次当前状态，不用每次都打印以免刷屏
                    // printf("收到: 方向(%d, %d), A=%d, 模式=%d\n", d_x, d_y, btn_a, current_mode);
                    
                } else {
                    printf("数据格式错误: %s\n", rx_buffer);
                }
            }
        }

        if (sock != -1) {
            close(sock);
        }
    }
    vTaskDelete(NULL);
}

//监视按键状态打印至OLED
void monitor_button_states(void *pvParameters) 
{
    while (1) {
        // 1. 每次循环开始时，统一清空显存
        oled_clear();

        // 2. 将所有状态写入显存，注意 y 坐标递增来实现换行（你的字体是8像素高，间距取10）
        
        // --- 绘制 B 键状态 (第 1 行：x=0, y=0) ---
        if (btn_b == 1) {
            oled_draw_string(0, 0, "B = 1");
        } else {
            oled_draw_string(0, 0, "B = 0");
        }

        // --- 绘制 A 键状态 (第 2 行：x=0, y=10) ---
        if (btn_a == 1) {
            oled_draw_string(0, 10, "A = 1");
        } else {
            oled_draw_string(0, 10, "A = 0");
        }

        // --- 绘制 Y 轴状态 (第 3 行：x=0, y=20) ---
        if (d_y == 1) {
            oled_draw_string(0, 20, "Y = 1");
        } else if (d_y == -1) {
            oled_draw_string(0, 20, "Y = -1");
        } else {
            oled_draw_string(0, 20, "Y = 0");
        }

        // --- 绘制 X 轴状态 (第 4 行：x=0, y=30) ---
        if (d_x == 1) {
            oled_draw_string(0, 30, "X = 1");
        } else if (d_x == -1) {
            oled_draw_string(0, 30, "X = -1");
        } else {
            oled_draw_string(0, 30, "X = 0");
        }

        // 3. 所有内容都绘制好后，统一刷新一次屏幕
        oled_update();

        vTaskDelay(100 / portTICK_PERIOD_MS); // 每100ms刷新一次
    }
}

// 启动 UDP 任务的函数
void start_udp_server(void)
{
    // 创建一个后台任务来跑 UDP，不影响主程序的其他逻辑
    xTaskCreate(udp_server_task, "udp_server", 4096, NULL, 5, NULL);
}

//// 启动 Monitor 任务的函数
void start_monitor_task(void)
{
    // 创建一个后台任务来跑 Monitor，不影响主程序的其他逻辑
    xTaskCreate(monitor_button_states, "monitor", 2048, NULL, 5, NULL);
}