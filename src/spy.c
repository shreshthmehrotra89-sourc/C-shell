#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <errno.h>
#include "spy.h"
/*
 * Return the TYPE of a file.
 */
static const char *get_file_type(mode_t mode)
{
    if (S_ISREG(mode))
        return "REG";
    if (S_ISDIR(mode))
        return "DIR";
    if (S_ISCHR(mode))
        return "CHR";
    if (S_ISBLK(mode))
        return "BLK";
    if (S_ISFIFO(mode))
        return "FIFO";
    if (S_ISSOCK(mode))
        return "SOCK";
    if (S_ISLNK(mode))
        return "LINK";
    return "UNKNOWN";
}
/*
 * Read the target of a symbolic link.
 *
 * Example:
 *
 * /proc/1234/cwd
 *       |
 *       +----> /home/user
 */
static int get_link_target(const char *link_path,char *target,size_t target_size)
{
    ssize_t length;
    length = readlink(link_path,target,target_size - 1);
    if (length == -1)
        return -1;
    target[length] = '\0';
    return 0;
}
/*
 * Print one entry.
 *
 * PID    FD    TYPE    PATH
 */
static void print_entry(pid_t pid,const char *fd,const char *path)
{
    struct stat st;
    /*
     * stat() follows the path and tells us
     * whether it is REG, DIR, CHR, etc.
     */
    if (stat(path, &st) == -1)
        return;
    printf("%-6d %-5s %-6s %s\n",pid,fd,get_file_type(st.st_mode),path);
}
/*
 * Print current working directory.
 *
 * /proc/<pid>/cwd
 */
static void print_cwd(pid_t pid)
{
    char link_path[PATH_MAX];
    char target[PATH_MAX];
    snprintf(link_path,sizeof(link_path),"/proc/%d/cwd",pid);
    if (get_link_target(link_path,target,sizeof(target)) == -1)
    return;
    print_entry(pid,"cwd",target);
}
/*
 * Print executable.
 *
 * /proc/<pid>/exe
 */
static void print_executable(pid_t pid)
{
    char link_path[PATH_MAX];
    char target[PATH_MAX];
    snprintf(link_path,sizeof(link_path),"/proc/%d/exe",pid);
    if (get_link_target(link_path,target,sizeof(target)) == -1)
    return;
    print_entry(pid,"txt",target);
}
/*
 * Check whether a string is completely numeric.
 *
 * We use this to identify:
 *
 * /proc/<pid>/fd/0
 * /proc/<pid>/fd/1
 * /proc/<pid>/fd/2
 * ...
 */
static int is_numeric(const char *str)
{
    if (str == NULL || str[0] == '\0')
        return 0;
    for (int i = 0; str[i] != '\0'; i++)
    {
        if (str[i] < '0' || str[i] > '9')
        return 0;
    }
    return 1;
}
/*
 * Print numeric file descriptors.
 *
 * Directory:
 *
 * /proc/<pid>/fd/
 */
static void print_file_descriptors(pid_t pid)
{
    char fd_directory[PATH_MAX];
    snprintf(fd_directory,sizeof(fd_directory),"/proc/%d/fd",pid);

    DIR *dir = opendir(fd_directory);
    if (dir == NULL)
        return;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL)
    {
        /*
         * We only want:
         *
         * 0
         * 1
         * 2
         * 3
         * ...
         */
        if (!is_numeric(entry->d_name))
            continue;

        char link_path[PATH_MAX];
        char target[PATH_MAX];

        snprintf(link_path,sizeof(link_path),"%s/%s",fd_directory,entry->d_name);
        if (get_link_target(link_path,target,sizeof(target)) == -1)
        continue;
        /*
         * Pipes and sockets are outside the scope
         * of this mini-project.
         */
        if (strncmp(target, "pipe:[", 6) == 0)
            continue;
        if (strncmp(target, "socket:[", 8) == 0)
            continue;
        print_entry(pid,entry->d_name,target);
    }
    closedir(dir);
}
/*
 * Check whether a memory-mapped file has already
 * been printed.
 */
static int already_seen(char **seen,int count,const char *path)
{
    for (int i = 0; i < count; i++)
    {
        if (strcmp(seen[i], path) == 0)
            return 1;
    }
    return 0;
}
/*
 * Print memory-mapped files.
 *
 * Information comes from:
 *
 * /proc/<pid>/maps
 */
static void print_memory_maps(pid_t pid)
{
    char maps_path[PATH_MAX];
    snprintf(maps_path,sizeof(maps_path),"/proc/%d/maps",pid);
    FILE *file = fopen(maps_path, "r");
    if (file == NULL)
        return;
    char line[4096];

    /*
     * Dynamic array containing paths that
     * we have already printed.
     */
    char **seen = NULL;
    int seen_count = 0;

    while (fgets(line, sizeof(line), file) != NULL)
    {
        /*
         * Example:
         *
         * 7f123000-7f124000 r--p ... /usr/lib/libc.so.6
         *
         * Find the beginning of the pathname.
         */
        char *path = strchr(line, '/');

        if (path == NULL)
            continue;
        /*
         * Remove newline.
         */
        path[strcspn(path, "\n")] = '\0';

        /*
         * If the same library occurs in several
         * mappings, print it only once.
         */
        if (already_seen(seen,seen_count,path))
        continue;
        struct stat st;
        if (stat(path, &st) == -1)
        continue;
        /*
         * Save this pathname.
         */
        char **new_seen;
        new_seen = realloc(seen,sizeof(char *) *(seen_count + 1));
        if (new_seen == NULL)
            break;
        seen = new_seen;
        seen[seen_count] = strdup(path);
        if (seen[seen_count] == NULL)
            break;
        seen_count++;
        print_entry(pid,"mem",path);
    }
    /*
     * Free saved paths.
     */
    for (int i = 0; i < seen_count; i++)
        free(seen[i]);
    free(seen);
    fclose(file);
}
/*
 * Main spy command.
 *
 * spy
 * spy <pid>
 */
void spy_command(char **argv)
{
    int argc = 0;
    while (argv[argc] != NULL)
    argc++;
    /*
     * Valid:
     *
     * spy
     * spy 1234
     *
     * Invalid:
     *
     * spy 1234 5678
     */
    if (argc > 2)
    {
        printf("spy: invalid syntax\n");
        return;
    }

    pid_t pid;

    /*
     * No PID:
     *
     * inspect the shell itself.
     */
    if (argc == 1)
    pid = getpid();
    else
    {
        /*
         * PID was supplied.
         */
        char *endptr;
        errno = 0;
        long value = strtol(argv[1],&endptr,10);

        /*
         * PID must be a positive integer.
         */
        if (argv[1][0] == '\0' ||*endptr != '\0' || errno == ERANGE || value <= 0)
        {
            printf("spy: no such process\n");
            return;
        }
        pid = (pid_t)value;
    }

    /*
     * Check whether /proc/<pid> exists.
     */
    char proc_path[PATH_MAX];

    snprintf(proc_path,sizeof(proc_path),"/proc/%d",pid);
    struct stat st;
    if (stat(proc_path, &st) == -1 || !S_ISDIR(st.st_mode))
    {
        printf("spy: no such process\n");
        return;
    }
    //Header.
    printf("PID    FD    TYPE   PATH\n");
    //Current working directory
    print_cwd(pid);
    //Executable
    print_executable(pid);
    print_memory_maps(pid);
    print_file_descriptors(pid);
}