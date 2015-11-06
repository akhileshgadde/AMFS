#include "amfsctl.h"
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>

int readargs(int argc, char **argv);
void print_usage();
int copy_args(char **buf, char *optarg);
unsigned long get_pattern_count(int fd);
int get_pattern_db(int fd);
void print_pattern_db(char *buf, unsigned long size);

char *pat_buf = NULL, *mnt_pt = NULL;
int list_flag = 0, add_flag = 0, del_flag = 0; 

int main(int argc, char **argv)
{
	int rc = 0;
	int fd = 0;
	if (argc < 2) {
		rc = -1;
		print_usage();
		goto out;
	}
	rc = readargs(argc, argv);
	printf("main: Pattern: %s, mount point: %s\n", pat_buf, mnt_pt);
	if (rc != 0) {
		print_usage();
		rc = -1;
		goto free_buf;
	}
	fd = open(mnt_pt, O_RDONLY);
	if (fd < 0) {
		printf("Error: opening file\n");
		rc = -1;
		goto free_buf;
	}
	if (list_flag == 1) {
		printf("List flag set, calling\n");
		rc = get_pattern_db(fd);
		if (rc < 0)
			goto free_buf;
	}
free_buf:
	#if 1
	if (pat_buf)
		free(pat_buf);
	if (mnt_pt)
		free(mnt_pt);
	#endif
out:
	if (fd > 0)
		close(fd);
	return rc;
}

void print_pattern_db(char *buf, unsigned long size)
{
	//int i = 0;
	printf("pattern database:\n");
	#if 0
	while (i < size) {
		printf("%c", buf[i]);
		i++;
	}
	#endif
	printf("%s", buf);
}

int get_pattern_db(int fd)
{
	int rc = 0;
	unsigned long count  = get_pattern_count(fd);
	if (count < 0) {
		rc = -1;
		goto out;
	}
	pat_buf = (char *) malloc(count);
	if(ioctl(fd, AMFSCTL_LIST_PATTERN, pat_buf) == -1) {
		printf("AMFSCTL_LIST_PATTERN: Error in getting pattern db\n");
		rc = -1;
		goto out;
	}
	print_pattern_db(pat_buf, count);
out:
	return rc;
}

unsigned long get_pattern_count(int fd)
{
	unsigned long count;
	if (ioctl(fd, AMFSCTL_LEN_PATTERN, &count) == -1) {
		printf("AMFSCTL_LEN_PATTERN: Error in getting length\n");
		count = -1;
		goto out;
	}
	else {
		printf("AMFSCTL_LEN: %lu\n", count);
	}
out:
	return count;
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
    (*buf)[strlen(optarg)] = '\0';
out:
	return rc;
}

int readargs(int argc, char **argv)
{
	int opt = 0;
	int rc = 0;
	int mnt_pt_flag = 0;
	//opterr = 0;
	while ((opt = getopt(argc, argv, "la:r:")) != -1) {
		switch (opt) {
			case 'l':
				list_flag = 1;	
				break;
			case 'a':
				add_flag = 1;
				rc = copy_args(&pat_buf, optarg);
				if (rc != 0)
					goto out;
				break;
			case 'r':
				del_flag = 1;
				rc = copy_args(&pat_buf, optarg);
                if (rc != 0)
                   	goto out;
				break;
			default: 
					rc = -1;
					goto out;
		}
	}
	if (optind < argc) {
		rc = copy_args(&mnt_pt, argv[optind]);
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
