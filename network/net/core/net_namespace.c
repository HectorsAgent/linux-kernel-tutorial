#include "learn_net.h"
#include <stdio.h>
#include <string.h>

void net_init(struct net *net, const char *name)
{
	const u8 mac[6] = {0};

	memset(net, 0, sizeof(*net));
	snprintf(net->name, sizeof(net->name), "%s", name);
	net->loopback_dev = netdev_add(net, "lo", LINK_LOOPBACK,
				     IP4(127, 0, 0, 1), 8, mac);
}

void net_cleanup(struct net *net)
{
	for (size_t i = 0; i < net->dev_count; i++) {
		struct sk_buff *skb;

		while ((skb = netdev_dequeue(&net->devices[i])) != NULL)
			kfree_skb(skb);
	}
}
