import pygame
import socket
import time

# ==========================================
# 配置参数 (根据你的 ESP32 实际情况修改)
# ==========================================
ESP32_IP = "192.168.4.1"  # ESP32 开启热点时的默认 IP
ESP32_PORT = 3333  # 我们在 C 代码里约定的端口号
REFRESH_RATE = 0.02  # 发送频率：0.02秒 = 50Hz (非常丝滑)


def main():
    # ==========================================
    # 1. 初始化 Pygame 和手柄
    # ==========================================
    pygame.init()
    pygame.joystick.init()

    if pygame.joystick.get_count() == 0:
        print("❌ 未检测到手柄！请通过 USB 或蓝牙连接手柄后再运行程序。")
        return

    joystick = pygame.joystick.Joystick(0)
    joystick.init()
    print(f"✅ 成功连接手柄: {joystick.get_name()}")

    # ==========================================
    # 2. 初始化 UDP 网络通信
    # ==========================================
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    # 【核心优化】设置非阻塞超时时间为 5 毫秒
    # 意味着：去听一下 ESP32 有没有回传数据，最多等 5ms，没有就立刻干下一步，绝不卡顿！
    sock.settimeout(0.005)

    print(f"🚀 开始向 ESP32-S3 ({ESP32_IP}:{ESP32_PORT}) 发送与接收数据...\n")
    print("-" * 60)

    try:
        while True:
            # ==========================================
            # 3. 获取手柄最新状态
            # ==========================================
            # 极其重要：清空底层事件队列，防止操作系统缓存导致手柄延迟
            for event in pygame.event.get():
                pass

                # 读取十字方向键 (Hat)
            # get_hat(0) 返回元组 (x, y)，取值通常为 -1, 0, 1
            hat = joystick.get_hat(0)
            d_x = hat[0]
            d_y = hat[1]

            # 读取 A 键(索引0) 和 B 键(索引1)
            btn_a = joystick.get_button(0)
            btn_b = joystick.get_button(1)

            # ==========================================
            # 4. 打包并发送控制指令
            # ==========================================
            # 格式例如: "DX:1,DY:0,A:1,B:0"
            tx_message = f"DX:{d_x},DY:{d_y},A:{btn_a},B:{btn_b}"
            sock.sendto(tx_message.encode('utf-8'), (ESP32_IP, ESP32_PORT))

            # ==========================================
            # 5. 接收并解析 ESP32 的遥测反馈 (重点)
            # ==========================================
            try:
                # 尝试接收最大 1024 字节的数据
                rx_data, _ = sock.recvfrom(1024)
                data_str = rx_data.decode('utf-8')

                # 【字典自动解析】把 "MODE:1,BAT:12.4,SPD:100,SRV:90" 自动变成 Python 字典
                status_dict = {}
                try:
                    items = data_str.split(',')  # 先用逗号切分成列表: ['MODE:1', 'BAT:12.4'...]
                    for item in items:
                        if ':' in item:  # 确保格式正确
                            key, value = item.split(':', 1)  # 再用冒号切分为键和值
                            status_dict[key] = value

                    # 动态提取我们需要显示的数据 (就算 C 语言那边还没写这个变量，这里也会优雅地显示 N/A)
                    mode = status_dict.get('MODE', 'N/A')
                    bat = status_dict.get('BAT', 'N/A')
                    spd = status_dict.get('SPD', 'N/A')
                    srv = status_dict.get('SRV', 'N/A')  # <--- 提取舵机角度

                    # \r 表示回到行首覆盖打印，这样就不会满屏滚代码了。留几个空格是为了清除长字符串残留
                    print(f"\r📡【ESP32遥测】模式:{mode} | 电池:{bat}V | 速度:{spd} | 舵机角度:{srv}°        ", end="")

                except Exception as e:
                    # 如果由于网络波动导致字符串错乱，直接忽略这一帧，不让程序崩溃
                    pass

            except socket.timeout:
                # 5ms 内没收到数据，直接跳过 (说明 ESP32 还没发，或者丢包了)
                pass

            # ==========================================
            # 6. 控制循环频率
            # ==========================================
            time.sleep(REFRESH_RATE)

    except KeyboardInterrupt:
        # 按下 Ctrl+C 时的优雅退出
        print("\n\n🛑 检测到退出指令，正在关闭连接...")
    finally:
        sock.close()
        pygame.quit()
        print("👋 程序已安全退出。")


if __name__ == "__main__":
    main()