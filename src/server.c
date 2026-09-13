#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#define PORT 8888
#define BUF_LEN 4096

int main()
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(server_fd, 1);

    printf("等待客户端连接，端口8888\n");
    int client_fd = accept(server_fd, NULL, NULL);

    // 创建文件
    int fd = open("recv_out.png", O_WRONLY|O_CREAT, 0644);
    char buf[BUF_LEN];
    int len;

    // 循环接收二进制图片
    while ((len = recv(client_fd, buf, BUF_LEN, 0)) > 0)
    {
        write(fd, buf, len);
    }

    printf("图片接收完成\n");
    close(fd);
    close(client_fd);
    close(server_fd);
    return 0;
}