#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "wifi.h"
#include <stdio.h>
#include "http.h"
#include "udp.h"  
#include "oled.h"
#include "servo.h"
#include "bldc_motor.h"

#define SERVO_GPIO_PIN  15

static const char *TAG = "MAIN";

void app_main(void)
{
    esp_err_t ret;

    ret = nvs_flash_init();     /* 初始化NVS */
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    oled_init();
    wifi_init_sta();
    start_webserver();
    start_udp_server();
    start_monitor_task();
    servo_init(SERVO_GPIO_PIN, LEDC_CHANNEL_0);
    
    // ==========================================
    // 修改点 1：使用双电机初始化函数
    // ==========================================
    bldc_motors_init();
    
    vTaskDelay(pdMS_TO_TICKS(2000)); 

    // ==========================================
    // 修改点 2：指定电机 ID 发送控制指令
    // ==========================================
    ESP_LOGI(TAG, "Starting Dual Motors...");
    
    // 让电机 1 跑在 1500 RPM
    bldc_motor_set_target_rpm(MOTOR_1, 1500.0f); 
    
    // 如果电机 2 也接好了线，可以取消下面这行的注释让它一起跑
    bldc_motor_set_target_rpm(MOTOR_2, 1200.0f); 

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000)); 
    }
}