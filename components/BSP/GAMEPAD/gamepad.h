#ifndef _GAMEPAD_H_
#define _GAMEPAD_H_

#include <stddef.h> // 提供 size_t 的定义

// 把收到的 UDP 字符串丢给这个函数解析，并执行硬件控制
void parse_gamepad_data(const char* rx_str);

// 获取打包好的系统状态字符串，用于 UDP 发送回电脑
void get_telemetry_string(char* buffer, size_t max_len);

#endif