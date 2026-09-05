#include <stdio.h>
#include <signal.h>

#include "activities.h"
#include "background.h"


void activities(void)
{
    sigset_t block_set;
    sigset_t old_set;
    sigemptyset(&block_set);
    sigaddset(&block_set, SIGCHLD);
    /*
     * Block SIGCHLD while accessing the job table.
     */
    sigprocmask(SIG_BLOCK,&block_set,&old_set);
    /*
     * Remove processes which have exited.
     */
    for (int i = 0; i < job_count; )
    {
        for (int j = 0;j < jobs[i].process_count; )
        {
            if (jobs[i].processes[j].completed)
            {
                /*
                 * Shift the remaining processes left.
                 */
                for (int k = j;k < jobs[i].process_count - 1;k++)
                jobs[i].processes[k] =jobs[i].processes[k + 1];
                jobs[i].process_count--;
                continue;
            }
            j++;
        }
        if (jobs[i].process_count == 0)
        {
            for (int j = i; j < job_count - 1;j++)
            jobs[j] =jobs[j + 1];
            job_count--;
            continue;
        }
        i++;
    }
    /*
     * Print jobs in launch order.
     */
    for (int i = 0;i < job_count;i++)
    {
        printf("[%d] pgid %d\n",jobs[i].job_number,jobs[i].pgid);
        /*
         * Print every process in the group.
         */
        for (int j = 0;j < jobs[i].process_count;j++)
        {
            printf("  %d %s %s\n",
                   jobs[i].processes[j].pid,
                   jobs[i].processes[j].command,
                   jobs[i].processes[j].stopped
                       ? "Stopped"
                       : "Running");
        }
    }
    fflush(stdout);
    /*
     * Restore SIGCHLD handling.
     */
    sigprocmask(SIG_SETMASK,&old_set,NULL);
}