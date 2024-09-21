#include "../kernel/types.h"
#include "../kernel/stat.h"
#include "user.h"

// sieve 函数负责通过管道筛选素数
// left_pipe[0] 用于读取上一个进程通过管道传递过来的数字
// right_pipe[1] 是当前进程用于传递数字给下一个进程（子进程或右邻进程）的管道。它负责将筛选后的数字传递给下一个进程。
void sieve(int left_pipe[2]) __attribute__((noreturn));  // 声明 sieve 函数不会返回

void sieve(int left_pipe[2])
{
    int prime, num;
    int right_pipe[2];

    // 从左边的管道读取第一个数字，并将该整数存储到变量 prime 中
    // read返回值就是读取的字节数，如果读取的字节数为0，表示管道的写端已经关闭且数字已经读完，关闭管道并退出
    if (read(left_pipe[0], &prime, sizeof(prime)) == 0)
    {
        close(left_pipe[0]);
        exit(0); // 没有更多数字，退出子进程
    }

    // 输出当前筛选出的素数
    printf("prime %d\n", prime);

    // 创建一个右边的管道，用于传递筛选后的数字给子进程
    if (pipe(right_pipe) < 0)
    {
        printf("pipe failed\n");
        exit(1); // 如果管道创建失败，输出错误信息并退出
    }

    // 创建一个子进程
    if (fork() == 0)
    {
        // 子进程接收右边的管道传来的数字，递归调用sieve函数
        close(left_pipe[0]);  // 子进程不再需要左管道的读端，关闭它（左管道写端在递归上一层已经关闭）
        close(right_pipe[1]); // 子进程只需要从右管道读数据，不需要写端
        sieve(right_pipe);    // 递归调用 sieve 函数，继续筛选
    }
    else
    {
        // 仍然在父进程中
        close(right_pipe[0]); // 父进程不需要右管道的读端，关闭它

        // 继续从左管道读取所有剩余数字，过滤掉不能被当前素数整除的数字
        while (read(left_pipe[0], &num, sizeof(num)) > 0)
        {
            // 如果数字不能被当前素数整除，写入右管道传递给下一个进程
            if (num % prime != 0)
            {
                write(right_pipe[1], &num, sizeof(num));
            }
        }

        // 关闭管道，因为所有数字都已经处理完
        close(left_pipe[0]);
        close(right_pipe[1]); // 关闭右管道的写端，表示没有更多数据

        // 等待子进程结束，确保子进程完成筛选工作
        wait(0);
        exit(0); // 父进程退出
    }
}

int main(void)
{
    int initial_pipe[2]; // 初始化一个管道，父进程用来生成数字，子进程用来筛选素数
    int i;

    // 创建初始的管道，用于父进程向子进程传递数字
    if (pipe(initial_pipe) < 0)
    {
        printf("pipe failed\n");
        exit(1); // 如果管道创建失败，输出错误信息并退出
    }

    // 创建第一个子进程用于筛选数字
    if (fork() == 0)
    {
        // 子进程处理素数筛选工作
        close(initial_pipe[1]); // 子进程不需要管道的写端，只从管道读数据
        sieve(initial_pipe);    // 调用 sieve 函数开始筛选素数
        // tips: 如果父进程还没有写入任何数据，子进程在读取时会被阻塞，直到有数据写入。
    }
    else
    {
        // 父进程生成 2 到 280 的数字，并将它们写入管道传递给子进程
        close(initial_pipe[0]); // 父进程不需要管道的读端，只写数据
        for (i = 2; i <= 280; i++)
        {
            write(initial_pipe[1], &i, sizeof(i)); // 将数字写入管道
        }

        // 写完所有数字后关闭管道的写端，表示没有更多数据
        close(initial_pipe[1]);

        // 等待子进程完成素数筛选工作
        wait(0);
        exit(0); // 父进程退出
    }

    return 0;
}
