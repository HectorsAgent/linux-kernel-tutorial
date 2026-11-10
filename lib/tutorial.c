#include <linux/skbuff.h>
#include <net/net_namespace.h>
#include <tutorial/trace.h>
#include <stdio.h>

void trace_skb(struct net *net, const char *where, const struct sk_buff *skb)
{
	u32 s = skb->iph.saddr, d = skb->iph.daddr;

	if (!net->trace)
		return;
	printf("[%-9s] %-24s %u.%u.%u.%u:%u -> %u.%u.%u.%u:%u ttl=%u\n",
	       net->name, where,
	       s >> 24, (s >> 16) & 255, (s >> 8) & 255, s & 255, skb->th.source,
	       d >> 24, (d >> 16) & 255, (d >> 8) & 255, d & 255, skb->th.dest,
	       skb->iph.ttl);
}

int drop_skb(struct net *net, struct sk_buff *skb, const char *reason)
{
	net->drops++;
	trace_skb(net, reason, skb);
	kfree_skb(skb);
	return -1;
}
