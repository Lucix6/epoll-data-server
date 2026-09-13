#!/usr/bin/env python3
"""
@file send_file.py
@brief 视频流发送客户端
@details 这是一个测试客户端，用于向epoll2服务器发送文件数据（可用于测试视频流分发）
         客户端遵循相同的协议格式，将文件数据封装成帧发送到服务器
         服务器会将这个数据流分发给所有订阅了该类型的其他客户端
"""
import socket
import struct
import sys

def send_data(sock, data_type, data):
    """
    @brief 按照协议格式发送数据
    @param sock 连接到服务器的socket对象
    @param data_type 数据流类型（0~253为数据类型，0xFE为订阅类型）
    @param data 要发送的二进制数据
    @details 协议格式：[1字节类型][4字节长度(大端/网络字节序)][数据内容]
    """
    # !BI表示网络字节序，B是unsigned char(1字节)，I是unsigned int(4字节)
    header = struct.pack('!BI', data_type, len(data))
    sock.sendall(header + data)

def main():
    if len(sys.argv) < 2:
        print("用法: python send_file.py <文件名>")
        print("示例: python send_file.py video.h264  # 发送视频文件到服务器进行分发")
        return
    
    filename = sys.argv[1]
    # 读取文件的所有二进制数据
    with open(filename, 'rb') as f:
        file_data = f.read()
    
    # 创建TCP连接连接到服务器
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_ip = '127.0.0.1'  # 修改为你的服务器IP地址
    server_port = 8888       # 与服务器监听端口一致
    sock.connect((server_ip, server_port))
    print(f"已连接到服务器 {server_ip}:{server_port}")
    
    # 如果需要从服务器接收其他客户端发送的数据，需要先发送订阅消息
    # 例如订阅类型1的数据流：send_data(sock, 0xFE, b'\x01')
    
    # 发送文件数据，类型为1的普通数据流
    # 服务器收到后，会将这个数据转发给所有订阅了类型1的客户端
    send_data(sock, 1, file_data)
    print(f"文件 {filename} ({len(file_data)} 字节) 发送完成")
    
    sock.close()

if __name__ == '__main__':
    main()