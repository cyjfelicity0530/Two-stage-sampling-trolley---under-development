import pygame
import socket
import time

ESP32_IP = "192.168.4.1"
ESP32_PORT = 3333


def main():
    pygame.init()
    pygame.joystick.init()

    if pygame.joystick.get_count() == 0:
        print("未检测到手柄！")
        return

    joystick = pygame.joystick.Joystick(0)
    joystick.init()
    print(f"成功连接手柄: {joystick.get_name()}")

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    print(f"开始向 ESP32-S3 ({ESP32_IP}:{ESP32_PORT}) 发送数据...\n")

    try:
        while True:
            pygame.event.pump()  # 刷新手柄状态

            # 1. 读取十字方向键 (Hat)
            # get_hat(0) 会返回一个元组 (x, y)
            # x: -1(左), 0(中), 1(右)
            # y: -1(下), 0(中), 1(上)
            hat = joystick.get_hat(0)
            d_x = hat[0]
            d_y = hat[1]

            # 2. 读取 A 键和 B 键 (Xbox手柄默认 A是0，B是1)
            btn_a = joystick.get_button(0)
            btn_b = joystick.get_button(1)

            # 3. 将这4个变量打包成字符串，格式如："DX:1,DY:0,A:1,B:0"
            message = f"DX:{d_x},DY:{d_y},A:{btn_a},B:{btn_b}"

            # 4. 发送给 ESP32-S3
            sock.sendto(message.encode('utf-8'), (ESP32_IP, ESP32_PORT))

            # 打印到控制台看看
            print(f"发送 -> {message}", end="   \r")

            # 延时 20ms (50Hz 刷新率)
            time.sleep(0.02)

    except KeyboardInterrupt:
        print("\n程序已退出")
    finally:
        sock.close()
        pygame.quit()


if __name__ == "__main__":
    main()