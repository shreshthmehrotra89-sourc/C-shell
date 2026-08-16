#ifndef LEXER_H
#define LEXER_H

typedef enum 
{
    TOKEN_WORD,
    TOKEN_PIPE,
    TOKEN_AMP,
    TOKEN_SEMI,
    TOKEN_LT,
    TOKEN_GT,
    TOKEN_GTGT
} TokenType;

typedef struct Token 
{
    TokenType type;
    char *value;
    struct Token *next;
} Token;

//converts raw input line to linked list of tokens
int lex_line(char *line, Token **tokens);

// Free the entire token list 
void free_tokens(Token *tokens);

void print_tokens(Token *tokens);

#endif