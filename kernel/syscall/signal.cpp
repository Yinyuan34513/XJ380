#include <stdint.h>
#include <cpu/msr.h>
#include <errno.h>
#include <fs/fs_syscall.h>
#include <pctable/gdt.h>
#include <pctable/idt.h>
#include <proto.hpp>
#include <syscall/syscall.h>

void do_signal(uint64_t msg_type, uint64_t data1, uint64_t data2)
{

}
