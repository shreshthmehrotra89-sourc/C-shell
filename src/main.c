#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "prompt.h"
#include "lexer.h"
#include "parser.h"
#include "hop.h"

int main(void)
{
    char *line = NULL;
    size_t size = 0;

    init_prompt();

    while (1)
    {
        print_prompt();

        if (getline(&line, &size, stdin) == -1)
            break;

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

        free_tokens(tokens);
    }

    free(line);

    return 0;
}