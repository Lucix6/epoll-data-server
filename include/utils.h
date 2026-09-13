/**
 * @file utils.h
 * @brief 工具函数模块头文件
 */
#ifndef UTILS_H
#define UTILS_H

/**
 * @brief 设置socket为非阻塞模式
 * @param fd socket文件描述符
 * @return 成功返回0，失败返回-1
 */
int set_nonblocking(int fd);

#endif