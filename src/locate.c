#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include "locate.h"

void search_current_directory(char *filename, int *found)
{
    char cwd[PATH_MAX];
    char path[PATH_MAX];

    if (getcwd(cwd, sizeof(cwd)) == NULL)
        return;
    snprintf(path, sizeof(path), "%s/%s", cwd, filename);
    if (access(path, X_OK) == 0)
    {
        printf("%s\n", path);
        *found = 1;
    }
}
void search_path(char *filename, int *found)
{
    char *path_env;
    char *path_copy;
    char *directory;
    path_env = getenv("PATH");

    if (path_env == NULL)
        return;
    path_copy = malloc(strlen(path_env) + 1);

    if (path_copy == NULL)
        return;

    strcpy(path_copy, path_env);
    directory = strtok(path_copy, ":");

    while (directory != NULL)
    {
        char path[PATH_MAX];
        if (directory[0] == '\0')
            snprintf(path, sizeof(path), "./%s", filename);
        else
            snprintf(path, sizeof(path), "%s/%s", directory, filename);
        if (access(path, X_OK) == 0)
        {
            char absolute_path[PATH_MAX];
            // realpath()  function converts relative path to absolute path(for confirming)
            if (realpath(path, absolute_path) != NULL)
            {
                printf("%s\n", absolute_path);
                *found = 1;
            }
        }
        directory = strtok(NULL, ":"); //extract next directory
    }
    free(path_copy);
}
void locate_command(char **args, int arg_count)
{
    for (int i = 0; i < arg_count; i++)
    {
        int found = 0;
        search_current_directory(args[i], &found);
        search_path(args[i], &found);
        if (!found)
        printf("locate: command not found (%s)\n", args[i]);
    }
}