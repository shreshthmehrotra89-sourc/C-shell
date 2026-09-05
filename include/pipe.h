#ifndef PIPE_H
#define PIPE_H

#include "lexer.h"
#include <sys/types.h>

int execute_pipeline(Token *tokens);

/*
 * Background version.
 *
 * pid_write_fd is used to send the PID of the first
 * pipeline command back to the shell.
 */
int execute_pipeline_background(Token *tokens,
                                int pid_write_fd);
int launch_pipeline_processes(Token *tokens, pid_t *pids, int *process_count);

#endif