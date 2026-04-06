#pragma once

#include "../stdint.h"

#include "../krlibc.h"

#ifdef __cplusplus
extern "C" {
#endif

uint64_t fork();
uint64_t vfork();

int close(int fd);
uint64_t execve(char *filename, char *argv[], char *envp[]);
int      chdir(char *path);
char    *getcwd(char *buffer, uint64_t length);

#ifdef __cplusplus
}
#endif
