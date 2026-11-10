#ifndef TUTORIAL_TUTORIAL_LAB_H
#define TUTORIAL_TUTORIAL_LAB_H

#include <linux/netdevice.h>

/* Labbstöd utan exakt Linux-motsvarighet. IP-adresser/portar är i host byte order. */
void net_init(struct net *net, const char *name);
void net_cleanup(struct net *net);
struct net_device *netdev_add(struct net *net, const char *name,
	enum link_kind kind, u32 address, u8 prefix_len, const u8 mac[6]);
void veth_pair(struct net_device *a, struct net_device *b);
struct sk_buff *netdev_dequeue(struct net_device *dev);
void loopback_setup(struct net_device *dev);
void veth_setup(struct net_device *dev);
void tutorial_ethernet_setup(struct net_device *dev);

#endif
