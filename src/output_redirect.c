#include "output_redirect.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

int open_output_files(char **files,int *append,int count,int *fds)
{
    for (int i = 0; i < count; i++)
    {
        int flags;
        if (append[i] == 0)
        flags = O_WRONLY | O_CREAT | O_TRUNC;
        else
        flags = O_WRONLY | O_CREAT | O_APPEND;
        
        fds[i] = open(files[i], flags, 0644);

        if (fds[i] == -1)
        {
            printf("cshell: unable to create file for writing\n");
            for (int j = 0; j < i; j++)
            close(fds[j]);
            return -1;
        }
    }
    return 0;
}
//copy temp output to every ouput file
int copy_output_to_files(int temp_fd,int *output_fds,int output_count)
{
    if (lseek(temp_fd, 0, SEEK_SET) == -1)
    {
        perror("cshell");
        return -1;
    }
    int read_fd = dup(temp_fd);
    if (read_fd == -1)
    {
        perror("cshell");
        return -1;
    }
    FILE *file = fdopen(read_fd, "r");
    if (file == NULL)
    {
        close(read_fd);
        perror("cshell");
        return -1;
    }

    char *line = NULL;
    size_t size = 0;
    ssize_t len;
    while ((len = getline(&line, &size, file)) != -1)
    {
        for (int i = 0; i < output_count; i++)
        write(output_fds[i], line, len);
    }
    free(line);
    fclose(file);
    return 0;
}