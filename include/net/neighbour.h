#ifndef TUTORIAL_NET_NEIGHBOUR_H
#define TUTORIAL_NET_NEIGHBOUR_H

#include <linux/types.h>

struct net_device;
struct net;
struct sock;
struct sk_buff;

struct neighbour {
	struct net_device *dev;
	u32 primary_key;
	u8 ha[6];
};

void neigh_add(struct net_device *dev, u32 ip, const u8 mac[6]);
int neigh_output(struct net *net, struct sock *sk, struct sk_buff *skb);

#endif
