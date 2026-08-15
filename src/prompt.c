#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <limits.h>
#include <string.h>
#include "prompt.h"

char home_directory[PATH_MAX];  // where shell was started
char username[LOGIN_NAME_MAX];
char hostname[HOST_NAME_MAX];

void init_prompt(void)
{
    if (getcwd(home_directory, sizeof(home_directory)) == NULL)  // get directory where shell was started
    {
        perror("cshell: getcwd");
        exit(EXIT_FAILURE);
    }
    uid_t uid = getuid();
  
    //Convert UID into information about the user.
    struct passwd *pw = getpwuid(uid);

    if (pw == NULL)
    {
        perror("cshell: getpwuid");
        exit(EXIT_FAILURE);
    }
    // store the username
    if (strlen(pw->pw_name) >= sizeof(username)) 
    {
        fprintf(stderr, "cshell: username too long\n");
        exit(EXIT_FAILURE);
    }
    strcpy(username, pw->pw_name);

    if (gethostname(hostname, sizeof(hostname)) != 0) 
    {
        perror("cshell: gethostname");
        exit(EXIT_FAILURE);
    }

    hostname[sizeof(hostname) - 1] = '\0';
}
void print_prompt(void)
{
    char current_directory[PATH_MAX];
    if (getcwd(current_directory, sizeof(current_directory)) == NULL) 
    {
        perror("cshell: getcwd");
        return;
    }
    // determine the path to be displayed
    char *display_path;

    /* Case 1:
     We are exactly in the directory where the shell started.
     */
    if (strcmp(current_directory, home_directory) == 0) 
    display_path = "~";
    
    /*Case 2:
     Current directory is inside the shell's home directory.
    */
    
    else if(strcmp(home_directory, "/") == 0 || (strncmp(current_directory,home_directory,strlen(home_directory)) == 0 && current_directory[strlen(home_directory)] == '/'))
    {
        size_t home_length = strlen(home_directory);
        char *remaining =current_directory + home_length;

        /*
         If home is "/", remaining already begins with the
         path after the root.
         */
        if (strcmp(home_directory, "/") == 0) 
        {
            char shortened_path[PATH_MAX + 2];
            snprintf(shortened_path,sizeof(shortened_path),"~%s",current_directory);
            display_path = shortened_path;
        }
        else 
        {
            char shortened_path[PATH_MAX + 2];
            snprintf(shortened_path,sizeof(shortened_path),"~%s",remaining);
            display_path = shortened_path;
        }
    }

    /* Case 3:
    Current directory is outside the shell's home directory.
    */
    else 
    display_path = current_directory;
    
    printf("<%s@%s:%s> ",username,hostname,display_path);
    fflush(stdout);
}