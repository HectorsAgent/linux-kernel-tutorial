#ifndef TUTORIAL_LINUX_SOCKET_H
#define TUTORIAL_LINUX_SOCKET_H

#include <linux/uio.h>

enum { AF_INET = 2, SOCK_STREAM = 1 };

struct msghdr {
	struct iov_iter msg_iter;
	unsigned int msg_flags;
};

#endif
