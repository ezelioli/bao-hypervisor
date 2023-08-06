#include <arch/sbi.h>
#include <arch/clic.h>
#include <printk.h>

#define VCLIC

#ifdef VCLIC

void vclic_delegate_irq(uint64_t irq)
{
    clic_intcfg_t cfg = clic_read_clicint(irq);
    if (cfg.attr.mode == 0) { // CLIC_INT_ATTR_MODE_M   
        // Forward SBI call to M-mode
        // printk("[BAO] Interrupt in M mode, issuing call to OpenSBI\r\n");
        ret = sbi_clic_delegate(irq);
        if (ret.error != SBI_SUCCESS) {
            printk("[BAO] SBI ecall to sbi_clic_delegate failed (%d)\r\n", ret.error);
            return;
        }
    }
    cfg = (clic_intcfg_t) {
        .ip        = CLIC_IP_CLEAR,
        .ie        = CLIC_IE_MASK,
        .attr.shv  = CLIC_INT_ATTR_SHV_OFF,
        .attr.trig = CLIC_INT_ATTR_TRIG_EDGE,
        .attr.mode = CLIC_INT_ATTR_MODE_S,
        .ctl       = 0x00
    };
    // printk("[BAO] Delegated irq %d to VM %d\r\n", irq, cpu()->vcpu->id);
    clic_intvcfg_t vcfg = (clic_intvcfg_t) {
        .v    = CLIC_V_ENABLE,
        .vsid = cpu()->vcpu->id+1               // TODO: set VSID according to VM issuing call
    };
    clic_set_clicint(irq, cfg);
    clic_set_clicintv(irq, vcfg);
}

#else

void vclic_delegate_irq(uint64_t irq)
{
    
}

#endif // VCLIC