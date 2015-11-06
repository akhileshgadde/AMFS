#ifndef AMFS_READ_PATTERN_
#define AMFS_READ_PATTERN_

#include <linux/fs.h>
#include "l_list.h"

int write_output_file(struct file *filp, void *buf, unsigned int size);
long write_to_pat_file(struct file *filp, struct ListNode *head);
int amfs_parse_options(char *data, char **file_name);
int amfs_read_pattern_file(struct file *filp, void *buf, size_t len);
int amfs_check_pattern_file(struct file *filp);
int amfs_open_pattern_file(const char *file_name, struct file **filp);
int amfs_read_pattern_file(struct file *filp, void *buf, size_t len);
int file_rename(struct file *tmp_filp, struct file *out_filp);
struct file* open_output_file(const char *filename, long *err, umode_t mode, int flags);

#endif
