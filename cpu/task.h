#ifndef TASK_H
#define TASK_H
#include <stdint.h>
#include "isr.h"
#include "../drivers/vesa.h"
#include "../libc/mem.h"
#include "../drivers/screen.h"
#include <string.h>
#include "../kernel/kernel.h"
#include "../kernel/terminal.h"
#include "../cpu/isr.h"
#include "../drivers/serial.h"
#include "../drivers/sound.h"
#include <debug.h>
#include "timer.h"

/* Task states */
#define RUNNING 0
#define BLOCKED 1
#define SLEEPING 2
#define IRQ_WAIT 3
/* Priorities */
#define HIGH 3
#define NORMAL 2
#define LOW 1
#define VERY_LOW 0
/* Cursor max (cursor pos wont be updated for tasks with a cursor_pos greater than this) */
#define CURSOR_MAX 4000000000
extern void initTasking();
 
typedef struct {
    uint64_t rax, rbx, rcx, rdx, rsi, rdi, rsp, rbp, r8, r9, r10, r11, r12, r13, r14, r15, rip, rflags, cr3;
} __attribute__((__packed__)) Registers;
 
typedef struct Task {
    Registers regs;
    uint8_t *start_esp; // The original allocated memory for the stack of this process
    uint32_t ticks_cpu_time;
    struct Task *next;
    uint8_t priority;
    uint32_t pid; // Process id of the task
    uint8_t state; // The state the task is in
    uint32_t waiting; // If state is SLEEPING, this is the tick to restart the task, if the state is IRQ_WAIT, this is the IRQ that it is waiting for
    uint32_t cursor_pos;
    vesa_buffer_t buffer;
    char name[21];
    char *scancode_buffer;
    uint16_t scancode_buffer_pos;
    uint32_t slept_ticks;
    uint32_t since_last_task;
} Task;

void initTasking();
extern uint32_t createTask(Task *task, void (*main)(), char *task_name);
extern int32_t kill_task(uint32_t pid); // 
extern void yield(); // Yield, will be optional
extern void switchTask(); // The function which actually switches
extern void print_tasks(); // Print all the tasks
void pick_task(); // Pick a task to be scheduled
void schedule_task(registers_t *r); // Load the task into a registers_t
void set_focused_task(Task *new_focus);
Task *get_focused_task();
Task *get_task_from_pid(uint32_t pid);

Task *running_task;
Task *global_old_task;
void store_global(uint32_t f, registers_t *ok);
void irq_schedule();
uint32_t global_esp;
uint32_t global_esp_old;
extern uint32_t task_size;
uint32_t oof;
uint32_t eax;
uint32_t eip;
uint32_t esp;
registers_t *temp_data1;
Task main_task;
#endif