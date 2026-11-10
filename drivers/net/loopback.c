#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <tutorial/lab.h>

static int loopback_xmit(struct sk_buff *skb, struct net_device *dev)
{
	skb->dev = dev;
	return netif_receive_skb(skb);
}

static const struct net_device_ops loopback_ops = {
	.ndo_start_xmit = loopback_xmit
};

void loopback_setup(struct net_device *dev)
{
	dev->netdev_ops = &loopback_ops;
}
