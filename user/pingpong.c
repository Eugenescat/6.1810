#include "../kernel/types.h"
#include "user.h"

int main() {
    int p1[2], p2[2]; // 创建两个管道，p1用于父到子，p2用于子到父
    char buf[1];
    int pid;

    // 创建管道
    pipe(p1);
    pipe(p2);

    pid = fork();  // 创建子进程

    if (pid == 0) {
        // 子进程
        read(p1[0], buf, 1);  // 从父进程的管道读取一个字节
        printf("%d: received ping\n", getpid());  // 打印收到信息
        write(p2[1], buf, 1);  // 将数据写回给父进程
        exit(0);  // 子进程结束
    } else {
        // 父进程
        write(p1[1], "x", 1);  // 向子进程发送一个字节
        read(p2[0], buf, 1);   // 从子进程接收一个字节
        printf("%d: received pong\n", getpid());  // 打印收到信息
        wait(0);  // 等待子进程结束
        exit(0);  // 父进程结束
    }
}
