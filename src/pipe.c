#include "pipe.h"
#include "reveal.h"
#include "peek.h"
#include "locate.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_OUTPUT_FILES 100
#define MAX_PATH_LENGTH 4096
//info about one pipe input files,output files,flags,count for each command
typedef struct
{
    char **argv;
    char **input_files;
    int input_count;
    char **output_files;
    int *output_append;
    int output_count;
    //temporary output file for output redirection
    int output_temp_fd;
} PipelineCommand;

//count commands pipes=commands-1
int count_commands(Token *tokens)
{
    int count = 1;
    Token *current = tokens;
    while (current != NULL)
    {
        if (current->type == TOKEN_PIPE)
            count++;
        current = current->next;
    }
    return count;
}

//Count arguments and redirections for one command
void count_parts(Token *start,int *argc,int *input_count,int *output_count)
{
    *argc = 0;
    *input_count = 0;
    *output_count = 0;
    Token *current = start;

    while (current != NULL && current->type != TOKEN_PIPE && current->type != TOKEN_SEMI && current->type != TOKEN_AMP)
    {
        if (current->type == TOKEN_WORD)
        (*argc)++;
        else if (current->type == TOKEN_LT)
        {
            current = current->next;
            if (current != NULL && current->type == TOKEN_WORD)
            (*input_count)++;
        }
        else if (current->type == TOKEN_GT || current->type == TOKEN_GTGT)
        {
            current = current->next;
            if (current != NULL && current->type == TOKEN_WORD)
            (*output_count)++;
        }
        current = current->next;
    }
}
/*
Parse ONE command from the token list
next_start tells us where the next command after | begins.
*/
int parse_command(Token *start,PipelineCommand *command,Token **next_start)
{
    int argc = 0;
    command->input_count = 0;
    command->output_count = 0;
    command->output_temp_fd = -1;
    count_parts(start,&argc,&command->input_count,&command->output_count);
    command->argv =malloc(sizeof(char *) * (argc + 1));
    if (command->argv == NULL)
    return -1;
    command->input_files = NULL;
    //allocat input files
    if (command->input_count > 0)
    {
        command->input_files =malloc(sizeof(char *) * command->input_count);
        if (command->input_files == NULL)
        {
            free(command->argv);
            return -1;
        }
    }
    //Allocate output files.
    command->output_files = NULL;
    command->output_append = NULL;

    if (command->output_count > 0)
    {
        command->output_files =malloc(sizeof(char *) * command->output_count);
        command->output_append =malloc(sizeof(int) * command->output_count);
        if (command->output_files == NULL || command->output_append == NULL)
        {
            free(command->argv);
            free(command->input_files);
            free(command->output_files);
            free(command->output_append);
            return -1;
        }
    }
    int arg_index = 0;
    int input_index = 0;
    int output_index = 0;

    Token *current = start;
    while (current != NULL && current->type != TOKEN_PIPE && current->type != TOKEN_SEMI && current->type != TOKEN_AMP)
    {
        if (current->type == TOKEN_WORD)
        command->argv[arg_index++] =current->value;
        else if (current->type == TOKEN_LT)
        {
            current = current->next;
            command->input_files[input_index++] =current->value;
        }
        else if (current->type == TOKEN_GT)
        {
            current = current->next;
            command->output_files[output_index] =current->value;
            command->output_append[output_index] = 0;
            output_index++;
        }
        else if (current->type == TOKEN_GTGT)
        {
            current = current->next;
            command->output_files[output_index] =current->value;
            command->output_append[output_index] = 1;
            output_index++;
        }
        current = current->next;
    }
    command->argv[arg_index] = NULL;
    //find next command
    if (current != NULL && current->type == TOKEN_PIPE)
    *next_start = current->next;
    else
    *next_start = NULL;
    return 0;
}

//free struct abt one command
void free_command(PipelineCommand *command)
{
    free(command->argv);
    free(command->input_files);
    free(command->output_files);
    free(command->output_append);
    command->argv = NULL;
    command->input_files = NULL;
    command->output_files = NULL;
    command->output_append = NULL;
}
/*
Create combined input
cat < a.txt < b.txt
becomes:
 a.txt
 +
 b.txt
 ↓
 temporary file
 ↓
 stdin
 */
int setup_input(PipelineCommand *command)
{
    if (command->input_count == 0)
        return 0;

    char temp_name[] = "/tmp/cshell_pipe_input_XXXXXX";
    int temp_fd = mkstemp(temp_name);
    if (temp_fd == -1)
    {
        perror("cshell");
        return -1;
    }
    unlink(temp_name);
    for (int i = 0;i < command->input_count;i++)
    {
        int fd =open(command->input_files[i],O_RDONLY);
        if (fd == -1)
        {
            printf("cshell: no such file or directory\n");
            close(temp_fd);
            return -1;
        }
        FILE *file = fdopen(fd, "r");
        if (file == NULL)
        {
            close(fd);
            close(temp_fd);

            perror("cshell");
            return -1;
        }

        char *line = NULL;
        size_t size = 0;
        ssize_t len;
        while ((len = getline(&line,&size,file)) != -1)
        {
            if (write(temp_fd, line, len) != len)
            {
                perror("cshell");
                free(line);
                fclose(file);
                close(temp_fd);
                return -1;
            }
        }
        free(line);
        fclose(file);
    }
    //back to beginning of file
    if (lseek(temp_fd, 0, SEEK_SET) == -1)
    {
        perror("cshell");
        close(temp_fd);
        return -1;
    }
    //linking stdin to combined input file
    if (dup2(temp_fd, STDIN_FILENO) == -1)
    {
        perror("cshell");
        close(temp_fd);
        return -1;
    }
    close(temp_fd);
    return 0;
}
/*
Create temporary output file
this is used for:
command > a.txt > b.txt
command writes ONCE to temp file.
After the command finishes, parent copies the temp output to every destination
*/
int create_output_temp(PipelineCommand *command)
{
    if (command->output_count == 0)
        return 0;
    char temp_name[] = "/tmp/cshell_pipe_output_XXXXXX";
    int fd = mkstemp(temp_name);
    if (fd == -1)
    {
        perror("cshell");
        return -1;
    }
    command->output_temp_fd = fd;
    return 0;
}
//copy temp output to all output files
void copy_output(PipelineCommand *command)
{
    if (command->output_count == 0)
        return;
    int temp_fd = command->output_temp_fd;
    if (temp_fd == -1)
        return;
    if (lseek(temp_fd, 0, SEEK_SET) == -1)
    {
        perror("cshell");
        return;
    }
    //open every output file
    int output_fds[MAX_OUTPUT_FILES];
    for (int i = 0;i < command->output_count;i++)
    {
        int flags;
        if (command->output_append[i] == 0)
        flags =O_WRONLY | O_CREAT | O_TRUNC;
        else
        flags =O_WRONLY | O_CREAT | O_APPEND;
        output_fds[i] =open(command->output_files[i],flags,0644);
        if (output_fds[i] == -1)
        {
            printf("cshell: unable to create file for writing\n");
            for (int j = 0; j < i; j++)
                close(output_fds[j]);
            return;
        }
    }
    FILE *file =fdopen(dup(temp_fd), "r");
    if (file == NULL)
    {
        perror("cshell");
        for (int i = 0;i < command->output_count;i++)
        close(output_fds[i]);
        return;
    }

    char *line = NULL;
    size_t size = 0;
    ssize_t len;
    while ((len = getline(&line,&size,file)) != -1)
    {
        for (int i = 0;i<command->output_count;i++)
        {
            if (write(output_fds[i],line,len) != len)
            perror("cshell");
        }
    }
    free(line);
    fclose(file);
    for (int i = 0;i < command->output_count;i++)
    close(output_fds[i]);
}
int execute_pipeline_builtin(PipelineCommand *command)
{
    if (command == NULL || command->argv == NULL || command->argv[0] == NULL)
    return 0;
    char *name = command->argv[0];
    int argc = 0;
    while (command->argv[argc] != NULL)
    argc++;
    //builtins
    if (strcmp(name, "reveal") == 0)
    {
        reveal_command(command->argv + 1, argc - 1);
        return 1;
    }
    if (strcmp(name, "peek") == 0)
    {
        peek_command(command->argv + 1, argc - 1);
        return 1;
    }
    if (strcmp(name, "locate") == 0)
    {
        locate_command(command->argv + 1, argc - 1);
        return 1;
    }
    return 0;
}
//execute one child command
void execute_pipeline_child(PipelineCommand *command,int command_index,int command_count,int pipe_fds[][2],int pipe_count)
{
    /*
    Explicit < has priority over pipe input.
    Example:cat | sort < input.txt
    sort gets input.txt, NOT cat's output.
    */
    if (command->input_count > 0)
    {
        if (setup_input(command) == -1)
        exit(EXIT_FAILURE);
    }
    else if (command_index > 0)
    {
        //stdin = previous pipe
       if (dup2(pipe_fds[command_index - 1][0],STDIN_FILENO) == -1)
        {
            perror("cshell");
            exit(EXIT_FAILURE);
        }
    }
    /*
    Explicit > / >> has priority over pipe output.
    Example:echo hi > out.txt | sort
    echo hi > out.txt | sort
    echo writes to out.txt, NOT to sort.
     */
    if (command->output_count > 0)
    {
        if (command->output_temp_fd == -1)
        {
            if (create_output_temp(command) == -1)
            exit(EXIT_FAILURE);
        }

        if (dup2(command->output_temp_fd,STDOUT_FILENO) == -1)
        {
            perror("cshell");
            exit(EXIT_FAILURE);
        }
    }
    else if (command_index < command_count - 1)
    {
        //stdout = next pipe
        if (dup2(pipe_fds[command_index][1],STDOUT_FILENO) == -1)
        {
            perror("cshell");
            exit(EXIT_FAILURE);
        }
    }
    //close all pipe fds
    for (int i = 0;i < pipe_count;i++)
    {
        close(pipe_fds[i][0]);
        close(pipe_fds[i][1]);
    }
    if (command->argv[0] == NULL)
    exit(EXIT_FAILURE);
    
    //builtins
    if (execute_pipeline_builtin(command))
    {
        fflush(NULL);
        _exit(EXIT_SUCCESS);
    }
    char *original_command =command->argv[0];
    if (original_command[0] == '%')
    {
        original_command++;
        if (original_command[0] == '\0')
        {
            printf("cshell: command not found ()\n");
            exit(EXIT_FAILURE);
        }
        command->argv[0] =original_command;
    }
    if (strchr(command->argv[0], '/') != NULL)
    {
        if (access(command->argv[0], X_OK) != 0)
        {
            printf("cshell: command not found (%s)\n",original_command);
            exit(EXIT_FAILURE);
        }
        execv(command->argv[0],command->argv);
        printf("cshell: command not found (%s)\n",original_command);
        exit(EXIT_FAILURE);
    }
    char local_path[MAX_PATH_LENGTH];
    snprintf(local_path,sizeof(local_path),"./%s",command->argv[0]);
    if (access(local_path, X_OK) == 0)
    {
        execv(local_path,command->argv);
        printf("cshell: command not found (%s)\n",original_command);
        exit(EXIT_FAILURE);
    }
    execvp(command->argv[0],command->argv);
    printf("cshell: command not found (%s)\n",original_command);
    exit(EXIT_FAILURE);
}

//start execution of pipeline
void execute_pipeline(Token *tokens)
{
    if (tokens == NULL)
        return;
    int command_count=count_commands(tokens);
    int pipe_count=command_count - 1;
    int pipe_fds[pipe_count][2];

    for (int i = 0;i < pipe_count;i++)
    {
        if (pipe(pipe_fds[i]) == -1)
        {
            perror("cshell");
            //Close previously created pipes.
            for (int j = 0; j < i; j++)
            {
                close(pipe_fds[j][0]);
                close(pipe_fds[j][1]);
            }
            return;
        }
    }
    //Allocate information for every command.
    PipelineCommand *commands =calloc(command_count,sizeof(PipelineCommand));
    if (commands == NULL)
    {
        printf("cshell: memory allocation failed\n");
        for (int i = 0;i < pipe_count;i++)
        {
            close(pipe_fds[i][0]);
            close(pipe_fds[i][1]);
        }
        return;
    }
    //Parse every command BEFORE forking.
    Token *current = tokens;
    for (int i = 0;i < command_count;i++)
    {
        Token *next_start = NULL;
        if (parse_command(current,&commands[i],&next_start) == -1)
        {
            printf("cshell: memory allocation failed\n");
            for (int j = 0; j <= i; j++)
            free_command(&commands[j]);
            free(commands);
            for (int j = 0;j < pipe_count;j++)
            {
                close(pipe_fds[j][0]);
                close(pipe_fds[j][1]);
            }
            return;
        }
        current = next_start;
    }
     /* Create temporary output files BEFORE fork.
    This is important because the parent must retain the
    descriptor so that it can copy the child's output later.
   */
    for (int i = 0;i < command_count;i++)
    {
        if (commands[i].output_count > 0)
        {
            if (create_output_temp(&commands[i]) == -1)
            {
                for (int j = 0;j < command_count;j++)
                {
                    if (commands[j].output_temp_fd != -1)
                    close(commands[j].output_temp_fd);
                    free_command(&commands[j]);
                }
                free(commands);
                for (int j = 0;j < pipe_count;j++)
                {
                    close(pipe_fds[j][0]);
                    close(pipe_fds[j][1]);
                }
                return;
            }
        }
    }
    pid_t *pids =malloc(sizeof(pid_t) * command_count);
    if (pids == NULL)
    {
        printf("cshell: memory allocation failed\n");
        for (int i = 0;i < command_count;i++)
        {
            if (commands[i].output_temp_fd != -1)
            close(commands[i].output_temp_fd);
            free_command(&commands[i]);
        }
        free(commands);
        for (int i = 0;i < pipe_count;i++)
        {
            close(pipe_fds[i][0]);
            close(pipe_fds[i][1]);
        }
        return;
    }
    //fork every command
    for (int i = 0;i < command_count;i++)
    {
        pid_t pid = fork();
        if (pid == -1)
        {
            perror("cshell");
            //parent closes unnecessary pipes
            for (int j = 0;j < pipe_count;j++)
            {
                close(pipe_fds[j][0]);
                close(pipe_fds[j][1]);
            }

            for (int j = 0; j < i; j++)
                waitpid(pids[j], NULL, 0);

            free(pids);

            for (int j = 0;j < command_count;j++)
            {
                if (commands[j].output_temp_fd != -1)
                    close(commands[j].output_temp_fd);
                free_command(&commands[j]);
            }
            free(commands);
            return;
        }
        if (pid == 0)
        {
            execute_pipeline_child(&commands[i],i,command_count,pipe_fds,pipe_count);
            exit(EXIT_FAILURE);
        }
        pids[i] = pid;
    }

    for (int i = 0;i < pipe_count;i++)
    {
        close(pipe_fds[i][0]);
        close(pipe_fds[i][1]);
    }

    for (int i = 0;i < command_count;i++)
    waitpid(pids[i], NULL, 0);
 
    for (int i = 0;i < command_count;i++)
    {
        if (commands[i].output_count > 0)
        {
            copy_output(&commands[i]);
            close(commands[i].output_temp_fd);
            commands[i].output_temp_fd = -1;
        }
    }
    for (int i = 0;i < command_count;i++)
    free_command(&commands[i]);
    free(commands);
    free(pids);
}