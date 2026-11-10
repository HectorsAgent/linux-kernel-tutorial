#ifndef TUTORIAL_LINUX_NETDEVICE_H
#define TUTORIAL_LINUX_NETDEVICE_H

#include <linux/ip.h>
#include <linux/tcp.h>

struct net;
struct net_device;
struct sk_buff;

struct net_device_ops {
	int (*ndo_start_xmit)(struct sk_buff *, struct net_device *);
};

enum link_kind { LINK_LOOPBACK, LINK_VETH, LINK_ETHERNET };
struct net_device {
	char name[16];
	struct net *nd_net;
	const struct net_device_ops *netdev_ops;
	enum link_kind kind;
	u32 address;
	u8 dev_addr[6];
	struct net_device *peer;
	/* En simulerad Ethernet-drivrutin lämnar paket i denna utkorg. */
	struct sk_buff *tx_queue[32];
	size_t tx_count;
	unsigned int tx_packets;
	struct iphdr last_tx_iph;
	struct tcphdr last_tx_th;
};

static inline struct net *dev_net(const struct net_device *dev)
{
	return dev->nd_net;
}

int dev_queue_xmit(struct sk_buff *skb);
int netif_receive_skb(struct sk_buff *skb);

#endif
