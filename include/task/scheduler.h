#pragma once
#include <stdint.h>

extern const uint64_t TIME_SLICE;
extern const uint64_t MIN_SLICE;

// 调度器函数声明
void scheduler_yield();
void scheduler_tick();

// tcb_t select_next_task_safe();