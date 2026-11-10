#ifndef TUTORIAL_LINUX_IP_H
#define TUTORIAL_LINUX_IP_H

#include <linux/types.h>
#include <linux/in.h>

struct iphdr {
	u32 saddr, daddr;
	u8 ttl, protocol;
};

struct sk_buff;
struct iphdr *ip_hdr(struct sk_buff *skb);

#endif
