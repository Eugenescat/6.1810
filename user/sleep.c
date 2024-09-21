#include "../kernel/types.h"
#include "../kernel/fcntl.h"
#include "user.h"

// e.g. sleep 10
int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Usage: sleep <ticks>\n");
        exit(1);
    }

    int ticks = atoi(argv[1]); // 将命令行参数转化为整数
    if (ticks < 0)
    {
        printf("Error: invalid number of ticks.\n");
        exit(1);
    }

    sleep(ticks); // 调用 xv6 的 sleep 系统调用
    exit(0);      // 退出程序
}