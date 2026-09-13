/**
 * @file queue.c
 * @brief 线程安全的任务队列实现
 * @details 实现了主线程（epoll线程）与工作线程池之间的通信队列，
 *          使用生产者-消费者模型，支持阻塞的push和pop操作，
 *          主线程作为生产者将收到的数据推入队列，工作线程作为消费者取出处理
 */
#include "queue.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static task_t queue[QUEUE_SIZE];  // 循环队列存储任务
static int head = 0, tail = 0, count = 0; // 队列头、尾、当前元素数量

// 线程同步原语
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;      // 保护队列的互斥锁
static pthread_cond_t not_full  = PTHREAD_COND_INITIALIZER;    // 队列不满的条件变量（生产者等待）
static pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER;    // 队列不空的条件变量（消费者等待）

/**
 * @brief 初始化任务队列
 */
void queue_init(void) {
    for (int i = 0; i < QUEUE_SIZE; i++) {
        queue[i].data = NULL;
        queue[i].len = 0;
        queue[i].fd = -1;
    }
}

/**
 * @brief 销毁任务队列，释放所有未处理任务的资源
 */
void queue_destroy(void) {
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < QUEUE_SIZE; i++) {
        if (queue[i].data) {
            free(queue[i].data);
            queue[i].data = NULL;
        }
    }
    pthread_mutex_unlock(&mutex);
}

/**
 * @brief 将任务推入队列（生产者操作）
 * @param fd 发送数据的客户端fd
 * @param data 数据缓冲区
 * @param len 数据长度
 * @details 主线程调用，队列满时会阻塞等待，线程安全
 */
void push_task(int fd, const char *data, int len) {
    pthread_mutex_lock(&mutex);
    
    // 如果队列已满，等待"队列不满"的条件信号
    while (count == QUEUE_SIZE) {
        pthread_cond_wait(&not_full, &mutex);
    }
    
    // 复制数据，分配堆内存（所有权转移给消费者）
    char *new_data = malloc(len);
    if (!new_data) {
        pthread_mutex_unlock(&mutex);
        return;
    }
    memcpy(new_data, data, len);
    
    // 将任务写入队列尾
    queue[tail].fd = fd;
    queue[tail].data = new_data;
    queue[tail].len = len;
    tail = (tail + 1) % QUEUE_SIZE;  // 循环队列，尾指针后移
    count++;
    
    // 发送"队列不空"信号，唤醒等待的工作线程
    pthread_cond_signal(&not_empty);
    pthread_mutex_unlock(&mutex);
}

/**
 * @brief 从队列中取出一个任务（消费者操作）
 * @param task 输出参数，存储取出的任务
 * @return 成功返回1
 * @details 工作线程调用，队列空时会阻塞等待，线程安全
 */
int pop_task(task_t *task) {
    pthread_mutex_lock(&mutex);
    
    // 如果队列为空，等待"队列不空"的条件信号
    while (count == 0) {
        pthread_cond_wait(&not_empty, &mutex);
    }
    
    // 从队列头取出任务
    task->fd   = queue[head].fd;
    task->data = queue[head].data;
    task->len  = queue[head].len;
    queue[head].data = NULL;
    
    head = (head + 1) % QUEUE_SIZE;  // 循环队列，头指针后移
    count--;
    
    // 发送"队列不满"信号，唤醒可能在等待的主线程
    pthread_cond_signal(&not_full);
    pthread_mutex_unlock(&mutex);
    
    return 1;
}