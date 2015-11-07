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
#include <linux/module.h>
#include "pattern_io.h"
#include "global_data.h"

/*
 * There is no need to lock the amfs_super_info's rwsem as there is no
 * way anyone can have a reference to the superblock at this point in time.
 */
static int amfs_read_super(struct super_block *sb, void *raw_data, int silent)
{
	int err = 0;
	struct super_block *lower_sb;
	struct path lower_path;
	char *dev_name = (char *) raw_data;
	struct inode *inode;

	if (!dev_name) {
		printk(KERN_ERR
		       "amfs: read_super: missing dev_name argument\n");
		err = -EINVAL;
		goto out;
	}

	/* parse lower path */
	err = kern_path(dev_name, LOOKUP_FOLLOW | LOOKUP_DIRECTORY,
			&lower_path);
	if (err) {
		printk(KERN_ERR	"amfs: error accessing "
		       "lower directory '%s'\n", dev_name);
		goto out;
	}

	/* allocate superblock private data */
	sb->s_fs_info = (struct amfs_sb_info *) kzalloc(sizeof(struct amfs_sb_info), GFP_KERNEL);
	if (!AMFS_SB(sb)) {
		printk(KERN_CRIT "amfs: read_super: out of memory\n");
		err = -ENOMEM;
		goto out_free;
	}

	/* set the lower superblock field of upper superblock */
	lower_sb = lower_path.dentry->d_sb;
	atomic_inc(&lower_sb->s_active);
	amfs_set_lower_super(sb, lower_sb);
	
	AMFS_SB(sb)->filename = (char *) kmalloc(strlen(filename) + 1, GFP_KERNEL);
	if (!AMFS_SB(sb)->filename) {
		err = -ENOMEM;
		goto out_free;
	}
	strcpy(AMFS_SB(sb)->filename, filename);
	//printk("SUPER: filename: %s\n", (AMFS_SB(sb)->filename));	
	//printk("SUPER: Head from sb_pr->head: %p\n", head);
	AMFS_SB(sb)->head = head;
	
	/* inherit maxbytes from lower file system */
	sb->s_maxbytes = lower_sb->s_maxbytes;

	/*
	 * Our c/m/atime granularity is 1 ns because we may stack on file
	 * systems whose granularity is as good.
	 */
	sb->s_time_gran = 1;

	sb->s_op = &amfs_sops;

	/* get a new inode and allocate our root dentry */
	inode = amfs_iget(sb, lower_path.dentry->d_inode);
	if (IS_ERR(inode)) {
		err = PTR_ERR(inode);
		goto out_sput;
	}
	sb->s_root = d_make_root(inode);
	if (!sb->s_root) {
		err = -ENOMEM;
		goto out_iput;
	}
	d_set_d_op(sb->s_root, &amfs_dops);

	/* link the upper and lower dentries */
	sb->s_root->d_fsdata = NULL;
	err = new_dentry_private_data(sb->s_root);
	if (err)
		goto out_freeroot;

	/* if get here: cannot have error */

	/* set the lower dentries for s_root */
	amfs_set_lower_path(sb->s_root, &lower_path);

	/*
	 * No need to call interpose because we already have a positive
	 * dentry, which was instantiated by d_make_root.  Just need to
	 * d_rehash it.
	 */
	d_rehash(sb->s_root);
	if (!silent)
		printk(KERN_INFO
		       "amfs: mounted on top of %s type %s\n",
		       dev_name, lower_sb->s_type->name);
	goto out; /* all is well */

	/* no longer needed: free_dentry_private_data(sb->s_root); */
out_freeroot:
	dput(sb->s_root);
out_iput:
	iput(inode);
out_sput:
	/* drop refs we took earlier */
	atomic_dec(&lower_sb->s_active);
	kfree(AMFS_SB(sb));
	sb->s_fs_info = NULL;
out_free:
	path_put(&lower_path);

out:
	return err;
}


struct dentry *amfs_mount(struct file_system_type *fs_type, int flags,
			    const char *dev_name, void *raw_data)
{
	int rc = 0;
	int bytes_read;
	struct file *filp;
	char *pat;
	char *page_buf;
	void *lower_path_name = (void *) dev_name;
	if ((rc = amfs_parse_options(raw_data, &filename)) != 0)
		goto out;
	//printk("KERN_AMFS_MOUNT: File_name: %s, len: %d\n", filename, strlen(filename));
	if ((rc = amfs_open_pattern_file(filename, &filp)) != 0)
		goto out;
	page_buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!page_buf) {
		rc = -ENOMEM;
		goto closefile;
	}
	memset(page_buf, 0, PAGE_SIZE);
	head = NULL;
	//printk("KERN_AMFS: Filename in ab_amfs_priv: %s\n", filename);
	while ((bytes_read = amfs_read_pattern_file(filp, page_buf, PAGE_SIZE)) > 0)
	{
		memset(page_buf+bytes_read, 0, PAGE_SIZE - bytes_read);
		while ((pat = strsep(&page_buf, "\n")) != NULL)
		{
			if (page_buf == NULL) /* write code to handle patterns extending beyond a single page */
			{
				printk("KERN_AMFS: No new line char found in page_buf\n");
			}
			else {
				if ((rc = addtoList(&head, pat, strlen(pat))) != 0)
					goto free_filename;
			}
			
		}
	}
//	printk("AMFS_MOUNT: Head of LL: %p\n", head);
//	printk("KERN_AMFS: Printing patterns after storing in prov_data:\n");
//	printList(&head);
	/* need to move this delete to umount/kill */
	//delAllFromList(&(sb_pr->head));	
	goto out;

free_filename:
	if (filename) {
		kfree(filename);
		filename = NULL;
	}
	if (head) {
		kfree(head);
		head = NULL;
	}
	if (page_buf) {
		kfree(page_buf);
		page_buf = NULL;
	}
closefile:
	filp_close(filp, NULL);
out:	
	if (rc != 0) 
		return ERR_PTR(rc);
	else
		return mount_nodev(fs_type, flags, lower_path_name,
			   amfs_read_super);
}


static struct file_system_type amfs_fs_type = {
	.owner		    = THIS_MODULE,
	.name		    = AMFS_NAME,
	.mount		    = amfs_mount,
	.kill_sb	    = generic_shutdown_super,
	.fs_flags	    = 0,
};
MODULE_ALIAS_FS(AMFS_NAME);

static int __init init_amfs_fs(void)
{
	int err;

	pr_info("Registering amfs " AMFS_VERSION "\n");
	err = amfs_init_inode_cache();
	if (err)
		goto out;
	err = amfs_init_dentry_cache();
	if (err)
		goto out;
	err = register_filesystem(&amfs_fs_type);
out:
	if (err) {
		amfs_destroy_inode_cache();
		amfs_destroy_dentry_cache();
	}
	return err;
}

static void __exit exit_amfs_fs(void)
{
	amfs_destroy_inode_cache();
	amfs_destroy_dentry_cache();
	unregister_filesystem(&amfs_fs_type);
	pr_info("Completed amfs module unload\n");
}

MODULE_AUTHOR("AKHILESH GADDE");
MODULE_DESCRIPTION("AMFS");
MODULE_LICENSE("GPL");

module_init(init_amfs_fs);
module_exit(exit_amfs_fs);
