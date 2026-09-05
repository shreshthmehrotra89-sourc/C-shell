#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include "execute.h"
#include "background.h"
#include "pipe.h"

BackgroundJob jobs[MAX_BACKGROUND_JOBS];

int job_count = 0;

int next_job_number = 1;
static volatile sig_atomic_t foreground_running = 0;


/*
 * Check whether a command contains a pipe.
 */
static int contains_pipe(Token *tokens)
{
    Token *current = tokens;

    while (current != NULL)
    {
        if (current->type == TOKEN_PIPE)
            return 1;

        current = current->next;
    }

    return 0;
}


/*
 * Store only the command name.
 *
 * Example:
 *
 * echo hello > a.txt
 *
 * stores:
 *
 * echo
 */
static void get_command_name(Token *tokens,
                             char *buffer,
                             int buffer_size)
{
    buffer[0] = '\0';

    if (tokens == NULL)
        return;

    while (tokens != NULL)
    {
        if (tokens->type == TOKEN_WORD)
        {
            snprintf(buffer,
                     buffer_size,
                     "%s",
                     tokens->value);

            return;
        }

        tokens = tokens->next;
    }
}




/*
 * Print a completed background job.
 *
 * This function is NOT called directly by SIGCHLD.
 */
static void print_completion(int index)
{
    if (index < 0 || index >= job_count)
        return;

    BackgroundJob *job = &jobs[index];

    if (WIFEXITED(job->status) &&
        WEXITSTATUS(job->status) == 0)
    {
        printf("%s with pid %d exited normally\n",
               job->command,
               job->reported_pid);
    }
    else
    {
        printf("%s with pid %d exited abnormally\n",
               job->command,
               job->reported_pid);
    }

    fflush(stdout);
}


/*
 * Convert a positive integer to a string.
 *
 * Used inside the signal handler because sprintf/printf
 * are not async-signal-safe.
 */
static int integer_to_string(char *buffer, int value)
{
    char temp[32];

    int i = 0;
    int j = 0;

    if (value == 0)
    {
        buffer[0] = '0';
        buffer[1] = '\0';
        return 1;
    }

    while (value > 0)
    {
        temp[i++] = '0' + (value % 10);
        value /= 10;
    }

    while (i > 0)
    {
        buffer[j++] = temp[--i];
    }

    buffer[j] = '\0';

    return j;
}


/*
 * Write completion message using write().
 *
 * write() is async-signal-safe.
 */
static void write_completion_from_handler(BackgroundJob *job)
{
    char buffer[8192];

    int pos = 0;

    /*
     * command
     */
    for (int i = 0;
         job->command[i] != '\0' &&
         pos < (int)sizeof(buffer) - 1;
         i++)
    {
        buffer[pos++] = job->command[i];
    }

    /*
     * " with pid "
     */
    const char text1[] = " with pid ";

    for (int i = 0;
         text1[i] != '\0' &&
         pos < (int)sizeof(buffer) - 1;
         i++)
    {
        buffer[pos++] = text1[i];
    }

    /*
     * PID
     */
    char pid_string[32];

    int pid_len =
        integer_to_string(pid_string,
                          (int)job->reported_pid);

    for (int i = 0;
         i < pid_len &&
         pos < (int)sizeof(buffer) - 1;
         i++)
    {
        buffer[pos++] = pid_string[i];
    }

    /*
     * Completion status.
     */
    if (WIFEXITED(job->status) &&
        WEXITSTATUS(job->status) == 0)
    {
        const char text2[] = " exited normally\n";

        for (int i = 0;
             text2[i] != '\0' &&
             pos < (int)sizeof(buffer) - 1;
             i++)
        {
            buffer[pos++] = text2[i];
        }
    }
    else
    {
        const char text2[] = " exited abnormally\n";

        for (int i = 0;
             text2[i] != '\0' &&
             pos < (int)sizeof(buffer) - 1;
             i++)
        {
            buffer[pos++] = text2[i];
        }
    }

    write(STDOUT_FILENO, buffer, pos);
}


/*
 * SIGCHLD handler.
 *
 * Requirement #6:
 *     install SIGCHLD handler
 *
 * Requirement #7:
 *     reap using waitpid(..., WNOHANG)
 *
 * We never block here.
 */
static void sigchld_handler(int signal_number)
{
    (void)signal_number;

    int saved_errno = errno;

    while (1)
    {
        int status;

        pid_t pid =
    waitpid(-1,
            &status,
            WNOHANG | WUNTRACED | WCONTINUED);

        if (pid <= 0)
            break;

        
        int job_index;
        int process_index;

        if (!find_process(pid,
                  &job_index,
                  &process_index))
        {
            continue;
        }

        if (WIFSTOPPED(status))
        {
            jobs[job_index]
                .processes[process_index]
                .stopped = 1;

            continue;
        }
        if (WIFCONTINUED(status))
        {
            jobs[job_index]
                .processes[process_index]
            .stopped = 0;

            continue;
        } 
        if (WIFEXITED(status) ||
    WIFSIGNALED(status))
{
    jobs[job_index]
        .processes[process_index]
        .completed = 1;

    jobs[job_index].status = status;

    int all_completed = 1;

    for (int i = 0;
         i < jobs[job_index].process_count;
         i++)
    {
        if (!jobs[job_index]
                 .processes[i]
                 .completed)
        {
            all_completed = 0;
            break;
        }
    }

    if (all_completed)
        jobs[job_index].completed = 1;
}
    }

    errno = saved_errno;
}


/*
 * Install SIGCHLD handler.
 */
void init_background(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = sigchld_handler;

    sigemptyset(&sa.sa_mask);

    /*
     * Do NOT use SA_RESTART.
     *
     * This allows getline() to return when SIGCHLD arrives.
     */
    sa.sa_flags = 0;

    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        perror("cshell: sigaction");
        exit(EXIT_FAILURE);
    }
}


/*
 * Set whether a foreground process is running.
 */
void set_foreground_running(int running)
{
    foreground_running = running;
}


/*
 * Print background processes that finished while
 * a foreground process was running.
 */
void print_pending_background_jobs(void)
{
    sigset_t block_set;
    sigset_t old_set;

    sigemptyset(&block_set);
    sigaddset(&block_set, SIGCHLD);

    /*
     * Block SIGCHLD temporarily so the handler cannot
     * modify the job table while we inspect it.
     */
    sigprocmask(SIG_BLOCK, &block_set, &old_set);

    for (int i = 0; i < job_count; i++)
    {
        if (jobs[i].completed &&
            !jobs[i].reported)
        {
            print_completion(i);

            jobs[i].reported = 1;
        }
    }

    /*
     * Restore signal mask.
     */
    sigprocmask(SIG_SETMASK, &old_set, NULL);
}
void add_background_process(int job_index,
                            pid_t pid,
                            char *command)
{
    if (job_index < 0 ||
        job_index >= job_count)
        return;

    if (jobs[job_index].process_count >= MAX_PROCESSES)
        return;

    int index =
        jobs[job_index].process_count;

    jobs[job_index].processes[index].pid = pid;

    snprintf(jobs[job_index].processes[index].command,
             sizeof(jobs[job_index].processes[index].command),
             "%s",
             command);

    jobs[job_index].processes[index].stopped = 0;
    jobs[job_index].processes[index].completed = 0;
    jobs[job_index].process_count++;
}

/*
 * Launch one normal command in the background.
 */
static int launch_normal_background(Token *tokens)
{
    if (job_count >= MAX_BACKGROUND_JOBS)
    {
        printf("cshell: too many background jobs\n");
        return -1;
    }

    char command_name[4096];

    get_command_name(tokens,
                     command_name,
                     sizeof(command_name));

    pid_t pid = fork();

    if (pid == -1)
    {
        perror("cshell: fork");
        return -1;
    }

    if (pid == 0)
    {
        setpgid(0, 0);
        /*
         * Background processes must not read from
         * the terminal.
         */
        int null_fd =
            open("/dev/null", O_RDONLY);

        if (null_fd == -1)
        {
            perror("cshell: /dev/null");
            _exit(EXIT_FAILURE);
        }

        if (dup2(null_fd, STDIN_FILENO) == -1)
        {
            perror("cshell: dup2");
            close(null_fd);
            _exit(EXIT_FAILURE);
        }

        close(null_fd);

        /*
         * Execute using your existing command executor.
         */
        int result =
            execute_one_command(tokens);

        if (result == 0)
            _exit(EXIT_SUCCESS);

        _exit(EXIT_FAILURE);
    }

    /*
     * Parent records the job.
     */
    if (pid > 0)
{
    setpgid(pid, pid);

    int job_index =
        create_background_job(pid,
                              pid,
                              command_name);

    if (job_index == -1)
        return -1;

    add_background_process(job_index,
                           pid,
                           command_name);

    printf("[%d] %d\n",
           jobs[job_index].job_number,
           pid);

    fflush(stdout);
}
return 0;
}
static int launch_pipeline_background(Token *tokens)
{
    if (job_count >= MAX_BACKGROUND_JOBS)
    {
        printf("cshell: too many background jobs\n");
        return -1;
    }

    pid_t pids[MAX_PROCESSES];
    int process_count = 0;

    /*
     * Launch every command in the pipeline.
     */
    if (launch_pipeline_processes(tokens,
                                  pids,
                                  &process_count) == -1)
    {
        return -1;
    }

    if (process_count <= 0)
        return -1;

    /*
     * First process is the process-group leader.
     */
    pid_t pgid = pids[0];

    /*
     * Get command name of first pipeline command.
     */
    char command_name[4096];

    get_command_name(tokens,
                     command_name,
                     sizeof(command_name));

    /*
     * Create ONE job for the entire pipeline.
     */
    int job_index =
        create_background_job(
            pgid,
            pids[0],
            command_name
        );

    if (job_index == -1)
    {
        for (int i = 0; i < process_count; i++)
            kill(pids[i], SIGTERM);

        return -1;
    }

    /*
     * Add EVERY process in the pipeline
     * to the same job.
     */
    Token *current = tokens;
int process_index = 0;

while (current != NULL &&
       process_index < process_count)
{
    if (current->type == TOKEN_WORD)
    {
        add_background_process(
            job_index,
            pids[process_index],
            current->value
        );

        process_index++;

        /*
         * Move to the next pipeline command.
         */
        while (current != NULL &&
               current->type != TOKEN_PIPE)
        {
            current = current->next;
        }

        if (current != NULL)
            current = current->next;
    }
    else
    {
        current = current->next;
    }
}

    /*
     * Report the PID of the first command.
     */
    printf("[%d] %d\n",
           jobs[job_index].job_number,
           pids[0]);

    fflush(stdout);

    return 0;
}


/*
 * Public function used by main.c.
 */
int launch_background_command(Token *tokens)
{
    if (tokens == NULL)
        return -1;

    /*
     * IMPORTANT:
     *
     * Block SIGCHLD while creating/registering the job.
     *
     * Otherwise a very short command such as:
     *
     *     echo hi &
     *
     * could finish before we put it into the job table.
     */
    sigset_t block_set;
    sigset_t old_set;

    sigemptyset(&block_set);
    sigaddset(&block_set, SIGCHLD);

    sigprocmask(SIG_BLOCK,
                &block_set,
                &old_set);

    int result;

    if (contains_pipe(tokens))
    {
        result =
            launch_pipeline_background(tokens);
    }
    else
    {
        result =
            launch_normal_background(tokens);
    }

    /*
     * Now it is safe to allow SIGCHLD again.
     */
    sigprocmask(SIG_SETMASK,
                &old_set,
                NULL);

    return result;
}

int create_background_job(pid_t pgid,
                          pid_t reported_pid,
                          char *command)
{
    if (job_count >= MAX_BACKGROUND_JOBS)
    {
        printf("cshell: too many background jobs\n");
        return -1;
    }

    int index = job_count;

    jobs[index].job_number =
        next_job_number++;

    jobs[index].pgid =
        pgid;

    jobs[index].reported_pid =
        reported_pid;

    snprintf(jobs[index].command,
             sizeof(jobs[index].command),
             "%s",
             command);

    jobs[index].process_count = 0;

    jobs[index].completed = 0;
    jobs[index].reported = 0;
    jobs[index].status = 0;

    job_count++;

    return index;
}
int find_process(pid_t pid,
                 int *job_index,
                 int *process_index)
{
    for (int i = 0; i < job_count; i++)
    {
        for (int j = 0;
             j < jobs[i].process_count;
             j++)
        {
            if (jobs[i].processes[j].pid == pid)
            {
                *job_index = i;
                *process_index = j;

                return 1;
            }
        }
    }

    return 0;
}