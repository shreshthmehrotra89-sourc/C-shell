#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"

int append_char(char **buffer,size_t *length,size_t *capacity,char c)
{
    if (*length + 1 >= *capacity) 
    {
        size_t new_capacity = (*capacity == 0) ? 16 : (*capacity * 2);
        char *new_buffer = realloc(*buffer, new_capacity);
        *buffer = new_buffer;
        *capacity = new_capacity;
    }
    (*buffer)[*length] = c;
    (*length)++;
    (*buffer)[*length] = '\0';
    return 0;
}

// Create a new token and append it to the linked list.
int add_token(Token **head,Token **tail,TokenType type,char *value)
{
    Token *token = malloc(sizeof(Token));
    token->type = type;
    token->next = NULL;

    if (value != NULL) 
    {
        token->value = strdup(value);
        if (token->value == NULL) 
        {
            free(token);
            return -1;
        }
    } 
    else 
    token->value = NULL; 

    if (*head == NULL) 
    {
        *head = token;
        *tail = token;
    } 
    else 
    {
        (*tail)->next = token;
        *tail = token;
    }
    return 0;
}
int is_space(char c)
{
    return c == ' ' ||  c == '\t' || c == '\n' || c == '\r';
}
int is_special(char c)
{
    return c == '|' || c == '&' || c == ';' || c == '<' || c == '>';
}
int lex_line(char *line, Token **tokens)
{
    Token *head = NULL;
    Token *tail = NULL;
    size_t i = 0;

    while (line[i] != '\0') 
    {
        if (is_space(line[i])) 
        {
            i++;
            continue;
        }
        if (line[i] == '|') 
        {
            add_token(&head, &tail, TOKEN_PIPE, NULL) ;
            i++;
            continue;
        }
        if (line[i] == '&') 
        {
            add_token(&head, &tail, TOKEN_AMP, NULL);
            i++;
            continue;
        }
        if (line[i] == ';') 
        {
            add_token(&head, &tail, TOKEN_SEMI, NULL);
            i++;
            continue;
        }
        if (line[i] == '<') 
        {
            add_token(&head, &tail, TOKEN_LT, NULL);
            i++;
            continue;
        }
        //We check >> before >
        if (line[i] == '>') 
        {
            if (line[i + 1] == '>') 
            {
                add_token(&head, &tail, TOKEN_GTGT, NULL);
                i += 2;
            } 
            else 
            {
                add_token(&head, &tail, TOKEN_GT, NULL);
                i++;
            }
            continue;
        }
        //Otherwise we are starting a WORD.
        char *buffer = NULL;
        size_t length = 0;
        size_t capacity = 0;
        int started_word = 0;
        while (line[i] != '\0') 
        {
            if (is_space(line[i]) || is_special(line[i])) 
            break;
            
            //Unquoted backslash. \c means: contribute c and remove the backslash.
            if (line[i] == '\\') 
            {
                started_word = 1;
                //Backslash at end of line is invalid.
                if (line[i + 1] == '\0' || line[i + 1] == '\n') 
                {
                    free(buffer);
                    free_tokens(head);
                    return -1;
                }
                i++;
                append_char(&buffer,&length,&capacity,line[i]);
                i++;
                continue;
            }
            if (line[i] == '"') 
            {
                started_word = 1;
                i++;
                while (line[i] != '\0' &&  line[i] != '"') 
                {
                    //Backslash inside double quotes.
                    if (line[i] == '\\') 
                    {
                        if (line[i + 1] == '\0' || line[i + 1] == '\n') 
                        {
                            free(buffer);
                            free_tokens(head);
                            return -1;
                        }
                        if (line[i + 1] == '"' || line[i + 1] == '\\') 
                        {
                            append_char(&buffer,&length,&capacity,line[i + 1]);
                            i += 2;
                        }
                        //For something like \n:it stays as TWO characters:backslash + n
                        else 
                        {
                            append_char(&buffer,&length,&capacity,'\\') ;
                            append_char(&buffer,&length,&capacity,line[i + 1]);
                            i += 2;
                        }
                        continue;
                    }
                    append_char(&buffer,&length,&capacity,line[i]);
                    i++;
                }
                if (line[i] != '"') 
                {
                    free(buffer);
                    free_tokens(head);
                    return -1;
                }
                i++;
                continue;
            }
            if (line[i] == '\'') 
            {
                started_word = 1;
                i++;
                //Everything inside single quotes is copied
                while (line[i] != '\0' &&  line[i] != '\'') 
                {
                    append_char(&buffer,&length,&capacity,line[i]);
                    i++;
                }
                if (line[i] != '\'') 
                {
                    free(buffer);
                    free_tokens(head);
                    return -1;
                }
                i++;
                continue;
            }
            started_word = 1;
            append_char(&buffer,&length,&capacity,line[i]);
            i++;
        }
        if (started_word) 
        {
            if (buffer == NULL) 
            buffer = strdup("");
            if (buffer == NULL) 
            {
                free_tokens(head);
                return -1;
            }
            add_token(&head,&tail,TOKEN_WORD,buffer);
            free(buffer);
        }
    }
    *tokens = head;
    return 0;
}
// Free token list.
void free_tokens(Token *tokens)
{
    while (tokens != NULL) 
    {
        Token *next = tokens->next;
        free(tokens->value);
        free(tokens);
        tokens = next;
    }
}
/*//for debugging
void print_tokens(Token *tokens)
{
    while (tokens != NULL) 
    {
        switch (tokens->type) 
        {
            case TOKEN_WORD:
                printf("WORD(\"%s\") ", tokens->value);
                break;
            case TOKEN_PIPE:
                printf("PIPE ");
                break;
            case TOKEN_AMP:
                printf("AMP ");
                break;
            case TOKEN_SEMI:
                printf("SEMI ");
                break;
            case TOKEN_LT:
                printf("LT ");
                break;
            case TOKEN_GT:
                printf("GT ");
                break;
            case TOKEN_GTGT:
                printf("GTGT ");
                break;
        }
        tokens = tokens->next;
    }
    printf("\n");
}*/