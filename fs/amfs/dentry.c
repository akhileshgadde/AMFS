/*
 * Copyright (c) 1998-2014 Erez Zadok
 * Copyright (c) 2009	   Shrikar Archak
 * Copyright (c) 2003-2014 Stony Brook University
 * Copyright (c) 2003-2014 The Research Foundation of SUNY
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "amfs.h"

/*
 * returns: -ERRNO if error (returned to user)
 *          0: tell VFS to invalidate dentry
 *          1: dentry is valid
 */
static int amfs_d_revalidate(struct dentry *dentry, unsigned int flags)
{
	struct path lower_path;
	struct dentry *lower_dentry;
	int err = 1;
	int ret = 0;

	if (flags & LOOKUP_RCU)
		return -ECHILD;

	amfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	/* Return zero for entries marked as Bad in xattr of lower FS */
	ret = lower_dentry->d_inode->i_op->getxattr(lower_dentry, AMFS_XATTR_NAME, \
            AMFS_XATTR_BAD, AMFS_XATTR_BAD_LEN);
	if (ret == AMFS_XATTR_BAD_LEN) {
		err = 0;
		goto out;
	}
	
	if (!(lower_dentry->d_flags & DCACHE_OP_REVALIDATE))
		goto out;
	err = lower_dentry->d_op->d_revalidate(lower_dentry, flags);
out:
	amfs_put_lower_path(dentry, &lower_path);
	return err;
}

static void amfs_d_release(struct dentry *dentry)
{
	/* release and reset the lower paths */
	amfs_put_reset_lower_path(dentry);
	free_dentry_private_data(dentry);
	return;
}

const struct dentry_operations amfs_dops = {
	.d_revalidate	= amfs_d_revalidate,
	.d_release	= amfs_d_release,
};
