#pragma once

#include "../stdint.h"

uint64_t fork();
uint64_t vfork();

uint64_t execve(const char *filename, char *const argv[], char *const envp[]);
