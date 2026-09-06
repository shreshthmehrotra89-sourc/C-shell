#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <time.h>
#include <sys/syscall.h>

typedef struct
{
    long number;
    const char *name;
} SyscallInfo;

static const SyscallInfo syscall_table[] =
{
    { 0,   "read" },
    { 1,   "write" },
    { 2,   "open" },
    { 3,   "close" },
    { 4,   "stat" },
    { 5,   "fstat" },
    { 6,   "lstat" },
    { 7,   "poll" },
    { 8,   "lseek" },
    { 9,   "mmap" },
    { 10,  "mprotect" },
    { 11,  "munmap" },
    { 12,  "brk" },
    { 13,  "rt_sigaction" },
    { 14,  "rt_sigprocmask" },
    { 15,  "rt_sigreturn" },
    { 16,  "ioctl" },
    { 17,  "pread64" },
    { 18,  "pwrite64" },
    { 19,  "readv" },
    { 20,  "writev" },
    { 21,  "access" },
    { 22,  "pipe" },
    { 23,  "select" },
    { 24,  "sched_yield" },
    { 25,  "mremap" },
    { 26,  "msync" },
    { 27,  "mincore" },
    { 28,  "madvise" },
    { 29,  "shmget" },
    { 30,  "shmat" },
    { 31,  "shmctl" },
    { 32, "dup" },
    { 33, "dup2" },
    { 34, "pause" },
    { 35, "nanosleep" },
    { 36, "getitimer" },
    { 37, "alarm" },
    { 38, "setitimer" },
    { 39, "getpid" },
    { 40, "sendfile" },
    { 41, "socket" },
    { 42, "connect" },
    { 43, "accept" },
    { 44, "sendto" },
    { 45, "recvfrom" },
    { 46, "sendmsg" },
    { 47, "recvmsg" },
    { 48, "shutdown" },
    { 49, "bind" },
    { 50, "listen" },
    { 51, "getsockname" },
    { 52, "getpeername" },
    { 53, "socketpair" },
    { 54, "setsockopt" },
    { 55, "getsockopt" },
    { 56, "clone" },
    { 57, "fork" },
    { 58, "vfork" },
    { 59, "execve" },
    { 60, "exit" },
    { 61, "wait4" },
    { 62, "kill" },
    { 63, "uname" },
    { 64, "semget" },
    { 65, "semop" },
    { 66, "semctl" },
    { 67, "shmdt" },
    { 68, "msgget" },
    { 69, "msgsnd" },
    { 70, "msgrcv" },
    { 71, "msgctl" },
    { 72, "fcntl" },
    { 73, "flock" },
    { 74, "fsync" },
    { 75, "fdatasync" },
    { 76, "truncate" },
    { 77, "ftruncate" },
    { 78, "getdents" },
    { 79, "getcwd" },
    { 80, "chdir" },
    { 81, "fchdir" },
    { 82, "rename" },
    { 83, "mkdir" },
    { 84, "rmdir" },
    { 85, "creat" },
    { 86, "link" },
    { 87, "unlink" },
    { 88, "symlink" },
    { 89, "readlink" },
    { 90, "chmod" },
    { 91, "fchmod" },
    { 92, "chown" },
    { 93, "fchown" },
    { 94, "lchown" },
    { 95, "umask" },
    { 96, "gettimeofday" },
    { 97, "getrlimit" },
    { 98, "getrusage" },
    { 99, "sysinfo" },
    { 100, "times" },
    { 101, "ptrace" },
    { 102, "getuid" },
    { 103, "syslog" },
    { 104, "getgid" },
    { 105, "setuid" },
    { 106, "setgid" },
    { 107, "geteuid" },
    { 108, "getegid" },
    { 109, "setpgid" },
    { 110, "getppid" },
    { 111, "getpgrp" },
    { 112, "setsid" },
    { 113, "setreuid" },
    { 114, "setregid" },
    { 115, "getgroups" },
    { 116, "setgroups" },
    { 117, "setresuid" },
    { 118, "getresuid" },
    { 119, "setresgid" },
    { 120, "getresgid" },
    { 121, "getpgid" },
    { 122, "setfsuid" },
    { 123, "setfsgid" },
    { 124, "getsid" },
    { 125, "capget" },
    { 126, "capset" },
    { 127, "rt_sigpending" },
    { 128, "rt_sigtimedwait" },
    { 129, "rt_sigqueueinfo" },
    { 130, "rt_sigsuspend" },
    { 131, "sigaltstack" },
    { 132, "utime" },
    { 133, "mknod" },
    { 134, "uselib" },
    { 135, "personality" },
    { 136, "ustat" },
    { 137, "statfs" },
    { 138, "fstatfs" },
    { 139, "sysfs" },
    { 140, "getpriority" },
    { 141, "setpriority" },
    { 142, "sched_setparam" },
    { 143, "sched_getparam" },
    { 144, "sched_setscheduler" },
    { 145, "sched_getscheduler" },
    { 146, "sched_get_priority_max" },
    { 147, "sched_get_priority_min" },
    { 148, "sched_rr_get_interval" },
    { 149, "mlock" },
    { 150, "munlock" },
    { 151, "mlockall" },
    { 152, "munlockall" },
    { 153, "vhangup" },
    { 154, "modify_ldt" },
    { 155, "pivot_root" },
    { 156, "_sysctl" },
    { 157, "prctl" },
    { 158, "arch_prctl" },
    { 159, "adjtimex" },
    { 160, "setrlimit" },
    { 161, "chroot" },
    { 162, "sync" },
    { 163, "acct" },
    { 164, "settimeofday" },
    { 165, "mount" },
    { 166, "umount2" },
    { 167, "swapon" },
    { 168, "swapoff" },
    { 169, "reboot" },
    { 170, "sethostname" },
    { 171, "setdomainname" },
    { 172, "iopl" },
    { 173, "ioperm" },
    { 174, "create_module" },
    { 175, "init_module" },
    { 176, "delete_module" },
    { 177, "get_kernel_syms" },
    { 178, "query_module" },
    { 179, "quotactl" },
    { 180, "nfsservctl" },
    { 181, "getpmsg" },
    { 182, "putpmsg" },
    { 183, "afs_syscall" },
    { 184, "tuxcall" },
    { 185, "security" },
    { 186, "gettid" },
    { 187, "readahead" },
    { 188, "setxattr" },
    { 189, "lsetxattr" },
    { 190, "fsetxattr" },
    { 191, "getxattr" },
    { 192, "lgetxattr" },
    { 193, "fgetxattr" },
    { 194, "listxattr" },
    { 195, "llistxattr" },
    { 196, "flistxattr" },
    { 197, "removexattr" },
    { 198, "lremovexattr" },
    { 199, "fremovexattr" },
    { 200, "tkill" },
    { 201, "time" },
    { 202, "futex" },
    { 203, "sched_setaffinity" },
    { 204, "sched_getaffinity" },
    { 205, "set_thread_area" },
    { 206, "io_setup" },
    { 207, "io_destroy" },
    { 208, "io_getevents" },
    { 209, "io_submit" },
    { 210, "io_cancel" },
    { 211, "get_thread_area" },
    { 212, "lookup_dcookie" },
    { 213, "epoll_create" },
    { 214, "epoll_ctl_old" },
    { 215, "epoll_wait_old" },
    { 216, "remap_file_pages" },
    { 217, "getdents64" },
    { 218, "set_tid_address" },
    { 219, "restart_syscall" },
    { 220, "semtimedop" },
    { 221, "fadvise64" },
    { 222, "timer_create" },
    { 223, "timer_settime" },
    { 224, "timer_gettime" },
    { 225, "timer_getoverrun" },
    { 226, "timer_delete" },
    { 227, "clock_settime" },
    { 228, "clock_gettime" },
    { 229, "clock_getres" },
    { 230, "clock_nanosleep" },
    { 231, "exit_group" },
    { 232, "epoll_wait" },
    { 233, "epoll_ctl" },
    { 234, "tgkill" },
    { 235, "utimes" },
    { 236, "vserver" },
    { 237, "mbind" },
    { 238, "set_mempolicy" },
    { 239, "get_mempolicy" },
    { 240, "mq_open" },
    { 241, "mq_unlink" },
    { 242, "mq_timedsend" },
    { 243, "mq_timedreceive" },
    { 244, "mq_notify" },
    { 245, "mq_getsetattr" },
    { 246, "kexec_load" },
    { 247, "waitid" },
    { 248, "add_key" },
    { 249, "request_key" },
    { 250, "keyctl" },
    { 251, "ioprio_set" },
    { 252, "ioprio_get" },
    { 253, "inotify_init" },
    { 254, "inotify_add_watch" },
    { 255, "inotify_rm_watch" },
    { 256, "migrate_pages" },
    { 257, "openat" },
    { 258, "mkdirat" },
    { 259, "mknodat" },
    { 260, "fchownat" },
    { 261, "futimesat" },
    { 262, "newfstatat" },
    { 263, "unlinkat" },
    { 264, "renameat" },
    { 265, "linkat" },
    { 266, "symlinkat" },
    { 267, "readlinkat" },
    { 268, "fchmodat" },
    { 269, "faccessat" },
    { 270, "pselect6" },
    { 271, "ppoll" },
    { 272, "unshare" },
    { 273, "set_robust_list" },
    { 274, "get_robust_list" },
    { 275, "splice" },
    { 276, "tee" },
    { 277, "sync_file_range" },
    { 278, "vmsplice" },
    { 279, "move_pages" },
    { 280, "utimensat" },
    { 281, "epoll_pwait" },
    { 282, "signalfd" },
    { 283, "timerfd_create" },
    { 284, "eventfd" },
    { 285, "fallocate" },
    { 286, "timerfd_settime" },
    { 287, "timerfd_gettime" },
    { 288, "accept4" },
    { 289, "signalfd4" },
    { 290, "eventfd2" },
    { 291, "epoll_create1" },
    { 292, "dup3" },
    { 293, "pipe2" },
    { 294, "inotify_init1" },
    { 295, "preadv" },
    { 296, "pwritev" },
    { 297, "rt_tgsigqueueinfo" },
    { 298, "perf_event_open" },
    { 299, "recvmmsg" },
    { 300, "fanotify_init" },
    { 301, "fanotify_mark" },
    { 302, "prlimit64" },
    { 303, "name_to_handle_at" },
    { 304, "open_by_handle_at" },
    { 305, "clock_adjtime" },
    { 306, "syncfs" },
    { 307, "sendmmsg" },
    { 308, "setns" },
    { 309, "getcpu" },
    { 310, "process_vm_readv" },
    { 311, "process_vm_writev" },
    { 312, "kcmp" },
    { 313, "finit_module" },
    { 314, "sched_setattr" },
    { 315, "sched_getattr" },
    { 316, "renameat2" },
    { 317, "seccomp" },
    { 318, "getrandom" },
    { 319, "memfd_create" },
    { 320, "kexec_file_load" },
    { 321, "bpf" },
    { 322, "execveat" },
    { 323, "socketpair" },
    { 324, "userfaultfd" },
    { 325, "membarrier" },
    { 326, "mlock2" },
    { 327, "copy_file_range" },
    { 328, "preadv2" },
    { 329, "pwritev2" },
    { 330, "pkey_mprotect" },
    { 331, "pkey_alloc" },
    { 332, "pkey_free" },
    { 333, "statx" },
    { 334, "io_pgetevents" },
    { 424, "pidfd_send_signal" },
    { 425, "io_uring_setup" },
    { 426, "io_uring_enter" },
    { 427, "io_uring_register" },
    { 428, "open_tree" },
    { 429, "move_mount" },
    { 430, "fsopen" },
    { 431, "fsconfig" },
    { 432, "fsmount" },
    { 433, "fspick" },
    { 434, "pidfd_open" },
    { 435, "clone3" },
    { 436, "close_range" },
    { 437, "openat2" },
    { 438, "pidfd_getfd" },
    { 439, "faccessat2" },
    { 440, "process_madvise" },
    { 441, "epoll_pwait2" },
    { 442, "mount_setattr" },
    { 443, "quotactl_fd" },
    { 444, "landlock_create_ruleset" },
    { 445, "landlock_add_rule" },
    { 446, "landlock_restrict_self" },
    { 447, "memfd_secret" },
    { 448, "process_mrelease" },
    { 449, "futex_waitv" },
    { 450, "set_mempolicy_home_node" }
};

#define SYSCALL_TABLE_SIZE \
    (sizeof(syscall_table) / sizeof(syscall_table[0]))

typedef struct
{
    long number;
    long long calls;
    double total_time;
    long first_order;
} SyscallStat;

#define MAX_SYSCALL_STATS 1024

static SyscallStat stats[MAX_SYSCALL_STATS];
static int stat_count = 0;
static long first_occurrence_counter = 0;
/*
 * Get syscall name.
 */
static const char *get_syscall_name(long number)
{
    for (size_t i = 0; i < SYSCALL_TABLE_SIZE; i++)
    {
        if (syscall_table[i].number == number)
        return syscall_table[i].name;
    }
    return NULL;
}
/*
 * Find an existing syscall entry.
 */
static int find_stat(long number)
{
    for (int i = 0; i < stat_count; i++)
    {
        if (stats[i].number == number)
        return i;
    }
    return -1;
}
/*
 * Add a syscall occurrence.
 */
static SyscallStat *get_stat(long number)
{
    int index = find_stat(number);

    if (index != -1)
        return &stats[index];

    if (stat_count >= MAX_SYSCALL_STATS)
        return NULL;

    stats[stat_count].number = number;
    stats[stat_count].calls = 0;
    stats[stat_count].total_time = 0.0;
    stats[stat_count].first_order = first_occurrence_counter++;
    return &stats[stat_count++];
}
/*
 * Get monotonic time in seconds.
 */
static double get_time_seconds(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1)
        return 0.0;
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}
/*
 * Compare syscall statistics.
 *
 * Primary:
 *      call count descending
 *
 * Secondary:
 *      first occurrence ascending
 */
static int compare_stats(const void *a, const void *b)
{
    const SyscallStat *x = a;
    const SyscallStat *y = b;
    if (x->calls < y->calls)
        return 1;
    if (x->calls > y->calls)
        return -1;
    if (x->first_order < y->first_order)
        return -1;
    if (x->first_order > y->first_order)
        return 1;
    return 0;
}
/*
 * Print final summary.
 */
static void print_summary(void)
{
    qsort(stats,stat_count,sizeof(SyscallStat),compare_stats);
    printf("%-15s %-7s %s\n","syscall","calls","time");
    for (int i = 0; i < stat_count; i++)
    {
        const char *name =get_syscall_name(stats[i].number);
        char unknown_name[64];
        if (name == NULL)
        {
            snprintf(unknown_name,sizeof(unknown_name),"syscall_%ld",stats[i].number);
            name = unknown_name;
        }
        printf("%-15s %-7lld %.3fs\n",name,stats[i].calls,stats[i].total_time);
    }
}
/*
 * Resolve command exactly like a normal shell command.
 *
 * Returns a newly allocated path.
 */
static char *resolve_command(const char *command)
{
    if (command == NULL || command[0] == '\0')
        return NULL;
    /*
     * If command contains '/', use it directly.
     */
    if (strchr(command, '/') != NULL)
    {
        if (access(command, X_OK) != 0)
            return NULL;
        return strdup(command);
    }

    /*
     * Search PATH.
     */
    const char *path_env = getenv("PATH");

    if (path_env == NULL)
        return NULL;

    char *path_copy = strdup(path_env);
    if (path_copy == NULL)
        return NULL;

    char *saveptr = NULL;
    char *directory =strtok_r(path_copy, ":", &saveptr);
    while (directory != NULL)
    {
        char candidate[4096];
        snprintf(candidate,sizeof(candidate),"%s/%s",directory,command);
        if (access(candidate, X_OK) == 0)
        {
            char *result = strdup(candidate);
            free(path_copy);
            return result;
        }
        directory =strtok_r(NULL, ":", &saveptr);
    }
    free(path_copy);
    return NULL;
}
/*
 * Trace a newly-created child.
 */
static int trace_new_process(char **argv, char *resolved_path)
{
    pid_t child = fork();
    if (child == -1)
    {
        perror("snoop: fork");
        free(resolved_path);
        return -1;
    }

    if (child == 0)
    {
        /*
         * Tell the kernel that the parent will trace us.
         */
        if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) == -1)
        {
            perror("snoop: ptrace");
            _exit(127);
        }

        /*
         * Execute the requested command.
         *
         * After a successful execve(), Linux generates
         * an initial SIGTRAP for the tracing parent.
         */
        execve(resolved_path, argv, environ);

        /*
         * execve() failed.
         */
        _exit(127);
    }

    int status;

    /*
     * Wait for the post-exec SIGTRAP.
     */
    if (waitpid(child, &status, 0) == -1)
    {
        perror("snoop: waitpid");
        return -1;
    }

    if (!WIFSTOPPED(status))
    {
        return -1;
    }

    /*
     * Make syscall stops appear as:
     *
     * SIGTRAP | 0x80
     */
    if (ptrace(PTRACE_SETOPTIONS,
               child,
               NULL,
               PTRACE_O_TRACESYSGOOD) == -1)
    {
        perror("snoop: ptrace");
        return -1;
    }

    /*
     * Start syscall tracing.
     */
    if (ptrace(PTRACE_SYSCALL,
               child,
               NULL,
               NULL) == -1)
    {
        perror("snoop: ptrace");
        return -1;
    }

    int entering_syscall = 1;
    long current_syscall = -1;
    double entry_time = 0.0;

    while (1)
    {
        if (waitpid(child, &status, 0) == -1)
        {
            if (errno == EINTR)
                continue;
            perror("snoop: waitpid");
            return -1;
        }

        /*
         * Process exited normally.
         */
        if (WIFEXITED(status))
        {
            break;
        }

        /*
         * Process was killed by a signal.
         */
        if (WIFSIGNALED(status))
        {
            break;
        }

        if (!WIFSTOPPED(status))
            continue;

        int signal_number = WSTOPSIG(status);

        /*
         * Syscall entry OR syscall exit.
         */
        if (signal_number == (SIGTRAP | 0x80))
        {
            struct user_regs_struct regs;
            if (ptrace(PTRACE_GETREGS,
                       child,
                       NULL,
                       &regs) == -1)
            {
                perror("snoop: ptrace");
                return -1;
            }

            long syscall_number =(long)regs.orig_rax;
            /*
             * SYSCALL ENTRY
             */
            if (entering_syscall)
            {
                current_syscall = syscall_number;
                entry_time = get_time_seconds();
                entering_syscall = 0;
            }

            /*
             * SYSCALL EXIT
             */
            else
            {
                double exit_time =get_time_seconds();
                double elapsed =exit_time - entry_time;
                SyscallStat *stat =get_stat(current_syscall);
                if (stat != NULL)
                {
                    stat->calls++;
                    stat->total_time += elapsed;
                }
                entering_syscall = 1;
            }

            /*
             * Continue to the next syscall stop.
             */
            if (ptrace(PTRACE_SYSCALL,
                       child,
                       NULL,
                       NULL) == -1)
            {
                perror("snoop: ptrace");
                return -1;
            }
        }

        /*
         * Some other signal.
         */
        else
        {
            int deliver_signal = signal_number;
            /*
             * Do not send tracing-related signals
             * back into the tracee.
             */
            if (signal_number == SIGTRAP ||
                signal_number == SIGSTOP)
            {
                deliver_signal = 0;
            }

            if (ptrace(PTRACE_SYSCALL,
                       child,
                       NULL,
                       deliver_signal) == -1)
            {
                perror("snoop: ptrace");
                return -1;
            }
        }
    }

    return 0;
}
/*
 * Attach to an already-running process.
 */
static int trace_existing_process(pid_t pid)
{
    /*
     * First check whether PID exists.
     */
    if (kill(pid, 0) == -1)
    {
        if (errno == ESRCH)
        {
            printf("snoop: no such process\n");
            return -1;
        }

        /*
         * EPERM means the process exists but
         * we don't have permission.
         */
        if (errno == EPERM)
        {
            /*
             * Continue to PTRACE_ATTACH so that we
             * can report the actual ptrace error.
             */
        }
        else
        {
            printf("snoop: no such process\n");
            return -1;
        }
    }

    /*
     * Attach.
     */
    if (ptrace(
            PTRACE_ATTACH,
            pid,
            NULL,
            NULL) == -1)
    {
        if (errno == ESRCH)
        {
            printf("snoop: no such process\n");
        }
        else
        {
            perror("snoop: ptrace");
        }

        return -1;
    }
    int status;

    if (waitpid(pid, &status, 0) == -1)
    {
        perror("snoop: waitpid");
        return -1;
    }

    if (!WIFSTOPPED(status))
    return -1;
    if (ptrace(
            PTRACE_SETOPTIONS,
            pid,
            NULL,
            PTRACE_O_TRACESYSGOOD) == -1)
    {
        perror("snoop: ptrace");
        return -1;
    }

    if (ptrace(
            PTRACE_SYSCALL,
            pid,
            NULL,
            NULL) == -1)
    {
        perror("snoop: ptrace");
        return -1;
    }

    int entering_syscall = 1;
    long current_syscall = -1;
    double entry_time = 0.0;

    while (1)
    {
        if (waitpid(pid, &status, 0) == -1)
        {
            if (errno == EINTR)
                continue;

            perror("snoop: waitpid");
            return -1;
        }

        /*
         * Tracee exited.
         */
        if (WIFEXITED(status) || WIFSIGNALED(status))
        break;
        if (WIFSTOPPED(status))
        {
            int signal_number =WSTOPSIG(status);
            if (signal_number ==(SIGTRAP | 0x80))
            {
                struct user_regs_struct regs;
                if (ptrace(
                        PTRACE_GETREGS,
                        pid,
                        NULL,
                        &regs) == -1)
                {
                    perror("snoop: ptrace");
                    return -1;
                }
                long syscall_number =(long)regs.orig_rax;
                if (entering_syscall)
                {
                    current_syscall =syscall_number;
                    entry_time =get_time_seconds();
                    entering_syscall = 0;
                }
                else
                {
                    double exit_time =get_time_seconds();
                    double elapsed =exit_time - entry_time;
                    SyscallStat *stat =get_stat(current_syscall);

                    if (stat != NULL)
                    {
                        stat->calls++;
                        stat->total_time += elapsed;
                    }
                    entering_syscall = 1;
                }

                if (ptrace(
                        PTRACE_SYSCALL,
                        pid,
                        NULL,
                        NULL) == -1)
                {
                    perror("snoop: ptrace");
                    return -1;
                }
            }
            else
            {
                /*
                 * Pass non-trap signals through.
                 */
                if (ptrace(
                        PTRACE_SYSCALL,
                        pid,
                        NULL,
                        signal_number) == -1)
                {
                    perror("snoop: ptrace");
                    return -1;
                }
            }
        }
    }

    return 0;
}
/*
 * Public snoop command.
 */
int snoop_command(char **argv)
{
    if (argv == NULL || argv[0] == NULL)
        return -1;
    /*
     * Reset statistics for every invocation.
     */
    memset(stats, 0, sizeof(stats));
    stat_count = 0;
    first_occurrence_counter = 0;

    /*
     * snoop -p PID
     */
    if (strcmp(argv[0], "snoop") == 0 && argv[1] != NULL && strcmp(argv[1], "-p") == 0)
    {
        /*
         * Exactly:
         *
         * snoop -p pid
         */
        if (argv[2] == NULL || argv[3] != NULL)
        {
            printf("snoop: invalid syntax\n");
            return -1;
        }
        char *endptr = NULL;
        errno = 0;
        long pid_value =strtol(argv[2], &endptr, 10);
        if (argv[2][0] == '\0' || endptr == argv[2] || *endptr != '\0' || errno == ERANGE || pid_value <= 0)
        {
            printf("snoop: invalid syntax\n");
            return -1;
        }
        int result =trace_existing_process((pid_t)pid_value);
        if (result == 0)
            print_summary();
        return result;
    }

    /*
     * snoop command [args...]
     */
    if (argv[1] == NULL)
    {
        printf("snoop: invalid syntax\n");
        return -1;
    }

    /*
     * The command is argv[1].
     */
    char *resolved_path =resolve_command(argv[1]);
    if (resolved_path == NULL)
    {
        printf("snoop: command not found\n");
        return -1;
    }

    /*
     * Build argv for execve().
     *
     * Original:
     *
     * snoop sleep 1
     *
     * becomes:
     *
     * sleep 1
     */
    int argc = 0;
    while (argv[argc] != NULL)
        argc++;

    char **child_argv =malloc(sizeof(char *) * argc);
    if (child_argv == NULL)
    {
        free(resolved_path);
        return -1;
    }
    for (int i = 1; i < argc; i++)
    child_argv[i - 1] = argv[i];

    child_argv[argc - 1] = NULL;
    int result =trace_new_process(child_argv,resolved_path);
    free(child_argv);
    free(resolved_path);

    if (result == 0)
        print_summary();

    return result;
}