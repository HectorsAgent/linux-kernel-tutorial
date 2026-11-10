#ifndef TUTORIAL_NET_SOCK_H
#define TUTORIAL_NET_SOCK_H

#include <linux/types.h>

struct net;

struct sock {
	struct net *sk_net;
	u32 sk_rcv_saddr, sk_daddr;
	u16 sk_num, sk_dport;
	u8 sk_protocol;
};

static inline struct net *sock_net(const struct sock *sk)
{
	return sk->sk_net;
}

#endif
