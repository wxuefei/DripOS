#include "apic.h"
#include "io/ports.h"
#include "mm/vmm.h"
#include "klibc/vector.h"
#include "drivers/serial.h"
#include "io/msr.h"

uint64_t lapic_base;
vector_t cpu_vector;
vector_t iso_vector;
vector_t ioapic_vector;
vector_t nmi_vector;

void parse_madt() {
    madt_t *madt = (madt_t *) search_sdt_header("APIC");

    if (madt) {
        /* Initialize all of our vectors */
        vector_init(&cpu_vector);
        vector_init(&iso_vector);
        vector_init(&ioapic_vector);
        vector_init(&nmi_vector);
        uint64_t bytes_for_entries = madt->header.length - (sizeof(madt->header) + sizeof(madt->local_apic_addr) + sizeof(madt->apic_flags));
        lapic_base = (uint64_t) madt->local_apic_addr;

        sprintf("\n[MADT]: MADT entries:");
        for (uint64_t e = 0; e < bytes_for_entries; e++) {
            uint8_t type = madt->entries[e++];
            uint8_t size = madt->entries[e++];

            if (type == 0) {
                madt_ent0_t *ent = (madt_ent0_t *) &(madt->entries[e]);
                sprintf("\nCPU:\n");
                sprintf("  ACPI ID: %u\n", (uint32_t) ent->acpi_processor_id);
                sprintf("  APIC ID: %u\n", (uint32_t) ent->apic_id);
                sprintf("  Flags: %u", ent->cpu_flags);
                vector_add(&cpu_vector, (void *) ent);
            } else if (type == 1) {
                madt_ent1_t *ent = (madt_ent1_t *) &(madt->entries[e]);
                sprintf("\nIOAPIC:\n");
                sprintf("  ID: %u\n", (uint32_t) ent->ioapic_id);
                sprintf("  Addr: %x\n", (uint32_t) ent->ioapic_addr);
                sprintf("  GSI Base: %x", (uint32_t) ent->gsi_base);
                vector_add(&ioapic_vector, (void *) ent);

                /* Map the IOAPICs into virtual memory */
                if (ent->ioapic_addr / 0x1000 != (ent->ioapic_addr + 0x20) / 0x1000) {
                    vmm_map((void *) (uint64_t) ent->ioapic_addr, 
                        (void *) ((uint64_t) ent->ioapic_addr + KERNEL_VM_OFFSET),
                        2, VMM_PRESENT | VMM_WRITE);
                } else {
                    vmm_map((void *) (uint64_t) ent->ioapic_addr, 
                        (void *) ((uint64_t) ent->ioapic_addr + KERNEL_VM_OFFSET),
                        1, VMM_PRESENT | VMM_WRITE);
                }
            } else if (type == 2) {
                madt_ent2_t *ent = (madt_ent2_t *) &(madt->entries[e]);
                sprintf("\nISO:\n");
                sprintf("  Bus src: %u\n", (uint32_t) ent->bus_src);
                sprintf("  IRQ src: %u\n", (uint32_t) ent->irq_src);
                sprintf("  GSI: %u\n", (uint32_t) ent->gsi);
                sprintf("  Flags: %x", (uint32_t) ent->flags);
                vector_add(&iso_vector, (void *) ent);
            } else if (type == 4) {
                madt_ent4_t *ent = (madt_ent4_t *) &(madt->entries[e]);
                sprintf("\nNMI:\n");
                sprintf("  Processor ID: %u\n", (uint32_t) ent->acpi_processor_id);
                sprintf("  LINT: %u\n", (uint32_t) ent->lint);
                sprintf("  Flags: %u", (uint32_t) ent->flags);
                vector_add(&nmi_vector, (void *) ent);
            } else if (type == 5) {
                madt_ent5_t *ent = (madt_ent5_t *) &(madt->entries[e]);
                sprintf("\nLAPIC override:\n");
                sprintf("  Addr: %lx", ent->local_apic_override);
                lapic_base = ent->local_apic_override;
            }

            e += size - 3;
        }
        sprintf("\nLAPIC addr: %lx", lapic_base);
        sprintf("\nCPUs: %lu", cpu_vector.items_count);
        sprintf("\nISOs: %lu", iso_vector.items_count);
        sprintf("\nIOAPICs: %lu", ioapic_vector.items_count);
        sprintf("\nNMIs: %lu", nmi_vector.items_count);
        /* Handle cross page mapping for the LAPIC */
        if (lapic_base / 0x1000 != (lapic_base + 0x400) / 0x1000) {
            vmm_map((void *) lapic_base, (void *) (lapic_base + KERNEL_VM_OFFSET), 2, VMM_PRESENT | VMM_WRITE);
        } else {
            vmm_map((void *) lapic_base, (void *) (lapic_base + KERNEL_VM_OFFSET), 1, VMM_PRESENT | VMM_WRITE);
        }
        lapic_base += KERNEL_VM_OFFSET; // Offset it into virtual higher half
    }
}

void write_lapic(uint16_t offset, uint32_t data) {
    volatile uint32_t *lapic_register = (volatile uint32_t *) (lapic_base + offset);
    *lapic_register = data;
}

uint32_t read_lapic(uint16_t offset) {
    volatile uint32_t *lapic_register = (volatile uint32_t *) (lapic_base + offset);
    return *lapic_register;
}

uint32_t ioapic_read(void *ioapic_base, uint32_t reg) {
    *(volatile uint32_t *) ((uint64_t) ioapic_base + KERNEL_VM_OFFSET) = reg;
    return *(volatile uint32_t *) ((uint64_t) ioapic_base + 16 + KERNEL_VM_OFFSET);
}

void ioapic_write(void *ioapic_base, uint32_t reg, uint32_t data) {
    *(volatile uint32_t *) ((uint64_t) ioapic_base + KERNEL_VM_OFFSET) = reg;
    *(volatile uint32_t *) ((uint64_t) ioapic_base + 16 + KERNEL_VM_OFFSET) = data;
}

uint8_t get_gsi_max(void *ioapic_base) {
    uint32_t data = ioapic_read(ioapic_base, 1) >> 16; // Read register 1
    uint8_t ret = (uint8_t) data;
    return ret & ~(1<<7);
}

void redirect_gsi(uint8_t irq, uint32_t gsi, uint16_t flags, uint8_t apic) {
    madt_ent1_t *valid_ioapic = (madt_ent1_t *) 0;
    uint32_t gsi_base = 0;

    /* Look for the  correct IOAPIC */
    madt_ent1_t **ioapics = (madt_ent1_t **) vector_items(&ioapic_vector);
    /* Iterate over every IOAPIC */
    for (uint64_t i = 0; i < ioapic_vector.items_count; i++) {
        madt_ent1_t *ioapic = ioapics[i]; // Get the data
        uint32_t gsi_max = (uint32_t) get_gsi_max((void *) (uint64_t) ioapic->ioapic_addr) + ioapic->gsi_base;

        if (ioapic->gsi_base <= gsi && gsi_max >= gsi) {
            /* Found the correct ioapic */
            valid_ioapic = ioapic;
            gsi_base = valid_ioapic->gsi_base;
        }
    }

    if (!valid_ioapic) {
        return;
    }
    uint64_t redirect = (uint64_t) irq + 32; // Offset so that the actual interrupt the cpu gets is past exceptions
    uint32_t reg = ((gsi - gsi_base) * 2) + 16;

    // active low
    if (flags & 1<<1) {
        redirect |= 1<<13;
    }
    // level triggered
    if (flags & 1<<3) {
        redirect|= 1<<15;
    }
    // Add the APIC id of the target processor to handle this GSI
    redirect |= ((uint64_t) apic) << 56;
    ioapic_write((void *) (uint64_t) (valid_ioapic->ioapic_addr), reg + 0, (uint32_t) redirect);
    ioapic_write((void *) (uint64_t) (valid_ioapic->ioapic_addr), reg + 1, (uint32_t) (redirect >> 32));
    sprintf("\n[APIC]: Mapped GSI %u to IRQ %u on APIC %u", gsi, (uint32_t) irq, (uint32_t) apic);
}

void configure_apic() {
    parse_madt();
    /* Remap the old PIC */
    port_outb(0x20, 0x11);
    port_outb(0xA0, 0x11);
    port_outb(0x21, 0x20);
    port_outb(0xA1, 0x28);
    port_outb(0x21, 0x04);
    port_outb(0xA1, 0x02);
    port_outb(0x21, 0x01);
    port_outb(0xA1, 0x01);
    port_outb(0x21, 0x0);
    port_outb(0xA1, 0x0);
    /* Disable the old PIC */
    port_outb(0xA1, 0xFF);
    port_outb(0x21, 0xFF);
    /* Now to configure the local APIC */
    write_msr(APIC_BASE_MSR, (read_msr(APIC_BASE_MSR) | APIC_BASE_MSR_ENABLE) & ~(1<<10)); // Set the LAPIC enable bit
    write_lapic(0xF0, read_lapic(0xF0) | 0x1FF); // Enable spurious interrupts

    uint8_t mapped_irqs[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    madt_ent2_t **isos = (madt_ent2_t **) vector_items(&iso_vector);
    for (uint64_t i = 0; i < iso_vector.items_count; i++) {
        madt_ent2_t *iso = isos[i];
        redirect_gsi(iso->irq_src, iso->gsi, iso->flags, 0);
        if (iso->irq_src < 16) {
            mapped_irqs[iso->irq_src] = 1;
        }
    }

    for (uint8_t i = 0; i < 16; i++) {
        if (!mapped_irqs[i]) {
            redirect_gsi(i, (uint32_t) i, 0, 0);
        }
    }
    
    madt_ent0_t **cpus = (madt_ent0_t **) vector_items(&cpu_vector);
    for (uint64_t i = 0; i < cpu_vector.items_count; i++) {
        sprintf("\n[APIC]: Vector APIC id: %lu", (uint32_t) cpus[i]->apic_id);
    }
}