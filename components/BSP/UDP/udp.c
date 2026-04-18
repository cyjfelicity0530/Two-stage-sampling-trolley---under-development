#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"

#define PORT 3333 // 我们自己约定的端口号

// 这是一个 FreeRTOS 任务，专门在后台死循环收数据
void udp_server_task(void *pvParameters)
{
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

            if (len > 0) {
                // 收到了数据！
                rx_buffer[len] = 0; // 添加字符串结束符
                
                // 将数据打印到串口看看
                printf("收到来自电脑的数据: %s\n", rx_buffer);
                
                // === 在这里写你的手柄解析代码 ===
                // 比如电脑发来的是 "X:128,Y:255,BTN:A"
                // 你可以用 sscanf 解析出来，然后去控制电机
            }
        }

        if (sock != -1) {
            close(sock);
        }
    }
    vTaskDelete(NULL);
}

// 启动 UDP 任务的函数
void start_udp_server(void)
{
    // 创建一个后台任务来跑 UDP，不影响主程序的其他逻辑
    xTaskCreate(udp_server_task, "udp_server", 4096, NULL, 5, NULL);
}