/**
 * @file protocol.c
 * @brief 协议解析模块实现
 * @details 实现了流式协议的粘包处理，能够从字节流中解析出完整的帧，
 *          处理TCP的粘包问题，确保每个帧被完整正确地解析
 */
#include "protocol.h"
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

/**
 * @brief 初始化帧解析器
 * @param p 解析器指针
 */
void frame_parser_init(frame_parser_t *p) {
    p->buffer = NULL;      // 累积的接收缓冲区
    p->buffer_len = 0;     // 当前缓冲区中数据的长度
    p->expected = 0;       // 期望接收的下一个完整帧的总长度
}

/**
 * @brief 销毁帧解析器，释放资源
 * @param p 解析器指针
 */
void frame_parser_destroy(frame_parser_t *p) {
    if (p->buffer) {
        free(p->buffer);
        p->buffer = NULL;
    }
}

/**
 * @brief 向解析器输入新收到的数据，解析完整帧
 * @param p 解析器指针
 * @param data 新收到的数据
 * @param len 数据长度
 * @param cb 解析出完整帧时的回调函数
 * @param user 用户数据，传递给回调函数
 * @return 成功解析的帧数量，错误返回-1
 * @details 处理TCP粘包问题，累积数据直到能解 析出完整的帧，
 *          每解析出一个完整帧就调用回调函数通知上层
 */
int frame_parser_feed(frame_parser_t *p, const char *data, size_t len,
                      frame_callback cb, void *user) {
    // 1. 将新收到的数据追加到解析器的缓冲区中
    size_t new_len = p->buffer_len + len;//计算新缓冲区的长度
    p->buffer = realloc(p->buffer, new_len);//重新分配内存，将旧缓冲区中的数据复制到新缓冲区中
    memcpy(p->buffer + p->buffer_len, data, len);//将新收到的数据复制到缓冲区中
    p->buffer_len = new_len;

    int processed = 0;  // 本次处理的完整帧数量
    
    // 2. 循环尝试从缓冲区中解析完整帧
    while (1) {
        if (p->expected == 0) {
            // 还不知道下一个帧的长度，先读取协议头部（需要至少5字节）
            if (p->buffer_len < PROTOCOL_HEADER_SIZE) break;
            
            // 解析头部：第1字节是类型，接下来4字节是数据长度（网络字节序）
            uint8_t type = p->buffer[0];
            uint32_t net_len;//开辟4字节空间，用于存储网络字节序的长度长度值
            memcpy(&net_len, p->buffer+1, 4);//将缓冲区中的第2字节开始的4字节复制到net_len中
            size_t data_len = ntohl(net_len);  // 转换为主机字节序
            
            // 安全检查：单帧数据不能超过10MB，防止恶意攻击
            if (data_len > 10*1024*1024) {
                p->buffer_len = 0;
                p->expected = 0;
                return -1;  // 协议错误
            }
            
            // 计算这个完整帧需要的总字节数
            p->expected = PROTOCOL_HEADER_SIZE + data_len;
        }
        
        // 缓冲区数据不足，等待更多数据到达
        if (p->buffer_len < p->expected) break;
        
        // ==================== 成功解析出一个完整帧 ====================
        size_t data_len = p->expected - PROTOCOL_HEADER_SIZE;
        uint8_t type = p->buffer[0];
        
        // 调用回调函数，将解析出的帧交给上层处理
        if (cb) {
            cb(user ? *(int*)user : -1, type, (char*)(p->buffer + PROTOCOL_HEADER_SIZE), data_len, user);
        }
        
        // 从缓冲区中移除已经处理过的数据
        size_t remaining = p->buffer_len - p->expected;
        if (remaining > 0) {
            // 还有剩余数据，移动到缓冲区开头，准备下一轮解析
            memmove(p->buffer, p->buffer + p->expected, remaining);
        }
        
        // 重置状态，准备解析下一个帧
        p->buffer_len = remaining;
        p->expected = 0;
        processed++;
    }
    
    return processed;  // 返回本次处理的完整帧数量
}