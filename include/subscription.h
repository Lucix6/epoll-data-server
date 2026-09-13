/**
 * @file subscription.h
 * @brief 订阅管理模块头文件
 */
#ifndef SUBSCRIPTION_H
#define SUBSCRIPTION_H

#include <stdint.h>

/**
 * @brief 初始化订阅管理模块
 */
void subscription_init(void);

/**
 * @brief 销毁订阅管理模块
 */
void subscription_destroy(void);

/**
 * @brief 添加客户端对某个数据流类型的订阅
 * @param client_fd 客户端socket文件描述符
 * @param type 要订阅的数据流类型
 */
void subscription_add(int client_fd, uint8_t type);

/**
 * @brief 移除客户端的所有订阅（客户端断开连接时调用）
 * @param client_fd 客户端socket文件描述符
 */
void subscription_remove_all(int client_fd);

/**
 * @brief 获取订阅了某个数据流类型的所有客户端fd
 * @param type 数据流类型
 * @param fds 输出数组，存储订阅者的fd
 * @param max_fds 数组最大长度
 * @return 实际返回的订阅者数量
 */
int subscription_get(uint8_t type, int *fds, int max_fds);

/**
 * @brief 移除客户端对某个特定类型的订阅
 * @param client_fd 客户端socket文件描述符
 * @param type 要取消订阅的数据流类型
 */
void subscription_remove(int client_fd, uint8_t type);

#endif