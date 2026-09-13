/**
 * @file epoll_loop.c
 * @brief epoll事件循环模块实现 - 服务器的"大门口接待员"
 * @details 这个模块就像服务器的大门口接待员，只做三件事：
 *          1. 盯紧大门（监听socket），有新客人（客户端）来就开门迎接
 *          2. 每个客人进来后，给他们发专属号牌，也交给管家（epoll）盯着
 *          3. 老客人说话了（发数据了），立刻把话记下来，扔进后面的处理篮（任务队列）
 *          它绝不处理具体业务，只做网络IO的中转站，把所有等待和收发集中在一个线程
 */

#include "epoll_loop.h"
#include "common.h"
#include "utils.h"
#include "queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static int epoll_fd = -1;          // 管家（epoll内核实例）的文件描述符，帮我们盯着所有socket
static int listen_fd = -1;         // 服务器大门（监听socket）的文件描述符，专门用来接新客人
static int running = 1;            // 接待员是否继续上班的标志，设为0就下班关门

/**
 * @brief 把新进来的客人添加到管家的监视列表里
 * @param fd 这个客人的专属号牌（客户端socket文件描述符）
 * @details 告诉管家：以后这个客人要是说话了（发数据），记得立刻叫我
 */
static void add_client(int fd) {
    struct epoll_event ev;
    ev.events = EPOLLIN;           // 只监听"有数据可读"事件，也就是客人说话了
    ev.data.fd = fd;               // 把这个客人的号牌和事件绑定，后面能找到是谁
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);  // 告诉管家把这个人加上
}

/**
 * @brief 客人走了，把他从管家的监视列表里删掉，送走
 * @param fd 这个客人的专属号牌（客户端socket文件描述符）
 */
static void remove_client(int fd) {
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);  // 告诉管家不用再盯这个人了
    close(fd);  // 把他的号牌回收，关闭连接
}

/**
 * @brief 让接待员下班，停止整个事件循环
 * @details 打烊了，告诉接待员不用再盯梢了，把管家也遣散，关大门
 */
void stop_epoll_loop(void) {
    running = 0;  // 告诉接待员该下班了
    if (epoll_fd != -1) {
        close(epoll_fd);  // 把管家（epoll实例）关掉
        epoll_fd = -1;
    }
}

/**
 * @brief 接待员上班，启动整个epoll事件循环
 * @details 先把大门立起来，把管家请来，然后开始无限期盯梢，直到打烊
 */
void start_epoll_loop(void) {
    // ========== 第一步：准备大门（服务器初始化阶段，只做一次） ==========
    // 1. 领一个门牌号，创建大门（TCP监听套接字）
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1) { perror("创建大门失败"); exit(1); }
    
    // 2. 设置地址可以重用，避免上次关门后端口还没释放，这次开不了门
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // 3. 在门牌上写清楚：绑定服务器的地址和端口，所有网卡都能进
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;  // 本机所有网卡都可以用这个门
    addr.sin_port = htons(PORT);        // 端口号要转成网络字节序，这是网络协议要求
    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("大门绑定地址失败"); close(listen_fd); exit(1);
    }
    
    // 4. 装好门铃，开始营业：调用listen开始监听连接
    if (listen(listen_fd, 10) == -1) { perror("开始监听失败"); close(listen_fd); exit(1); }
    
    // 5. 把大门设成"铃响了才去开"的非阻塞模式，绝对不能卡死在开门上
    // 如果用阻塞模式，没客人的时候接待员会一直站在门口等，啥也干不了
    set_nonblocking(listen_fd);

    // 6. 把管家（epoll内核实例）请来，他帮我们盯所有socket
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) { perror("请管家失败"); close(listen_fd); exit(1); }
    
    // 7. 先把大门交给管家盯：告诉他"大门有人敲（新连接）就立刻叫我"
    struct epoll_event ev;
    ev.events = EPOLLIN;  // 监听大门的可读事件，也就是有人敲门要进来
    ev.data.fd = listen_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev);

    printf("服务器大门打开，接待员开始上班，监听端口 %d\n", PORT);

    char buffer[BUFFER_SIZE];           // 接待员的小本本，临时记客人说的话
    struct epoll_event events[MAX_EVENTS]; // 管家每天交的"动静名单"，存所有有动静的socket

    // ========== 第二步：无限循环盯梢（服务器运行期间99.9%的时间都在这） ==========
    while (running) {
        // 拿着管家给的名单，站在大厅发呆等动静，-1表示无限等，没事就睡觉不占CPU
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            if (!running) break;  // 如果是让我们下班了，正常退出循环
            perror("管家传消息出错了");
            break;
        }
        
        // 挨个处理管家说的所有有动静的人
        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;  // 拿出这个有动静的人的号牌
            
            if (fd == listen_fd) {
                // ========== 情况A：动静来自大门！有新客人要进来 ==========
                // 循环开门，把门口排队的所有客人都接进来，一个都不能漏
                while (1) {
                    struct sockaddr_in client_addr;
                    socklen_t len = sizeof(client_addr);
                    // 开门接客人，accept会给这个新客人发一个专属的号牌（client_fd）
                    int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &len);
                    
                    if (client_fd == -1) {
                        // 非阻塞模式下，门口的客人都接完了，就会返回EAGAIN，这时候停止开门
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        perror("开门接客人失败");
                        break;
                    }
                    
                    // 给这个新客人的socket也设成非阻塞，不然会影响接待员盯梢
                    set_nonblocking(client_fd);
                    // 把这个新客人的号牌交给管家，以后他说话了也叫我
                    add_client(client_fd);
                    
                    // 打印日志：记下这个客人从哪来的，号牌是多少
                    char ip[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
                    printf("新客人进来了：%s:%d，他的号牌是%d\n", ip, ntohs(client_addr.sin_port), client_fd);
                }
            } else {
                // ========== 情况B：动静来自某个老客人！他说话了，发数据了 ==========
                // 循环听，把这个客人当前说的所有话都听完，一个字都不能漏
                while (1) {
                    // 把客人说的话读到小本本（buffer）里
                    int n = recv(fd, buffer, sizeof(buffer), 0);
                    
                    if (n == -1) {
                        // 这个客人的话听完了，非阻塞模式下会返回EAGAIN，停止监听
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        perror("听客人说话出错了");
                        remove_client(fd);  // 出问题了，把这个客人送走
                        break;
                    } else if (n == 0) {
                        // 客人主动走了，不说了，要断开连接
                        printf("号牌%d的客人走了\n", fd);
                        remove_client(fd);
                        break;
                    } else {
                        // 听完了，立刻把他说的话写在纸条上，连他的号牌一起，扔进后面的处理篮
                        // 接待员绝不分析内容，扔完立刻回去盯梢，一秒都不耽误
                        push_task(fd, buffer, n);
                    }
                }
            }
        }
    }
    
    // 下班了，把大门关上
    close(listen_fd);
}