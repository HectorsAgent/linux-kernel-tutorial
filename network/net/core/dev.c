#include "learn_net.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int loopback_xmit(struct sk_buff *skb, struct net_device *dev)
{
	skb->dev = dev;
	return netif_receive_skb(skb);
}

static int veth_xmit(struct sk_buff *skb, struct net_device *dev)
{
	if (!dev->peer)
		return drop_skb(dev_net(dev), skb, "veth: ingen peer");
	/* Conntrack/route från sändande namespace får inte följa med till peer. */
	nf_reset_ct(skb);
	skb->sk = NULL;
	skb->dev = dev->peer;
	return netif_receive_skb(skb);
}

static int ethernet_xmit(struct sk_buff *skb, struct net_device *dev)
{
	if (dev->tx_count == 32)
		return drop_skb(dev_net(dev), skb, "tx: full utkorg");
	dev->tx_queue[dev->tx_count++] = skb;
	return 0;
}

static const struct net_device_ops loopback_ops = { .ndo_start_xmit = loopback_xmit };
static const struct net_device_ops veth_ops = { .ndo_start_xmit = veth_xmit };
static const struct net_device_ops ethernet_ops = { .ndo_start_xmit = ethernet_xmit };

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
	dev->netdev_ops = kind == LINK_LOOPBACK ? &loopback_ops :
			 kind == LINK_VETH ? &veth_ops : &ethernet_ops;
	/* IP på eth0 är ändå en lokal destination; lokal TX går via lo. */
	fib_add_route(&net->ipv4.local, address, kind == LINK_LOOPBACK ? 8 : 32,
		      dev, 0, true);
	if (kind != LINK_LOOPBACK)
		fib_add_route(&net->ipv4.main, address, prefix_len, dev, 0, false);
	return dev;
}

void veth_pair(struct net_device *a, struct net_device *b)
{
	assert(a->kind == LINK_VETH && b->kind == LINK_VETH);
	a->peer = b;
	b->peer = a;
}

void neigh_add(struct net_device *dev, u32 ip, const u8 mac[6])
{
	struct net *net = dev_net(dev);
	struct neighbour *n = NULL;

	for (size_t i = 0; i < net->neighbour_count; i++)
		if (net->neighbours[i].dev == dev && net->neighbours[i].primary_key == ip)
			n = &net->neighbours[i];
	if (!n) {
		assert(net->neighbour_count < MAX_NEIGH);
		n = &net->neighbours[net->neighbour_count++];
	}
	n->dev = dev;
	n->primary_key = ip;
	memcpy(n->ha, mac, 6);
}

int neigh_output(struct net *net, struct sock *sk, struct sk_buff *skb)
{
	struct net_device *dev = skb->dst->dev;
	u32 next_hop = skb->rt.gateway ? skb->rt.gateway : skb->iph.daddr;
	(void)sk;

	if (dev->kind == LINK_LOOPBACK)
		return dev_queue_xmit(skb);
	for (size_t i = 0; i < net->neighbour_count; i++) {
		struct neighbour *n = &net->neighbours[i];

		if (n->dev == dev && n->primary_key == next_hop) {
			memcpy(skb->eth.h_source, dev->dev_addr, 6);
			memcpy(skb->eth.h_dest, n->ha, 6);
			return dev_queue_xmit(skb);
		}
	}
	/* Vi förkonfigurerar grannar i labbet; ingen ARP-state machine. */
	return drop_skb(net, skb, "neigh: saknar granne");
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

struct sk_buff *netdev_dequeue(struct net_device *dev)
{
	struct sk_buff *skb;

	if (!dev->tx_count)
		return NULL;
	skb = dev->tx_queue[0];
	dev->tx_count--;
	memmove(dev->tx_queue, dev->tx_queue + 1,
		dev->tx_count * sizeof(dev->tx_queue[0]));
	return skb;
}
