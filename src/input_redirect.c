#include "input_redirect.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

int handle_input_redirection(char **files, int count)
{
    int temp_fd;
    temp_fd = open("/tmp/cshell_input.tmp",O_CREAT | O_RDWR | O_TRUNC,0600);
    if (temp_fd == -1)
    {
        perror("cshell");
        return -1;
    }

    for (int i = 0; i < count; i++)
    {
        int fd;
        FILE *file;
        char *line = NULL;
        size_t size = 0;
        ssize_t len;
        fd = open(files[i], O_RDONLY);
        if (fd == -1)
        {
            printf("cshell: no such file or directory\n");
            close(temp_fd);
            unlink("/tmp/cshell_input.tmp");
            return -1;
        }
        file = fdopen(fd, "r");

        if (file == NULL)
        {
            close(fd);
            close(temp_fd);
            unlink("/tmp/cshell_input.tmp");
            perror("cshell");
            return -1;
        }
        while ((len = getline(&line, &size, file)) != -1)
        write(temp_fd, line, len);
        free(line);
        fclose(file);
    }

    lseek(temp_fd, 0, SEEK_SET);
    // make combined file to stdin
    if (dup2(temp_fd, STDIN_FILENO) == -1)
    {
        perror("cshell");
        close(temp_fd);
        unlink("/tmp/cshell_input.tmp");
        return -1;
    }
    close(temp_fd);
    unlink("/tmp/cshell_input.tmp");
    return 0;
}