#ifndef __CLIC_H__
#define __CLIC_H__

#include <bao.h>
#include <platform.h>

/* Max number of interrupts that a CLIC implementation can support */

#define CLIC_MAX_INTERRUPTS (4096)

/**************************
** CLIC registers layout **
**************************/

/* CLICCFG register */

#define CLIC_CLICCFG_MNLBITS_OFFSET           0
#define CLIC_CLICCFG_NMBITS_OFFSET            4
#define CLIC_CLICCFG_SNLBITS_OFFSET          16
#define CLIC_CLICCFG_UNLBITS_OFFSET          24

#define CLIC_CLICCFG_MNLBITS_MASK    0x0000000FU
#define CLIC_CLICCFG_NMBITS_MASK     0x00000030U
#define CLIC_CLICCFG_SNLBITS_MASK    0x000F0000U
#define CLIC_CLICCFG_UNLBITS_MASK    0x0F000000U

/* CLICINT registers */

#define CLIC_CLICINT_IP_OFFSET                0
#define CLIC_CLICINT_IE_OFFSET                7
#define CLIC_CLICINT_ATTR_SHV_OFFSET         16
#define CLIC_CLICINT_ATTR_TRIG_OFFSET        17
#define CLIC_CLICINT_ATTR_MODE_OFFSET        22
#define CLIC_CLICINT_CTL_OFFSET              24

#define CLIC_CLICINT_IP_MASK         0x00000001U
#define CLIC_CLICINT_IE_MASK         0x00000080U
#define CLIC_CLICINT_ATTR_SHV_MASK   0x00010000U
#define CLIC_CLICINT_ATTR_TRIG_MASK  0x00060000U
#define CLIC_CLICINT_ATTR_MODE_MASK  0x00C00000U
#define CLIC_CLICINT_CTL_MASK        0xFF000000U

/* CLICINTV registers */

#define CLIC_CLICINTV_V_OFFSET                0
#define CLIC_CLICINTV_VSID_OFFSET             2

#define CLIC_CLICINTV_V_MASK         0x00000001U
#define CLIC_CLICINTV_VSID_MASK      0x000000FCU

/******************
** Helper macros **
*******************/

/* Generates the correct register layout for the CLICCFG register starting from its fields */
#define MAKE_CLICCFG(nmbits, mnlbits, snlbits, unlbits) \
    ( (uint32_t) ( \
        (((uint32_t) nmbits  << CLIC_CLICCFG_NMBITS_OFFSET)  & CLIC_CLICCFG_NMBITS_MASK)  | \
        (((uint32_t) mnlbits << CLIC_CLICCFG_MNLBITS_OFFSET) & CLIC_CLICCFG_MNLBITS_MASK) | \
        (((uint32_t) snlbits << CLIC_CLICCFG_SNLBITS_OFFSET) & CLIC_CLICCFG_SNLBITS_MASK) | \
        (((uint32_t) unlbits << CLIC_CLICCFG_UNLBITS_OFFSET) & CLIC_CLICCFG_UNLBITS_MASK)   \
        ) \
    )

/* Generates the correct register layout for the CLICINT register starting from a `clic_intcfg_t` struct */
#define MAKE_CLICINT(cfg) \
    ( (uint32_t) ( \
        (((uint32_t) cfg.ip        << CLIC_CLICINT_IP_OFFSET)         & CLIC_CLICINT_IP_MASK)        | \
        (((uint32_t) cfg.ie        << CLIC_CLICINT_IE_OFFSET)         & CLIC_CLICINT_IE_MASK)        | \
        (((uint32_t) cfg.attr.shv  << CLIC_CLICINT_ATTR_SHV_OFFSET)   & CLIC_CLICINT_ATTR_SHV_MASK)  | \
        (((uint32_t) cfg.attr.trig << CLIC_CLICINT_ATTR_TRIG_OFFSET)  & CLIC_CLICINT_ATTR_TRIG_MASK) | \
        (((uint32_t) cfg.attr.mode << CLIC_CLICINT_ATTR_MODE_OFFSET)  & CLIC_CLICINT_ATTR_MODE_MASK) | \
        (((uint32_t) cfg.ctl       << CLIC_CLICINT_CTL_OFFSET)        & CLIC_CLICINT_CTL_MASK)         \
        ) \
    )

/* Generates the correct register layout for the CLICINTV register starting from a `clic_intvcfg_t` struct */
#define MAKE_CLICINTV(cfg) \
    ( (uint32_t) ( \
        (((uint32_t) cfg.v    << CLIC_CLICINTV_V_OFFSET)    & CLIC_CLICINTV_V_MASK)   | \
        (((uint32_t) cfg.vsid << CLIC_CLICINTV_VSID_OFFSET) & CLIC_CLICINTV_VSID_MASK)  \
        ) \
    )

/* Emits the code to fill a `clic_intcfg_t` struct given a `uint32_t` variable containing the value of a CLICINT register */
#define CLIC_PARSE_CLICINT(cfg, clicint) \
    cfg.ip        = (clicint & CLIC_CLICINT_IP_MASK)        >> CLIC_CLICINT_IP_OFFSET;        \
    cfg.ie        = (clicint & CLIC_CLICINT_IE_MASK)        >> CLIC_CLICINT_IE_OFFSET;        \
    cfg.attr.shv  = (clicint & CLIC_CLICINT_ATTR_SHV_MASK)  >> CLIC_CLICINT_ATTR_SHV_OFFSET;  \
    cfg.attr.trig = (clicint & CLIC_CLICINT_ATTR_TRIG_MASK) >> CLIC_CLICINT_ATTR_TRIG_OFFSET; \
    cfg.attr.mode = (clicint & CLIC_CLICINT_ATTR_MODE_MASK) >> CLIC_CLICINT_ATTR_MODE_OFFSET; \
    cfg.ctl       = (clicint & CLIC_CLICINT_CTL_MASK)       >> CLIC_CLICINT_CTL_OFFSET;

/* Declare global variables that should be accessible from other source files */
extern size_t CLIC_IMPL_INTERRUPTS;
extern volatile void *clic_global;

/*********************
** Type definitions **
*********************/

typedef enum {
    CLIC_INT_ATTR_SHV_OFF   = 0x00,
    CLIC_INT_ATTR_SHV_ON    = 0x01
} clic_intattr_shv_t;

typedef enum {
    CLIC_INT_ATTR_TRIG_LVL  = 0x0,
    CLIC_INT_ATTR_TRIG_EDGE = 0x1
} clic_intattr_trig_t;

typedef enum {
    CLIC_INT_ATTR_MODE_M  = 0x3,
    CLIC_INT_ATTR_MODE_S  = 0x1,
    CLIC_INT_ATTR_MODE_U  = 0x0,
} clic_intattr_mode_t;

typedef enum {
    CLIC_IP_CLEAR = 0x00,
    CLIC_IP_SET   = 0x01
} clic_ip_t;

typedef enum {
    CLIC_IE_MASK   = 0x00,
    CLIC_IE_ENABLE = 0x01
} clic_ie_t;

typedef enum {
    CLIC_V_DISABLE = 0x00,
    CLIC_V_ENABLE  = 0x01
} clic_v_t;

typedef struct {
    clic_intattr_shv_t  shv;
    clic_intattr_trig_t trig;
    clic_intattr_mode_t mode;
} clic_intattr_t;

typedef struct {
    clic_v_t       v;
    uint8_t        vsid;
} clic_intvcfg_t;

typedef struct {
    clic_ip_t      ip;
    clic_ie_t      ie;
    clic_intattr_t attr;
    uint8_t        ctl;
} clic_intcfg_t;

/************************
** Function prototypes **
************************/

void clic_init();
void clic_set_clicint(uint32_t irq_id, clic_intcfg_t cfg);
void clic_set_clicintv(uint32_t irq_id, clic_intvcfg_t cfg);
clic_intcfg_t clic_read_clicint(uint32_t irq_id);
void clic_enable_interrupt(uint32_t irq_id);
void clic_disable_interrupt(uint32_t irq_id);
void clic_set_enable(uint32_t irq_id, bool en);
void clic_set_priority(uint32_t irq_id, uint8_t prio);
bool clic_get_pend(uint32_t irq_id);
void clic_set_pend(uint32_t irq_id, bool pending);
void clic_delegate_to_vm(uint32_t irq_id, unsigned long vm_id);

#endif /* __CLIC_H__ */