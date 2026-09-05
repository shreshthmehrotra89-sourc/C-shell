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
pid_t shell_pgid;
int shell_terminal;
static volatile sig_atomic_t resume_timed_out = 0;
static volatile sig_atomic_t resume_pgid = -1;
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
static void sigalrm_handler(int signal_number)
{
    (void)signal_number;
    if (resume_pgid > 0)
    kill(-resume_pgid, SIGTERM);
    resume_timed_out = 1;
    const char message[] = "resume: job timed out\n";
    write(STDOUT_FILENO, message, sizeof(message) - 1);
}
static void get_full_command(Token *tokens,char *buffer,int buffer_size)
{
    buffer[0] = '\0';
    int first = 1;
    while (tokens != NULL)
    {
        if (tokens->type == TOKEN_WORD)
        {
            if (!first)
            strncat(buffer, " ",buffer_size - strlen(buffer) - 1);
            strncat(buffer,tokens->value,buffer_size - strlen(buffer) - 1);
            first = 0;
        }
        else if (tokens->type == TOKEN_PIPE)
        strncat(buffer, " | ", buffer_size - strlen(buffer) - 1);
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

    if (WIFEXITED(job->status) &&  WEXITSTATUS(job->status) == 0)
    printf("%s with pid %d exited normally\n",job->command,job->reported_pid);
    else
    printf("%s with pid %d exited abnormally\n",job->command,job->reported_pid);
    fflush(stdout);
}
static void sigchld_handler(int signal_number)
{
    (void)signal_number;
    int saved_errno = errno;

    for (int i = 0; i < job_count; i++)
    {
        BackgroundJob *job = &jobs[i];
        if (job->completed)
        continue;

        if (foreground_running && resume_pgid > 0 && job->pgid == resume_pgid)
        continue;
        for (int j = 0; j < job->process_count; j++)
        {
            BackgroundProcess *process =&job->processes[j];
            if (process->completed)
            continue;
            int status;
            pid_t result =waitpid(process->pid,&status,WNOHANG | WUNTRACED | WCONTINUED);

            if (result <= 0)
                continue;
            // Process stopped.
            
            if (WIFSTOPPED(status))
            {
                process->stopped = 1;
                job->stopped = 1;
                continue;
            }
            // Process continued.
            if (WIFCONTINUED(status))
            {
                process->stopped = 0;
                /*
                 * Recalculate the job's stopped state.
                 */
                int any_stopped = 0;
                for (int k = 0;k < job->process_count;k++)
                {
                    if (!job->processes[k].completed && job->processes[k].stopped)
                    {
                        any_stopped = 1;
                        break;
                    }
                }
                job->stopped = any_stopped;
                continue;
            }

            /*
             * Process exited normally or was killed
             * by a signal.
             */
            if (WIFEXITED(status) || WIFSIGNALED(status))
            {
                process->completed = 1;
                process->stopped = 0;
                /*
                 * Store the status.
                 *
                 * For your completion message, this is
                 * the status of the process that finished.
                 */
                job->status = status;
                /*
                 * Check whether every process in this
                 * background job has finished.
                 */
                int all_completed = 1;
                for (int k = 0;k < job->process_count;k++)
                {
                    if (!job->processes[k].completed)
                    {
                        all_completed = 0;
                        break;
                    }
                }
                if (all_completed)
                {
                    job->completed = 1;
                    job->stopped = 0;
                }
            }
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

    /* SIGCHLD */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        perror("cshell: sigaction");
        exit(EXIT_FAILURE);
    }

    /* SIGALRM */
    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = sigalrm_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGALRM, &sa, NULL) == -1)
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
        if (jobs[i].completed && !jobs[i].reported)
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
void add_background_process(int job_index,pid_t pid,char *command)
{
    if (job_index < 0 || job_index >= job_count)
        return;
    if (jobs[job_index].process_count >= MAX_PROCESSES)
        return;
    int index =jobs[job_index].process_count;
    jobs[job_index].processes[index].pid = pid;
    snprintf(jobs[job_index].processes[index].command,sizeof(jobs[job_index].processes[index].command),
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
    get_full_command(tokens,command_name,sizeof(command_name));
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

        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        int null_fd =open("/dev/null", O_RDONLY);

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
        execute_one_background_command(tokens);
        _exit(EXIT_FAILURE);
    }
    /*
     * Parent records the job.
     */
    if (pid > 0)
    {
        setpgid(pid, pid);
        int job_index = create_background_job(pid,pid,command_name);
        if (job_index == -1)
            return -1;
        add_background_process(job_index,pid,command_name);
        printf("[%d] %d\n",jobs[job_index].job_number,pid);
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
    if (launch_pipeline_processes(tokens,pids,&process_count) == -1)
    return -1;
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
    get_full_command(tokens,command_name,sizeof(command_name));
    /*
     * Create ONE job for the entire pipeline.
     */
    int job_index =create_background_job(pgid,pids[0],command_name);
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
    while (current != NULL && process_index < process_count)
    {
        if (current->type == TOKEN_WORD)
        {
            add_background_process(job_index,pids[process_index],current->value);
            process_index++;
            /*
            * Move to the next pipeline command.
            */
            while (current != NULL && current->type != TOKEN_PIPE)
            current = current->next;
            if (current != NULL)
            current = current->next;
        }
        else
        current = current->next;
    }
    /*
     * Report the PID of the first command.
     */
    printf("[%d] %d\n",jobs[job_index].job_number,pids[0]);
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
    sigprocmask(SIG_BLOCK,&block_set,&old_set);
    int result;
    if (contains_pipe(tokens))
    result =launch_pipeline_background(tokens);
    else
    result =launch_normal_background(tokens);
    /*
     * Now it is safe to allow SIGCHLD again.
     */
    sigprocmask(SIG_SETMASK,&old_set,NULL);
    return result;
}

int create_background_job(pid_t pgid,pid_t reported_pid,char *command)
{
    if (job_count >= MAX_BACKGROUND_JOBS)
    {
        printf("cshell: too many background jobs\n");
        return -1;
    }
    int index = job_count;
    jobs[index].job_number =next_job_number++;
    jobs[index].pgid =pgid;
    jobs[index].reported_pid =reported_pid;

    snprintf(jobs[index].command,sizeof(jobs[index].command),"%s",command);

    jobs[index].process_count = 0;
    jobs[index].stopped = 0;
    jobs[index].completed = 0;
    jobs[index].reported = 0;
    jobs[index].status = 0;

    job_count++;
    return index;
}
int find_process(pid_t pid,int *job_index,int *process_index)
{
    for (int i = 0; i < job_count; i++)
    {
        for (int j = 0;j < jobs[i].process_count;j++)
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

void init_job_control(void)
{
    shell_terminal = STDIN_FILENO;
    /*
     * Put the shell in its own process group.
     */
    shell_pgid = getpid();

    if (setpgid(shell_pgid, shell_pgid) == -1)
    {
        /*
         * It may already be in its own process group.
         */
        if (errno != EACCES)
        {
            perror("cshell: setpgid");
            exit(EXIT_FAILURE);
        }
    }
    /*
     * Make the shell's process group own the terminal.
     */
    if (tcsetpgrp(shell_terminal, shell_pgid) == -1)
    {
        perror("cshell: tcsetpgrp");
        exit(EXIT_FAILURE);
    }
}

void init_shell_signals(void)
{
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
}

int wait_for_foreground_job(pid_t pgid,int process_count,int *job_status)
{
    int stopped_count = 0;
    int completed_count = 0;
    while (1)
    {
        int status;
        pid_t pid = waitpid(-pgid,&status,WUNTRACED);
        if (pid == -1)
        {
            if (errno == EINTR)
            {
                /*
                 * SIGALRM or another signal interrupted waitpid.
                 *
                 * If timeout happened, continue waiting so that
                 * the SIGTERM'ed children are reaped.
                 */
                if (resume_timed_out)
                    continue;

                continue;
            }
            if (errno == ECHILD)
                break;
            perror("cshell: waitpid");
            break;
        }
        if (WIFSTOPPED(status))
        {
            stopped_count++;
            *job_status = status;

            /*
             * Ctrl-Z stops the entire process group.
             * Once one process reports stopped, the
             * foreground job is considered stopped.
             */
            return 1;
        }

        if (WIFEXITED(status) || WIFSIGNALED(status))
        {
            completed_count++;
           *job_status = status;
            if (completed_count == process_count)
                return 0;
        }
    }
    return stopped_count > 0;
}
int has_stopped_jobs(void)
{
    sigset_t block_set;
    sigset_t old_set;

    sigemptyset(&block_set);
    sigaddset(&block_set, SIGCHLD);
    sigprocmask(SIG_BLOCK,&block_set,&old_set);
    int found = 0;
    for (int i = 0; i < job_count; i++)
    {
        if (!jobs[i].completed &&
            jobs[i].stopped)
        {
            found = 1;
            break;
        }
    }
    sigprocmask(SIG_SETMASK,&old_set,NULL);
    return found;
}
void send_sighup_to_jobs(void)
{
    sigset_t block_set;
    sigset_t old_set;
    sigemptyset(&block_set);
    sigaddset(&block_set, SIGCHLD);
    sigprocmask(SIG_BLOCK,&block_set,&old_set);
    for (int i = 0; i < job_count; i++)
    {
        if (!jobs[i].completed)
        {
            kill(-jobs[i].pgid, SIGHUP);
        }
    }
    sigprocmask(SIG_SETMASK,&old_set,NULL);
}

static int find_job_by_number(int job_number)
{
    for (int i = 0; i < job_count; i++)
    {
        if (jobs[i].job_number == job_number)
            return i;
    }

    return -1;
}
int ping_job(const char *target, const char *signal_text)
{
    if (signal_text == NULL || signal_text[0] == '\0')
    {
        printf("ping: invalid syntax\n");
        return -1;
    }
    if (signal_text[0] == '-')
    {
        printf("ping: invalid syntax\n");
        return -1;
    }
    char *endptr = NULL;
    errno = 0;
    long signal_number = strtol(signal_text, &endptr, 10);

    if (signal_text[0] == '\0' || endptr == signal_text || *endptr != '\0' || errno == ERANGE || signal_number < 0)
    {
        printf("ping: invalid syntax\n");
        return -1;
    }
    /*
     * The actual signal is signal_number % 64.
     */
    int actual_signal = (int)(signal_number % 64);
    if (target == NULL || target[0] == '\0')
    {
        printf("ping: no such process found\n");
        return -1;
    }
    if (target[0] == '%')
    {
        /*
         * Must have something after '%'.
         */
        if (target[1] == '\0')
        {
            printf("ping: no such process found\n");
            return -1;
        }
        char *job_endptr = NULL;
        errno = 0;
        long job_number =strtol(target + 1, &job_endptr, 10);

        if (job_endptr == target + 1 || *job_endptr != '\0' ||  errno == ERANGE || job_number <= 0)
        {
            printf("ping: no such process found\n");
            return -1;
        }
        int job_index =find_job_by_number((int)job_number);
        /*
         * Unknown job.
         */
        if (job_index == -1)
        {
            printf("ping: no such process found\n");
            return -1;
        }
        BackgroundJob *job = &jobs[job_index];
        /*
         * Completed jobs are no longer valid targets.
         */
        if (job->completed)
        {
            printf("ping: no such process found\n");
            return -1;
        }
        /*
         * Send signal to the ENTIRE process group.
         *
         * Negative PID means process group in kill().
         */
        if (kill(-job->pgid, actual_signal) == -1)
        {
            /*
             * If the process group no longer exists,
             * treat it as unknown.
             */
            if (errno == ESRCH)
            {
                printf("ping: no such process found\n");
                return -1;
            }

            printf("ping: no such process found\n");
            return -1;
        }
        printf("Sent signal %ld to %s\n",signal_number,target);
        fflush(stdout);
        return 0;
    }
    if (target[0] == '-')
    {
        printf("ping: no such process found\n");
        return -1;
    }
    char *pid_endptr = NULL;
    errno = 0;
    long pid_value =strtol(target, &pid_endptr, 10);

    if (pid_endptr == target || *pid_endptr != '\0' || errno == ERANGE ||pid_value <= 0)
    {
        printf("ping: no such process found\n");
        return -1;
    }
    pid_t pid = (pid_t)pid_value;
    int job_index = -1;
    int process_index = -1;

    if (!find_process(pid, &job_index, &process_index))
    {
        printf("ping: no such process found\n");
        return -1;
    }

    if (jobs[job_index].completed || jobs[job_index].processes[process_index].completed)
    {
        printf("ping: no such process found\n");
        return -1;
    }
    /*
     * Send the signal to this particular PID.
     */
    if (kill(pid, actual_signal) == -1)
    {
        if (errno == ESRCH)
        {
            printf("ping: no such process found\n");
            return -1;
        }
        printf("ping: no such process found\n");
        return -1;
    }
    printf("Sent signal %ld to %s\n",signal_number,target);
    fflush(stdout);
    return 0;
}
int resume_job(int job_number, int foreground, int timeout_seconds)
{
    sigset_t block_set;
    sigset_t old_set;
    sigemptyset(&block_set);
    sigaddset(&block_set, SIGCHLD);
    sigprocmask(SIG_BLOCK, &block_set, &old_set);
    int job_index = find_job_by_number(job_number);

    if (job_index == -1)
    {
        sigprocmask(SIG_SETMASK, &old_set, NULL);
        printf("resume: no such job\n");
        fflush(stdout);
        return -1;
    }
    BackgroundJob *job = &jobs[job_index];

    if (job->completed)
    {
        sigprocmask(SIG_SETMASK, &old_set, NULL);

        printf("resume: no such job\n");
        fflush(stdout);

        return -1;
    }

    if (kill(-job->pgid, SIGCONT) == -1)
    {
        sigprocmask(SIG_SETMASK, &old_set, NULL);

        printf("resume: no such job\n");
        fflush(stdout);

        return -1;
    }

    /*
     * The job is now running.
     */
    job->stopped = 0;

    for (int i = 0; i < job->process_count; i++)
    job->processes[i].stopped = 0;
        /*
     * Background resume.
     */
    if (!foreground)
    {
        printf("[%d] + Running    %s\n",
               job->job_number,
               job->command);

        fflush(stdout);

        /*
         * Unblock SIGCHLD only after printing.
         */
        sigprocmask(SIG_SETMASK, &old_set, NULL);

        return 0;
    }

    /*
     * Foreground resume.
     */
    set_foreground_running(1);

    resume_pgid = job->pgid;
    resume_timed_out = 0;

    /*
     * Give terminal to the job.
     */
    if (tcsetpgrp(shell_terminal, job->pgid) == -1)
    {
        perror("cshell: tcsetpgrp");

        resume_pgid = -1;
        set_foreground_running(0);

        sigprocmask(SIG_SETMASK, &old_set, NULL);

        return -1;
    }

    /*
     * Print the command being brought to foreground.
     */
    printf("%s\n", job->command);
    fflush(stdout);

    /*
     * Start timeout if requested.
     */
    if (timeout_seconds > 0)
        alarm(timeout_seconds);

    /*
     * Allow SIGCHLD again.
     */
    sigprocmask(SIG_SETMASK, &old_set, NULL);

    int status = 0;
    int stopped =wait_for_foreground_job(job->pgid,job->process_count,&status );
    /*
     * Stop the alarm if the job finished/stopped normally.
     */
    if (!resume_timed_out)
        alarm(0);

    /*
     * Take terminal back.
     */
    if (tcsetpgrp(shell_terminal, shell_pgid) == -1)
        perror("cshell: tcsetpgrp");

    resume_pgid = -1;
    set_foreground_running(0);

    /*
     * Job stopped again with Ctrl-Z.
     */
    if (stopped)
    {
        job->stopped = 1;
        job->status = status;
        for (int i = 0; i < job->process_count; i++)
        job->processes[i].stopped = 1;
        printf("[%d] + Stopped    %s\n",job->job_number,job->command);
        fflush(stdout);
    }
    else
    {
        job->completed = 1;
        job->status = status;
        for (int i = 0; i < job->process_count; i++)
        {
            job->processes[i].completed = 1;
            job->processes[i].stopped = 0;
        }
    }

    return 0;
}