
int find_full_path(const char *filename)
{
	int err = 0;
	struct super_block *lower_sb;
	struct path lower_path;
	char *path;
	char *kbuf;
	err = kern_path(filename, LOOKUP_FOLLOW | LOOKUP_DIRECTORY,
			&lower_path);
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
	
