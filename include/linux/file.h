#ifndef TUTORIAL_LINUX_FILE_H
#define TUTORIAL_LINUX_FILE_H

#include <linux/fs.h>

struct fd {
	struct file *file;
};

struct file *fget(unsigned int fd);
void fput(struct file *file);
struct fd fdget(unsigned int fd);
void fdput(struct fd fd);
int get_unused_fd_flags(unsigned int flags);
void fd_install(unsigned int fd, struct file *file);
int close_fd(unsigned int fd);

#endif
