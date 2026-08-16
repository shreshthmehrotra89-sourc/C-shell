#include "peek.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#define PEEK_BUFFER_SIZE 4096


int is_non_empty_line(const char *line, size_t len)
{
    size_t i;
    for (i = 0; i < len; i++)
    {
        if (line[i] != '\n' && line[i] != '\r')
        return 1;
    }
    return 0;
}

void print_line(const char *line, size_t len,int number_lines, int line_number)
{
    if (number_lines && is_non_empty_line(line, len))
        printf("%d ", line_number);
    fwrite(line, 1, len, stdout);
}

void normal_file(FILE *file, int number_lines)
{
    char *line = NULL;
    size_t capacity = 0;
    int line_number = 1;

    while (getline(&line, &capacity, file) != -1)
    {
        size_t len = strlen(line);
        print_line(line, len, number_lines, line_number);
        if (is_non_empty_line(line, len))
            line_number++;
    }
    free(line);
}

int count_non_empty_lines(int fd)
{
    int duplicate_fd;
    FILE *file;

    char *line = NULL;
    size_t capacity = 0;

    int count = 0;

    if (lseek(fd, 0, SEEK_SET) == (off_t)-1)
        return -1;
    duplicate_fd = dup(fd);
    if (duplicate_fd == -1)
        return -1;

    file = fdopen(duplicate_fd, "r");

    if (file == NULL)
    {
        close(duplicate_fd);
        return -1;
    }
    while (getline(&line, &capacity, file) != -1)
    {
        size_t len = strlen(line);
        if (is_non_empty_line(line, len))
            count++;
    }

    free(line);
    fclose(file);

    //reset the original descriptor
    if (lseek(fd, 0, SEEK_SET) == (off_t)-1)
        return -1;
    return count;
}

// find position of previous \n
off_t find_previous_newline(int fd, off_t position)
{
    char buffer[PEEK_BUFFER_SIZE];
    while (position > 0)
    {
        off_t chunk_start;
        if (position > PEEK_BUFFER_SIZE)
            chunk_start = position - PEEK_BUFFER_SIZE;
        else
            chunk_start = 0;

        size_t chunk_size = (size_t)(position - chunk_start);

        //Move to the beginning of this chunk.
        if (lseek(fd, chunk_start, SEEK_SET) == (off_t)-1)
            return -1;
        ssize_t bytes_read = read(fd, buffer, chunk_size);

        if (bytes_read <= 0)
            return -1;
        ssize_t i;

        for (i = bytes_read - 1; i >= 0; i--)
        {
            if (buffer[i] == '\n')
                return chunk_start + i;
        }

        position = chunk_start;
    }
    return -1;
}
//print everything between two positions of file
void print_file_range(int fd, off_t start, off_t end)
{
    char buffer[PEEK_BUFFER_SIZE];

    off_t position = start;
    off_t remaining = end - start;

    while (remaining > 0)
    {
        size_t to_read;
        if (remaining > PEEK_BUFFER_SIZE)
            to_read = PEEK_BUFFER_SIZE;
        else
            to_read = (size_t)remaining;

        if (lseek(fd, position, SEEK_SET) == (off_t)-1)
            return;
        ssize_t bytes_read = read(fd, buffer, to_read);

        if (bytes_read <= 0)
            return;

        fwrite(buffer, 1, bytes_read, stdout);
        position += bytes_read;
        remaining -= bytes_read;
    }
}
// check whether a line in a file in non-empty
int file_line_is_non_empty(int fd, off_t start, off_t end)
{
    char buffer[PEEK_BUFFER_SIZE];
    off_t position = start;
    off_t remaining = end - start;

    while (remaining > 0)
    {
        size_t to_read;
        if (remaining > PEEK_BUFFER_SIZE)
            to_read = PEEK_BUFFER_SIZE;
        else
            to_read = (size_t)remaining;
        if (lseek(fd, position, SEEK_SET) == (off_t)-1)
            return 0;

        ssize_t bytes_read = read(fd, buffer, to_read);
        if (bytes_read <= 0)
            return 0;

        size_t i;
        for (i = 0; i < (size_t)bytes_read; i++)
        {
            if (buffer[i] != '\n' && buffer[i] != '\r')
                return 1;
        }
        position += bytes_read;
        remaining -= bytes_read;
    }
    return 0;
}
//reverse a file
void reverse_file(int fd, int number_lines)
{
    struct stat st;
    if (fstat(fd, &st) == -1)
        return;
    off_t file_size = st.st_size;

    if (file_size == 0)
    return;
    
    int current_number = count_non_empty_lines(fd);

    if (current_number < 0)
        return;
    off_t end = file_size;
    // if last char is '\n end--
    char last_char;

    if (lseek(fd, file_size - 1, SEEK_SET) == (off_t)-1)
        return;
    if (read(fd, &last_char, 1) != 1)
        return;

    if (last_char == '\n')
        end--;

    while (end > 0)
    {
        off_t previous_newline =find_previous_newline(fd, end);
        off_t start;

        if (previous_newline == (off_t)-1)
            start = 0;
        else
            start = previous_newline + 1;

        off_t print_end = end;

        if (print_end < file_size)
        print_end++;
        
        int non_empty =file_line_is_non_empty(fd, start, end);

        if (number_lines && non_empty)
            printf("%d ", current_number);
        print_file_range(fd, start, print_end);

        if (non_empty)
        current_number--;
        if (start == 0)
            break;
        end = start - 1;
    }
}
//Reverse a non-seekable stream such as stdin or a pipe.
void reverse_stream(FILE *file, int number_lines)
{
    char **lines = NULL;

    size_t count = 0;
    size_t capacity = 0;

    char *line = NULL;
    size_t line_capacity = 0;

    /*
     * Read one complete line at a time.
     */
    while (getline(&line, &line_capacity, file) != -1)
    {
        if (count >= capacity)
        {
            size_t new_capacity;
            if (capacity == 0)
                new_capacity = 16;
            else
                new_capacity = capacity * 2;

            char **new_lines =realloc(lines,new_capacity * sizeof(char *));
            if (new_lines == NULL)
            {
                free(line);
                while (count > 0)
                    free(lines[--count]);
                free(lines);
                return;
            }
            lines = new_lines;
            capacity = new_capacity;
        }

        //store a copy of line
        lines[count] = malloc(strlen(line) + 1);

        if (lines[count] == NULL)
        {
            free(line);
            while (count > 0)
                free(lines[--count]);
            free(lines);
            return;
        }
        strcpy(lines[count], line);
        count++;
    }
    free(line);
    //find number of non-empty lines
    int current_number = 0;
    size_t i;

    for (i = 0; i < count; i++)
    {
        size_t len = strlen(lines[i]);
        if (is_non_empty_line(lines[i], len))
            current_number++;
    }
    for (i = count; i > 0; i--)
    {
        char *current_line = lines[i - 1];
        size_t len = strlen(current_line);
        print_line(current_line,len,number_lines, current_number);
        if (is_non_empty_line(current_line, len))
            current_number--;
    }
    for (i = 0; i < count; i++)
        free(lines[i]);
    free(lines);
}
// process one file name
void process_file(char *filename,int number_lines,int reverse)
{
    //No filename or "-" means stdin.
    if (filename == NULL || strcmp(filename, "-") == 0)
    {
        if (reverse)
            reverse_stream(stdin, number_lines);
        else
            normal_file(stdin, number_lines);
        return;
    }
    struct stat st;
    //if filename exists
    if (stat(filename, &st) == -1)
    {
        fprintf(stderr,"peek: no such file or directory\n");
        return;
    }
    if (S_ISDIR(st.st_mode))
    {
        fprintf(stderr, "peek: is a directory\n");
        return;
    }
    int fd = open(filename, O_RDONLY);
    if (fd == -1)
    {
        fprintf(stderr,"peek: no such file or directory\n");
        return;
    }
    if (reverse && S_ISREG(st.st_mode))
    {
        reverse_file(fd, number_lines);
        close(fd);
        return;
    }
    FILE *file = fdopen(fd, "r");
    if (file == NULL)
    {
        close(fd);
        return;
    }
    normal_file(file, number_lines);
    fclose(file);
}
void peek_command(char **args, int argc)
{
    int number_lines = 0;
    int reverse = 0;
    int file_start = 0;
    while (file_start < argc)
    {
        char *arg = args[file_start];
        if (arg[0] != '-' || strcmp(arg, "-") == 0)
            break;
        int i;
        for (i = 1; arg[i] != '\0'; i++)
        {
            if (arg[i] == 'n')
                number_lines = 1;
            else if (arg[i] == 'r')
                reverse = 1;
        }
        file_start++;
    }
    if (file_start == argc)
    {
        process_file(NULL,number_lines,reverse);
        return;
    }
    while (file_start < argc)
    {
        process_file(args[file_start],number_lines,reverse);
        file_start++;
    }
}