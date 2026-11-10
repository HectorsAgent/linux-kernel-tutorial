#include <linux/fs.h>
#include <linux/file.h>
#include <linux/net.h>
#include <linux/sched.h>
#include <linux/syscalls.h>
#include <net/inet_common.h>
#include <net/sock.h>
#include <tutorial/lab.h>
#include <tutorial/trace.h>
#include <errno.h>
#include <stdlib.h>

static int sock_close(struct inode *inode, struct file *file)
{
	(void)inode;
	/* tcp_sock ägs fortfarande av namespacet; TCP-stängning modelleras inte. */
	free(file->private_data);
	return 0;
}

static const struct file_operations socket_file_ops = { .release = sock_close };

struct file *sock_alloc_file(struct socket *sock)
{
	struct file *file = calloc(1, sizeof(*file));

	if (!file)
		return NULL;
	file->f_op = &socket_file_ops;
	file->private_data = sock;
	file->f_count = 1;
	sock->file = file;
	return file;
}

struct socket *sock_from_file(struct file *file)
{
	if (file->f_op != &socket_file_ops)
		return NULL;
	return file->private_data;
}

int sock_sendmsg(struct socket *sock, struct msghdr *msg)
{
	trace_socket("sock_sendmsg", sock->sk);
	if (!sock->ops || !sock->ops->sendmsg)
		return -EOPNOTSUPP;
	return sock->ops->sendmsg(sock, msg, msg->msg_iter.count);
}

int __sys_sendto(int fd, const void *buf, size_t len, unsigned int flags,
	const struct sockaddr *addr, int addr_len)
{
	struct fd f;
	struct socket *sock;
	struct msghdr msg = { .msg_flags = flags };
	int result;

	trace_task("__sys_sendto", fd);
	/* Bara anslutna sockets och flags=0 i den första syscall-modellen. */
	if (flags || addr || addr_len)
		return -EOPNOTSUPP;
	result = import_ubuf(buf, len, &msg.msg_iter);
	if (result < 0)
		return result;
	f = fdget((unsigned int)fd);
	if (!f.file)
		return -EBADF;
	sock = sock_from_file(f.file);
	if (!sock) {
		fdput(f);
		return -ENOTSOCK;
	}
	trace_socket("sock_from_file", sock->sk);
	result = sock_sendmsg(sock, &msg);
	fdput(f);
	return result;
}

/* Labbet installerar en redan ansluten IPv4/TCP-socket som ett filobjekt. */
int task_install_socket(struct task_struct *task, struct sock *sk)
{
	struct task_struct *previous = current;
	struct socket *sock;
	struct file *file;
	int fd;

	if (!sk)
		return -EINVAL;
	current = task;
	fd = get_unused_fd_flags(0);
	current = previous;
	if (fd < 0)
		return fd;
	sock = calloc(1, sizeof(*sock));
	if (!sock)
		return -ENOMEM;
	sock->type = SOCK_STREAM;
	sock->sk = sk;
	sock->ops = &inet_stream_ops;
	file = sock_alloc_file(sock);
	if (!file) {
		free(sock);
		return -ENOMEM;
	}
	current = task;
	fd_install((unsigned int)fd, file);
	current = previous;
	return fd;
}
