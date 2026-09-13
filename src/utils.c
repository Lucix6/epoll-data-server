/**
 * @file utils.c
 * @brief 工具函数模块实现
 */
#include "utils.h"
#include <fcntl.h>
#include <errno.h>

/**
 * @brief 设置文件描述符为非阻塞模式
 * @param fd 要设置的文件描述符
 * @return 成功返回0，失败返回-1
 * @details 所有socket都需要设置为非阻塞，配合epoll使用，避免网络I/O阻塞主线程
 */
int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}