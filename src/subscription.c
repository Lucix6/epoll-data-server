/**
 * @file subscription.c
 * @brief 订阅管理模块实现
 * @details 维护每个数据流类型的订阅者列表，线程安全地管理客户端的订阅和取消订阅，
 *          支持快速查询某个数据流类型的所有订阅者，实现视频流的一对多分发
 */
#include "subscription.h"
#include <pthread.h>
#include <string.h>

#define MAX_TYPES 256          // 最大支持的数据流类型数量（0~255）
#define MAX_SUBSCRIBERS 1000   // 单个数据流类型最大支持的订阅者数量

/**
 * 订阅表：subs[type][i] 表示订阅了type类型数据流的第i个客户端的fd
 * 二维数组实现：行对应数据流类型，列对应该类型的订阅者列表
 */
static int subs[MAX_TYPES][MAX_SUBSCRIBERS];
static int sub_count[MAX_TYPES];  // 每个数据流类型当前的订阅者数量
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; // 保护订阅表的互斥锁

/**
 * @brief 初始化订阅管理模块
 * @details 清空所有订阅列表，初始化sub_count数组
 */
    void subscription_init(void) {
    for (int i = 0; i < MAX_TYPES; i++) {
        sub_count[i] = 0;
        for (int j = 0; j < MAX_SUBSCRIBERS; j++) {
            subs[i][j] = -1;
        }
    }
}

/**
 * @brief 销毁订阅管理模块
 */
void subscription_destroy(void) {
    // 无需特殊清理
}

/**
 * @brief 添加一个客户端到某个数据流类型的订阅列表
 * @param client_fd 客户端socket文件描述符
 * @param type 要订阅的数据流类型
 * @details 线程安全，自动去重，避免重复订阅
 */
void subscription_add(int client_fd, uint8_t type) {
    if (type >= MAX_TYPES) return;
    pthread_mutex_lock(&mutex);
    
    // 检查是否已经订阅过，避免重复添加
    for (int i = 0; i < sub_count[type]; i++) {
        if (subs[type][i] == client_fd) {
            pthread_mutex_unlock(&mutex);
            return;
        }
    }
    
    // 还有空间，添加到订阅列表末尾
    if (sub_count[type] < MAX_SUBSCRIBERS) {
        subs[type][sub_count[type]++] = client_fd;
    }
    
    pthread_mutex_unlock(&mutex);
}

/**
 * @brief 移除一个客户端的所有订阅
 * @param client_fd 客户端socket文件描述符
 * @details 当客户端断开连接时调用，从所有数据流类型的订阅列表中移除该客户端
 */
void subscription_remove_all(int client_fd) {
    pthread_mutex_lock(&mutex);
    
    // 遍历所有数据流类型
    for (int type = 0; type < MAX_TYPES; type++) {
        // 在该类型的订阅列表中查找客户端
        for (int i = 0; i < sub_count[type]; i++) {
            if (subs[type][i] == client_fd) {
                // 找到后，用最后一个元素填补空位（保持数组紧凑，避免空洞）
                subs[type][i] = subs[type][sub_count[type]-1];
                sub_count[type]--;
                break;
            }			
        }
    }
    
    pthread_mutex_unlock(&mutex);
}

/**
 * @brief 获取某个数据流类型的所有订阅者列表
 * @param type 数据流类型
 * @param fds 输出参数，存储订阅者的fd数组
 * @param max_fds 数组最大长度
 * @return 实际返回的订阅者数量
 * @details 线程安全地拷贝订阅者列表，供数据分发使用
 */
int subscription_get(uint8_t type, int *fds, int max_fds) {
    if (type >= MAX_TYPES) return 0;
    pthread_mutex_lock(&mutex);
    
    int cnt = sub_count[type];
    if (cnt > max_fds) cnt = max_fds;  // 不超过数组容量
    
    // 拷贝订阅者fd列表到输出数组
    for (int i = 0; i < cnt; i++) {
        fds[i] = subs[type][i];
    }
    
    pthread_mutex_unlock(&mutex);
    return cnt;
}