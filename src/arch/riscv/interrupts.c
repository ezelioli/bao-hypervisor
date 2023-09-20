/**
 * SPDX-License-Identifier: Apache-2.0 
 * Copyright (c) Bao Project and Contributors. All rights reserved.
 */

#include <bao.h>
#include <interrupts.h>

#include <arch/plic.h>
#include <arch/clic.h>
#include <printk.h>
#include <arch/sbi.h>
#include <cpu.h>
#include <mem.h>
#include <platform.h>
#include <vm.h>
#include <arch/csrs.h>
#include <fences.h>

void interrupts_arch_init()
{
    if (cpu()->id == CPU_MASTER) {
        plic_global = (void*) mem_alloc_map_dev(&cpu()->as, SEC_HYP_GLOBAL, INVALID_VA, 
            platform.arch.plic_base, NUM_PAGES(sizeof(struct plic_global_hw)));

        plic_hart = (void*) mem_alloc_map_dev(&cpu()->as, SEC_HYP_GLOBAL, INVALID_VA, 
            platform.arch.plic_base + PLIC_CLAIMCMPLT_OFF,
            NUM_PAGES(sizeof(struct plic_hart_hw)*PLIC_PLAT_CNTXT_NUM));

        if((void *) platform.arch.clic_base != NULL) {
            printk("[BAO] platform has CLIC\r\n");
            clic_global = (void*) mem_alloc_map_dev(&cpu()->as, SEC_HYP_GLOBAL, INVALID_VA, 
                platform.arch.clic_base, NUM_PAGES(0x3FFFFFF));
            // printk("[BAO] CLIC mapped to VA 0x%x\r\n", (uint64_t)clic_global);
        }

        fence_sync();

        if((void *) platform.arch.clic_base != NULL) {
            clic_init();
            /* Ask firmware to switch to CLIC mode */
            sbi_clic_enable();
            // printk("[BAO] CLIC implemented interrupts: %d\n\r", CLIC_IMPL_INTERRUPTS);
        }

        plic_init();
    }

    /* Wait for master hart to finish plic initialization */
    cpu_sync_barrier(&cpu_glb_sync);

    plic_cpu_init();

    /**
     * Enable external interrupts.
     */
    CSRS(sie, SIE_SEIE);
    /* If CLIC is present, enable Supervisor external interrupts (from PLIC) and timer interrupts */
    interrupts_arch_enable(TIMR_INT_ID, true);
    // interrupts_arch_enable(CLIC_MAX_INTERRUPTS + 1, true); // UART
    // interrupts_arch_enable(CLIC_MAX_INTERRUPTS + 5, true);
    if ((void *) platform.arch.clic_base != NULL) {
        interrupts_arch_enable(TIMR_INT_ID, true);
        interrupts_arch_enable(EXTR_INT_ID, true);
    }
}

void interrupts_arch_ipi_send(cpuid_t target_cpu, irqid_t ipi_id)
{
    sbi_send_ipi(1ULL << target_cpu, 0);
}

void interrupts_arch_cpu_enable(bool en)
{
    if (en) {
        CSRS(sstatus, SSTATUS_SIE_BIT);
    } else {
        CSRC(sstatus, SSTATUS_SIE_BIT);
    }
}

void interrupts_arch_enable(irqid_t int_id, bool en)
{

    // printk("[BAO] Enable irq %d\r\n", int_id);

    if ((void *) platform.arch.clic_base != NULL) {
        if (int_id < CLIC_MAX_INTERRUPTS){
            if (int_id < CLIC_IMPL_INTERRUPTS) {
                // printk("[BAO] Enabling CLIC irq %d\r\n", int_id);
                // set irq priority and enable
                clic_set_enable(int_id, en);
                clic_set_priority(int_id, 0xFE);
            } else {
                printk("[BAO] Ignoring enabling irq %d in CLIC mode\r\n", int_id);
            }
        } else {
            // printk("[BAO] Enabling PLIC irq %d\r\n", int_id - CLIC_MAX_INTERRUPTS);
            /* PLIC interrupts are mapped to [CLIC_MAX_INTERRUPTS, CLIC_MAX_INTERRUPTS + PLIC_MAX_INTERRUPTS] */
            plic_set_enbl(cpu()->arch.plic_cntxt, int_id - CLIC_MAX_INTERRUPTS, en);
            plic_set_prio(int_id - CLIC_MAX_INTERRUPTS, 0xFE);
        }
    } else {
        if (int_id == SOFT_INT_ID) {
            if (en)
                CSRS(sie, SIE_SSIE);
            else
                CSRC(sie, SIE_SSIE);
        } else if (int_id == TIMR_INT_ID) {
            if (en)
                CSRS(sie, SIE_STIE);
            else
                CSRC(sie, SIE_STIE);
        } else {
            /* PLIC interrupts are mapped to [CLIC_MAX_INTERRUPTS, CLIC_MAX_INTERRUPTS + PLIC_MAX_INTERRUPTS] */
            plic_set_enbl(cpu()->arch.plic_cntxt, int_id - CLIC_MAX_INTERRUPTS, en);
            plic_set_prio(int_id - CLIC_MAX_INTERRUPTS, 0xFE);
        }
    }
}

void interrupts_arch_clic_handle(unsigned long scause)
{
    unsigned long int_id = scause & 0x0FFF;
    // printk("[BAO] Irq %d\r\n");
    switch(int_id) {
        case EXTR_INT_ID:
            // printk("[BAO] External interrupt\r\n");
            plic_handle();
            break;
        default:
            // printk("[BAO] Generic interrupt\r\n");
            interrupts_handle(int_id);
            break;
    }
}

void interrupts_arch_handle()
{
    unsigned long _scause = CSRR(scause);

    // printk("[BAO] IRQ received: scause = 0x%lx\r\n", _scause);

    if (is_clic_mode()) {
        interrupts_arch_clic_handle(_scause);
    } else {
        switch (_scause) {
            case SCAUSE_CODE_SSI:
                interrupts_handle(SOFT_INT_ID);
                CSRC(sip, SIP_SSIP);
                break;
            case SCAUSE_CODE_STI:
                interrupts_handle(TIMR_INT_ID);
                /**
                 * Clearing the timer pending bit actually has no
                 * effect. We should call sbi_set_timer(-1), but at
                 * the moment this is having no effect on opensbi/qemu.
                 */
                // CSRC(sip, SIP_STIP);
                // sbi_set_timer(-1);
                break;
            case SCAUSE_CODE_SEI:
                plic_handle();
                break;
            default:
                WARNING("[BAO] Ignoring unknown interrupt (scause=%lx)\r\n", _scause);
                break;
        }
    }
}

bool interrupts_arch_check(irqid_t int_id)
{
    if (is_clic_mode()) {
        return clic_get_pend(int_id);
    } else {
        if (int_id == SOFT_INT_ID) {
            return CSRR(sip) & SIP_SSIP;
        } else if (int_id == TIMR_INT_ID) {
            return CSRR(sip) & SIP_STIP;
        } else {
            /* PLIC interrupts are mapped to [CLIC_MAX_INTERRUPTS, CLIC_MAX_INTERRUPTS + PLIC_MAX_INTERRUPTS] */
            return plic_get_pend(int_id - CLIC_MAX_INTERRUPTS);
        }
    }
}

void interrupts_arch_clear(irqid_t int_id)
{
    if (int_id == SOFT_INT_ID) {
        CSRC(sip, SIP_SSIP);
    } else {
        /**
         * It is not actually possible to clear timer
         * or external interrupt pending bits by software.
         */
        WARNING("trying to clear timer or external interrupt");
    }
}

inline bool interrupts_arch_conflict(bitmap_t* interrupt_bitmap, irqid_t int_id)
{
    return bitmap_get(interrupt_bitmap, int_id);
}

void interrupts_arch_vm_assign(struct vm *vm, irqid_t id)
{
    if ((void *) platform.arch.clic_base != NULL) {
        if (id < CLIC_MAX_INTERRUPTS)
            clic_delegate_to_vm(id, vm->id);
        else
            vplic_set_hw(vm, id - CLIC_MAX_INTERRUPTS);    
    } else {
        vplic_set_hw(vm, id - CLIC_MAX_INTERRUPTS);
    }
}
