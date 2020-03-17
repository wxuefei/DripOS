#include <stdint.h>
#include <stddef.h>
#include "mm/pmm.h"
#include "fs/vfs/vfs.h"
#include "fs/devfs/devfs.h"
#include "fs/fd.h"
#include "sys/apic.h"
#include "sys/int/isr.h"
#include "klibc/stdlib.h"
#include "drivers/pit.h"
#include "drivers/serial.h"
#include "drivers/vesa.h"
#include "drivers/tty/tty.h"
#include "drivers/pci.h"
#include "multiboot.h"

/* Testing includes */
#include "proc/scheduler.h"
#include "io/msr.h"

#include "klibc/string.h"
#include "sys/smp.h"

#include "fs/filesystems/echfs.h"

#define TODO_LIST_SIZE 7
char *todo_list[TODO_LIST_SIZE] = {"Better syscall error handling", "Filesystem driver", "ELF Loading", "userspace libc", "minor: Sync TLB across CPUs", "minor: Add MMIO PCI", "minor: Retry AHCI commands"};

void kernel_task() {
    kprintf("\n[DripOS] Loading VFS");
    vfs_init(); // Setup VFS
    devfs_init();
    kprintf("\n[DripOS] Loaded VFS");
    vfs_ops_t ops = dummy_ops;
    ops.open = devfs_open;
    ops.close = devfs_close;
    ops.write = tty_dev_write;
    ops.read = tty_dev_read;
    register_device("tty1", ops, (void *) 0);

    pci_init(); // Setup PCI devices and their drivers

    kprintf("\n[DripOS Kernel] Bultin todo list:");
    for (uint64_t i = 0; i < TODO_LIST_SIZE; i++) {
        kprintf("\n  %s", todo_list[i]);
    }

    echfs_test("/dev/satadeva");

    sprintf("\ndone kernel work");

    while (1) { asm volatile("hlt"); }
}

// Kernel main function, execution starts here :D
void kmain(multiboot_info_t *mboot_dat) {
    init_serial(COM1);

    if (mboot_dat) {
        sprintf("[DripOS]: Setting up memory bitmaps");
        pmm_memory_setup(mboot_dat);
    }

    sprintf("\n[DripOS] Initializing TTY");
    init_vesa(mboot_dat);
    tty_init(&base_tty, 8, 8);

    sprintf("\n[DripOS] Configuring LAPICs and IOAPIC routing");
    configure_apic();

    new_cpu_locals(); // Setup CPU locals for our CPU
    load_tss();
    set_panic_stack((uint64_t) kmalloc(0x1000) + 0x1000);
    set_kernel_stack((uint64_t) kmalloc(0x1000) + 0x1000);
    sprintf("\n[DripOS] Set kernel stacks");
    scheduler_init_bsp();

    sprintf("\n[DripOS] Registering interrupts and setting interrupt flag");
    configure_idt();
    sprintf("\n[DripOS] Setting timer speed to 1000 hz");
    set_pit_freq();


    new_kernel_process("Kernel process", kernel_task);

    launch_cpus();
    tty_clear(&base_tty);

    scheduler_enabled = 1;

    while (1) {
        asm volatile("hlt");
    }
}