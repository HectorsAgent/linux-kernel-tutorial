#ifndef TUTORIAL_NET_ROUTE_H
#define TUTORIAL_NET_ROUTE_H

#include <linux/types.h>
#include <net/dst.h>

struct rtable {
	struct dst_entry dst;
	u32 gateway;
	bool local;
};

int ip_route_input_noref(struct sk_buff *skb);
int ip_route_output_flow(struct net *net, struct sk_buff *skb);

#endif
