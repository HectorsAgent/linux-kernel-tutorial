#ifndef TUTORIAL_LINUX_SKBUFF_H
#define TUTORIAL_LINUX_SKBUFF_H

#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/if_ether.h>
#include <net/route.h>
#include <net/netfilter/nf_conntrack.h>
#include <tutorial/limits.h>

struct net_device;
struct sock;

/* Ett förenklat skb med typade headers; inte ett riktigt wire-format. */
struct sk_buff {
	struct iphdr iph;
	struct tcphdr th;
	struct ethhdr eth;
	u8 data[PACKET_DATA_SIZE];
	size_t len; /* Bara payload-längd här, inte hela paketets längd. */
	struct net_device *dev;
	struct sock *sk;
	struct rtable rt; /* Route per paket; inga route-cache/refcounts. */
	struct dst_entry *dst;
	struct nf_conn *nfct;
	enum ip_conntrack_info nfctinfo;
	bool nfct_owned; /* skb äger en ännu obekräftad conntrack-post. */
};

/* Paketvägarna tar över ägandet, även när de returnerar ett fel. */
struct sk_buff *alloc_skb(void);
void kfree_skb(struct sk_buff *skb);

#endif
