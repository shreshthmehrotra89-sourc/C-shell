#include <stdio.h>
#include <stdlib.h>
#include "prompt.h"
#include "lexer.h"
#include "parser.h"

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
        printf("cshell: invalid syntax\n");
        
        free_tokens(tokens);
    }
    free(line);
    return 0;
}