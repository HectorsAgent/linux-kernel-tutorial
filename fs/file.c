#include <linux/fdtable.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/sched.h>
#include <tutorial/trace.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>

struct file *fget(unsigned int fd)
{
	struct fdtable *fdt;
	struct file *file;

	if (!current || !current->files)
		return NULL;
	fdt = &current->files->fdt;
	if (fd >= fdt->max_fds || !fdt->fd[fd])
		return NULL;
	file = fdt->fd[fd];
	file->f_count++;
	return file;
}

void fput(struct file *file)
{
	if (!file)
		return;
	assert(file->f_count > 0);
	if (--file->f_count)
		return;
	if (file->f_op && file->f_op->release)
		file->f_op->release(NULL, file);
	free(file);
}

struct fd fdget(unsigned int fd)
{
	trace_task("fdget", (int)fd);
	return (struct fd) { .file = fget(fd) };
}

void fdput(struct fd fd)
{
	fput(fd.file);
}

int get_unused_fd_flags(unsigned int flags)
{
	struct fdtable *fdt;

	if (!current || !current->files)
		return -ESRCH;
	if (flags)
		return -EOPNOTSUPP;
	fdt = &current->files->fdt;
	for (unsigned int i = 0; i < fdt->max_fds; i++)
		if (!fdt->fd[i])
			return (int)i;
	return -EMFILE;
}

void fd_install(unsigned int fd, struct file *file)
{
	assert(current && current->files && file);
	assert(fd < current->files->fdt.max_fds);
	assert(!current->files->fdt.fd[fd]);
	/* FD-tabellen tar över filens befintliga referens. */
	current->files->fdt.fd[fd] = file;
}

int close_fd(unsigned int fd)
{
	struct file *file;

	if (!current || !current->files || fd >= current->files->fdt.max_fds)
		return -EBADF;
	file = current->files->fdt.fd[fd];
	if (!file)
		return -EBADF;
	current->files->fdt.fd[fd] = NULL;
	fput(file);
	return 0;
}
