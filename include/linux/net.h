#ifndef TUTORIAL_LINUX_NET_H
#define TUTORIAL_LINUX_NET_H

#include <linux/socket.h>

struct file;
struct sock;
struct socket;

struct proto_ops {
	int family;
	int (*sendmsg)(struct socket *, struct msghdr *, size_t);
};

struct socket {
	short type;
	struct file *file;
	struct sock *sk;
	const struct proto_ops *ops;
};

struct socket *sock_from_file(struct file *file);
struct file *sock_alloc_file(struct socket *sock);
int sock_sendmsg(struct socket *sock, struct msghdr *msg);

#endif
