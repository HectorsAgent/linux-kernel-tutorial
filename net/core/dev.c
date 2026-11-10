#include <linux/skbuff.h>
#include <net/net_namespace.h>
#include <net/ip.h>
#include <tutorial/lab.h>
#include <tutorial/trace.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

struct net_device *netdev_add(struct net *net, const char *name,
	enum link_kind kind, u32 address, u8 prefix_len, const u8 mac[6])
{
	struct net_device *dev;

	assert(net->dev_count < MAX_DEVS && prefix_len <= 32);
	dev = &net->devices[net->dev_count++];
	snprintf(dev->name, sizeof(dev->name), "%s", name);
	dev->nd_net = net;
	dev->kind = kind;
	dev->address = address;
	memcpy(dev->dev_addr, mac, 6);
	if (kind == LINK_LOOPBACK)
		loopback_setup(dev);
	else if (kind == LINK_VETH)
		veth_setup(dev);
	else
		tutorial_ethernet_setup(dev);
	/* IP på eth0 är ändå en lokal destination; lokal TX går via lo. */
	fib_add_route(&net->ipv4.local, address, kind == LINK_LOOPBACK ? 8 : 32,
		      dev, 0, true);
	if (kind != LINK_LOOPBACK)
		fib_add_route(&net->ipv4.main, address, prefix_len, dev, 0, false);
	return dev;
}

int dev_queue_xmit(struct sk_buff *skb)
{
	struct net_device *dev = skb->dev;

	trace_skb(dev_net(dev), "dev_queue_xmit", skb);
	dev->tx_packets++;
	dev->last_tx_iph = skb->iph;
	dev->last_tx_th = skb->th;
	return dev->netdev_ops->ndo_start_xmit(skb, dev);
}

int netif_receive_skb(struct sk_buff *skb)
{
	struct net_device *dev = skb->dev;

	skb->dst = NULL;
	trace_skb(dev_net(dev), "netif_receive_skb", skb);
	if (dev->kind != LINK_LOOPBACK &&
	    memcmp(skb->eth.h_dest, dev->dev_addr, 6) != 0)
		return drop_skb(dev_net(dev), skb, "ethernet: fel MAC");
	return ip_rcv(skb);
}
