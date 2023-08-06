#include <arch/clic.h>
#include <stdint.h>
#include <interrupts.h>
#include <bao.h>
#include <cpu.h>
#include <arch/sbi.h>

/* CLIC registers memory map (for S-mode configuration) */

#define CLIC_CLICCFG_OFFSET          0x0008000ul
#define CLIC_CLICINT_OFFSET(i)      (0x0009000ul + 4*(i))
#define CLIC_CLICINTV_OFFSET(i)     (0x000d000ul + 4*(i))

/*
** Global variables used to keep general information about the CLIC.
**
** clic_global          : Pointer that holds the virtual memory address that the CLIC is mapped to.
**                        It is set during early initialization in the interrupts_arch_init() function (as for the PLIC).
** CLIC_IMPL_INTERRUPTS : Number of source interrupts implemented by the CLIC (detected during CLIC initialization).
**
*/
size_t CLIC_IMPL_INTERRUPTS;
volatile void *clic_global;
// bool clic_enabled;

static inline void write32(uint32_t val, volatile void *addr)
{
    asm volatile("sw %0, 0(%1)" : : "r"(val), "r"(addr));
}

static inline uint32_t read32(const volatile void *addr)
{
    uint32_t val;
    asm volatile("lw %0, 0(%1)" : "=r"(val) : "r"(addr));
    return val;
}

/* Sets the CLICCFG configuration register  */
void clic_set_mcfg(uint8_t nmbits, uint8_t mnlbits, uint8_t snlbits, uint8_t unlbits)
{
     write32(MAKE_CLICCFG(nmbits, mnlbits, snlbits, unlbits), (void *) ((uint64_t) clic_global + CLIC_CLICCFG_OFFSET));
}

/* Sets the CLICINT configuration register for interrupt `irq_id` */
void clic_set_clicint(uint32_t irq_id, clic_intcfg_t cfg)
{
    write32(MAKE_CLICINT(cfg), (void *) ((uint64_t) clic_global + CLIC_CLICINT_OFFSET(irq_id)));
}

/* Sets the CLICINTV configuration register for interrupt `irq_id` */
void clic_set_clicintv(uint32_t irq_id, clic_intvcfg_t cfg)
{
    write32(MAKE_CLICINTV(cfg), (void *) ((uint64_t) clic_global + CLIC_CLICINTV_OFFSET(irq_id)));
}

/* Reads the CLICINT configuration register for interrupt `irq_id`. Returns a `clic_intcfg_t` struct */
clic_intcfg_t clic_read_clicint(uint32_t irq_id)
{
    uint32_t clicint = read32((void *) ((uint64_t) clic_global + CLIC_CLICINT_OFFSET(irq_id)));
    clic_intcfg_t cfg;
    CLIC_PARSE_CLICINT(cfg, clicint);
    return cfg;
}

/* Set enable bit for interrupt `irq_id` */
void clic_set_enable(uint32_t irq_id, bool en)
{
    clic_intcfg_t intcfg = clic_read_clicint(irq_id);
    intcfg.ie = en ? CLIC_IE_ENABLE : CLIC_IE_MASK;
    clic_set_clicint(irq_id, intcfg);
}

/* Enable interrupt `irq_id` */
void clic_enable_interrupt(uint32_t irq_id)
{
    clic_intcfg_t intcfg = clic_read_clicint(irq_id);
    intcfg.ie = CLIC_IE_ENABLE;
    clic_set_clicint(irq_id, intcfg);
}

/* Disable interrupt `irq_id` */
void clic_disable_interrupt(uint32_t irq_id)
{
    clic_intcfg_t intcfg = clic_read_clicint(irq_id);
    intcfg.ie = CLIC_IE_MASK;
    clic_set_clicint(irq_id, intcfg);
}

/* Set priority / level for interrupt `irq_id` */
void clic_set_priority(uint32_t irq_id, uint8_t prio)
{
    clic_intcfg_t intcfg = clic_read_clicint(irq_id);
    intcfg.ctl = prio;
    clic_set_clicint(irq_id, intcfg);
}

/* Get interrupt pending bit */
bool clic_get_pend(uint32_t irq_id)
{
    clic_intcfg_t intcfg = clic_read_clicint(irq_id);
    return (intcfg.ip == CLIC_IP_SET);
}

/* Set interrupt pending bit */
void clic_set_pend(uint32_t irq_id, bool pending)
{
    clic_intcfg_t intcfg = clic_read_clicint(irq_id);
    intcfg.ip = pending ? CLIC_IP_SET : CLIC_IP_CLEAR;
    clic_set_clicint(irq_id, intcfg);
}

/* 
** CLIC initialization
**
** - Sets CLICCFG to default value.
** - Detects number of implemented interrupts and stores the number in CLIC_IMPL_INTERRUPTS.
** - Configures each interrupt as edge-triggered, HS-delegated and masked. 
** - Clears all pending interrupts.
** 
*/
void clic_init()
{
    /*
    ** smcliccfg extension
    ** 
    ** Set number of privilege modes to 2 (M/S)
    ** Set number of interrupt level bits to 8 (for U/S/M modes)
    ** 
    */
    // clic_set_mcfg(0x02, 0xFF, 0xFF, 0xFF); // cannot do from S-mode

    /* Default CLICINT configuration */
    clic_intcfg_t intcfg = {
        .ip        = CLIC_IP_CLEAR,
        .ie        = CLIC_IE_MASK,
        .attr.shv  = CLIC_INT_ATTR_SHV_OFF,
        .attr.trig = CLIC_INT_ATTR_TRIG_EDGE,
        .attr.mode = CLIC_INT_ATTR_MODE_S,
        .ctl       = 0x00
    };

    struct sbiret ret;

    /* Read number of CLIC interrupt sources (needs ecall to OpenSBI firmware) */
    ret = sbi_clic_get_num_sources();
    if(ret.error != SBI_SUCCESS){
        printk("[BAO] Cannot get number of CLIC interrupts from OpenSBI\r\n");
        CLIC_IMPL_INTERRUPTS = 0;
    } else {
        CLIC_IMPL_INTERRUPTS = ret.value;
    }

    /*
    ** Ask OpenSBI to delegate interrupts to S mode,
    ** and set default configuration.
    */
    for (uint32_t irq_id = 1; irq_id < CLIC_IMPL_INTERRUPTS; irq_id++) {
        ret = sbi_clic_delegate(irq_id);
        if (ret.error != SBI_SUCCESS) {
            // printk("[BAO] OpenSBI firmware returned error on `sbi_clic_delegate` ecall\r\n");
            continue;
        }
        /* Configure delegated interrupt */
        clic_set_clicint(irq_id, intcfg);
    }
}

/* Delegate interrupt `irq_id` to VM `vm_id` */
void clic_delegate_to_vm(uint32_t irq_id, unsigned long vm_id)
{
    if(vm_id >= 64){
        WARNING("[BAO] Cannot delegate interrupt to VM %d\r\n", vm_id);
        return;
    }
    clic_intvcfg_t intvcfg = {
        .v    = CLIC_V_ENABLE,
        .vsid = (uint8_t) vm_id
    };
    clic_set_clicintv(irq_id, intvcfg);
}


    // clic_intcfg_t cfg_enable = {
    //     .ip        = CLIC_IP_CLEAR,
    //     .ie        = CLIC_IE_ENABLE,
    //     .attr.shv  = CLIC_INT_ATTR_SHV_OFF,
    //     .attr.trig = CLIC_INT_ATTR_TRIG_LVL,
    //     .attr.mode = CLIC_INT_ATTR_MODE_S,
    //     .ctl       = 0x00
    // };

    // clic_intcfg_t cfg_init = {
    //     .ip        = CLIC_IP_CLEAR,
    //     .ie        = CLIC_IE_MASK,
    //     .attr.shv  = CLIC_INT_ATTR_SHV_OFF,
    //     .attr.trig = CLIC_INT_ATTR_TRIG_LVL,
    //     .attr.mode = CLIC_INT_ATTR_MODE_S,
    //     .ctl       = 0x00
    // };

    // for (size_t i = 0; i < CLIC_MAX_INTERRUPTS; i++) {
    //     clic_set_clicint(i, cfg_enable);
    //     clic_intcfg_t cfg = clic_read_clicint(i);
    //     if(cfg.ie == CLIC_IE_MASK){
    //         CLIC_IMPL_INTERRUPTS = i;
    //         break;
    //     }
    //     clic_set_clicint(i, cfg_init);
    // }