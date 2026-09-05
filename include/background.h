#ifndef BACKGROUND_H
#define BACKGROUND_H

#include "lexer.h"
#include <sys/types.h>

#define MAX_BACKGROUND_JOBS 1000
#define MAX_PROCESSES 100
extern pid_t shell_pgid;

typedef struct
{
    pid_t pid;
    char command[4096];
    int stopped;
    int completed;
} BackgroundProcess;


typedef struct
{
    int job_number;

    /*
     * Process group ID of this background job.
     *
     * For a normal command:
     *
     *     pgid == pid
     *
     * For a pipeline:
     *
     *     pgid == PID of first command
     */
    pid_t pgid;

    /*
     * PID of the first command.
     *
     * This is also the reported PID required by D2.
     */
    pid_t reported_pid;

    /*
     * Command name of the first command.
     */
    char command[4096];

    /*
     * All processes belonging to this job.
     */
    BackgroundProcess processes[MAX_PROCESSES];

    int process_count;

    /*
     * Used by the existing background completion code.
     */
    int completed;
    int reported;
    int status;
    int stopped;

} BackgroundJob;


extern BackgroundJob jobs[MAX_BACKGROUND_JOBS];

extern int job_count;

extern int next_job_number;


void init_background(void);

int launch_background_command(Token *tokens);

void set_foreground_running(int running);

void print_pending_background_jobs(void);

void activities(void);
void add_background_process(int job_index,
                            pid_t pid,
                            char *command);
int create_background_job(pid_t pgid,
                          pid_t reported_pid,
                          char *command);
int find_process(pid_t pid,
                 int *job_index,
                 int *process_index);  
                 
void init_job_control(void);
void init_shell_signals(void);
void send_sighup_to_jobs(void);

int wait_for_foreground_job(pid_t pgid,
                            int process_count,
                            int *job_status);
int has_stopped_jobs(void);
int execute_one_background_command(Token *tokens);
int resume_job(int job_number, int foreground, int timeout_seconds);
#endif