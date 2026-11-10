#ifndef TUTORIAL_LINUX_SYSCALLS_H
#define TUTORIAL_LINUX_SYSCALLS_H

#include <linux/types.h>

struct sockaddr;

/* Simulerad syscall: bara ansluten socket, flags=0 och addr=NULL stöds. */
int __sys_sendto(int fd, const void *buf, size_t len, unsigned int flags,
	const struct sockaddr *addr, int addr_len);

#endif
