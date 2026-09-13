import socket
import struct
import sys

def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(('127.0.0.1', 8888))
    print("已连接服务器，等待接收数据...")
    
    # 先发送订阅消息，订阅类型1
    subscribe_payload = b'\x01'
    header = struct.pack('!BI', 0xFE, len(subscribe_payload))
    sock.sendall(header + subscribe_payload)
    
    # 接收数据：先读5字节头部，再读数据体
    header = sock.recv(5)
    if len(header) < 5:
        print("接收头失败")
        return
    data_type, data_len = struct.unpack('!BI', header)
    print(f"收到数据类型 {data_type}, 长度 {data_len}")
    data = b''
    while len(data) < data_len:
        chunk = sock.recv(data_len - len(data))
        if not chunk:
            break
        data += chunk
    if data_type == 1:
        with open('received.jpg', 'wb') as f:
            f.write(data)
        print("图片已保存为 received.jpg")
    else:
        print("未知数据类型")
    sock.close()

if __name__ == '__main__':
    main()