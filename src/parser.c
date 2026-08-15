#include "parser.h"
#include <stddef.h>

// end of input means epsilon

int parse_line_rule(Token **current); //Starts parsing a complete command line
int parse_arg(Token **current);  //Handles arguments, redirections, pipes, ;, and &
int parse_cmd(Token **current);  //Checks that a new command after `
int parse_tgt(Token **current); //Checks that a redirection (<, >, >>) is followed by a filename
int parse_bg(Token **current);  //Checks that a background command after & starts with a word

int parse_line_rule(Token **current)
{
    if (*current == NULL) 
    return 1;
    if ((*current)->type != TOKEN_WORD) 
    return 0;
    *current = (*current)->next;
    return parse_arg(current);
}
int parse_arg(Token **current)
{
    if (*current == NULL) 
    return 1;

    if ((*current)->type == TOKEN_WORD) 
    {
        *current = (*current)->next;
        return parse_arg(current);
    }
    if ((*current)->type == TOKEN_LT) 
    {
        *current = (*current)->next;
        return parse_tgt(current);
    }
    if ((*current)->type == TOKEN_GT) 
    {
        *current = (*current)->next;
        return parse_tgt(current);
    }
    if ((*current)->type == TOKEN_GTGT) 
    {
        *current = (*current)->next;
        return parse_tgt(current);
    }
    if ((*current)->type == TOKEN_PIPE) 
    {
       *current = (*current)->next;
        return parse_cmd(current);
    }
    if ((*current)->type == TOKEN_SEMI) 
    {
        *current = (*current)->next;
        return parse_cmd(current);
    }
    if ((*current)->type == TOKEN_AMP) 
    {
        *current = (*current)->next;
        return parse_bg(current);
    }
    return 0;
}
int parse_cmd(Token **current)
{
    if (*current == NULL || (*current)->type != TOKEN_WORD) 
    return 0;
    *current = (*current)->next;
    return parse_arg(current);
}

int parse_tgt(Token **current)
{
    if (*current == NULL ||(*current)->type != TOKEN_WORD) 
    return 0;
    *current = (*current)->next;
    return parse_arg(current);
}

int parse_bg(Token **current)
{
    if (*current == NULL) 
    return 1;
    if ((*current)->type != TOKEN_WORD) 
    return 0;
    *current = (*current)->next;
    return parse_arg(current);
}

//Entry point; starts the parser and ensures everything was consumed
int parse_line(Token *tokens)
{
    Token *current = tokens;
    if (!parse_line_rule(&current)) 
    return 0;
    return current == NULL;
}