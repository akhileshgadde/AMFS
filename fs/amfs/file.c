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
#include "amfsctl.h"
#include "pattern_io.h"
#include "l_list.h"

static ssize_t amfs_read(struct file *file, char __user *buf,
			   size_t count, loff_t *ppos)
{
	int err;
	ssize_t bytes = 0;
	struct super_block *sb;
	struct file *lower_file;
	struct dentry *dentry = file->f_path.dentry;
	
	lower_file = amfs_lower_file(file);
	err = vfs_read(lower_file, buf, count, ppos);
	if (err < 0) {
		printk("VFS_READ from lower file failed\n");
		goto out;
	}
	else
		bytes = err;
	sb = amfs_get_super(file);
	//printk("amfs_read: Head : %p\n", AMFS_SB(sb)->head);
	
	/* code to check if buffer contains any bad patterns */
	err = check_pattern_in_buf(buf, AMFS_SB(sb)->head); /* o if not found, -1 if found/no pattern database */
	if (err != 0) {
		printk("Read: Pattern found in database\n");
		/* set xattr and flush to disk */
		err = dentry->d_inode->i_op->setxattr(dentry,AMFS_XATTR_NAME, AMFS_XATTR_BAD, \
                        AMFS_XATTR_BAD_LEN, 0); 
        if (err != 0) 
			printk("AMFS_READ: Setting Xattr failed\n");
		else
			printk("READ: Set xttar successfully\n");
		err = -EPERM;
		goto out;
	}
	
	/* update our inode atime upon a successful lower read */
	if (err >= 0)
		fsstack_copy_attr_atime(dentry->d_inode,
					file_inode(lower_file));
out:
	if (err < 0)
		return err;
	else
		return bytes;
}

static ssize_t amfs_write(struct file *file, const char __user *buf,
			    size_t count, loff_t *ppos)
{
	int err;
	int pat_check = 0;
	struct super_block *sb;
	struct file *lower_file;
	struct dentry *dentry = file->f_path.dentry;

	lower_file = amfs_lower_file(file);
	/* code to check if malware exists in buffer and not allow write to happen */
	sb = amfs_get_super(file);
	printk("In AMFS_WRITE\n");
	pat_check = check_pattern_in_buf(buf, AMFS_SB(sb)->head);
	if (pat_check != 0) {
		printk("AMFS_WRITE: Pattern found in file. Allowing write and setting xattr.\n");
		err = dentry->d_inode->i_op->setxattr(dentry,AMFS_XATTR_NAME, AMFS_XATTR_BAD, \
						AMFS_XATTR_BAD_LEN, 0);
		if (err != 0) {
			err = -EPERM;
            printk("AMFS_WRITE:Setting Xattr for file failed\n");
			goto out;
		}
	}
	
	err = vfs_write(lower_file, buf, count, ppos);
	/* update our inode times+sizes upon a successful lower write */
	if (err >= 0) {
		fsstack_copy_inode_size(dentry->d_inode,
					file_inode(lower_file));
		fsstack_copy_attr_times(dentry->d_inode,
					file_inode(lower_file));
	}
out:
	return err;
}


struct amfs_getdents_callback {
	struct dir_context ctx;
	struct dir_context *caller;
	struct super_block *sb;
	struct dentry *dentry;
};

/* Based on ecrypts_filldir in fs/ecryptfs/file.c */
static int 
amfs_filldir(struct dir_context *ctx, const char *lower_name,
			 int lower_namelen, loff_t offset, u64 ino, unsigned int d_type)
{
	struct amfs_getdents_callback *buf = 
			container_of(ctx, struct amfs_getdents_callback, ctx);
	int rc = 0;
	#if 1
	int ret = 0;
	struct dentry *f_dentry = NULL;
	f_dentry = lookup_one_len(lower_name, buf->dentry, lower_namelen);
	if (IS_ERR(f_dentry)) {
		rc = PTR_ERR(f_dentry);
	    printk("AMFS_ERR_FILLDIR: lookup_one_len() returned [%d]\n", rc);
	} 	else {
		ret = f_dentry->d_inode->i_op->getxattr(f_dentry, AMFS_XATTR_NAME, \
            AMFS_XATTR_BAD, AMFS_XATTR_BAD_LEN);
		if (ret > 0) {
			printk("FillDir: AMFS_XATTR_BAD set for for the file. Ignoring\n");
			goto out;
		}
		printk("Filldir: GETXATTR() return value: %d\n", rc);
		dput(f_dentry);
	}
	/* code to be added here */
#endif
	buf->caller->pos = buf->ctx.pos;
	rc = !dir_emit(buf->caller, lower_name, lower_namelen, ino, d_type);
	printk("AMFS_FILLDIR: name: %s\n", lower_name);
out:
	printk("AMFS_FILLDIR: Return: [%d]\n", rc);
	return rc;
}

static int amfs_readdir(struct file *file, struct dir_context *ctx)
{
	int err;
	struct file *lower_file = NULL;
	struct inode *inode = file_inode(file);
	//struct dentry *dentry = file->f_path.dentry;
	struct amfs_getdents_callback buf = {
		.ctx.actor = amfs_filldir,
		.caller = ctx,
		//.sb = inode->i_sb,
		//.dentry = dentry,
	};
	if (!file) {
		err = -EBADF;
		goto out;
	}
	lower_file = amfs_lower_file(file);
	buf.dentry = lower_file->f_path.dentry;
	lower_file->f_pos = ctx->pos;
	err = iterate_dir(lower_file, &buf.ctx);
	ctx->pos = buf.ctx.pos;
	file->f_pos = lower_file->f_pos;
	if (err < 0)
		goto out;
	if (err >= 0)		/* copy the atime */
		fsstack_copy_attr_atime(inode, file_inode(lower_file));
out:
	return err;
}


static long amfs_unlocked_ioctl(struct file *file, unsigned int cmd,
				  unsigned long arg)
{
	long err = 0;
	int handle_flag = 0;
	int add_flag = 0, del_flag = 0;
	struct pat_struct *p_struct = NULL;
	char *pat_buf = NULL;
	struct file *lower_file;
	struct file *tmp_filp = NULL, *out_filp = NULL;
	char *temp_filename = NULL;
	unsigned long size = 0;
    struct super_block *sb;
    struct amfs_sb_info *sb_info;

	if (_IOC_TYPE(cmd) != AMFSCTL_IOCTL) {
        err = -ENOTTY;
        goto out;
    }
    if (_IOC_NR(cmd) > AMFSCTL_IOC_MAXNR) {
        err = -ENOTTY;
        goto out;
    }
    if (_IOC_DIR(cmd) & _IOC_READ)
        err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
    else if (_IOC_DIR(cmd) & _IOC_WRITE)
        err =  !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
    if (err != 0) {
        err = -EFAULT;
        goto out;
    }

    sb = amfs_get_super(file);
    sb_info = amfs_get_fs_info(sb);
	size = get_patterndb_len(sb_info->head);

	switch(cmd)
    {
        case AMFSCTL_LIST_PATTERN:
			handle_flag = 1;
			pat_buf = (char *) kmalloc(size, GFP_KERNEL);
			if (!pat_buf) {
				err = -ENOMEM;
				goto out;
			}
			copy_pattern_db(sb_info->head, pat_buf, size);
			//printk("pattern Db after copying to buf:\n");
			//print_pattern_db(pat_buf, size);
			if (copy_to_user((char *) arg, pat_buf, size)) {
				printk("IOCTL: copy_to_user error\n");
				err = -EACCES;
				goto free_patbuf;
			}
            break;

        case AMFSCTL_ADD_PATTERN:
			handle_flag = 1;
			add_flag = 1;
			/* writing pattern db to tmp file and then rename to original filename */
			break;
 
       case AMFSCTL_DEL_PATTERN:
			handle_flag = 1;
			del_flag = 1;
            break;
        case AMFSCTL_LEN_PATTERN:
			/* add check for the pointers not null in case FS is not mounted */
			handle_flag = 1;
            if (copy_to_user((unsigned long *) arg, &size, sizeof(size))) {
				printk("IOCTL_ERR: Copy_to_user error\n");
                err = -EACCES;
                goto out;
            }
            break;
	}

	if ((handle_flag) && (add_flag || del_flag)) {
		p_struct = (struct pat_struct *) kmalloc(sizeof (struct pat_struct), GFP_KERNEL);
		if (!p_struct) {
			err = -ENOMEM;
			goto out;
		}
		if (copy_from_user(p_struct, (struct pat_struct *) arg, sizeof (struct pat_struct))) {
			printk("Copy_from_user failed for p_struct\n");
			err = -EACCES;
			goto free_pat_struct;
		}
		p_struct->pattern = NULL;
		//printk("Size after copying to ker_buf: %u\n", p_struct->size);
		p_struct->pattern = (char *) kmalloc(p_struct->size + 1, GFP_KERNEL);
		if (!p_struct->pattern) {
			err = -ENOMEM;
			goto free_pat_struct;
		}
		if (copy_from_user(p_struct->pattern, STRUCT_PAT(arg), p_struct->size)) {
			err = -EACCES;
			goto free_struct_pattern;
		}
		/* Adding NULL to pattern */
		p_struct->pattern[p_struct->size] = '\0';
		if (add_flag) {
			if ((err = addtoList(&sb_info->head, p_struct->pattern, p_struct->size)) != 0)
 	       		goto free_struct_pattern;
		}
		else if (del_flag) {
			if ((err = deletePatternInList(&sb_info->head, p_struct->pattern, p_struct->size)) != 0)
        		goto free_struct_pattern;
		}
	
		temp_filename = (char *) kmalloc(strlen(sb_info->filename) + 4, GFP_KERNEL);
		if (!temp_filename) {
			err = -ENOMEM;
			goto free_struct_pattern;
		}
		strcpy(temp_filename, sb_info->filename);
		strcat(temp_filename, ".tmp");
		tmp_filp = open_output_file(temp_filename, &err, 0, 0);
		if (!tmp_filp) {
			if (err == -EACCES)
				goto closeTmpFile;
			else
				goto free_filename;
		}
		err = write_to_pat_file(tmp_filp, sb_info->head);
		if (err != 0) {
			printk("Writing to temp file failed\n");
			err = -EACCES;
			goto closeTmpFile;
		}
		out_filp = open_output_file(sb_info->filename, &err, 0, 1);
		if (!out_filp) {
			printk("out_filp error\n");
			if (err == -EACCES)
				goto closeOutputFile;
			else
				goto closeTmpFile;
		}
		err = file_rename(tmp_filp, out_filp);
		if (err != 0) {
			printk("Rename failed\n");
			err = -EACCES;
		}
		//printList(&sb_info->head); 
		goto closeOutputFile;
	}

	if (!handle_flag) {
		lower_file = amfs_lower_file(file);

		/* XXX: use vfs_ioctl if/when VFS exports it */
		if (!lower_file || !lower_file->f_op)
			goto out;
		if (lower_file->f_op->unlocked_ioctl)
			err = lower_file->f_op->unlocked_ioctl(lower_file, cmd, arg);

		/* some ioctls can change inode attributes (EXT2_IOC_SETFLAGS) */
		if (!err)
			fsstack_copy_attr_all(file_inode(file),
					      file_inode(lower_file));
	}
closeOutputFile:
	if (out_filp)
        filp_close(out_filp, NULL);

closeTmpFile:
    if (err != 0) 
		vfs_unlink(tmp_filp->f_path.dentry->d_parent->d_inode, tmp_filp->f_path.dentry, NULL);
     if (tmp_filp)
        filp_close(tmp_filp, NULL);

free_filename:
	if (temp_filename)
		kfree(temp_filename);
free_struct_pattern:
	if ((p_struct) && (p_struct->pattern))
		kfree(p_struct->pattern);
free_pat_struct:
	if (p_struct)
		kfree(p_struct);
free_patbuf:
	if (pat_buf)
		kfree(pat_buf);
out:
	return err;
}

#ifdef CONFIG_COMPAT
static long amfs_compat_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	long err = -ENOTTY;
	struct file *lower_file;

	lower_file = amfs_lower_file(file);

	/* XXX: use vfs_ioctl if/when VFS exports it */
	if (!lower_file || !lower_file->f_op)
		goto out;
	if (lower_file->f_op->compat_ioctl)
		err = lower_file->f_op->compat_ioctl(lower_file, cmd, arg);

out:
	return err;
}
#endif

static int amfs_mmap(struct file *file, struct vm_area_struct *vma)
{
	int err = 0;
	bool willwrite;
	struct file *lower_file;
	const struct vm_operations_struct *saved_vm_ops = NULL;

	/* this might be deferred to mmap's writepage */
	willwrite = ((vma->vm_flags | VM_SHARED | VM_WRITE) == vma->vm_flags);

	/*
	 * File systems which do not implement ->writepage may use
	 * generic_file_readonly_mmap as their ->mmap op.  If you call
	 * generic_file_readonly_mmap with VM_WRITE, you'd get an -EINVAL.
	 * But we cannot call the lower ->mmap op, so we can't tell that
	 * writeable mappings won't work.  Therefore, our only choice is to
	 * check if the lower file system supports the ->writepage, and if
	 * not, return EINVAL (the same error that
	 * generic_file_readonly_mmap returns in that case).
	 */
	lower_file = amfs_lower_file(file);
	if (willwrite && !lower_file->f_mapping->a_ops->writepage) {
		err = -EINVAL;
		printk(KERN_ERR "amfs: lower file system does not "
		       "support writeable mmap\n");
		goto out;
	}

	/*
	 * find and save lower vm_ops.
	 *
	 * XXX: the VFS should have a cleaner way of finding the lower vm_ops
	 */
	if (!AMFS_F(file)->lower_vm_ops) {
		err = lower_file->f_op->mmap(lower_file, vma);
		if (err) {
			printk(KERN_ERR "amfs: lower mmap failed %d\n", err);
			goto out;
		}
		saved_vm_ops = vma->vm_ops; /* save: came from lower ->mmap */
	}

	/*
	 * Next 3 lines are all I need from generic_file_mmap.  I definitely
	 * don't want its test for ->readpage which returns -ENOEXEC.
	 */
	file_accessed(file);
	vma->vm_ops = &amfs_vm_ops;

	file->f_mapping->a_ops = &amfs_aops; /* set our aops */
	if (!AMFS_F(file)->lower_vm_ops) /* save for our ->fault */
		AMFS_F(file)->lower_vm_ops = saved_vm_ops;

out:
	return err;
}

static int amfs_open(struct inode *inode, struct file *file)
{
	int err = 0;
	int ret = 0;
	struct file *lower_file = NULL;
	struct path lower_path;
	struct dentry *dentry = file->f_path.dentry;
	
	/* don't open unhashed/deleted files */
	if (d_unhashed(file->f_path.dentry)) {
		err = -ENOENT;
		goto out_err;
	}

	file->private_data =
		kzalloc(sizeof(struct amfs_file_info), GFP_KERNEL);
	if (!AMFS_F(file)) {
		err = -ENOMEM;
		goto out_err;
	}

	/* open lower object and link amfs's file struct to lower's */
	amfs_get_lower_path(file->f_path.dentry, &lower_path);
	/* Checking if file xattr is bad and then not allowing open to happen if bad */
	
	if(S_ISDIR(file->f_path.dentry->d_inode->i_mode))
		printk("Opening a directory. skipping getattr()\n");
	else {	
		printk("Calling getxattr()\n");
		ret = dentry->d_inode->i_op->getxattr(dentry, AMFS_XATTR_NAME, \
						AMFS_XATTR_BAD, AMFS_XATTR_BAD_LEN); 	
		printk("AMFS_OPEN: Getxattr() return: %d\n", ret);
		if (ret > 0) {
			err = -ENOENT;
			path_put(&lower_path);
			amfs_set_lower_file(file, NULL);
			kfree(AMFS_F(file));
			goto out_err;
		}
	}
	lower_file = dentry_open(&lower_path, file->f_flags, current_cred());
	path_put(&lower_path);
	if (IS_ERR(lower_file)) {
		err = PTR_ERR(lower_file);
		lower_file = amfs_lower_file(file);
		if (lower_file) {
			amfs_set_lower_file(file, NULL);
			fput(lower_file); /* fput calls dput for lower_dentry */
		}
	} else {
		amfs_set_lower_file(file, lower_file);
	}

	if (err)
		kfree(AMFS_F(file));
	else
		fsstack_copy_attr_all(inode, amfs_lower_inode(inode));
out_err:
	printk("Returning from AMFS_OPEN with err: %d\n", err);
	return err;
}

static int amfs_flush(struct file *file, fl_owner_t id)
{
	int err = 0;
	struct file *lower_file = NULL;

	lower_file = amfs_lower_file(file);
	if (lower_file && lower_file->f_op && lower_file->f_op->flush) {
		filemap_write_and_wait(file->f_mapping);
		err = lower_file->f_op->flush(lower_file, id);
	}

	return err;
}

/* release all lower object references & free the file info structure */
static int amfs_file_release(struct inode *inode, struct file *file)
{
	struct file *lower_file;

	lower_file = amfs_lower_file(file);
	if (lower_file) {
		amfs_set_lower_file(file, NULL);
		fput(lower_file);
	}

	kfree(AMFS_F(file));
	return 0;
}

static int amfs_fsync(struct file *file, loff_t start, loff_t end,
			int datasync)
{
	int err;
	struct file *lower_file;
	struct path lower_path;
	struct dentry *dentry = file->f_path.dentry;

	err = __generic_file_fsync(file, start, end, datasync);
	if (err)
		goto out;
	lower_file = amfs_lower_file(file);
	amfs_get_lower_path(dentry, &lower_path);
	err = vfs_fsync_range(lower_file, start, end, datasync);
	amfs_put_lower_path(dentry, &lower_path);
out:
	return err;
}

static int amfs_fasync(int fd, struct file *file, int flag)
{
	int err = 0;
	struct file *lower_file = NULL;

	lower_file = amfs_lower_file(file);
	if (lower_file->f_op && lower_file->f_op->fasync)
		err = lower_file->f_op->fasync(fd, lower_file, flag);

	return err;
}

static ssize_t amfs_aio_read(struct kiocb *iocb, const struct iovec *iov,
			       unsigned long nr_segs, loff_t pos)
{
	int err = -EINVAL;
	struct file *file, *lower_file;

	file = iocb->ki_filp;
	lower_file = amfs_lower_file(file);
	if (!lower_file->f_op->aio_read)
		goto out;
	/*
	 * It appears safe to rewrite this iocb, because in
	 * do_io_submit@fs/aio.c, iocb is a just copy from user.
	 */
	get_file(lower_file); /* prevent lower_file from being released */
	iocb->ki_filp = lower_file;
	err = lower_file->f_op->aio_read(iocb, iov, nr_segs, pos);
	iocb->ki_filp = file;
	fput(lower_file);
	/* update upper inode atime as needed */
	if (err >= 0 || err == -EIOCBQUEUED)
		fsstack_copy_attr_atime(file->f_path.dentry->d_inode,
					file_inode(lower_file));
out:
	return err;
}

static ssize_t amfs_aio_write(struct kiocb *iocb, const struct iovec *iov,
				unsigned long nr_segs, loff_t pos)
{
	int err = -EINVAL;
	struct file *file, *lower_file;

	file = iocb->ki_filp;
	lower_file = amfs_lower_file(file);
	if (!lower_file->f_op->aio_write)
		goto out;
	/*
	 * It appears safe to rewrite this iocb, because in
	 * do_io_submit@fs/aio.c, iocb is a just copy from user.
	 */
	get_file(lower_file); /* prevent lower_file from being released */
	iocb->ki_filp = lower_file;
	err = lower_file->f_op->aio_write(iocb, iov, nr_segs, pos);
	iocb->ki_filp = file;
	fput(lower_file);
	/* update upper inode times/sizes as needed */
	if (err >= 0 || err == -EIOCBQUEUED) {
		fsstack_copy_inode_size(file->f_path.dentry->d_inode,
					file_inode(lower_file));
		fsstack_copy_attr_times(file->f_path.dentry->d_inode,
					file_inode(lower_file));
	}
out:
	return err;
}

/*
 * Amfs cannot use generic_file_llseek as ->llseek, because it would
 * only set the offset of the upper file.  So we have to implement our
 * own method to set both the upper and lower file offsets
 * consistently.
 */
static loff_t amfs_file_llseek(struct file *file, loff_t offset, int whence)
{
	int err;
	struct file *lower_file;

	err = generic_file_llseek(file, offset, whence);
	if (err < 0)
		goto out;

	lower_file = amfs_lower_file(file);
	err = generic_file_llseek(lower_file, offset, whence);

out:
	return err;
}

/*
 * Amfs read_iter, redirect modified iocb to lower read_iter
 */
ssize_t
amfs_read_iter(struct kiocb *iocb, struct iov_iter *iter)
{
	int err;
	struct file *file = iocb->ki_filp, *lower_file;

	lower_file = amfs_lower_file(file);
	if (!lower_file->f_op->read_iter) {
		err = -EINVAL;
		goto out;
	}

	get_file(lower_file); /* prevent lower_file from being released */
	iocb->ki_filp = lower_file;
	err = lower_file->f_op->read_iter(iocb, iter);
	iocb->ki_filp = file;
	fput(lower_file);
	/* update upper inode atime as needed */
	if (err >= 0 || err == -EIOCBQUEUED)
		fsstack_copy_attr_atime(file->f_path.dentry->d_inode,
					file_inode(lower_file));
out:
	return err;
}

/*
 * Amfs write_iter, redirect modified iocb to lower write_iter
 */
ssize_t
amfs_write_iter(struct kiocb *iocb, struct iov_iter *iter)
{
	int err;
	struct file *file = iocb->ki_filp, *lower_file;

	lower_file = amfs_lower_file(file);
	if (!lower_file->f_op->write_iter) {
		err = -EINVAL;
		goto out;
	}

	get_file(lower_file); /* prevent lower_file from being released */
	iocb->ki_filp = lower_file;
	err = lower_file->f_op->write_iter(iocb, iter);
	iocb->ki_filp = file;
	fput(lower_file);
	/* update upper inode times/sizes as needed */
	if (err >= 0 || err == -EIOCBQUEUED) {
		fsstack_copy_inode_size(file->f_path.dentry->d_inode,
					file_inode(lower_file));
		fsstack_copy_attr_times(file->f_path.dentry->d_inode,
					file_inode(lower_file));
	}
out:
	return err;
}

const struct file_operations amfs_main_fops = {
	.llseek		= generic_file_llseek,
	.read		= amfs_read,
	.write		= amfs_write,
	.unlocked_ioctl	= amfs_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= amfs_compat_ioctl,
#endif
	.mmap		= amfs_mmap,
	.open		= amfs_open,
	.flush		= amfs_flush,
	.release	= amfs_file_release,
	.fsync		= amfs_fsync,
	.fasync		= amfs_fasync,
	.aio_read	= amfs_aio_read,
	.aio_write	= amfs_aio_write,
	.read_iter	= amfs_read_iter,
	.write_iter	= amfs_write_iter,
};

/* trimmed directory options */
const struct file_operations amfs_dir_fops = {
	.llseek		= amfs_file_llseek,
	.read		= generic_read_dir,
	.iterate	= amfs_readdir,
	.unlocked_ioctl	= amfs_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= amfs_compat_ioctl,
#endif
	.open		= amfs_open,
	.release	= amfs_file_release,
	.flush		= amfs_flush,
	.fsync		= amfs_fsync,
	.fasync		= amfs_fasync,
};
