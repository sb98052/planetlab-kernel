/* linux/drivers/block/ckrm_io.c : Block I/O Resource Controller for CKRM
 *
 * Copyright (C) Shailabh Nagar, IBM Corp. 2004
 * 
 * 
 * Provides best-effort block I/O bandwidth control for CKRM 
 * This file provides the CKRM API. The underlying scheduler is a 
 * modified Complete-Fair Queueing (CFQ) iosched.
 *
 * Latest version, more details at http://ckrm.sf.net
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

/* Changes
 *
 * 29 July 2004
 *          Third complete rewrite for CKRM's current API
 *
 */


#ifndef _LINUX_CKRM_IO_H
#define _LINUX_CKRM_IO_H

typedef void *(*icls_tsk_t) (struct task_struct *tsk);
typedef int (*icls_ioprio_t) (struct task_struct *tsk);


#ifdef CONFIG_CKRM_RES_BLKIO

extern void *cki_tsk_icls (struct task_struct *tsk);
extern int cki_tsk_ioprio (struct task_struct *tsk);

#endif /* CONFIG_CKRM_RES_BLKIO */

#endif 
