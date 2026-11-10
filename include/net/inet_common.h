#ifndef TUTORIAL_NET_INET_COMMON_H
#define TUTORIAL_NET_INET_COMMON_H

#include <linux/net.h>

extern const struct proto_ops inet_stream_ops;
int inet_sendmsg(struct socket *sock, struct msghdr *msg, size_t size);

#endif
