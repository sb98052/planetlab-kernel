/* -*- linux-c -*- --------------------------------------------------------- *
 *
 * linux/fs/devpts/inode.c
 *
 *  Copyright 1998-2004 H. Peter Anvin -- All Rights Reserved
 *
 * This file is part of the Linux kernel and is made available under
 * the terms of the GNU General Public License, version 2, or at your
 * option, any later version, incorporated herein by reference.
 *
 * ------------------------------------------------------------------------- */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/namei.h>
#include <linux/mount.h>
#include <linux/tty.h>
#include <linux/devpts_fs.h>
#include <linux/vs_base.h>
#include <linux/xattr.h>

#define DEVPTS_SUPER_MAGIC 0x1cd1

extern struct xattr_handler devpts_xattr_security_handler;

static struct xattr_handler *devpts_xattr_handlers[] = {
#ifdef CONFIG_DEVPTS_FS_SECURITY
	&devpts_xattr_security_handler,
#endif
	NULL
};

static struct vfsmount *devpts_mnt;
static struct dentry *devpts_root;

static struct {
	int setuid;
	int setgid;
	uid_t   uid;
	gid_t   gid;
	umode_t mode;
} config = {.mode = 0600};

static int devpts_remount(struct super_block *sb, int *flags, char *data)
{
	int setuid = 0;
	int setgid = 0;
	uid_t uid = 0;
	gid_t gid = 0;
	umode_t mode = 0600;
	char *this_char;

	this_char = NULL;
	while ((this_char = strsep(&data, ",")) != NULL) {
		int n;
		char dummy;
		if (!*this_char)
			continue;
		if (sscanf(this_char, "uid=%i%c", &n, &dummy) == 1) {
			setuid = 1;
			uid = n;
		} else if (sscanf(this_char, "gid=%i%c", &n, &dummy) == 1) {
			setgid = 1;
			gid = n;
		} else if (sscanf(this_char, "mode=%o%c", &n, &dummy) == 1)
			mode = n & ~S_IFMT;
		else {
			printk("devpts: called with bogus options\n");
			return -EINVAL;
		}
	}
	config.setuid  = setuid;
	config.setgid  = setgid;
	config.uid     = uid;
	config.gid     = gid;
	config.mode    = mode;

	return 0;
}

static int devpts_readdir(struct file * filp, void * dirent, filldir_t filldir)
{
	struct dentry *dentry = filp->f_dentry;
	struct dentry *cursor = filp->private_data;
	struct list_head *p, *q = &cursor->d_child;
	ino_t ino;
	int i = filp->f_pos;

	switch (i) {
		case 0:
			ino = dentry->d_inode->i_ino;
			if (filldir(dirent, ".", 1, i, ino, DT_DIR) < 0)
				break;
			filp->f_pos++;
			i++;
			/* fallthrough */
		case 1:
			ino = parent_ino(dentry);
			if (filldir(dirent, "..", 2, i, ino, DT_DIR) < 0)
				break;
			filp->f_pos++;
			i++;
			/* fallthrough */
		default:
			spin_lock(&dcache_lock);
			if (filp->f_pos == 2) {
				list_del(q);
				list_add(q, &dentry->d_subdirs);
			}
			for (p=q->next; p != &dentry->d_subdirs; p=p->next) {
				struct dentry *next;
				next = list_entry(p, struct dentry, d_child);
				if (d_unhashed(next) || !next->d_inode)
					continue;
				if (!vx_check(next->d_inode->i_xid, VX_IDENT))
					continue;

				spin_unlock(&dcache_lock);
				if (filldir(dirent, next->d_name.name,
					next->d_name.len, filp->f_pos,
					next->d_inode->i_ino, DT_CHR) < 0)
					return 0;
				spin_lock(&dcache_lock);
				/* next is still alive */
				list_del(q);
				list_add(q, p);
				p = q;
				filp->f_pos++;
			}
			spin_unlock(&dcache_lock);
	}
	return 0;
}

static struct file_operations devpts_dir_operations = {
	.open		= dcache_dir_open,
	.release	= dcache_dir_close,
	.llseek		= dcache_dir_lseek,
	.read		= generic_read_dir,
	.readdir	= devpts_readdir,
};

static struct super_operations devpts_sops = {
	.statfs		= simple_statfs,
	.remount_fs	= devpts_remount,
};

static int
devpts_fill_super(struct super_block *s, void *data, int silent)
{
	struct inode * inode;

	s->s_blocksize = 1024;
	s->s_blocksize_bits = 10;
	s->s_magic = DEVPTS_SUPER_MAGIC;
	s->s_op = &devpts_sops;
	s->s_xattr = devpts_xattr_handlers;

	inode = new_inode(s);
	if (!inode)
		goto fail;
	inode->i_ino = 1;
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
	inode->i_blocks = 0;
	inode->i_blksize = 1024;
	inode->i_uid = inode->i_gid = 0;
	inode->i_mode = S_IFDIR | S_IRUGO | S_IXUGO | S_IWUSR;
	inode->i_op = &simple_dir_inode_operations;
	inode->i_fop = &devpts_dir_operations;
	inode->i_nlink = 2;
	inode->i_xid = vx_current_xid();

	devpts_root = s->s_root = d_alloc_root(inode);
	if (s->s_root)
		return 0;
	
	printk("devpts: get root dentry failed\n");
	iput(inode);
fail:
	return -ENOMEM;
}

static struct super_block *devpts_get_sb(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *data)
{
	return get_sb_single(fs_type, flags, data, devpts_fill_super);
}

static struct file_system_type devpts_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "devpts",
	.get_sb		= devpts_get_sb,
	.kill_sb	= kill_anon_super,
};

/*
 * The normal naming convention is simply /dev/pts/<number>; this conforms
 * to the System V naming convention
 */

static struct dentry *get_node(int num)
{
	char s[12];
	struct dentry *root = devpts_root;
	down(&root->d_inode->i_sem);
	return lookup_one_len(s, root, sprintf(s, "%d", num));
}

#ifdef CONFIG_DEVPTS_FS_XATTR
static int devpts_permission(struct inode *inode, int mask, struct nameidata *nd)
{
	int ret = -EACCES;

	if (vx_check(inode->i_xid, VX_IDENT))
		ret = generic_permission(inode, mask, NULL);
	return ret;
}
#endif

struct inode_operations devpts_file_inode_operations = {
#ifdef CONFIG_DEVPTS_FS_XATTR
	.setxattr	= generic_setxattr,
	.getxattr	= generic_getxattr,
	.listxattr	= generic_listxattr,
	.removexattr	= generic_removexattr,
	.permission	= devpts_permission,
#endif
};

int devpts_pty_new(struct tty_struct *tty)
{
	int number = tty->index;
	struct tty_driver *driver = tty->driver;
	dev_t device = MKDEV(driver->major, driver->minor_start+number);
	struct dentry *dentry;
	struct inode *inode = new_inode(devpts_mnt->mnt_sb);

	/* We're supposed to be given the slave end of a pty */
	BUG_ON(driver->type != TTY_DRIVER_TYPE_PTY);
	BUG_ON(driver->subtype != PTY_TYPE_SLAVE);

	if (!inode)
		return -ENOMEM;

	inode->i_ino = number+2;
	inode->i_blksize = 1024;
	inode->i_uid = config.setuid ? config.uid : current->fsuid;
	inode->i_gid = config.setgid ? config.gid : current->fsgid;
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
	init_special_inode(inode, S_IFCHR|config.mode, device);
	inode->i_xid = vx_current_xid();
	inode->i_op = &devpts_file_inode_operations;
	inode->u.generic_ip = tty;

	dentry = get_node(number);
	if (!IS_ERR(dentry) && !dentry->d_inode)
		d_instantiate(dentry, inode);

	up(&devpts_root->d_inode->i_sem);

	return 0;
}

struct tty_struct *devpts_get_tty(int number)
{
	struct dentry *dentry = get_node(number);
	struct tty_struct *tty;

	tty = NULL;
	if (!IS_ERR(dentry)) {
		if (dentry->d_inode)
			tty = dentry->d_inode->u.generic_ip;
		dput(dentry);
	}

	up(&devpts_root->d_inode->i_sem);

	return tty;
}

void devpts_pty_kill(int number)
{
	struct dentry *dentry = get_node(number);

	if (!IS_ERR(dentry)) {
		struct inode *inode = dentry->d_inode;
		if (inode) {
			inode->i_nlink--;
			d_delete(dentry);
			dput(dentry);
		}
		dput(dentry);
	}
	up(&devpts_root->d_inode->i_sem);
}

static int __init init_devpts_fs(void)
{
	int err = register_filesystem(&devpts_fs_type);
	if (!err) {
		devpts_mnt = kern_mount(&devpts_fs_type);
		if (IS_ERR(devpts_mnt))
			err = PTR_ERR(devpts_mnt);
	}
	return err;
}

static void __exit exit_devpts_fs(void)
{
	unregister_filesystem(&devpts_fs_type);
	mntput(devpts_mnt);
}

module_init(init_devpts_fs)
module_exit(exit_devpts_fs)
MODULE_LICENSE("GPL");
