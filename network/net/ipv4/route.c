#include "learn_net.h"
#include <assert.h>

bool prefix_match(u32 ip, u32 prefix, u8 len)
{
	u32 mask;

	assert(len <= 32);
	mask = len ? UINT32_MAX << (32 - len) : 0;
	return (ip & mask) == (prefix & mask);
}

void fib_add_route(struct fib_table *table, u32 prefix, u8 len,
	struct net_device *dev, u32 gateway, bool local)
{
	assert(table->count < MAX_ROUTES && len <= 32);
	table->routes[table->count++] = (struct fib_route) {
		.prefix = prefix, .prefix_len = len, .dev = dev,
		.gateway = gateway, .local = local
	};
}

static const struct fib_route *fib_lookup(const struct fib_table *table, u32 ip)
{
	const struct fib_route *best = NULL;

	for (size_t i = 0; i < table->count; i++) {
		const struct fib_route *route = &table->routes[i];

		if (prefix_match(ip, route->prefix, route->prefix_len) &&
		    (!best || route->prefix_len > best->prefix_len))
			best = route;
	}
	return best;
}

static int route_lookup(struct net *net, struct sk_buff *skb)
{
	/* Förenklad policy routing: local före main. */
	const struct fib_route *route = fib_lookup(&net->ipv4.local, skb->iph.daddr);

	if (!route)
		route = fib_lookup(&net->ipv4.main, skb->iph.daddr);
	if (!route)
		return -1;
	skb->rt = (struct rtable) {
		.dst = {
			.dev = route->local ? net->loopback_dev : route->dev,
			.input = route->local ? ip_local_deliver : ip_forward,
			.output = ip_output
		},
		.gateway = route->gateway, .local = route->local
	};
	skb->dst = &skb->rt.dst;
	trace_skb(net, route->local ? "route: LOCAL" : "route: UNICAST", skb);
	return 0;
}

int ip_route_input_noref(struct sk_buff *skb)
{
	return route_lookup(dev_net(skb->dev), skb);
}

int ip_route_output_flow(struct net *net, struct sk_buff *skb)
{
	return route_lookup(net, skb);
}

int dst_input(struct sk_buff *skb)
{
	return skb->dst->input(dev_net(skb->dev), NULL, skb);
}

int dst_output(struct net *net, struct sock *sk, struct sk_buff *skb)
{
	return skb->dst->output(net, sk, skb);
}
