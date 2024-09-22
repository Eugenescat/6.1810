#include "../kernel/types.h"
#include "../kernel/stat.h"
#include "user.h"
#include "../kernel/fcntl.h"
#include "../kernel/param.h" // for MAXARG

// it reads lines from the standard input, and it runs the command for each line, appending the line to the command's arguments.
int
main(int argc, char *argv[])
{
    char buf[512];
    char *cmd[MAXARG]; // 存放命令和参数
    int i, n;
    
    // 保存xargs之后待执行的命令到 cmd 数组
    for (i = 1; i < argc && i < MAXARG - 1; i++) 
    {
        cmd[i - 1] = argv[i]; // cmd[0] 存放命令，cmd[1] 存放第一个参数，以此类推（argv[0] 是 xargs 命令本身）例如：xargs echo hello
    }
    cmd[i - 1] = 0; // 末尾要有一个 null 指针来标记命令的结束

    // 从标准输入读取用户输入（管道传递给 xargs 的输入）
    while ((n = read(0, buf, sizeof(buf))) > 0) // 第一个参数是文件描述符，0 代表标准输入stdin, 1 代表标准输出stdout, 2 代表标准错误stderr
    {
        int pid;
        int start = 0;
        
        for (int j = 0; j < n; j++) 
        {
            // 当读取到换行符时，表明我们读取了一行输入
            if (buf[j] == '\n') 
            {
                buf[j] = '\0'; // 当找到换行符时，将其替换为字符串结束符 '\0'，这可以让程序将这一行的输入视为一个完整的字符串，方便后续的命令处理
                
                // 将这个字符串作为一个参数追加到命令参数列表的末尾
                // cmd[i - 1] 是 null 指针，所以这里可以直接赋值
                cmd[i - 1] = buf + start; // buf是指向buf数组的指针，start是buf中当前字符串的起始下标，所以cmd[i - 1]指向buf数组中当前字符串的起始地址
                cmd[i] = 0; // 最后一个元素设为 null
                
                // 创建子进程来执行命令
                if ((pid = fork()) == 0) 
                {
                    exec(cmd[0], cmd); // 子进程执行命令
                    // 调用 exec() 后，当前进程的内容会被新程序的内容替代。如果 exec() 成功调用，接下来的代码将不会执行，因为新的程序已经替换了当前进程的代码。
                    fprintf(2, "exec failed\n");
                    exit(1); // 如果 exec 失败，子进程退出
                } else 
                {
                    wait(0); // 父进程等待子进程完成
                }

                // 重置 start，准备处理下一行输入（\n之后的第一个下标）
                start = j + 1;
            }
        }
    }

    exit(0);
}
