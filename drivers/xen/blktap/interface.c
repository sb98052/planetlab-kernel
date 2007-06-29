/******************************************************************************
 * drivers/xen/blktap/interface.c
 * 
 * Block-device interface management.
 * 
 * Copyright (c) 2004, Keir Fraser
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation; or, when distributed
 * separately from the Linux kernel or incorporated into other
 * software packages, subject to the following license:
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this source file (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.

 */

#include "common.h"
#include <xen/evtchn.h>

static struct kmem_cache *blkif_cachep;

blkif_t *tap_alloc_blkif(domid_t domid)
{
	blkif_t *blkif;

	blkif = kmem_cache_alloc(blkif_cachep, GFP_KERNEL);
	if (!blkif)
		return ERR_PTR(-ENOMEM);

	memset(blkif, 0, sizeof(*blkif));
	blkif->domid = domid;
	spin_lock_init(&blkif->blk_ring_lock);
	atomic_set(&blkif->refcnt, 1);
	init_waitqueue_head(&blkif->wq);
	blkif->st_print = jiffies;
	init_waitqueue_head(&blkif->waiting_to_free);

	return blkif;
}

static int map_frontend_page(blkif_t *blkif, unsigned long shared_page)
{
	struct gnttab_map_grant_ref op;
	int ret;

	gnttab_set_map_op(&op, (unsigned long)blkif->blk_ring_area->addr,
			  GNTMAP_host_map, shared_page, blkif->domid);

	lock_vm_area(blkif->blk_ring_area);
	ret = HYPERVISOR_grant_table_op(GNTTABOP_map_grant_ref, &op, 1);
	unlock_vm_area(blkif->blk_ring_area);
	BUG_ON(ret);

	if (op.status) {
		DPRINTK(" Grant table operation failure !\n");
		return op.status;
	}

	blkif->shmem_ref = shared_page;
	blkif->shmem_handle = op.handle;

	return 0;
}

static void unmap_frontend_page(blkif_t *blkif)
{
	struct gnttab_unmap_grant_ref op;
	int ret;

	gnttab_set_unmap_op(&op, (unsigned long)blkif->blk_ring_area->addr,
			    GNTMAP_host_map, blkif->shmem_handle);

	lock_vm_area(blkif->blk_ring_area);
	ret = HYPERVISOR_grant_table_op(GNTTABOP_unmap_grant_ref, &op, 1);
	unlock_vm_area(blkif->blk_ring_area);
	BUG_ON(ret);
}

int tap_blkif_map(blkif_t *blkif, unsigned long shared_page, 
		  unsigned int evtchn)
{
	blkif_sring_t *sring;
	int err;
	struct evtchn_bind_interdomain bind_interdomain;

	/* Already connected through? */
	if (blkif->irq)
		return 0;

	if ( (blkif->blk_ring_area = alloc_vm_area(PAGE_SIZE)) == NULL )
		return -ENOMEM;

	err = map_frontend_page(blkif, shared_page);
	if (err) {
		free_vm_area(blkif->blk_ring_area);
		return err;
	}

	bind_interdomain.remote_dom  = blkif->domid;
	bind_interdomain.remote_port = evtchn;

	err = HYPERVISOR_event_channel_op(EVTCHNOP_bind_interdomain,
					  &bind_interdomain);
	if (err) {
		unmap_frontend_page(blkif);
		free_vm_area(blkif->blk_ring_area);
		return err;
	}

	blkif->evtchn = bind_interdomain.local_port;

	sring = (blkif_sring_t *)blkif->blk_ring_area->addr;
	BACK_RING_INIT(&blkif->blk_ring, sring, PAGE_SIZE);

	blkif->irq = bind_evtchn_to_irqhandler(
		blkif->evtchn, tap_blkif_be_int, 0, "blkif-backend", blkif);

	return 0;
}

void tap_blkif_unmap(blkif_t *blkif)
{
	if (blkif->irq) {
		unbind_from_irqhandler(blkif->irq, blkif);
		blkif->irq = 0;
	}
	if (blkif->blk_ring.sring) {
		unmap_frontend_page(blkif);
		free_vm_area(blkif->blk_ring_area);
		blkif->blk_ring.sring = NULL;
	}
}

void tap_blkif_free(blkif_t *blkif)
{
	atomic_dec(&blkif->refcnt);
	wait_event(blkif->waiting_to_free, atomic_read(&blkif->refcnt) == 0);

	tap_blkif_unmap(blkif);
	kmem_cache_free(blkif_cachep, blkif);
}

void __init tap_blkif_interface_init(void)
{
	blkif_cachep = kmem_cache_create("blktapif_cache", sizeof(blkif_t), 
					 0, 0, NULL, NULL);
}