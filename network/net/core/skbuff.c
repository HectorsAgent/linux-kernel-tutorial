#include "learn_net.h"
#include <stdio.h>
#include <stdlib.h>

struct sk_buff *alloc_skb(void)
{
	return calloc(1, sizeof(struct sk_buff));
}

void nf_reset_ct(struct sk_buff *skb)
{
	if (skb->nfct_owned)
		free(skb->nfct);
	skb->nfct = NULL;
	skb->nfct_owned = false;
}

void kfree_skb(struct sk_buff *skb)
{
	if (!skb)
		return;
	nf_reset_ct(skb);
	free(skb);
}

struct net *dev_net(const struct net_device *dev) { return dev->nd_net; }
struct net *sock_net(const struct sock *sk) { return sk->sk_net; }
struct tcp_sock *tcp_sk(struct sock *sk) { return (struct tcp_sock *)sk; }
struct iphdr *ip_hdr(struct sk_buff *skb) { return &skb->iph; }
struct tcphdr *tcp_hdr(struct sk_buff *skb) { return &skb->th; }

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
