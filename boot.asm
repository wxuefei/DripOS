[bits 32]
ALIGN_MULTIBOOT equ 1<<0
MEMINFO equ 1<<1
VIDEO_MODE equ 0x00000004
FLAGS equ ALIGN_MULTIBOOT | MEMINFO | VIDEO_MODE
MAGIC equ 0x1BADB002
CHECKSUM equ -(MAGIC + FLAGS)
VIDMODE equ 0
WIDTH equ 720
HEIGHT equ 480
DEPTH equ 32

section .multiboot
align 4
dd MAGIC
dd FLAGS
dd CHECKSUM
dd 0
dd 0
dd 0
dd 0
dd 0
dd VIDMODE
dd WIDTH
dd HEIGHT
dd DEPTH

section .data
 
GDT64:                           ; Global Descriptor Table (64-bit).
    .Null: equ $ - GDT64         ; The null descriptor.
    dw 0xFFFF                    ; Limit (low).
    dw 0                         ; Base (low).
    db 0                         ; Base (middle)
    db 0                         ; Access.
    db 1                         ; Granularity.
    db 0                         ; Base (high).
    .Code: equ $ - GDT64         ; The code descriptor.
    dw 0                         ; Limit (low).
    dw 0                         ; Base (low).
    db 0                         ; Base (middle)
    db 10011010b                 ; Access (exec/read).
    db 10101111b                 ; Granularity, 64 bits flag, limit19:16.
    db 0                         ; Base (high).
    .Data: equ $ - GDT64         ; The data descriptor.
    dw 0                         ; Limit (low).
    dw 0                         ; Base (low).
    db 0                         ; Base (middle)
    db 10010010b                 ; Access (read/write).
    db 00000000b                 ; Granularity.
    db 0                         ; Base (high).
    .Pointer:                    ; The GDT-pointer.
    dw $ - GDT64 - 1             ; Limit.
    dq GDT64                     ; Base.

section .bss
align 16
stack_bottom:
resb 65536
stack_top:

global multiboot_header_pointer
multiboot_header_pointer:
resb 4

section .text
extern long_mode_on
global _start
_start:
    mov esp, stack_top ; Set the stack up
    push ebx
    ; Paging
    mov edi, 0x1000 ; The Page map level 4 table
    mov cr3, edi ; Point cr3 to the PML4T
    xor eax, eax       ; Nullify the A-register.
    mov ecx, 4096      ; Set the C-register to 4096.
    rep stosd          ; Clear the memory.
    mov edi, cr3       ; Set the destination index to control register 3.
    ; Set the tables to point to the correct places, with the first two bits set
    ; to indicate that the page is present and that it is readable and writeable
    mov DWORD [edi], 0x2003 ; Point to the Page directory pointer table
    add edi, 0x1000 ; EDI is now pointing to where the Page directory pointer table is
    mov DWORD [edi], 0x3003 ; Point to the Page directory
    add edi, 0x1000 ; EDI is now pointing to where the Page directory is
    mov DWORD [edi], 0x4003 ; Point to the page table
    add edi, 0x1000 ; EDI is now pointing at the Page table

    mov ebx, 0x3 ; Set the first two bits for every page in the table
    mov ecx, 512 ; Loop counter
.fill_table_loop:
    mov DWORD [edi], ebx ; Point the table to the address set in ebx
    add ebx, 0x1000 ; Add 0x1000 to ebx, for the next page
    add edi, 8 ; Add 8 to the destination pointer, because these paging tables are for long mode
    loop .fill_table_loop ; Loop over this label, using ecx as a counter
    mov eax, cr4                 ; Set the A-register to control register 4.
    or eax, 1 << 5               ; Set the PAE-bit, which is the 6th bit (bit 5).
    mov cr4, eax                 ; Set control register 4 to the A-register.
    ; Switch to long mode
    mov ecx, 0xC0000080          ; Set the C-register to 0xC0000080, which is the EFER MSR.
    rdmsr                        ; Read from the model-specific register.
    or eax, 1 << 8               ; Set the LM-bit which is the 9th bit (bit 8).
    wrmsr                        ; Write to the model-specific register.
    mov eax, cr0                 ; Set the A-register to control register 0.
    or eax, 1 << 31              ; Set the PG-bit, which is the 32nd bit (bit 31).
    mov cr0, eax                 ; Set control register 0 to the A-register.
    pop ebx

    ; Set up GDT

    lgdt [GDT64.Pointer]         ; Load the 64-bit global descriptor table.

    mov ax, GDT64.Data            ; Set the A-register to the data descriptor.
    mov ds, ax                    ; Set the data segment to the A-register.
    mov es, ax                    ; Set the extra segment to the A-register.
    mov fs, ax                    ; Set the F-segment to the A-register.
    mov gs, ax                    ; Set the G-segment to the A-register.
    mov ss, ax                    ; Set the stack segment to the A-register.
    jmp GDT64.Code:long_mode_on   ; Set the code segment and enter 64-bit long mode.