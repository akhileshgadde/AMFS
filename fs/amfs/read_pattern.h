#ifndef AMFS_READ_PATTERN_
#define AMFS_READ_PATTERN_

struct amfs_sb_private *sb_pr;

int amfs_read_pattern_file(struct file *filp, void *buf, size_t len);

#endif
