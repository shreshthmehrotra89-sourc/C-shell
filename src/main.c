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

int main(void)
{
    char *line = NULL;
    size_t size = 0;

    init_prompt();

    while (1)
    {
        print_prompt();

        if (getline(&line, &size, stdin) == -1)
        {
            clearerr(stdin);
            printf("\n");
            continue;
        }

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
         * Execute the hop built-in.
         */
        if (tokens != NULL &&
            tokens->type == TOKEN_WORD &&
            strcmp(tokens->value, "hop") == 0)
        {
            int argc = 0;
            Token *current = tokens->next;

            while (current != NULL)
            {
                if (current->type != TOKEN_WORD)
                {
                    argc = -1;
                    break;
                }

                argc++;
                current = current->next;
            }

            if (argc >= 0)
            {
                char **args = malloc(sizeof(char *) * argc);

                if (argc == 0 || args != NULL)
                {
                    current = tokens->next;

                    for (int i = 0; i < argc; i++)
                    {
                        args[i] = current->value;
                        current = current->next;
                    }

                    hop_command(args, argc);

                    free(args);
                }
            }
        }
        /*
         * Execute the reveal built-in.
         */
        if (tokens != NULL &&
            tokens->type == TOKEN_WORD &&
            strcmp(tokens->value, "reveal") == 0)
        {
            int argc = 0;
            Token *current = tokens->next;

            while (current != NULL)
            {
                if (current->type != TOKEN_WORD)
                {
                    argc = -1;
                    break;
                }

                argc++;
                current = current->next;
            }

            if (argc >= 0)
            {
                char **args = malloc(sizeof(char *) * argc);

                if (argc == 0 || args != NULL)
                {
                    current = tokens->next;

                    for (int i = 0; i < argc; i++)
                    {
                        args[i] = current->value;
                        current = current->next;
                    }

                    reveal_command(args, argc);

                    free(args);
                }
            }
        }
        //Execute the peek built-in.
 
        if (tokens != NULL && tokens->type == TOKEN_WORD && strcmp(tokens->value, "peek") == 0)
        {
            int argc = 0;
            Token *current = tokens->next;

            while (current != NULL)
            {
                if (current->type != TOKEN_WORD)
                {
                    argc = -1;
                    break;
                }

                argc++;
                current = current->next;
            }

            if (argc >= 0)
            {
                char **args = malloc(sizeof(char *) * argc);

                if (argc == 0 || args != NULL)
                {
                    current = tokens->next;

                    for (int i = 0; i < argc; i++)
                    {
                        args[i] = current->value;
                        current = current->next;
                    }

                    peek_command(args, argc);

                    free(args);
                }
            }
        }
        /*
        * Execute the locate built-in.
        */
        if (tokens != NULL && tokens->type == TOKEN_WORD && strcmp(tokens->value, "locate") == 0)
        {
            int argc = 0;
            Token *current = tokens->next;

            while (current != NULL)
            {
                if (current->type != TOKEN_WORD)
                {
                    argc = -1;
                    break;
                }

                argc++;
                current = current->next;
            }

            if (argc >= 0)
            {
                char **args = malloc(sizeof(char *) * argc);

                if (argc == 0 || args != NULL)
                {
                    current = tokens->next;

                    for (int i = 0; i < argc; i++)
                    {
                        args[i] = current->value;
                        current = current->next;
                    }

                    locate_command(args, argc);

                    free(args);
                }
            }
        }

                /*
         * Execute an external command.
         *
         * Only execute the first command group.
         * Stop at |, ;, or &.
         */
        /*
 * Execute an external command.
 *
 * Separate normal arguments from input redirection files.
 */
if (tokens != NULL &&
    tokens->type == TOKEN_WORD &&
    strcmp(tokens->value, "hop") != 0 &&
    strcmp(tokens->value, "reveal") != 0 &&
    strcmp(tokens->value, "peek") != 0 &&
    strcmp(tokens->value, "locate") != 0)
{
    int argc = 0;
    int input_count = 0;

    Token *current = tokens;

    /*
     * First count arguments and input files.
     */
    while (current != NULL &&
           current->type != TOKEN_PIPE &&
           current->type != TOKEN_SEMI &&
           current->type != TOKEN_AMP)
    {
        if (current->type == TOKEN_WORD)
        {
            argc++;
        }
        else if (current->type == TOKEN_LT)
        {
            current = current->next;

            if (current != NULL && current->type == TOKEN_WORD)
            {
                input_count++;
            }
        }

        current = current->next;
    }

    /*
     * Allocate argument array.
     */
    char **args = malloc(sizeof(char *) * (argc + 1));

    /*
     * Allocate input file array.
     */
    char **input_files = NULL;

    if (input_count > 0)
    {
        input_files = malloc(sizeof(char *) * input_count);
    }

    if (args != NULL && (input_count == 0 || input_files != NULL))
    {
        int arg_index = 0;
        int input_index = 0;

        current = tokens;

        while (current != NULL &&
               current->type != TOKEN_PIPE &&
               current->type != TOKEN_SEMI &&
               current->type != TOKEN_AMP)
        {
            if (current->type == TOKEN_WORD)
            {
                args[arg_index++] = current->value;
            }
            else if (current->type == TOKEN_LT)
            {
                current = current->next;

                if (current != NULL && current->type == TOKEN_WORD)
                {
                    input_files[input_index++] = current->value;
                }
            }

            current = current->next;
        }

        args[arg_index] = NULL;

        execute_command(args, input_files, input_count);

        free(args);
        free(input_files);
    }
}
                                                                                       
        free_tokens(tokens);
    }

    free(line);

    return 0;
}