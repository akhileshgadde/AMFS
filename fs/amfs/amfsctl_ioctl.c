#include <linux/ioctl.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include "amfs.h"
#include "amfsctl.h"
#include "read_pattern.h"

static int my_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	int rc = 0;
	if (_IOC_TYPE(cmd) != AMFSCTL_IOCTL) {
		rc = -ENOTTY;
		goto out;
	}		
	if (_IOC_NR(cmd) > AMFSCTL_IOC_MAXNR) {
		rc = -ENOTTY;
		goto out;
	}
	if (_IOC_DIR(cmd) & _IOC_READ)
    	rc = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
    	rc =  !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	if (rc != 0) { 
		rc = -EFAULT;
		goto out;
	}
	
	switch(cmd)
	{
		case AMFSCTL_LIST_PATTERN:
			
			break;
		case AMFSCTL_ADD_PATTERN:
			
			break;
		case AMFSCTL_DEl_PATTERN:
			
			break;
		case AMFSCTL_LEN_PATTERN: 
			
			break;
		default:
				rc = -ENOTTY;
				goto out;
	}
out:
	return rc;
}	
