#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <time.h>
#include "hop.h"

#define MAX_PATH_LEN PATH_MAX
#define TEMP_PATH_LEN (PATH_MAX + 5)
#define MAX_LINE 4096

char previous_dir[MAX_PATH_LEN] = "";
int has_previous = 0;

void get_history_file(char *path)
{
    char *home = getenv("HOME");
    if (home == NULL)
    {
        path[0] = '\0';
        return;
    }
    snprintf(path, MAX_PATH_LEN, "%s/.hop_history", home);
}

//updating history after each hop
void update_history(char *dir)
{
    char history_file[MAX_PATH_LEN];
    char temp_file[TEMP_PATH_LEN];

    get_history_file(history_file);

    if (history_file[0] == '\0')
        return;

    snprintf(temp_file, MAX_PATH_LEN, "%s.tmp", history_file);
    FILE *in = fopen(history_file, "r");
    FILE *out = fopen(temp_file, "w");

    if (out == NULL)
    {
        if (in != NULL)
            fclose(in);

        return;
    }

    char line[MAX_LINE];
    char stored_path[MAX_PATH_LEN];
    int frequency;
    long last_visit;
    int found = 0;
    long current_time = time(NULL);

    while (in != NULL && fgets(line, sizeof(line), in) != NULL)
    {
        if (sscanf(line, "%s %d %ld",stored_path,&frequency,&last_visit) == 3)
        {
            if (strcmp(stored_path, dir) == 0)
            {
                frequency++;
                last_visit = current_time;
                found = 1;
            }
            fprintf(out, "%s %d %ld\n",stored_path,frequency,last_visit);
        }
    }
    if (in != NULL)
        fclose(in);

    if (!found)
    fprintf(out, "%s 1 %ld\n",dir,current_time);
    fclose(out);
    rename(temp_file, history_file);
}

// finding best match acc to frecency
int find_frecency_match(char *name, char *result)
{
    char history_file[MAX_PATH_LEN];
    get_history_file(history_file);

    if (history_file[0] == '\0')
        return 0;

    FILE *file = fopen(history_file, "r");

    if (file == NULL)
        return 0;

    char line[MAX_LINE];
    char path[MAX_PATH_LEN];

    int frequency;
    long last_visit;
    long current_time = time(NULL);

    double best_score = -1;
    int found = 0;

    while (fgets(line, sizeof(line), file) != NULL)
    {
        if (sscanf(line, "%s %d %ld",path,&frequency,&last_visit) != 3)
        continue;
        
        //First requirement:The path must contain the name. /home/user/osnmp1 contains "osn"
        if (strstr(path, name) == NULL)
            continue;
        //check for deleted directory
        if (access(path, F_OK) != 0)
            continue;
        long age = current_time - last_visit;
        if (age < 0)
            age = 0;
        double recency = 1000000.0 / (age + 1);
        double score = (100.0 * frequency) + recency;
        if (score > best_score)
        {
            best_score = score;
            strcpy(result, path);
            found = 1;
        }
    }
    fclose(file);
    return found;
}

//Change the current directory. used by all sorts of hop
int change_directory(char *path)
{
    char current_dir[MAX_PATH_LEN];
    char new_dir[MAX_PATH_LEN];

    if (getcwd(current_dir, sizeof(current_dir)) == NULL)
        return 0;
    if (chdir(path) != 0)
        return 0;
    //path of changed dir
    if (getcwd(new_dir, sizeof(new_dir)) == NULL)
        return 0;

    strcpy(previous_dir, current_dir);
    has_previous = 1;
    update_history(new_dir);
    return 1;
}
//one arg for hop

void hop_one(char *arg)
{
    char path[MAX_PATH_LEN];

    //hop ~
    if (strcmp(arg, "~") == 0)
    {
        char *home = getenv("HOME");
        if (home != NULL)
        {
            if (!change_directory(home))
                printf("hop: no such directory\n");
        }
        return;
    }
    //hop .
    if (strcmp(arg, ".") == 0)
    return;
    //hop ..
    if (strcmp(arg, "..") == 0)
    {
        change_directory("..");
        return;
    }
    //hop -
    if (strcmp(arg, "-") == 0)
    {
        if (!has_previous)
            return;
        change_directory(previous_dir);
        return;
    }

    // first try direct path
    if (change_directory(arg))
    return;
    //if failed check frecency lookup
    if (find_frecency_match(arg, path))
    {
        if (change_directory(path))
        return;
    }
    printf("hop: no such directory\n");
}

void hop_command(char **args, int argc)
{
    if (argc == 0)
    {
        hop_one("~");
        return;
    }
    //process arg sequentially
    for (int i = 0; i < argc; i++)
    hop_one(args[i]);
}