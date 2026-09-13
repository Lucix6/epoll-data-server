/**
 * @file worker.c
 * @brief 工作线程池模块实现
 * @details 维护一个线程池，负责从任务队列取出数据，进行协议解析，
 *          处理订阅请求，将视频流数据分发给所有订阅的客户端
 *          实现了"主线程负责I/O，工作线程负责计算"的多线程架构
 */
#include "worker.h"
#include "common.h"
#include "queue.h"
#include "protocol.h"
#include "subscription.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

static pthread_t *threads;           // 存储工作线程ID的数组
static int thread_count = THREAD_COUNT; // 工作线程数量
static volatile int running = 1;      // 线程池运行标志

/**
 * 每个客户端连接对应的协议解析器（按文件描述符索引，因为fd范围不大）
 * 每个客户端独立维护自己的帧解析状态，处理粘包/分包问题
 */
static frame_parser_t *parsers[MAX_CLIENTS];
static pthread_mutex_t parser_mutex = PTHREAD_MUTEX_INITIALIZER; // 保护parsers数组的互斥锁

/**
 * @brief 获取指定客户端的协议解析器，不存在则创建
 * @param fd 客户端socket文件描述符
 * @return 解析器指针
 */
static frame_parser_t* get_parser(int fd) {
    pthread_mutex_lock(&parser_mutex);
    if (parsers[fd] == NULL) {
        // 首次访问该客户端，创建并初始化解析器
        parsers[fd] = malloc(sizeof(frame_parser_t));
        frame_parser_init(parsers[fd]);
    }
    pthread_mutex_unlock(&parser_mutex);
    return parsers[fd];
}

/**
 * @brief 释放指定客户端的协议解析器资源
 * @param fd 客户端socket文件描述符
 */
static void free_parser(int fd) {
    pthread_mutex_lock(&parser_mutex);
    if (parsers[fd]) {
        frame_parser_destroy(parsers[fd]);
        free(parsers[fd]);
        parsers[fd] = NULL;
    }
    pthread_mutex_unlock(&parser_mutex);
}

/**
 * @brief 帧解析完成回调函数
 * @param fd 发送该帧的客户端fd
 * @param type 帧类型
 * @param data 帧数据部分
 * @param len 数据长度
 * @param user 用户自定义数据
 * @details 每解析出一个完整帧时调用，根据帧类型处理：
 *          - 订阅类型：将客户端添加到对应数据流的订阅列表
 *          - 数据类型：将该帧数据转发给所有订阅了此类型的客户端
 */
static void on_frame(int fd, uint8_t type, const char *data, size_t len, void *user) {
    if (type == TYPE_SUBSCRIBE) {
        // 处理订阅请求：data中是客户端想要订阅的所有数据流类型（每个类型1字节）
        for (size_t i = 0; i < len; i++) {
            uint8_t t = (uint8_t)data[i];
            subscription_add(fd, t);  // 将客户端添加到该类型的订阅列表
            printf("客户端 %d 订阅数据流类型 %d\n", fd, t);
        }
        return;
    }
    
    // ==================== 视频流数据分发 ====================
    // 普通数据流：获取所有订阅了该类型的客户端列表
    int fds[1024];
    int cnt = subscription_get(type, fds, 1024);
    
    // 遍历所有订阅者，转发数据（排除发送者自己）
    for (int i = 0; i < cnt; i++) {
        if (fds[i] == fd) continue;  // 不转发给发送者自己
        
        // 重新组帧：按照协议格式构造完整帧再发送
        // 帧格式：[1字节类型][4字节长度(网络字节序)][数据]
        uint32_t net_len = htonl(len);  // 转换为网络字节序
        char header[PROTOCOL_HEADER_SIZE];
        header[0] = type;
        memcpy(header+1, &net_len, 4);
        
        // 先发送协议头部，再发送数据内容
        send(fds[i], header, PROTOCOL_HEADER_SIZE, 0);
        send(fds[i], data, len, 0);
    }
}

/**
 * @brief 工作线程主函数
 * @param arg 线程参数（未使用）
 * @return NULL
 * @details 线程循环从任务队列取出任务，交给协议解析器处理
 */
static void* worker_func(void *arg) {
    task_t task;
    while (running) {
        // 从队列中取出一个任务（阻塞等待）
        if (!pop_task(&task)) continue;
        
        // 获取该客户端的协议解析器，将收到的数据喂给解析器
        frame_parser_t *parser = get_parser(task.fd);
        
        // 解析数据，每解析出完整帧会调用on_frame回调
        int ret = frame_parser_feed(parser, task.data, task.len, on_frame, NULL);
        
        if (ret < 0) {
            // 协议解析错误，断开该客户端连接
            printf("协议错误，关闭客户端 %d\n", task.fd);
            subscription_remove_all(task.fd);  // 移除该客户端的所有订阅
            free_parser(task.fd);              // 释放解析器资源
            close(task.fd);                    // 关闭socket连接
        }
        
        free(task.data);  // 释放任务数据的内存
    }
    return NULL;
}

/**
 * @brief 启动所有工作线程
 * @details 创建THREAD_COUNT个工作线程，进入线程循环
 */
void start_workers(void) {
    threads = malloc(thread_count * sizeof(pthread_t));
    for (int i = 0; i < thread_count; i++) {
        pthread_create(&threads[i], NULL, worker_func, NULL);
    }
    printf("启动 %d 个工作线程\n", thread_count);
}

/**
 * @brief 停止所有工作线程
 * @details 设置运行标志为false，等待所有线程退出，释放资源
 */
void stop_workers(void) {
    running = 0;
    // 等待所有工作线程结束
    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }
    free(threads);
}