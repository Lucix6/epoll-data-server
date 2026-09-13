/**
 * @file protocol.h
 * @brief 协议解析模块头文件
 */
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

/**
 * @brief 帧解析器状态结构体
 * @details 每个客户端连接维护一个解析器，处理TCP粘包问题，累积数据直到解析出完整帧
 */
typedef struct {
    uint8_t *buffer;     // 累积的接收缓冲区
    size_t   buffer_len; // 缓冲区中当前数据长度
    size_t   expected;   // 期望接收的字节数：0表示需要先读取协议头部，>0表示还需要多少字节才能完成当前帧
} frame_parser_t;

/**
 * @brief 初始化帧解析器
 * @param p 解析器指针
 */
void frame_parser_init(frame_parser_t *p);

/**
 * @brief 销毁帧解析器，释放资源
 * @param p 解析器指针
 */
void frame_parser_destroy(frame_parser_t *p);

/**
 * @brief 帧解析完成回调函数类型
 * @param fd 发送该帧的客户端fd
 * @param type 帧类型
 * @param data 帧数据部分
 * @param len 数据长度
 * @param user 用户自定义数据
 */
typedef void (*frame_callback)(int fd, uint8_t type, const char *data, size_t len, void *user);

/**
 * @brief 向解析器输入新数据，解析完整帧
 * @param p 解析器指针
 * @param data 新收到的数据
 * @param len 数据长度
 * @param cb 解析出完整帧时的回调函数
 * @param user 用户数据，传递给回调函数
 * @return 成功解析的帧数量，错误返回-1
 */
int frame_parser_feed(frame_parser_t *p, const char *data, size_t len,
                      frame_callback cb, void *user);

#endif