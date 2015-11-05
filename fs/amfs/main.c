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
#include "read_pattern.h"

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
	((struct amfs_sb_info *)sb->s_fs_info)->filename = (char *) kmalloc(strlen(filename) + 1, GFP_KERNEL);
	if (!((struct amfs_sb_info *)sb->s_fs_info)->filename) {
		err = -ENOMEM;
		goto out_free;
	}
	strcpy(((struct amfs_sb_info *)sb->s_fs_info)->filename, filename);
	printk("SUPER: filename: %s\n", ((struct amfs_sb_info *)sb->s_fs_info)->filename);	
	printk("SUPER: Head from sb_pr->head: %p\n", head);
	((struct amfs_sb_info *)sb->s_fs_info)->head = head;
	//amfs_set_sb_private(sb, sb_pr);
	//amfs_set_sb_ll_head(AMFS_SB(sb)->amfs_sb_pr, sb_pr->head);
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
	if (filename)
		kfree(filename);
	if (head)
		kfree(head);
	return err;
}

/* Function to parse and read the pattdb filename from raw_data and return the file pointer */
static int amfs_parse_options(char *data, char **file_name)
{
	int rc = 0;
	char *token1, *token2;
	if (!data) {
		rc = -EINVAL;
		goto out;
	}
	token1 = strstr(data, "pattdb=");
	if (!token1) {
		rc = -EINVAL;
		goto out;
	}
	token2 = strstr(token1, "=");
	*file_name = token2+1;
	
out:
	return rc;	
}

/*
*   Function to check if file exists and if it is regular or not 
*   Input: File structure pointer. 
*   Return: 0 if success, errno on failure.
*/
int amfs_check_pattern_file(struct file *filp)
{
    int ret = 0;
    //printk("KERN: Checking File is regular\n");
    if (S_ISDIR(filp->f_path.dentry->d_inode->i_mode)) {
        ret = -EISDIR;
        goto end;
    }	
    if (!S_ISREG(filp->f_path.dentry->d_inode->i_mode)) {
        ret = -EPERM;
        goto end;
	}
end:
    return ret;
}

/* Function to verify the pattern file path, permissions,etc and open the file */
static int amfs_open_pattern_file(const char *file_name, struct file **filp)
{
	int rc = 0;
	//*filp = NULL;
	if (file_name == NULL) {
		rc = -ENOENT;
		goto out;
	}
	*filp = filp_open(file_name, O_RDONLY, 0);
	if ((*filp == NULL) || (IS_ERR(*filp))) {
		rc = PTR_ERR(*filp);
		printk("KERNEL_AMFS: Error opening pattern file: %d\n", rc);
		goto out;
	}
	if ((rc = amfs_check_pattern_file(*filp)) != 0)
		goto out;
	#if 1 
	if ((!(*filp)->f_op) || (!(*filp)->f_op->read)) {
		printk("KERNEL_AMFS: No read permission on pattern file\n");
		rc = -EACCES;
		goto out;
	}
	(*filp)->f_pos = 0;	
	#endif
out:
	return rc;
}

/* 
*   Read data from the file given by the File structure and return the # of bytes read
*   Input: File structure, Buffer to read and length
*   Output: Number of bytes read
*/
int amfs_read_pattern_file(struct file *filp, void *buf, size_t len)
{
    mm_segment_t oldfs;
    int bytes = 0;
    oldfs = get_fs();
    set_fs(KERNEL_DS);
    bytes = vfs_read(filp, buf, len, &filp->f_pos);
    set_fs(oldfs);
    printk("KERN_AMFS: Read file: Bytes: %d\n", bytes);
    return bytes;
}


struct dentry *amfs_mount(struct file_system_type *fs_type, int flags,
			    const char *dev_name, void *raw_data)
{
	int rc = 0;
	int bytes_read;
	//char *file_name;
	//long long p_size;
	struct file *filp;
	char *pat;
	char *page_buf;
	//char *dup_page_buf;
	void *lower_path_name = (void *) dev_name;
	if ((rc = amfs_parse_options(raw_data, &filename)) != 0)
		goto out;
	printk("KERN_AMFS_MOUNT: File_name: %s, len: %d\n", filename, strlen(filename));
	if ((rc = amfs_open_pattern_file(filename, &filp)) != 0)
		goto out;
	page_buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!page_buf) {
		rc = -ENOMEM;
		goto closefile;
	}
	memset(page_buf, 0, PAGE_SIZE);
	#if 0
	sb_pr = (struct amfs_sb_private *) kzalloc(sizeof(struct amfs_sb_private), GFP_KERNEL);
	if (!sb_pr) {
		rc = -ENOMEM;
		goto free_page_buf;
	}
	sb_pr->filename = (char *) kmalloc(strlen(file_name) + 1, GFP_KERNEL);
	if (!sb_pr->filename) {
		rc = -ENOMEM;
		goto free_sb_private_data;
	}
	#endif
	//printk("AMFS_MOUNT: sb_pr: %p\n", sb_pr);
	//strcpy(sb_pr->filename, file_name);
	//sb_pr->filename[strlen(file_name)] = '\0';
	head = NULL;
	printk("KERN_AMFS: Filename in ab_amfs_priv: %s\n", filename);
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
				//printk("KERN_AMFS: line after strsep(): %s, len: %zu\n", pat, strlen(pat));
			}
			
		}
	}
	printk("AMFS_MOUNT: Head of LL: %p\n", head);
	printk("KERN_AMFS: Printing patterns after storing in prov_data:\n");
	printList(&head);
	/* need to move this delete to umount/kill */
	//delAllFromList(&(sb_pr->head));	
	goto out;

free_filename:
	if (filename)
		kfree(filename);
	if (head)
		kfree(head);
	if (page_buf)
		kfree(page_buf);
closefile:
	filp_close(filp, NULL);
out:	
	if (rc != 0) 
		return ERR_PTR(rc);
	else
		return mount_nodev(fs_type, flags, lower_path_name,
			   amfs_read_super);
}

#if 0
static void amfs_kill_block_super(struct super_block *sb)
{
	struct amfs_sb_private *sb_pr = ((struct amfs_sb_info *)sb->s_fs_info)->amfs_sb_pr;
	printList(&(sb_pr->head));
	kill_anon_super(sb);
	printk("AMFS_KILL: freeing head\n");
	if (!sb_pr) {
		printk("AMFS_KILL: sb_pr NULL\n");
		return;
	}
	delAllFromList(&(sb_pr->head));
}
#endif

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
