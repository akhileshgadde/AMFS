WRAPFS_VERSION="0.1"

EXTRA_CFLAGS += -DAMFS_VERSION=\"$(AMFS_VERSION)\"

obj-$(CONFIG_AM_FS) += amfs.o

amfs-y := dentry.o l_list.o file.o inode.o pattern_io.o main.o super.o lookup.o mmap.o
