/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) Bao Project and Contributors. All rights reserved.
 */

#ifndef __VCLIC_H__
#define __VCLIC_H__

#include <bao.h>
#include <arch/clic.h>
#include <arch/spinlock.h>
#include <bitmap.h>
#include <emul.h>

struct vclic {
    spinlock_t lock;
    BITMAP_ALLOC(pend, CLIC_MAX_INTERRUPTS);
    BITMAP_ALLOC(enbl, CLIC_MAX_INTERRUPTS);
    uint32_t prio[CLIC_MAX_INTERRUPTS];
    struct emul_mem clic_cliccfg_emul;
    struct emul_mem clic_clicint_emul;
};

// struct vm;
// struct vcpu;
// void vplic_init(struct vm *vm, vaddr_t vplic_base);
// void vplic_inject(struct vcpu *vcpu, irqid_t id);
// void vplic_set_hw(struct vm *vm, irqid_t id);

#endif /* __VCLIC_H__ */
