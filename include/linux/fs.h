#ifndef TUTORIAL_LINUX_FS_H
#define TUTORIAL_LINUX_FS_H

struct inode;
struct file;

struct file_operations {
	int (*release)(struct inode *, struct file *);
};

struct file {
	const struct file_operations *f_op;
	void *private_data;
	unsigned int f_count; /* Enkel referensräkning, inga atomics i labbet. */
};

#endif
