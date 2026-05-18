#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "udp.h"
#include "gamepad.h"
#include "system_state.h"
#include "oled.h"

#define PORT 3333

//  UDP 收发

void udp_server_task(void *pvParameters)
{
    char rx_buffer[128];
    char reply_msg[256];
    struct sockaddr_in dest_addr;

    while (1) {
        dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(PORT);

        int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (sock < 0) { vTaskDelay(1000/portTICK_PERIOD_MS); continue; }

        int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err < 0)  { close(sock); vTaskDelay(1000/portTICK_PERIOD_MS); continue; }

        printf("UDP 服务器启动成功！正在监听端口 %d...\n", PORT);

        struct sockaddr_storage source_addr;
        socklen_t socklen = sizeof(source_addr);

        while (1) {
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, 
                               (struct sockaddr *)&source_addr, &socklen);
            if (len > 0) {
                rx_buffer[len] = 0;

                // 核心解耦：直接把收到的字符串扔给 gamepad.c 去解析！
                parse_gamepad_data(rx_buffer);

                // 核心解耦：直接从 gamepad.c 获取打包好的状态！
                get_telemetry_string(reply_msg, sizeof(reply_msg));
                
                // 原路发回给电脑
                sendto(sock, reply_msg, strlen(reply_msg), 0, 
                       (struct sockaddr *)&source_addr, socklen);
            }
        }
        if (sock != -1) close(sock);
    }
    vTaskDelete(NULL);
}

// ========================================================
// 任务 2：OLED 显示任务 (通过 g_state 读取全局状态)
// ========================================================


void start_udp_server(void) {
    xTaskCreate(udp_server_task, "udp_server", 4096, NULL, 5, NULL);
}

