#ifndef AMFS_READ_PATTERN_
#define AMFS_READ_PATTERN_

#include <linux/fs.h>


int amfs_parse_options(char *data, char **file_name);
int amfs_read_pattern_file(struct file *filp, void *buf, size_t len);
int amfs_check_pattern_file(struct file *filp);
int amfs_open_pattern_file(const char *file_name, struct file **filp);
int amfs_read_pattern_file(struct file *filp, void *buf, size_t len);
int write_output_file(struct file *filp, void *buf, int size);

#endif
