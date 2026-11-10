#include <linux/skbuff.h>
#include <net/net_namespace.h>
#include <net/ip.h>
#include <tutorial/trace.h>

static int ip_forward_finish(struct net *net, struct sock *sk, struct sk_buff *skb)
{
	/* Samma sista sändningsväg som lokalt skapade paket använder. */
	return dst_output(net, sk, skb);
}

int ip_forward(struct net *net, struct sock *sk, struct sk_buff *skb)
{
	trace_skb(net, "ip_forward", skb);
	if (!net->ipv4.ip_forward)
		return drop_skb(net, skb, "forwarding avstängt");
	if (skb->iph.ttl <= 1)
		return drop_skb(net, skb, "TTL slut");
	skb->iph.ttl--;
	return NF_HOOK(NFPROTO_IPV4, NF_INET_FORWARD, net, sk, skb,
		       skb->dev, skb->dst->dev, ip_forward_finish);
}
