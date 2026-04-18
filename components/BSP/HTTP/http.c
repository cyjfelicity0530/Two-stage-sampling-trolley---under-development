#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_mac.h"
#include <netdb.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_http_server.h"

esp_err_t hello_get_handler(httpd_req_t *req)
{
    const char* resp_str = "Hello from ESP32-S3! Web Server is running!";
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

httpd_uri_t hello_uri = {
    .uri       = "/hello",
    .method    = HTTP_GET,
    .handler   = hello_get_handler,
    .user_ctx  = NULL
};

// ========================================================
// 2. 处理 /cmd 接收电脑数据的函数和 URI 定义 (必须放在启动函数上面)
// ========================================================
esp_err_t cmd_get_handler(httpd_req_t *req)
{
    char buf[100];
    char param[32]; 

    size_t buf_len = httpd_req_get_url_query_len(req) + 1;
    
    if (buf_len > 1) {
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            if (httpd_query_key_value(buf, "msg", param, sizeof(param)) == ESP_OK) {
                printf("--------------------------------\n");
                printf("成功收到电脑发来的指令: %s\n", param);
                printf("--------------------------------\n");
            }
        }
    }

    const char* resp_str = "Command Received by ESP32-S3!";
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

httpd_uri_t cmd_uri = {
    .uri       = "/cmd",
    .method    = HTTP_GET,
    .handler   = cmd_get_handler,
    .user_ctx  = NULL
};

// ========================================================
// 3. 启动服务器 (因为上面已经定义好了 hello_uri 和 cmd_uri，这里就能直接用了)
// ========================================================
httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    if (httpd_start(&server, &config) == ESP_OK) {
        // 注册刚刚定义好的两个路径
        httpd_register_uri_handler(server, &hello_uri);
        httpd_register_uri_handler(server, &cmd_uri); 
        
        printf("Web server started successfully!\n");
        return server;
    }
    
    printf("Error starting server!\n");
    return NULL;
}