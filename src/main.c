#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "peek.h"
#include "prompt.h"
#include "lexer.h"
#include "parser.h"
#include "hop.h"
#include "reveal.h"
#include "locate.h"
#include "execute.h"
#include "input_redirect.h"
#include "output_redirect.h"
#include "pipe.h"
#include "background.h"
#include "activities.h"
#include <errno.h>

int execute_one_command(Token *tokens)
{
    if (tokens == NULL)
        return 0;
    //Check whether this command contains a pipe.
    int has_pipe = 0;
    Token *pipe_check = tokens;
    while (pipe_check != NULL)
    {
        if (pipe_check->type == TOKEN_PIPE)
        {
            has_pipe = 1;
            break;
        }
        pipe_check = pipe_check->next;
    }
    if (has_pipe)
    return execute_pipeline(tokens);

    if (tokens->type == TOKEN_WORD &&
    strcmp(tokens->value, "activities") == 0)
{
    if (tokens->next != NULL)
        return -1;

    activities();

    return 0;
}
    
    //hop
    if (tokens->type == TOKEN_WORD && strcmp(tokens->value, "hop") == 0)
    {
        int argc = 0;
        Token *current = tokens->next;
        while (current != NULL)
        {
            if (current->type != TOKEN_WORD)
                return -1;
            argc++;
            current = current->next;
        }
        char **args = malloc(sizeof(char *) * argc);
        if (argc > 0 && args == NULL)
            return -1;
        current = tokens->next;
        for (int i = 0; i < argc; i++)
        {
            args[i] = current->value;
            current = current->next;
        }
        hop_command(args, argc);
        free(args);
        return 0;
    }
    //reveal
    if (tokens->type == TOKEN_WORD && strcmp(tokens->value, "reveal") == 0)
    {
        int argc = 0;
        Token *current = tokens->next;
        while (current != NULL)
        {
            if (current->type != TOKEN_WORD)
                return -1;
            argc++;
            current = current->next;
        }
        char **args = malloc(sizeof(char *) * argc);
        if (argc > 0 && args == NULL)
            return -1;
        current = tokens->next;
        for (int i = 0; i < argc; i++)
        {
            args[i] = current->value;
            current = current->next;
        }
        reveal_command(args, argc);
        free(args);
        return 0;
    }
    //peek
    if (tokens->type == TOKEN_WORD && strcmp(tokens->value, "peek") == 0)
    {
        int argc = 0;
        Token *current = tokens->next;
        while (current != NULL)
        {
            if (current->type != TOKEN_WORD)
                return -1;
            argc++;
            current = current->next;
        }
        char **args = malloc(sizeof(char *) * argc);
        if (argc > 0 && args == NULL)
            return -1;
        current = tokens->next;
        for (int i = 0; i < argc; i++)
        {
            args[i] = current->value;
            current = current->next;
        }
        peek_command(args, argc);
        free(args);
        return 0;
    }
    //locate
    if (tokens->type == TOKEN_WORD && strcmp(tokens->value, "locate") == 0)
    {
        int argc = 0;
        Token *current = tokens->next;
        while (current != NULL)
        {
            if (current->type != TOKEN_WORD)
                return -1;
            argc++;
            current = current->next;
        }
        char **args = malloc(sizeof(char *) * argc);
        if (argc > 0 && args == NULL)
            return -1;
        current = tokens->next;
        for (int i = 0; i < argc; i++)
        {
            args[i] = current->value;
            current = current->next;
        }
        locate_command(args, argc);
        free(args);
        return 0;
    }
    //input outpur
    if (tokens->type == TOKEN_WORD)
    {
        int argc = 0;
        int input_count = 0;
        int output_count = 0;

        Token *current = tokens;
        while (current != NULL)
        {
            if (current->type == TOKEN_WORD)
            argc++;
            else if (current->type == TOKEN_LT)
            {
                current = current->next;
                if (current != NULL && current->type == TOKEN_WORD)
                input_count++;
            }
            else if (current->type == TOKEN_GT || current->type == TOKEN_GTGT)
            {
                current = current->next;
                if (current != NULL && current->type == TOKEN_WORD)
                output_count++;
            }
            current = current->next;
        }

        char **args = malloc(sizeof(char *) * (argc + 1));
        char **input_files = NULL;

        if (input_count > 0)
        input_files = malloc(sizeof(char *) * input_count);
        char **output_files = NULL;
        int *output_append = NULL;

        if (output_count > 0)
        {
            output_files = malloc(sizeof(char *) * output_count);
            output_append = malloc(sizeof(int) * output_count);
        }

        if (args == NULL || (input_count > 0 && input_files == NULL) || (output_count > 0 && (output_files == NULL || output_append == NULL)))
        {
            free(args);
            free(input_files);
            free(output_files);
            free(output_append);

            return -1;
        }
        int arg_index = 0;
        int input_index = 0;
        int output_index = 0;
        current = tokens;
        while (current != NULL)
        {
            if (current->type == TOKEN_WORD)
            args[arg_index++] = current->value;
            else if (current->type == TOKEN_LT)
            {
                current = current->next;
                if (current != NULL && current->type == TOKEN_WORD)
                input_files[input_index++] = current->value;
            }
            else if (current->type == TOKEN_GT)
            {
                current = current->next;
                if (current != NULL && current->type == TOKEN_WORD)
                {
                    output_files[output_index] = current->value;
                    output_append[output_index] = 0;
                    output_index++;
                }
            }
            else if (current->type == TOKEN_GTGT)
            {
                current = current->next;
                if (current != NULL && current->type == TOKEN_WORD)
                {
                    output_files[output_index] = current->value;
                    output_append[output_index] = 1;
                    output_index++;
                }
            }
            current = current->next;
        }

        args[arg_index] = NULL;
        int result = execute_command(args,input_files,input_count,output_files,output_append,output_count);
        free(args);
        free(input_files);
        free(output_files);
        free(output_append);
        return result;
    }
    return -1;
}

int execute_one_background_command(Token *tokens)
{
    if (tokens == NULL)
        return -1;

    /*
     * Background pipelines are already handled separately
     * by launch_pipeline_background().
     */
    Token *pipe_check = tokens;

    while (pipe_check != NULL)
    {
        if (pipe_check->type == TOKEN_PIPE)
            return -1;

        pipe_check = pipe_check->next;
    }

    /*
     * Background builtins such as hop should NOT be
     * executed in the child because they modify the
     * shell's state, not the background child.
     *
     * Normal background execution here is for external
     * commands.
     */

    if (tokens->type != TOKEN_WORD)
        return -1;

    int argc = 0;
    int input_count = 0;
    int output_count = 0;

    Token *current = tokens;

    while (current != NULL)
    {
        if (current->type == TOKEN_WORD)
        {
            argc++;
        }
        else if (current->type == TOKEN_LT)
        {
            current = current->next;

            if (current != NULL &&
                current->type == TOKEN_WORD)
            {
                input_count++;
            }
        }
        else if (current->type == TOKEN_GT ||
                 current->type == TOKEN_GTGT)
        {
            current = current->next;

            if (current != NULL &&
                current->type == TOKEN_WORD)
            {
                output_count++;
            }
        }

        current = current->next;
    }

    char **args =
        malloc(sizeof(char *) * (argc + 1));

    char **input_files = NULL;

    if (input_count > 0)
    {
        input_files =
            malloc(sizeof(char *) * input_count);
    }

    char **output_files = NULL;
    int *output_append = NULL;

    if (output_count > 0)
    {
        output_files =
            malloc(sizeof(char *) * output_count);

        output_append =
            malloc(sizeof(int) * output_count);
    }

    if (args == NULL ||
        (input_count > 0 && input_files == NULL) ||
        (output_count > 0 &&
         (output_files == NULL ||
          output_append == NULL)))
    {
        free(args);
        free(input_files);
        free(output_files);
        free(output_append);

        return -1;
    }

    int arg_index = 0;
    int input_index = 0;
    int output_index = 0;

    current = tokens;

    while (current != NULL)
    {
        if (current->type == TOKEN_WORD)
        {
            args[arg_index++] = current->value;
        }
        else if (current->type == TOKEN_LT)
        {
            current = current->next;

            if (current != NULL &&
                current->type == TOKEN_WORD)
            {
                input_files[input_index++] =
                    current->value;
            }
        }
        else if (current->type == TOKEN_GT)
        {
            current = current->next;

            if (current != NULL &&
                current->type == TOKEN_WORD)
            {
                output_files[output_index] =
                    current->value;

                output_append[output_index] = 0;

                output_index++;
            }
        }
        else if (current->type == TOKEN_GTGT)
        {
            current = current->next;

            if (current != NULL &&
                current->type == TOKEN_WORD)
            {
                output_files[output_index] =
                    current->value;

                output_append[output_index] = 1;

                output_index++;
            }
        }

        current = current->next;
    }

    args[arg_index] = NULL;

    /*
     * IMPORTANT:
     *
     * This does NOT wait.
     * It does NOT change terminal ownership.
     */
    execute_background_command(
        args,
        input_files,
        input_count,
        output_files,
        output_append,
        output_count
    );

    /*
     * Normally never reached because
     * execute_background_command() calls exec().
     */
    free(args);
    free(input_files);
    free(output_files);
    free(output_append);

    return 0;
}

int main(void)
{
    initialize_hop();
    init_shell_signals();
    init_job_control();
    init_background();



    char *line = NULL;
    size_t size = 0;
    init_prompt();
    int stopped_job_warning = 0;
    while (1)
    {
        print_prompt();

        if (getline(&line, &size, stdin) == -1)
        {
            if (errno == EINTR)
            {
                clearerr(stdin);

                printf("\n");

                print_pending_background_jobs();

                continue;
            }

            if (feof(stdin))
            {
                clearerr(stdin);

                /*
                * Ctrl-D.
                */
                if (has_stopped_jobs())
                {
                    printf("cshell: there are stopped jobs\n");
                    fflush(stdout);

                    /*
                    * Clear EOF so the next Ctrl-D can
                    * be detected separately.
                    */
                    clearerr(stdin);

                    /*
                    * We need the next Ctrl-D to exit.
                    */
                    if (stopped_job_warning)
                    break;

                    stopped_job_warning = 1;

                    continue;
                }

                /*
                * No stopped jobs.
                * Exit immediately.
                */
                break;
            }

            clearerr(stdin);
            continue;
        }
        stopped_job_warning = 0;
        Token *tokens = NULL;
        if (lex_line(line, &tokens) != 0)
        {
            printf("cshell: invalid syntax\n");
            free_tokens(tokens);
            continue;
        }
        if (!parse_line(tokens))
        {
            printf("cshell: invalid syntax\n");
            free_tokens(tokens);
            continue;
        }
        /*
        * Execute commands separated by ';' and '&'.
        *
        * ';' -> foreground
        *
        * '&' -> background
        */
        Token *current = tokens;

        while (current != NULL)
        {
            Token *command_start = current;

            Token *separator = current;
            Token *prev = NULL;

    /*
     * Find either:
     *
     * TOKEN_SEMI
     *
     * or
     *
     * TOKEN_AMP
     */
            while (separator != NULL &&
           separator->type != TOKEN_SEMI &&
           separator->type != TOKEN_AMP)
            {       
                prev = separator;
                separator = separator->next;
            }

    /*
     * Remember what comes after the separator.
     */
            Token *next_command = NULL;

            if (separator != NULL)
            next_command = separator->next;

    /*
     * Disconnect the current command from the rest
     * of the token list.
     *
     * Example:
     *
     * echo hello -> AMP -> sleep -> 10
     *
     * becomes:
     *
     * echo hello -> NULL
     */
            if (separator != NULL && prev != NULL)
            prev->next = NULL;

    /*
     * Determine whether this command should run
     * in the background.
     */
            int is_background = 0;

            if (separator != NULL &&
            separator->type == TOKEN_AMP)
            {
                is_background = 1;
            }

            int result;

            if (is_background)
            {
        /*
         * Background command.
         *
         * We do NOT wait here.
         */
                result = launch_background_command(command_start);
            }
            else
            {
        /*
         * Foreground command.
         */

                set_foreground_running(1);

                result = execute_one_command(command_start);

                set_foreground_running(0);

        /*
         * If background processes finished while
         * this foreground command was running,
         * report them NOW.
         */
                print_pending_background_jobs();
            }

    /*
     * Restore linked list.
     */
            if (separator != NULL && prev != NULL)
            prev->next = separator;

    /*
     * If execution failed, stop.
     */
            if (result != 0)
            break;

    /*
     * No separator means this was the last command.
     */
            if (separator == NULL)
            break;

    /*
     * Move to the command after ';' or '&'.
     */
            current = next_command;
        }
    }
    send_sighup_to_jobs();

    free(line);

    return 0;
}
