#include "amfsctl.h"
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int readargs(int argc, char **argv, char **pat_buf, char **mnt_pt);
void print_usage();
int copy_args(char **buf, char *optarg);

int main(int argc, char **argv)
{
	int rc = 0;
	char *pat_buf, *mnt_pt;
	if (argc < 2) {
		rc = -1;
		print_usage();
		goto out;
	}
	
	rc = readargs(argc, argv, &pat_buf, &mnt_pt);
	if (rc != 0) {
		print_usage();
		rc = -1;
		goto free_buf;
	}
	else {
		printf("main: pat_buf: %s, mnt_pt: %s\n", pat_buf, mnt_pt);
	}

free_buf:
	if (pat_buf)
		free(pat_buf);
	if (mnt_pt)
		free(mnt_pt);
out:
	return rc;
}


int copy_args(char **buf, char *optarg)
{
	int rc = 0;
	*buf = malloc(strlen(optarg) + 1);
    if (!*buf) {
    	rc = -ENOMEM;
        goto out;
    }
	printf("In copy_args\n");
    strcpy(*buf, optarg);
    //*buf[strlen(optarg)] = '\0';
out:
	return rc;
}

int readargs(int argc, char **argv, char **pat_buf, char **mnt_pt)
{
	int opt = 0;
	int rc = 0;
	int list_flag = 0, add_flag = 0, del_flag = 0, mnt_pt_flag = 0;;
	//opterr = 0;
	while ((opt = getopt(argc, argv, "la:r:")) != -1) {
		switch (opt) {
			case 'l':
				list_flag = 1;	
				break;
			case 'a':
				add_flag = 1;
				rc = copy_args(pat_buf, optarg);
				if (rc != 0)
					goto out;
				break;
			case 'r':
				del_flag = 1;
				rc = copy_args(pat_buf, optarg);
                if (rc != 0)
                   	goto out;
				break;
			default: 
					rc = -1;
					goto out;
		}
	}
	if (optind < argc) {
		rc = copy_args(mnt_pt, argv[optind]);
		if (rc != 0)
			goto out;
		mnt_pt_flag = 1;
		optind++;
	}
	printf("optind: %d, argc: %d\n",optind, argc);
	if ((optind != argc) || (mnt_pt_flag == 0)) {
		rc = -1;
		printf("mnt_pt not given, freeing \n");
		goto out;
	}
	goto out;
out:
	return rc;
}

void print_usage()
{
	printf("ERROR: Use the correct format as listed below\n");
	printf("./amfsctl [-l] [-a new-pattern] [-r old-pattern] mnt-point\n");
	printf("-l: list the existing patterns.\n");
	printf("-a <new-pattern>: Adding a given new-pattern.\n");
	printf("-r <old-patt>: Deleting an old pattern.\n");
}

