/**
 * @file queue.h
 * @brief 线程安全任务队列模块头文件
 */
#ifndef QUEUE_H
#define QUEUE_H

/**
 * @brief 任务结构体
 * @details 主线程收到的数据封装成任务，交给工作线程处理
 */
typedef struct {
    int   fd;     // 发送该数据的客户端socket文件描述符
    char *data;   // 数据缓冲区（堆内存，工作线程负责释放）
    int   len;    // 数据长度
} task_t;

/**
 * @brief 初始化任务队列
 */
void queue_init(void);

/**
 * @brief 销毁任务队列，释放所有资源
 */
void queue_destroy(void);

/**
 * @brief 将任务推入队列（生产者，主线程调用）
 * @param fd 客户端fd
 * @param data 数据缓冲区
 * @param len 数据长度
 */
void push_task(int fd, const char *data, int len);

/**
 * @brief 从队列中取出任务（消费者，工作线程调用）
 * @param task 输出参数，存储取出的任务
 * @return 成功返回1，失败返回0
 */
int  pop_task(task_t *task);

#endif