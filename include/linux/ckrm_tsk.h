/* ckrm_tsk.h - No. of tasks resource controller for CKRM
 *
 * Copyright (C) Chandra Seetharaman, IBM Corp. 2003
 * 
 * Provides No. of tasks resource controller for CKRM
 *
 * Latest version, more details at http://ckrm.sf.net
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#ifndef _LINUX_CKRM_TSK_H
#define _LINUX_CKRM_TSK_H

#ifdef CONFIG_CKRM_TYPE_TASKCLASS
#include <linux/ckrm_rc.h>

typedef int (*get_ref_t) (struct ckrm_core_class *, int);
typedef void (*put_ref_t) (struct ckrm_core_class *);

extern int numtasks_get_ref(struct ckrm_core_class *, int);
extern void numtasks_put_ref(struct ckrm_core_class *);
extern void ckrm_numtasks_register(get_ref_t, put_ref_t);

#else /* CONFIG_CKRM_TYPE_TASKCLASS */

#define numtasks_get_ref(core_class, ref) (1)
#define numtasks_put_ref(core_class)  do {} while (0)

#endif /* CONFIG_CKRM_TYPE_TASKCLASS */
#endif /* _LINUX_CKRM_RES_H */