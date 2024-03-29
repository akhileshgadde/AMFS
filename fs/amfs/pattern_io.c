#include "pattern_io.h"
#include "amfs.h"
#include "l_list.h"

int find_full_path(const char *filename)
{
    int err = 0;
    struct path lower_path;
    char *path;
    char *kbuf;
    err = kern_path(filename, LOOKUP_FOLLOW | LOOKUP_DIRECTORY,
            &lower_path);
    if (err) {
  		printk(KERN_ERR "amfs: error accessing "
        	       "lower directory '%s'\n", filename);
    	goto out;
    }

    kbuf = kmalloc(PAGE_SIZE, GFP_KERNEL);
    if (!kbuf)
        goto out;
    path = d_path(&lower_path, kbuf, PAGE_SIZE);
    if (IS_ERR(path)) {
        err = PTR_ERR(path);
        goto out_free;
    }
    printk("Full path: %s\n", path);
    
out_free:
    kfree(kbuf);
    return err;
out:
    return err;
}


int amfs_parse_options(char *data, char **file_name)
{
    int rc = 0;
    char *token1, *token2;
	//char *kbuf;
	//char *path;
    if (!data) {
        rc = -EINVAL;
        goto out;
    }
    token1 = strstr(data, "pattdb=");
    if (!token1) {
		printk("KERN_ERROR: AMFS_PARSE_OPTIONS: Invalid options.\n");
        rc = -EINVAL;
        goto out;
    }
    token2 = strstr(token1, "=");
	if ((!token2) || !(token2+1)) {
		printk("KERN_ERROR: AMFS_PARSE_OPTIONS: Invalid options specified at mount.\n");
		rc = -EINVAL;
		goto out;
	}
	token2++;
	/* check for any spaces after '=' in patdb */
	while (*token2 == ' ')
		token2++;
    *file_name = token2;
	//rc = find_full_path(*file_name);

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
int amfs_open_pattern_file(const char *file_name, struct file **filp)
{
    int rc = 0;
    if (file_name == NULL) {
        rc = -ENOENT;
        goto out;
    }
    *filp = filp_open(file_name, O_RDONLY, 0);
    if ((*filp == NULL) || (IS_ERR(*filp))) {
        rc = PTR_ERR(*filp);
        printk("KERNEL_ERR: AMFS: Error opening pattern file: %s\n", file_name);
        goto out;
    }
    if ((rc = amfs_check_pattern_file(*filp)) != 0)
        goto out;
    if ((!(*filp)->f_op) || (!(*filp)->f_op->read)) {
        printk("AMFS: No read permission on pattern file\n");
        rc = -EACCES;
        goto out;
    }
    (*filp)->f_pos = 0;
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
    //printk("KERN_AMFS: Read file: Bytes: %d\n", bytes);
    return bytes;
}

long write_to_pat_file(struct file *filp, struct ListNode *head)
{
	long rc = 0;
	int bytes = 0;
	struct ListNode *temp = head;
	if ((!filp) || (!head)) {
		rc = -EINVAL;
		goto out;
	}
	while (temp != NULL) {
		bytes = write_output_file(filp, temp->pattern, temp->len);
		if (bytes < 0) {
			rc = -EINVAL;
			goto out;
		}
		temp = temp->next;
	}
out:
	return rc;
}

/* Check if the buffer contains any of the malware patterns */
int check_pattern_in_buf(const char *buf, struct ListNode *head)
{
	int rc = 0;
	char *token = NULL;
	struct ListNode *temp = head;
	if (head == NULL) {
		rc = -1;
		goto out;
	}
	while (temp != NULL) {
		token = strstr(buf, temp->pattern);
		if (token != NULL) {
			rc = -1; /* Found */
			goto out;
		}
		temp = temp->next;		
	}
out:	
	return rc;
}


/* 
*       Write data to the file given by the File structure and return the # of bytes read
*       Input: File structure, Buffer to read and size
*       Output: #Bytes written
*/
int write_output_file(struct file *filp, void *buf, unsigned int size)
{
    mm_segment_t oldfs;
    int bytes = 0;
    oldfs = get_fs();
    set_fs(KERNEL_DS);
    bytes = vfs_write(filp, buf, size, &filp->f_pos);
	vfs_write(filp, "\n", 1, &filp->f_pos);
	bytes += 1;
    set_fs(oldfs);
    return bytes;
}

/*
*       Function to open the output file for writing and check for correct permissions on the opened file.
*       Input: File path and err to fill in errno and flags to indicate temp or output file.
*       Output: File structure of opoened file on SUCCESS and NULL on FAILURE
*/
struct file* open_output_file(const char *filename, long *err, umode_t mode, int flags)
{
    struct file *filp = NULL;
    if (filename == NULL) {
        *err = -EBADF;
        goto returnFailure;
    }
    if (flags == 0) /* opening temp file */
        filp = filp_open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0);
    else if (flags == 1)  /* opening output file */
        filp = filp_open(filename, O_WRONLY, 0);
	
	if (!filp) {
        if ((*err = amfs_check_pattern_file(filp)) != 0)
                goto returnFailure;
    }
    if (IS_ERR(filp)) {
        printk("KERN_ERR: AMFS: Outputfile write error %d\n", (int) PTR_ERR(filp));
        *err = -EPERM;
        filp = NULL;
        goto returnFailure;
    }
    if ((!filp->f_op) || (!filp->f_op->write)) {
        printk("KERN: AMFS: No write permission on Output file %d\n", (int) PTR_ERR(filp));
        *err = -EACCES;
        filp = NULL;
        goto returnFailure;
    }
    filp->f_pos = 0;
returnFailure:
    return filp;
}

/*
*   Function to rename two given files (passed as their file structure pointers)
*   Input: File1 structure to be renamed and File2 structure to which the File1 should be renamed.
    output: 0 on SUCCESS and errno on FAILURE. Unlinks the temp file if rename fails.   
*/
int file_rename(struct file *tmp_filp, struct file *out_filp)
{
    int err = 0;
    if ((!tmp_filp) || (!out_filp)) {
        err = -EINVAL;
        goto end;
    }
	if (tmp_filp->f_path.dentry->d_inode->i_ino == out_filp->f_path.dentry->d_inode->i_ino) {
    	err = -EPERM;
        goto end;
    }
    err = vfs_rename(tmp_filp->f_path.dentry->d_parent->d_inode, tmp_filp->f_path.dentry, \
            out_filp->f_path.dentry->d_parent->d_inode, out_filp->f_path.dentry, NULL, 0);
    if (err != 0) {
        printk("KERN_ERR: AMFS_RENAME: File rename error\n");
        err = -EACCES;
        goto end;
    }
end:
    return err;
}
