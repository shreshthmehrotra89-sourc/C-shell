#ifndef OUTPUT_REDIRECT_H
#define OUTPUT_REDIRECT_H

int open_output_files(char **files,int *append,int count,int *fds);
int copy_output_to_files(int temp_fd,int *output_fds,int output_count);

#endif