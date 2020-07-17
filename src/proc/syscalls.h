#ifndef SYSCALLS_H
#define SYSCALLS_H
#include <stdint.h>
#include "proc/scheduler.h"

/* All the useful syscalls */
void syscall_read(syscall_reg_t *r);                 // 0
void syscall_write(syscall_reg_t *r);                // 1
void syscall_open(syscall_reg_t *r);                 // 2
void syscall_close(syscall_reg_t *r);                // 3
void syscall_getpid(syscall_reg_t *r);               // 5
void syscall_seek(syscall_reg_t *r);                 // 8
void syscall_mmap(syscall_reg_t *r);                 // 9
void syscall_munmap(syscall_reg_t *r);               // 11
void syscall_exit(syscall_reg_t *r);                 // 12
void syscall_getppid(syscall_reg_t *r);              // 14
void syscall_yield(syscall_reg_t *r);                // 24
void syscall_nanosleep(syscall_reg_t *r);            // 35
void syscall_sprint(syscall_reg_t *r);               // 50
void syscall_fork(syscall_reg_t *r);                 // 57
void syscall_execve(syscall_reg_t *r);               // 59
void syscall_ipc_read(syscall_reg_t *r);             // 60
void syscall_ipc_write(syscall_reg_t *r);            // 61
void syscall_core_count(syscall_reg_t *r);           // 70
void syscall_get_core_performance(syscall_reg_t *r); // 71
void syscall_ms_sleep(syscall_reg_t *r);             // 72
void syscall_set_fs(syscall_reg_t *r);               // 300

/* Meme syscalls (very temporary) */
void syscall_print_num(syscall_reg_t *r);

/* Empty syscall (stub for an unimplemented syscall) */
void syscall_empty(syscall_reg_t *r);

void init_syscalls();

extern uint64_t memcpy_from_userspace(void *dst, void *src, uint64_t byte_count);
extern uint64_t strcpy_from_userspace(char *dst, char *src);
extern uint64_t strlen_from_userspace(char *dst);

#endif