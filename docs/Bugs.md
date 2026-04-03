# XJ380 Bug Review

Date: 2026-03-15

Status update: the original review below is now a historical record. The current source tree already includes the following fixes landed on 2026-03-15.

## Resolved In Source Tree

- Fixed kernel stack bootstrap corruption in `create_user_thread()` and `create_message_thread()`. Initial frames are now built on the allocated kernel stack instead of past the `thread_control_block` object.
- Fixed `Process Killer` double-destroy behavior. `check_group()` no longer calls `kill_proc0()` after `kill_proc()` already did process teardown.
- Fixed `Process Killer` traversal and dead-state logic. Child traversal now walks actual queue nodes, and thread dead-state checks no longer use the always-true `!= OUT || != DEATH` condition.
- Fixed `process_setup()` writing through `kernel_group` before `kernel_group = kernel_pcb`.
- Fixed missing `group_index` assignment when creating threads inside a non-null process group.
- Fixed `cmdline` allocation and initialization by using a copied string instead of undersized uninitialized storage.
- Fixed `SYS_GETTID` returning PID instead of TID.
- Fixed `XAPI_REMOVEDIR` falling through into `XAPI_FLUSH_TIME`.
- Hardened page fault reporting so the handler no longer blindly dereferences `get_current_task()` while printing diagnostics.
- Fixed a `Process Killer` bootstrap race where a newly created process with an empty `thread_queue` could be destroyed before its first thread was created.
- Fixed `kill_proc0()` freeing `thread_control_block` objects with `free_frames()` even though they were allocated with `malloc()`.
- Fixed `parse_elf_file()` storing a freed temporary ELF buffer into `pcb->elf_file`, which later caused invalid frees and stale pointers during process teardown.
- Fixed `create_user_process_from_file()` cloning the kernel page directory with `all_copy = true`; it now uses the same partial-clone strategy as normal process creation paths.
- Fixed page-table clone recursion so non-present entries are skipped and newly allocated page tables are cleared before use.
- Fixed thread creation helpers leaving the scheduler disabled and interrupts closed after returning. `create_kernel_thread()`, `create_user_thread()`, and `create_message_thread()` now restore the previous runtime state.
- Fixed `create_user_thread()` and `create_message_thread()` failure paths leaking `create_thread_lock`.
- Fixed `process_execve()` failure rollback so it restores the previous page directory and runtime state instead of returning with mismatched process state.
- Fixed stale `cmdline` and `envp` state leaking across `execve()` and process creation updates.
- Hardened user-task stdio bootstrap so a missing `/dev/stdio` no longer turns into an immediate null dereference.
- Fixed `parse_elf_file()` cleanup so VFS references and temporary ELF buffers are released on both success and failure paths.

## Remaining Validation Gap

- A full end-to-end runtime verification is still needed. The latest `serial.log` now reaches first user-process creation, but runtime is not yet confirmed past the first `fmanager` thread bring-up and user-mode handoff.

## Latest Runtime Status

The current `serial.log` in this tree is newer than the historical log used by the original review below.

- The old `Process Killer` page-fault sequence is no longer present in the current boot log.
- The current boot log reaches desktop bring-up, `components flusher` startup, and first user-process creation.
- The latest observed stopping point is after `Creating User Thread. Thread Name: fmanager` and `CWD: /system`.
- Inference: the highest-value next debugging target has moved from process-killer teardown into early user-process startup, especially scheduler handoff, first user-mode transition, and the `fmanager` launch path.

Relevant current boot log lines:

- [serial.log:121](/Z:/home/lu/XJ380/serial.log:121): `Process Killer is ready`
- [serial.log:123](/Z:/home/lu/XJ380/serial.log:123): `desktop flusher is running`
- [serial.log:137](/Z:/home/lu/XJ380/serial.log:137): `components flusher is running`
- [serial.log:138](/Z:/home/lu/XJ380/serial.log:138): `Creating User Process. Process Name: fmanager`
- [serial.log:139](/Z:/home/lu/XJ380/serial.log:139): `Creating User Thread. Thread Name: fmanager`
- [serial.log:140](/Z:/home/lu/XJ380/serial.log:140): `CWD: /system`

This document records the bugs found during a static review of the current tree and the existing boot log in `serial.log`.

## Scope

- Reviewed boot and runtime evidence from [serial.log](/Z:/home/lu/XJ380/serial.log)
- Reviewed process, thread, scheduler, page fault, and syscall code
- The historical review below was originally written before the fixes listed above were applied
- Full end-to-end runtime validation is still incomplete in the current environment

## Highest Priority Bugs

### 1. User and message thread bootstrap corrupt kernel memory

Severity: critical

Files:
- [kernel/task/pcb.cpp:738](/Z:/home/lu/XJ380/kernel/task/pcb.cpp:738)
- [kernel/task/pcb.cpp:812](/Z:/home/lu/XJ380/kernel/task/pcb.cpp:812)

Details:
- `create_user_thread()` and `create_message_thread()` compute an initial stack pointer with `(uint64_t)new_task + KERNEL_STACK_SIZE`.
- `new_task` is only a `thread_control_block` allocation, not a kernel stack region.
- The code then writes return frames via `*(--stack_top)`, which writes far past the TCB allocation.
- The actual allocated kernel stack stored in `new_task->kernel_stack` is not used for this bootstrap frame.

Impact:
- Deterministic heap corruption
- Broken task bootstrap state
- Likely random crashes later in scheduling or fault handling

Recommended fix:
- Build the initial stack frame on the allocated kernel stack, not relative to the TCB object.
- Set `stack_top` from the allocated stack base before writing bootstrap return addresses.

### 2. Process killer can destroy the same process twice

Severity: critical

Files:
- [kernel/task/killer.cpp:43](/Z:/home/lu/XJ380/kernel/task/killer.cpp:43)
- [kernel/task/pcb.cpp:67](/Z:/home/lu/XJ380/kernel/task/pcb.cpp:67)
- [kernel/task/pcb.cpp:118](/Z:/home/lu/XJ380/kernel/task/pcb.cpp:118)

Details:
- `check_group()` calls `kill_proc(group, 0, false)`.
- In the non-zombie path, `kill_proc()` already calls `kill_proc0(pcb)`.
- `check_group()` then immediately calls `kill_proc0(group)` again.

Impact:
- Double free
- Use-after-free
- Queue corruption
- High probability of the observed `Process Killer` page fault

Evidence:
- [serial.log:121](/Z:/home/lu/XJ380/serial.log:121) shows `Process Killer is ready`
- [serial.log:125](/Z:/home/lu/XJ380/serial.log:125) shows a page fault in thread `Process Killer`

Recommended fix:
- Remove the second `kill_proc0(group)` call from `check_group()`.
- Re-review all caller/callee ownership around process teardown.

### 3. Process killer liveness check is logically wrong

Severity: critical

Files:
- [kernel/task/killer.cpp:17](/Z:/home/lu/XJ380/kernel/task/killer.cpp:17)
- [kernel/task/killer.cpp:32](/Z:/home/lu/XJ380/kernel/task/killer.cpp:32)

Details:
- Child traversal uses `group->child_index` as loop count instead of `group->child_pcb->size`.
- `child_index` is an insertion index, not the current queue length.
- The thread state check uses `thread->status != OUT || thread->status != DEATH`, which is always true.

Impact:
- Incorrect dead-process detection
- Possible null dereference during child traversal
- Unreliable reaper behavior

Recommended fix:
- Iterate child and thread queues by actual queue contents, not historical index values.
- Replace the status condition with `&&` or a clearer positive check.

## High Priority Bugs

### 4. `process_setup()` writes through `kernel_group` before assigning it

Severity: high

File:
- [kernel/task/pcb.cpp:938](/Z:/home/lu/XJ380/kernel/task/pcb.cpp:938)

Details:
- The code assigns `kernel_group->virt_queue` and `kernel_group->ipc_queue` before `kernel_group = kernel_pcb`.
- At that point `kernel_group` is still null in this source tree.

Impact:
- Startup-time null pointer write

Note:
- The current `serial.log` suggests the running image may not exactly match this source tree, because the logged boot gets past process setup.

Recommended fix:
- Initialize `kernel_pcb->virt_queue` and `kernel_pcb->ipc_queue`.
- Assign `kernel_group = kernel_pcb` before using the global.

### 5. Some threads are enqueued without saving `group_index`

Severity: high

Files:
- [kernel/task/pcb.cpp:692](/Z:/home/lu/XJ380/kernel/task/pcb.cpp:692)
- [kernel/task/pcb.cpp:763](/Z:/home/lu/XJ380/kernel/task/pcb.cpp:763)
- [kernel/task/pcb.cpp:837](/Z:/home/lu/XJ380/kernel/task/pcb.cpp:837)
- [kernel/task/pcb.cpp:128](/Z:/home/lu/XJ380/kernel/task/pcb.cpp:128)

Details:
- In several `pcb != NULL` thread creation paths, `queue_enqueue(pcb->thread_queue, new_task)` is called but the returned index is discarded.
- Destruction later relies on `thread->group_index` for `queue_remove_at()`.

Impact:
- Removing the wrong queue node
- Failure to remove a dead thread
- Queue corruption during teardown

Recommended fix:
- Always save the return value of `queue_enqueue()` into `new_task->group_index`.
- Audit existing callers for the same pattern.

### 6. `cmdline` allocation is undersized and never initialized

Severity: high

Files:
- [kernel/task/pcb.cpp:916](/Z:/home/lu/XJ380/kernel/task/pcb.cpp:916)
- [kernel/task/pcb.cpp:212](/Z:/home/lu/XJ380/kernel/task/pcb.cpp:212)

Details:
- `create_process_group()` uses `malloc(strlen(cmdline))`, which omits space for the null terminator.
- It also never copies the actual string into `pcb->cmdline`.

Impact:
- Uninitialized string reads
- Out-of-bounds string operations
- Corruption when later code calls `strdup(task->parent_group->cmdline)`

Recommended fix:
- Replace with `strdup(cmdline)` or `malloc(strlen(cmdline) + 1)` followed by a copy.

## Medium Priority Bugs

### 7. `SYS_GETTID` returns PID instead of TID

Severity: medium

File:
- [kernel/syscall/syscall.cpp:139](/Z:/home/lu/XJ380/kernel/syscall/syscall.cpp:139)

Details:
- `SYS_GETTID` returns `get_current_task()->parent_group->pid`.
- It should return the current thread ID.

Impact:
- Incorrect userspace behavior
- Broken thread-aware code

Recommended fix:
- Return `get_current_task()->tid`.

### 8. `XAPI_REMOVEDIR` falls through into `XAPI_FLUSH_TIME`

Severity: medium

File:
- [kernel/syscall/syscall.cpp:256](/Z:/home/lu/XJ380/kernel/syscall/syscall.cpp:256)

Details:
- `case XAPI_REMOVEDIR` has no `break`.
- It falls through into `case XAPI_FLUSH_TIME`.

Impact:
- Removing a directory also triggers time flush logic
- Side effects are surprising and hard to trace

Recommended fix:
- Add `break` after `sys_rmdir(...)`.

### 9. Page fault handler can fault again while reporting

Severity: medium

File:
- [kernel/memory/page.cpp:66](/Z:/home/lu/XJ380/kernel/memory/page.cpp:66)

Details:
- The error path unconditionally dereferences `get_current_task()` for logging.
- If the fault happened before task state was valid, or if task state is already corrupted, the handler itself can fault again.

Impact:
- Lost original crash context
- Nested fault or hang during exception handling

Recommended fix:
- Guard all logging dereferences with null checks.
- Log a reduced diagnostic path when task state is unavailable.

## Existing Runtime Evidence

Relevant boot log sequence:

- [serial.log:121](/Z:/home/lu/XJ380/serial.log:121): `Process Killer is ready`
- [serial.log:123](/Z:/home/lu/XJ380/serial.log:123): `desktop flusher is running`
- [serial.log:124](/Z:/home/lu/XJ380/serial.log:124): `Initializing Sheet Manager...`
- [serial.log:125](/Z:/home/lu/XJ380/serial.log:125): page fault begins
- [serial.log:127](/Z:/home/lu/XJ380/serial.log:127): fault thread is `Process Killer`

This strongly suggests the current boot-time failure is at least partially tied to process/thread teardown or memory corruption in task management.

## Suggested Fix Order

1. Fix thread bootstrap stack corruption in `create_user_thread()` and `create_message_thread()`
2. Fix `Process Killer` double-destroy path
3. Fix `Process Killer` traversal and dead-state logic
4. Fix `process_setup()` null global write
5. Fix missing `group_index` assignment
6. Fix `cmdline` allocation and initialization
7. Fix syscall correctness issues
8. Harden the page fault handler logging path

## Notes For The Next Agent

- Do not assume the current source tree exactly matches the image that produced `serial.log`.
- After the first four fixes, rebuild and reproduce before making broad refactors.
- The task, queue, and teardown code should be treated as memory-corruption-sensitive. Make minimal, well-scoped changes first.
