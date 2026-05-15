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

#define SERVO_GPIO_PIN  4

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
    bldc_motor_init();
    while (1)
    {
      
    vTaskDelay(pdMS_TO_TICKS(1000));

    // 2. 启动电机并设定目标转速为 1500 RPM (闭环将自动调节 PWM)
    ESP_LOGI(TAG, "Command: Run at 1500 RPM");
    bldc_motor_set_target_rpm(1500.0f);

    vTaskDelay(pdMS_TO_TICKS(5000)); // 运行 5 秒

    // 3. 提速到 2500 RPM
    ESP_LOGI(TAG, "Command: Speed up to 2500 RPM");
    bldc_motor_set_target_rpm(2500.0f);

    vTaskDelay(pdMS_TO_TICKS(5000)); // 运行 5 秒

    // 4. 停止电机
    ESP_LOGI(TAG, "Command: Stop Motor");
    bldc_motor_set_target_rpm(0.0f);

    vTaskDelay(pdMS_TO_TICKS(2000)); // 等待电机完全停稳

    // 5. 切换方向并重新启动 (必须确保完全停稳，上面延时2秒就是为了这个)
    ESP_LOGI(TAG, "Command: Switch to CCW and Run at 1000 RPM");
    bldc_motor_set_direction(BLDC_DIR_CCW);
    bldc_motor_set_target_rpm(1000.0f);
    }
    
}

