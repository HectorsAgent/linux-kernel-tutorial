#include <linux/skbuff.h>
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

struct iphdr *ip_hdr(struct sk_buff *skb) { return &skb->iph; }
struct tcphdr *tcp_hdr(struct sk_buff *skb) { return &skb->th; }
