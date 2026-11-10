#include <linux/skbuff.h>
#include <net/net_namespace.h>
#include <net/neighbour.h>
#include <tutorial/trace.h>
#include <assert.h>
#include <string.h>

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
			skb->eth.h_proto = ETH_P_IP;
			return dev_queue_xmit(skb);
		}
	}
	/* Vi förkonfigurerar grannar i labbet; ingen ARP-state machine. */
	return drop_skb(net, skb, "neigh: saknar granne");
}
