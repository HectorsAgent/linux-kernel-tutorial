#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <tutorial/lab.h>
#include <tutorial/trace.h>
#include <string.h>

/* Labbets fiktiva Ethernet-drivrutin: en utkorg i stället för hårdvara.
 * Denna fil har ingen direkt motsvarighet i Linux.
 */
static int ethernet_xmit(struct sk_buff *skb, struct net_device *dev)
{
	if (dev->tx_count == 32)
		return drop_skb(dev_net(dev), skb, "tx: full utkorg");
	trace_skb(dev_net(dev), "ethernet_xmit (kabel)", skb);
	dev->tx_queue[dev->tx_count++] = skb;
	return 0;
}

static const struct net_device_ops tutorial_ops = {
	.ndo_start_xmit = ethernet_xmit
};

void tutorial_ethernet_setup(struct net_device *dev)
{
	dev->netdev_ops = &tutorial_ops;
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
