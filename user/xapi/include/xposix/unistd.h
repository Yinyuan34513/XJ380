#pragma once

#include "../stdint.h"

#include "../krlibc.h"

uint64_t fork();
uint64_t vfork();

int close(int fd);
uint64_t execve(char *filename, char *argv[], char *envp[]);
