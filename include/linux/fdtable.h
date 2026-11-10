#ifndef TUTORIAL_LINUX_FDTABLE_H
#define TUTORIAL_LINUX_FDTABLE_H

#include <tutorial/limits.h>

struct file;

struct fdtable {
	unsigned int max_fds;
	struct file *fd[MAX_FDS];
};

struct files_struct {
	struct fdtable fdt;
};

#endif
