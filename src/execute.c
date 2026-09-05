#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include "execute.h"
#include "input_redirect.h"
#include "output_redirect.h"
#include "background.h"
#include <fcntl.h>

#define OUTPUT_TEMP_FILE "/tmp/cshell_output.tmp"
#define MAX_OUTPUT_FILES 100


int execute_command(char **argv,char **input_files,int input_count,char **output_files,int *output_append,int output_count)
{
    if (argv == NULL || argv[0] == NULL)
    return -1;

    char *command = argv[0];
    char local_path[4096];
    

    if (command[0] == '%')
    {
        command++;
        if (command[0] == '\0')
        {
            printf("cshell: command not found ()\n");
            return -1;
        }
        argv[0] = command;
    }
    else if (strchr(command, '/') != NULL)
    {
        if (access(command, X_OK) != 0)
        {
            printf("cshell: command not found (%s)\n", command);
            return -1;
        }
    }
    else
    {
        snprintf(local_path, sizeof(local_path), "./%s", command);
        if (access(local_path, X_OK) == 0)
        argv[0] = local_path;
        
    }
    int input_fd=-1;
    if (input_count > 0)
    {
        input_fd = handle_input_redirection(input_files,input_count);
        if (input_fd == -1)
        return -1;
    }
    int output_fds[MAX_OUTPUT_FILES];
    if (output_count > 0)
    {
        if (open_output_files(output_files,output_append,output_count,output_fds) == -1)
        {
            if (input_fd != -1)
            {
                close(input_fd);
                unlink("/tmp/cshell_input.tmp");
            }
            return -1;
        }
    }
    int output_temp_fd = -1;
    if (output_count > 0)
    {
        output_temp_fd = open(OUTPUT_TEMP_FILE,O_CREAT | O_RDWR | O_TRUNC,0600);
        if (output_temp_fd == -1)
        {
            perror("cshell");
            if (input_fd != -1)
            {
                close(input_fd);
                unlink("/tmp/cshell_input.tmp");
            }
            for (int i = 0; i < output_count; i++)
            close(output_fds[i]);
            return -1;
        }
    }
    pid_t pid = fork();
    if (pid == 0)
    {
        setpgid(0, 0);

            /*
     * Restore default terminal signal behaviour.
     */
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);




        if (input_fd != -1)
        {
            if (dup2(input_fd, STDIN_FILENO) == -1)
            {
                perror("cshell");
                exit(EXIT_FAILURE);
            }
            close(input_fd);
        }
        if (output_count > 0)
        {
            if (dup2(output_temp_fd, STDOUT_FILENO) == -1)
            {
                perror("cshell");
                exit(EXIT_FAILURE);
            }
            close(output_temp_fd);
        }
        if (strchr(argv[0], '/') != NULL)
        execv(argv[0], argv);
        else
        execvp(argv[0], argv);
        printf("cshell: command not found (%s)\n", command);
        exit(EXIT_FAILURE);
    }
    else
    {
        setpgid(pid, pid);

    /*
     * Give terminal to foreground process group.
     */
    if (tcsetpgrp(STDIN_FILENO, pid) == -1)
    {
        perror("cshell: tcsetpgrp");
    }

    int status;

    /*
     * WUNTRACED is essential for Ctrl-Z.
     */
    if (waitpid(pid, &status, WUNTRACED) == -1)
    {
        perror("cshell: waitpid");
    }

    /*
     * Shell gets terminal back.
     */
    if (tcsetpgrp(STDIN_FILENO, shell_pgid) == -1)
    {
        perror("cshell: tcsetpgrp");
    }

    if (WIFSTOPPED(status))
{
    int job_index =
        create_background_job(
            pid,
            pid,
            argv[0]
        );

    if (job_index != -1)
    {
        add_background_process(
            job_index,
            pid,
            argv[0]
        );
        jobs[job_index].processes[0].stopped = 1;
        jobs[job_index].stopped = 1;

        printf("[%d] + Stopped %s\n",
               jobs[job_index].job_number,
               argv[0]);

        fflush(stdout);
    }
    return 0;
}

        
        if (input_fd != -1)
        {
            close(input_fd);
            unlink("/tmp/cshell_input.tmp");
        }
        if (output_count > 0)
        {
            copy_output_to_files(output_temp_fd,output_fds,output_count);
            close(output_temp_fd);
            unlink(OUTPUT_TEMP_FILE);
            for (int i = 0; i < output_count; i++)
            close(output_fds[i]);
        }
        return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
    }
}

void execute_background_command(char **argv,
                                char **input_files,
                                int input_count,
                                char **output_files,
                                int *output_append,
                                int output_count)
{
    if (argv == NULL || argv[0] == NULL)
        _exit(EXIT_FAILURE);

    char *command = argv[0];
    char local_path[4096];

    /*
     * Same command-path handling as execute_command().
     */
    if (command[0] == '%')
    {
        command++;

        if (command[0] == '\0')
        {
            printf("cshell: command not found ()\n");
            _exit(EXIT_FAILURE);
        }

        argv[0] = command;
    }
    else if (strchr(command, '/') != NULL)
    {
        if (access(command, X_OK) != 0)
        {
            printf("cshell: command not found (%s)\n",
                   command);
            _exit(EXIT_FAILURE);
        }
    }
    else
    {
        snprintf(local_path,
                 sizeof(local_path),
                 "./%s",
                 command);

        if (access(local_path, X_OK) == 0)
            argv[0] = local_path;
    }

    /*
     * Input redirection.
     */
    int input_fd = -1;

    if (input_count > 0)
    {
        input_fd =
            handle_input_redirection(input_files,
                                     input_count);

        if (input_fd == -1)
            _exit(EXIT_FAILURE);
    }

    /*
     * Output redirection.
     */
    int output_fds[MAX_OUTPUT_FILES];

    if (output_count > 0)
    {
        if (open_output_files(output_files,
                              output_append,
                              output_count,
                              output_fds) == -1)
        {
            if (input_fd != -1)
            {
                close(input_fd);
                unlink("/tmp/cshell_input.tmp");
            }

            _exit(EXIT_FAILURE);
        }
    }

    /*
     * Same temporary output mechanism
     * as the foreground executor.
     */
    int output_temp_fd = -1;

    if (output_count > 0)
    {
        output_temp_fd =
            open(OUTPUT_TEMP_FILE,
                 O_CREAT | O_RDWR | O_TRUNC,
                 0600);

        if (output_temp_fd == -1)
        {
            perror("cshell");

            if (input_fd != -1)
            {
                close(input_fd);
                unlink("/tmp/cshell_input.tmp");
            }

            for (int i = 0;
                 i < output_count;
                 i++)
            {
                close(output_fds[i]);
            }

            _exit(EXIT_FAILURE);
        }
    }

    /*
     * Apply input redirection.
     */
    if (input_fd != -1)
    {
        if (dup2(input_fd, STDIN_FILENO) == -1)
        {
            perror("cshell: dup2");
            _exit(EXIT_FAILURE);
        }

        close(input_fd);
    }

    /*
     * Apply output redirection.
     */
    if (output_count > 0)
    {
        if (dup2(output_temp_fd, STDOUT_FILENO) == -1)
        {
            perror("cshell: dup2");
            _exit(EXIT_FAILURE);
        }

        close(output_temp_fd);
    }

    /*
     * IMPORTANT:
     *
     * There is NO fork().
     * There is NO waitpid().
     * There is NO tcsetpgrp().
     *
     * This process is already the background child.
     */

    if (strchr(argv[0], '/') != NULL)
        execv(argv[0], argv);
    else
        execvp(argv[0], argv);

    printf("cshell: command not found (%s)\n",
           command);

    _exit(EXIT_FAILURE);
}