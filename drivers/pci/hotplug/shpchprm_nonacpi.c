/*
 * SHPCHPRM NONACPI: PHP Resource Manager for Non-ACPI/Legacy platform
 *
 * Copyright (C) 1995,2001 Compaq Computer Corporation
 * Copyright (C) 2001 Greg Kroah-Hartman (greg@kroah.com)
 * Copyright (C) 2001 IBM Corp.
 * Copyright (C) 2003-2004 Intel Corporation
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 * NON INFRINGEMENT.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Send feedback to <greg@kroah.com>, <dely.l.sy@intel.com>
 *
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#ifdef CONFIG_IA64
#include <asm/iosapic.h>
#endif
#include "shpchp.h"
#include "shpchprm.h"
#include "shpchprm_nonacpi.h"

void shpchprm_cleanup(void)
{
	return;
}

int shpchprm_print_pirt(void)
{
	return 0;
}

void * shpchprm_get_slot(struct slot *slot)
{
	return NULL;
}

int shpchprm_get_physical_slot_number(struct controller *ctrl, u32 *sun, u8 busnum, u8 devnum)
{
	int	offset = devnum - ctrl->slot_device_offset;

	dbg("%s: ctrl->slot_num_inc %d, offset %d\n", __FUNCTION__, ctrl->slot_num_inc, offset);
	*sun = (u8) (ctrl->first_slot + ctrl->slot_num_inc * offset);
	return 0;
}

static void print_pci_resource ( struct pci_resource	*aprh)
{
	struct pci_resource	*res;

	for (res = aprh; res; res = res->next)
		dbg("        base= 0x%x length= 0x%x\n", res->base, res->length);
}


static void phprm_dump_func_res( struct pci_func *fun)
{
	struct pci_func *func = fun;

	if (func->bus_head) {
		dbg(":    BUS Resources:\n");
		print_pci_resource (func->bus_head);
	}
	if (func->io_head) {
		dbg(":    IO Resources:\n");
		print_pci_resource (func->io_head);
	}
	if (func->mem_head) {
		dbg(":    MEM Resources:\n");
		print_pci_resource (func->mem_head);
	}
	if (func->p_mem_head) {
		dbg(":    PMEM Resources:\n");
		print_pci_resource (func->p_mem_head);
	}
}

static int phprm_get_used_resources (
	struct controller *ctrl,
	struct pci_func *func
	)
{
	return shpchp_save_used_resources (ctrl, func, !DISABLE_CARD);
}

static int phprm_delete_resource(
	struct pci_resource **aprh,
	ulong base,
	ulong size)
{
	struct pci_resource *res;
	struct pci_resource *prevnode;
	struct pci_resource *split_node;
	ulong tbase;

	shpchp_resource_sort_and_combine(aprh);

	for (res = *aprh; res; res = res->next) {
		if (res->base > base)
			continue;

		if ((res->base + res->length) < (base + size))
			continue;

		if (res->base < base) {
			tbase = base;

			if ((res->length - (tbase - res->base)) < size)
				continue;

			split_node = (struct pci_resource *) kmalloc(sizeof(struct pci_resource), GFP_KERNEL);
			if (!split_node)
				return -ENOMEM;

			split_node->base = res->base;
			split_node->length = tbase - res->base;
			res->base = tbase;
			res->length -= split_node->length;

			split_node->next = res->next;
			res->next = split_node;
		}

		if (res->length >= size) {
			split_node = (struct pci_resource*) kmalloc(sizeof(struct pci_resource), GFP_KERNEL);
			if (!split_node)
				return -ENOMEM;

			split_node->base = res->base + size;
			split_node->length = res->length - size;
			res->length = size;

			split_node->next = res->next;
			res->next = split_node;
		}

		if (*aprh == res) {
			*aprh = res->next;
		} else {
			prevnode = *aprh;
			while (prevnode->next != res)
				prevnode = prevnode->next;

			prevnode->next = res->next;
		}
		res->next = NULL;
		kfree(res);
		break;
	}

	return 0;
}


static int phprm_delete_resources(
	struct pci_resource **aprh,
	struct pci_resource *this
	)
{
	struct pci_resource *res;

	for (res = this; res; res = res->next)
		phprm_delete_resource(aprh, res->base, res->length);

	return 0;
}


static int configure_existing_function(
	struct controller *ctrl,
	struct pci_func *func
	)
{
	int rc;

	/* see how much resources the func has used. */
	rc = phprm_get_used_resources (ctrl, func);

	if (!rc) {
		/* subtract the resources used by the func from ctrl resources */
		rc  = phprm_delete_resources (&ctrl->bus_head, func->bus_head);
		rc |= phprm_delete_resources (&ctrl->io_head, func->io_head);
		rc |= phprm_delete_resources (&ctrl->mem_head, func->mem_head);
		rc |= phprm_delete_resources (&ctrl->p_mem_head, func->p_mem_head);
		if (rc)
			warn("aCEF: cannot del used resources\n");
	} else
		err("aCEF: cannot get used resources\n");

	return rc;
}

static int bind_pci_resources_to_slots ( struct controller *ctrl)
{
	struct pci_func *func;
	int busn = ctrl->slot_bus;
	int devn, funn;
	u32	vid;

	for (devn = 0; devn < 32; devn++) {
		for (funn = 0; funn < 8; funn++) {
			/*
			if (devn == ctrl->device && funn == ctrl->function)
				continue;
			 */
			/* find out if this entry is for an occupied slot */
			vid = 0xFFFFFFFF;

			pci_bus_read_config_dword(ctrl->pci_dev->subordinate, PCI_DEVFN(devn, funn), PCI_VENDOR_ID, &vid);

			if (vid != 0xFFFFFFFF) {
				func = shpchp_slot_find(busn, devn, funn);
				if (!func)
					continue;
				configure_existing_function(ctrl, func);
				dbg("aCCF:existing PCI 0x%x Func ResourceDump\n", ctrl->bus);
				phprm_dump_func_res(func);
			}
		}
	}

	return 0;
}

static void phprm_dump_ctrl_res( struct controller *ctlr)
{
	struct controller *ctrl = ctlr;

	if (ctrl->bus_head) {
		dbg(":    BUS Resources:\n");
		print_pci_resource (ctrl->bus_head);
	}
	if (ctrl->io_head) {
		dbg(":    IO Resources:\n");
		print_pci_resource (ctrl->io_head);
	}
	if (ctrl->mem_head) {
		dbg(":    MEM Resources:\n");
		print_pci_resource (ctrl->mem_head);
	}
	if (ctrl->p_mem_head) {
		dbg(":    PMEM Resources:\n");
		print_pci_resource (ctrl->p_mem_head);
	}
}

/*
 * phprm_find_available_resources
 *
 *  Finds available memory, IO, and IRQ resources for programming
 *  devices which may be added to the system
 *  this function is for hot plug ADD!
 *
 * returns 0 if success
 */
int shpchprm_find_available_resources(struct controller *ctrl)
{
	struct pci_func func;
	u32 rc;

	memset(&func, 0, sizeof(struct pci_func));

	func.bus = ctrl->bus;
	func.device = ctrl->device;
	func.function = ctrl->function;
	func.is_a_board = 1;

	/* Get resources for this PCI bridge */
	rc = shpchp_save_used_resources (ctrl, &func, !DISABLE_CARD);
	dbg("%s: shpchp_save_used_resources rc = %d\n", __FUNCTION__, rc);

	if (func.mem_head)
		func.mem_head->next = ctrl->mem_head;
	ctrl->mem_head = func.mem_head;

	if (func.p_mem_head)
		func.p_mem_head->next = ctrl->p_mem_head;
	ctrl->p_mem_head = func.p_mem_head;

	if (func.io_head)
		func.io_head->next = ctrl->io_head;
	ctrl->io_head = func.io_head;

	if(func.bus_head)
		func.bus_head->next = ctrl->bus_head;
	ctrl->bus_head = func.bus_head;
	if (ctrl->bus_head)
		phprm_delete_resource(&ctrl->bus_head, ctrl->pci_dev->subordinate->number, 1);

	dbg("%s:pre-Bind PCI 0x%x Ctrl Resource Dump\n", __FUNCTION__, ctrl->bus);
	phprm_dump_ctrl_res(ctrl);
	bind_pci_resources_to_slots (ctrl);

	dbg("%s:post-Bind PCI 0x%x Ctrl Resource Dump\n", __FUNCTION__, ctrl->bus);
	phprm_dump_ctrl_res(ctrl);


	/* If all of the following fail, we don't have any resources for hot plug add */
	rc = 1;
	rc &= shpchp_resource_sort_and_combine(&(ctrl->mem_head));
	rc &= shpchp_resource_sort_and_combine(&(ctrl->p_mem_head));
	rc &= shpchp_resource_sort_and_combine(&(ctrl->io_head));
	rc &= shpchp_resource_sort_and_combine(&(ctrl->bus_head));

	return (rc);
}

int shpchprm_set_hpp(
	struct controller *ctrl,
	struct pci_func *func,
	u8	card_type)
{
	u32 rc;
	u8 temp_byte;
	struct pci_bus lpci_bus, *pci_bus;
	unsigned int	devfn;
	memcpy(&lpci_bus, ctrl->pci_bus, sizeof(lpci_bus));
	pci_bus = &lpci_bus;
	pci_bus->number = func->bus;
	devfn = PCI_DEVFN(func->device, func->function);

	temp_byte = 0x40;	/* hard coded value for LT */
	if (card_type == PCI_HEADER_TYPE_BRIDGE) {
		/* set subordinate Latency Timer */
		rc = pci_bus_write_config_byte(pci_bus, devfn, PCI_SEC_LATENCY_TIMER, temp_byte);

		if (rc) {
			dbg("%s: set secondary LT error. b:d:f(%02x:%02x:%02x)\n", __FUNCTION__, func->bus, 
				func->device, func->function);
			return rc;
		}
	}

	/* set base Latency Timer */
	rc = pci_bus_write_config_byte(pci_bus, devfn, PCI_LATENCY_TIMER, temp_byte);

	if (rc) {
		dbg("%s: set LT error. b:d:f(%02x:%02x:%02x)\n", __FUNCTION__, func->bus, func->device, func->function);
		return rc;
	}

	/* set Cache Line size */
	temp_byte = 0x08;	/* hard coded value for CLS */

	rc = pci_bus_write_config_byte(pci_bus, devfn, PCI_CACHE_LINE_SIZE, temp_byte);

	if (rc) {
		dbg("%s: set CLS error. b:d:f(%02x:%02x:%02x)\n", __FUNCTION__, func->bus, func->device, func->function);
	}

	/* set enable_perr */
	/* set enable_serr */

	return rc;
}

void shpchprm_enable_card(
	struct controller *ctrl,
	struct pci_func *func,
	u8 card_type)
{
	u16 command, bcommand;
	struct pci_bus lpci_bus, *pci_bus;
	unsigned int devfn;
	int rc;

	memcpy(&lpci_bus, ctrl->pci_bus, sizeof(lpci_bus));
	pci_bus = &lpci_bus;
	pci_bus->number = func->bus;
	devfn = PCI_DEVFN(func->device, func->function);

	rc = pci_bus_read_config_word(pci_bus, devfn, PCI_COMMAND, &command);

	command |= PCI_COMMAND_PARITY | PCI_COMMAND_SERR
		| PCI_COMMAND_MASTER | PCI_COMMAND_INVALIDATE
		| PCI_COMMAND_IO | PCI_COMMAND_MEMORY;

	rc = pci_bus_write_config_word(pci_bus, devfn, PCI_COMMAND, command);

	if (card_type == PCI_HEADER_TYPE_BRIDGE) {

		rc = pci_bus_read_config_word(pci_bus, devfn, PCI_BRIDGE_CONTROL, &bcommand);

		bcommand |= PCI_BRIDGE_CTL_PARITY | PCI_BRIDGE_CTL_SERR
			| PCI_BRIDGE_CTL_NO_ISA;

		rc = pci_bus_write_config_word(pci_bus, devfn, PCI_BRIDGE_CONTROL, bcommand);
	}
}

static int legacy_shpchprm_init_pci(void)
{
	return 0;
}

int shpchprm_init(enum php_ctlr_type ctrl_type)
{
	int retval;

	switch (ctrl_type) {
	case PCI:
		retval = legacy_shpchprm_init_pci();
		break;
	default:
		retval = -ENODEV;
		break;
	}

	return retval;
}
