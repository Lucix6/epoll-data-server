/**
 * @file common.h
 * @brief 通用常量和协议定义头文件
 * @details 定义服务器运行所需的所有常量配置和通信协议规范
 */
#ifndef COMMON_H
#define COMMON_H

// ==================== 服务器配置常量 ====================
#define PORT            8888    // 服务器监听端口
#define MAX_EVENTS      64      // epoll单次等待的最大事件数
#define BUFFER_SIZE     4096    // 网络读写缓冲区大小
#define MAX_CLIENTS     1000    // 最大支持的客户端连接数
#define THREAD_COUNT    3       // 工作线程池的线程数量
#define QUEUE_SIZE      100     // 主线程与工作线程间的任务队列大小

// ==================== 通信协议定义 ====================
#define PROTOCOL_HEADER_SIZE 5 // 协议头部总长度（1字节类型 + 4字节长度）
#define TYPE_SUBSCRIBE  0xFE   // 订阅消息类型：客户端发送此类型请求订阅数据流
#define TYPE_DATA_MIN   0x00   // 普通数据流类型最小值（0~253为数据类型）
#define TYPE_DATA_MAX   0xFD   // 普通数据流类型最大值

#endif