#ifndef AMFS_READ_PATTERN_
#define AMFS_READ_PATTERN_

struct  ListNode *head;
char *filename;

int amfs_read_pattern_file(struct file *filp, void *buf, size_t len);

#endif
