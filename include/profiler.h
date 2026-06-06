#pragma once

#include <stddef.h>
#include <stdint.h>

void profiler_syscall_exit(uint64_t nr, uint64_t elapsed_ns, int64_t ret);
void profiler_block_io(bool write, int drive, uint64_t offset, uint64_t length, uint64_t elapsed_ns, int64_t ret);

