#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include "execute.h"
#include "input_redirect.h"

void execute_command(char **argv,char **input_files, int input_count)
{
    if (argv == NULL || argv[0] == NULL)
    return;

    char *command = argv[0];
    char local_path[4096];

    if (command[0] == '%')
    {
        command++;
        if (command[0] == '\0')
        {
            printf("cshell: command not found ()\n");
            return;
        }
        argv[0] = command;
    }
    else if (strchr(command, '/') != NULL)
    {
        if (access(command, X_OK) != 0)
        {
            printf("cshell: command not found (%s)\n", command);
            return;
        }
    }
    else
    {
        snprintf(local_path, sizeof(local_path), "./%s", command);
        if (access(local_path, X_OK) == 0)
        argv[0] = local_path;
        
    }
    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork");
        return;
    }
    if (pid == 0)
    {
        //child process
        if (input_count > 0)
        {
            if (handle_input_redirection(input_files, input_count) == -1)
            exit(EXIT_FAILURE);
        }
        if (strchr(argv[0], '/') != NULL)
        execv(argv[0], argv);
        else
        execvp(argv[0], argv);
        
        printf("cshell: command not found (%s)\n", command);
        exit(EXIT_FAILURE);
    }
    //parent process waits
    waitpid(pid, NULL, 0);
}