#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include "reveal.h"
#include "hop.h"

#define MAX_PATH_LEN PATH_MAX

//check whether dir is hidden
int is_hidden(char *name)
{
    if (name[0] == '.')
        return 1;
    return 0;
}
int is_directory(char *path)
{
    struct stat st;
    if (stat(path, &st) != 0)
        return 0;
    return S_ISDIR(st.st_mode);
}
//qsort
int compare_strings(const void *a, const void *b)
{
    char *const *s1 = a;
    char *const *s2 = b;
    return strcmp(*s1, *s2);
}

/*
Reveal the contents of a directory.
path       :actual path on the filesystem
prefix     :path used while printing for recursive eg include\n include/parser.h
show_all   :whether hidden files should be shown for -a
recursive  :whether to enter subdirectories for -t
*/

void reveal_directory(char *path,char *prefix,int show_all,int recursive)
{
    DIR *dir;
    struct dirent *entry;
    int capacity = 20;
    int count = 0;
    char **entries;
    dir = opendir(path);

    if (dir == NULL)
    return;
    entries = malloc(capacity * sizeof(char *));
    
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        continue;
        if (!show_all && is_hidden(entry->d_name))
        continue;

        if (count == capacity)
        {
            capacity *= 2;
            char **temp =realloc(entries,capacity * sizeof(char *));
            if (temp == NULL)
            {
                for (int i = 0; i < count; i++)
                free(entries[i]);
                free(entries);
                closedir(dir);
                return;
            }
            entries = temp;
        }
        entries[count]=malloc(strlen(entry->d_name) + 1);

        if (entries[count] == NULL)
        continue;
        strcpy(entries[count], entry->d_name);
        count++;
    }
    closedir(dir);
    qsort(entries,count,sizeof(char *),compare_strings);
    
    for (int i = 0; i < count; i++)
    {
        char full_path[MAX_PATH_LEN];
        snprintf(full_path,sizeof(full_path),"%s/%s",path,entries[i]);
        if (prefix[0] == '\0')
        {
            if (is_directory(full_path))
            printf("%s/\n", entries[i]);
            else
            printf("%s\n", entries[i]);
        }
        else
        {
            if (is_directory(full_path))
            printf("%s/%s/\n",prefix,entries[i]);
            else
            printf("%s/%s\n",prefix,entries[i]);
        }
    }
    if (!recursive)
    {
        for (int i = 0; i < count; i++)
        free(entries[i]);
        free(entries);
        return;
    }
    for (int i = 0; i < count; i++)
    {
        char full_path[MAX_PATH_LEN];
        char new_prefix[MAX_PATH_LEN];

        snprintf(full_path,sizeof(full_path),"%s/%s",path,entries[i]);
        if (!is_directory(full_path))
        continue;
        if (prefix[0] == '\0')
        snprintf(new_prefix,sizeof(new_prefix),"%s",entries[i]);
        else
        snprintf(new_prefix,sizeof(new_prefix),"%s/%s",prefix,entries[i]);
        //recurse into directory
        reveal_directory(full_path,new_prefix,show_all,recursive);
    }
    for (int i = 0; i < count; i++)
    free(entries[i]);
    free(entries);
}

void reveal_command(char **args, int argc)
{
    int show_all = 0;
    int recursive = 0;
    char target[MAX_PATH_LEN];
    int path_given = 0;

    for (int i = 0; i < argc; i++)
    {
        char *arg = args[i];
        if (arg[0] == '-' && strcmp(arg, "-") != 0)
        {
            for (int j = 1; arg[j] != '\0'; j++)
            {
                if (arg[j] == 'a')
                show_all = 1;
                else if (arg[j] == 't')
                recursive = 1;
                else
                {
                    printf("reveal: invalid syntax\n");
                    return;
                }
            }
            continue;
        }
        if (path_given)
        {
            printf("reveal: invalid syntax\n");
            return;
        }
        path_given = 1;
        //~
        if (strcmp(arg, "~") == 0)
        {
            char *home = getenv("HOME");
            if (home == NULL)
            {
                printf("reveal: no such directory\n");
                return;
            }
            strcpy(target, home);
        }
        //.
        else if (strcmp(arg, ".") == 0)
        {
            if (getcwd(target,sizeof(target)) == NULL)
            {
                printf("reveal: no such directory\n");
                return;
            }
        }
        //..
        else if (strcmp(arg, "..") == 0)
        {
            if (getcwd(target,sizeof(target)) == NULL)
            {
                printf("reveal: no such directory\n");
                return;
            }
            strcat(target, "/..");
        }
        //-
        else if (strcmp(arg, "-") == 0)
        {
            char *previous;
            previous = get_previous_directory();
            if (previous == NULL)
            {
                printf("reveal: no such directory\n");
                return;
            }
            strcpy(target, previous);
        }
        // ~/something
        else if (arg[0] == '~' && arg[1] == '/')
        {
            char *home = getenv("HOME");
            if (home == NULL)
            {
                printf("reveal: no such directory\n");
                return;
            }
            snprintf(target,sizeof(target),"%s%s",home,arg + 1);
        }
        else
        strcpy(target, arg);
    }
    // if no path use current directory
    if (!path_given)
    {
        if (getcwd(target,sizeof(target)) == NULL)
        {
            printf("reveal: no such directory\n");
            return;
        }
    }
    if (!is_directory(target))
    {
        printf("reveal: no such directory\n");
        return;
    }
    reveal_directory(target,"",show_all,recursive);
}