#ifndef _ASM_IRQ_H
#define _ASM_IRQ_H

/*
 *	linux/include/asm/irq.h
 *
 *	(C) 1992, 1993 Linus Torvalds, (C) 1997 Ingo Molnar
 *
 *	IRQ/IPI changes taken from work by Thomas Radke
 *	<tomsoft@informatik.tu-chemnitz.de>
 */

#include <linux/sched.h>
/* include comes from machine specific directory */
#include "irq_vectors.h"
#include <asm/thread_info.h>

static __inline__ int irq_canonicalize(int irq)
{
	return ((irq == 2) ? 9 : irq);
}

#define ARCH_HAS_NMI_WATCHDOG		/* See include/linux/nmi.h */

# define irq_ctx_init(cpu) do { } while (0)

#ifdef CONFIG_HOTPLUG_CPU
#include <linux/cpumask.h>
extern void fixup_irqs(cpumask_t map);
#endif

#define __ARCH_HAS_DO_SOFTIRQ 1

#endif /* _ASM_IRQ_H */