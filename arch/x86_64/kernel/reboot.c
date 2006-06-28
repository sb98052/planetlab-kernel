/* Various gunk just to reboot the machine. */ 
#include <linux/module.h>
#include <linux/reboot.h>
#include <linux/init.h>
#include <linux/smp.h>
#include <linux/kernel.h>
#include <linux/ctype.h>
#include <linux/string.h>
#include <asm/io.h>
#include <asm/kdebug.h>
#include <asm/delay.h>
#include <asm/hw_irq.h>
#include <asm/system.h>
#include <asm/pgtable.h>
#include <asm/tlbflush.h>
#include <asm/apic.h>

/*
 * Power off function, if any
 */
void (*pm_power_off)(void);

static long no_idt[3];
static enum { 
	BOOT_TRIPLE = 't',
	BOOT_KBD = 'k'
} reboot_type = BOOT_KBD;
static int reboot_mode = 0;
int reboot_force;

/* reboot=t[riple] | k[bd] [, [w]arm | [c]old]
   warm   Don't set the cold reboot flag
   cold   Set the cold reboot flag
   triple Force a triple fault (init)
   kbd    Use the keyboard controller. cold reset (default)
   force  Avoid anything that could hang.
 */ 
static int __init reboot_setup(char *str)
{
	for (;;) {
		switch (*str) {
		case 'w': 
			reboot_mode = 0x1234;
			break;

		case 'c':
			reboot_mode = 0;
			break;

		case 't':
		case 'b':
		case 'k':
			reboot_type = *str;
			break;
		case 'f':
			reboot_force = 1;
			break;
		}
		if((str = strchr(str,',')) != NULL)
			str++;
		else
			break;
	}
	return 1;
}

__setup("reboot=", reboot_setup);

/* overwrites random kernel memory. Should not be kernel .text */
#define WARMBOOT_TRAMP 0x1000UL

static void reboot_warm(void)
{
	extern unsigned char warm_reboot[], warm_reboot_end[];
	printk("warm reboot\n");

	local_irq_disable(); 
		
	/* restore identity mapping */
	init_level4_pgt[0] = __pml4(__pa(level3_ident_pgt) | 7); 
	__flush_tlb_all(); 

	/* Move the trampoline to low memory */
	memcpy(__va(WARMBOOT_TRAMP), warm_reboot, warm_reboot_end - warm_reboot); 

	/* Start it in compatibility mode. */
	asm volatile( "   pushq $0\n" 		/* ss */
		     "   pushq $0x2000\n" 	/* rsp */
	             "   pushfq\n"		/* eflags */
		     "   pushq %[cs]\n"
		     "   pushq %[target]\n"
		     "   iretq" :: 
		      [cs] "i" (__KERNEL_COMPAT32_CS), 
		      [target] "b" (WARMBOOT_TRAMP));
}

static inline void kb_wait(void)
{
	int i;
  
	for (i=0; i<0x10000; i++)
		if ((inb_p(0x64) & 0x02) == 0)
			break;
}
  
void machine_shutdown(void)
{
	/* Stop the cpus and apics */
#ifdef CONFIG_SMP
	int reboot_cpu_id;
  
	/* The boot cpu is always logical cpu 0 */
	reboot_cpu_id = 0;

	/* Make certain the cpu I'm about to reboot on is online */
	if (!cpu_isset(reboot_cpu_id, cpu_online_map)) {
		reboot_cpu_id = smp_processor_id();
	}

	/* Make certain I only run on the appropriate processor */
	set_cpus_allowed(current, cpumask_of_cpu(reboot_cpu_id));

	/* O.K Now that I'm on the appropriate processor,
	 * stop all of the others.
	 */
	smp_send_stop();
#endif

	local_irq_disable();
  
#ifndef CONFIG_SMP
	disable_local_APIC();
#endif

	disable_IO_APIC();

	local_irq_enable();
}

static void smp_halt(void)
{
	int cpuid = safe_smp_processor_id(); 
	static int first_entry = 1;

	if (reboot_force)
		return;

	if (first_entry) {
		first_entry = 0;
		smp_call_function((void *)machine_restart, NULL, 1, 0);
	}
			
	smp_stop_cpu(); 

	/* AP calling this. Just halt */
	if (cpuid != boot_cpu_id) { 
		for (;;) 
			asm("hlt");
	}

	/* Wait for all other CPUs to have run smp_stop_cpu */
	while (!cpus_empty(cpu_online_map))
		rep_nop(); 
}

void machine_restart(char * __unused)
{
	int i;

	machine_shutdown();
	printk("machine restart\n");

#ifdef CONFIG_SMP
	smp_halt(); 
#endif

	if (!reboot_force) {
		local_irq_disable();
#ifndef CONFIG_SMP
		disable_local_APIC();
#endif
		disable_IO_APIC();
		local_irq_enable();
	}
	
	/* Tell the BIOS if we want cold or warm reboot */
	*((unsigned short *)__va(0x472)) = reboot_mode;
       
	for (;;) {
		/* Could also try the reset bit in the Hammer NB */
		switch (reboot_type) { 
		case BOOT_KBD:
		for (i=0; i<100; i++) {
			kb_wait();
			udelay(50);
			outb(0xfe,0x64);         /* pulse reset low */
			udelay(50);
		}

		case BOOT_TRIPLE: 
			__asm__ __volatile__("lidt (%0)": :"r" (&no_idt));
			__asm__ __volatile__("int3");

			reboot_type = BOOT_KBD;
			break;
		}      
	}      
}

EXPORT_SYMBOL(machine_restart);

void machine_halt(void)
{
}

EXPORT_SYMBOL(machine_halt);

void machine_power_off(void)
{
	if (pm_power_off)
		pm_power_off();
}

EXPORT_SYMBOL(machine_power_off);
