/**
 * @file worker.h
 * @brief 工作线程池模块头文件
 */
#ifndef WORKER_H
#define WORKER_H

/**
 * @brief 启动所有工作线程
 */
void start_workers(void);

/**
 * @brief 停止所有工作线程
 */
void stop_workers(void);

#endif