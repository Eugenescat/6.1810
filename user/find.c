#include "../kernel/types.h"
#include "../kernel/stat.h"
#include "user.h"
#include "../kernel/fs.h"
#include "../kernel/fcntl.h"

void find(char *path, char *filename) {
    char buf[512], *p; // buffer to store the path
    int fd; // open the directory of the path
    struct dirent de; // directory entry
    struct stat st; // get the status of the path

    if ((fd = open(path, O_RDONLY)) < 0) // 0: read only
    {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    if(fstat(fd, &st) < 0)
    {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }
    
    switch(st.type)
    {
        case T_DEVICE:
            printf("find: %s is not a directory\n", path);
            close(fd);
            return;
        case T_FILE:
            printf("find: %s is not a directory\n", path);
            close(fd);
            return;
        case T_DIR:
            // if the path is a directory
            if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
                printf("find: path too long\n");
                break;
            }

            while(read(fd, &de, sizeof(de)) == sizeof(de)) // Read through the directory entries
            {
                if(de.inum == 0) // if the directory entry is empty
                    continue;

                if(strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) // 每个文件夹的子文件夹中会包括.和..两个文件夹，这两个需要跳过
                    continue;

                // create a new path by appending the directory entry name to the current path
                strcpy(buf, path);
                p = buf+strlen(buf); // point to the end of the path
                *p++ = '/'; 
                memmove(p, de.name, DIRSIZ); // copy the name of the directory entry to the buffer
                p[DIRSIZ] = 0; // null terminate the buffer

                // Get stats for the current file/directory
                if (stat(buf, &st) < 0) {
                    printf("find: cannot stat %s\n", buf);
                    continue;
                }

                // check if the current file/directory is the one we are looking for
                if(strcmp(de.name, filename) == 0){
                    printf("%s\n", buf);
                }

                // if the current file/directory is a directory, recursively call find
                if(st.type == T_DIR)
                    find(buf, filename);
            }
            break;
    }
    close(fd);
}

int main(int argc, char *argv[]) 
{
    if (argc < 3) {
        printf("Usage: find <path> <filename>\n");
        exit(1);
    }

    // Start finding from the specified path
    find(argv[1], argv[2]);
    exit(0);
}