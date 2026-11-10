#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <tutorial/lab.h>
#include <tutorial/trace.h>
#include <assert.h>

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

static const struct net_device_ops veth_netdev_ops = {
	.ndo_start_xmit = veth_xmit
};

void veth_setup(struct net_device *dev)
{
	dev->netdev_ops = &veth_netdev_ops;
}

void veth_pair(struct net_device *a, struct net_device *b)
{
	assert(a->kind == LINK_VETH && b->kind == LINK_VETH);
	a->peer = b;
	b->peer = a;
}
