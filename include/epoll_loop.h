/**
 * @file epoll_loop.h
 * @brief epoll事件循环模块头文件
 */
#ifndef EPOLL_LOOP_H
#define EPOLL_LOOP_H

/**
 * @brief 启动epoll事件循环
 */
void start_epoll_loop(void);

/**
 * @brief 停止epoll事件循环
 */
void stop_epoll_loop(void);

#endif