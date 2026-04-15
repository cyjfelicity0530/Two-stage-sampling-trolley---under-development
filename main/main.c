#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "dynamo.h"

static const char *TAG = "MAIN";

// 定义与 ESP32-C6 通信的 UART 配置
#define UART_NUM       UART_NUM_1
#define TXD_PIN        (17) // 根据实际连线修改
#define RXD_PIN        (16) // 根据实际连线修改
#define BUF_SIZE       (1024)

// 从蓝牙(C6)接收并解析数据的任务
void uart_control_task(void *arg) {
    // 1. 初始化 UART1
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM, &uart_config);
    uart_set_pin(UART_NUM, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    ESP_LOGI(TAG, "UART 控制接收任务已启动...");
    
    uint8_t data[128];
    while (1) {
        int len = uart_read_bytes(UART_NUM, data, sizeof(data) - 1, 20 / portTICK_PERIOD_MS);
        if (len > 0) {
            data[len] = '\0';
            int left_speed = 0, right_speed = 0;
            // 假设 C6 发送的格式为 "L:500 R:500\n"
            if (sscanf((char *)data, "L:%d R:%d", &left_speed, &right_speed) == 2) {
                // ESP_LOGI(TAG, "解析到遥控指令: 左=%d, 右=%d", left_speed, right_speed);
                dynamo_set_target_speed((float)left_speed, (float)right_speed);
            }
        }
        // 建议加入超时看门狗保护机制：如果长时间未收到 C6 数据，则自动停车
    }
}

#include "driver/gpio.h"
#include "driver/ledc.h"

// 测试 TB6612 电机参数
#define TEST_PWMA_PIN  27
#define TEST_AIN1_PIN  32
#define TEST_AIN2_PIN  36

void test_tb6612_motor(void) {
    ESP_LOGI(TAG, "启动 TB6612 电机调试，引脚 PWMA:27, AIN1:32, AIN2:36...");

    // 1. 配置 AIN1 和 AIN2 为输出
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << TEST_AIN1_PIN) | (1ULL << TEST_AIN2_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    
    // 正转模式：AIN1 = 1, AIN2 = 0
    gpio_set_level(TEST_AIN1_PIN, 1);
    gpio_set_level(TEST_AIN2_PIN, 0);

    // 2. 配置 PWMA (通过 LEDC 输出 PWM)
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = LEDC_TIMER_1,
        .duty_resolution  = LEDC_TIMER_10_BIT, // 0-1023
        .freq_hz          = 20000,             // 20kHz
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .channel    = LEDC_CHANNEL_4,
        .duty       = 0,
        .gpio_num   = TEST_PWMA_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_TIMER_1
    };
    ledc_channel_config(&ledc_channel);

    ESP_LOGI(TAG, "电机逐渐加速中...");
    
    // 逐渐增大占空比 (0 到 ~80% 的 1023，即最大约 800)
    for (int duty = 0; duty <= 800; duty += 20) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_4, duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_4);
        vTaskDelay(pdMS_TO_TICKS(100)); // 每 100ms 加一次速
    }
    
    ESP_LOGI(TAG, "电机保持正转!");
}

void app_main(void)
{
    ESP_LOGI(TAG, "系统启动 - 进入快速调试模式...");

    test_tb6612_motor();

    /* -- 以下为原有的代码，已临时屏蔽 --
    esp_err_t err = dynamo_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "系统挂起！");
        return;
    }
    xTaskCreate(uart_control_task, "uart_control_task", 4096, NULL, 5, NULL);
    */
}
