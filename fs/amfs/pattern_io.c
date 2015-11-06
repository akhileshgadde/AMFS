#include "pattern_io.h"
#include "amfs.h"


int amfs_parse_options(char *data, char **file_name)
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
int amfs_open_pattern_file(const char *file_name, struct file **filp)
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

/* 
*       Write data to the file given by the File structure and return the # of bytes read
*       Input: File structure, Buffer to read and size
*       Output: #Bytes written
*/
int write_output_file(struct file *filp, void *buf, int size)
{
    mm_segment_t oldfs;
    int bytes = 0;
    oldfs = get_fs();
    set_fs(KERNEL_DS);
    bytes = vfs_write(filp, buf, size, &filp->f_pos);
    set_fs(oldfs);
        printk("KERN: Write to file: buytes: %d\n", bytes);
        return bytes;
}

