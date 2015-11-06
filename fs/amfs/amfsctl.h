#ifndef AMFSCTL_IOCTL_H
#define AMFSCTL_IOCTL_H

struct pat_struct {
	unsigned int size;
	char *pattern;
};

/* using k as magic number */
#define AMFSCTL_IOCTL 'k'

#define AMFSCTL_LIST_PATTERN _IOR(AMFSCTL_IOCTL, 1, char *)
#define AMFSCTL_ADD_PATTERN _IOW(AMFSCTL_IOCTL, 2, struct pat_buf *)
#define AMFSCTL_DEL_PATTERN _IOW(AMFSCTL_IOCTL, 3, struct pat_buf *)
#define AMFSCTL_LEN_PATTERN _IOR(AMFSCTL_IOCTL, 4, unsigned long *)

#define AMFSCTL_IOC_MAXNR 4

#endif
