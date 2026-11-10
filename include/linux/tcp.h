#ifndef TUTORIAL_LINUX_TCP_H
#define TUTORIAL_LINUX_TCP_H

#include <linux/types.h>

/* Samma två portfält används för UDP i denna modell. */
struct tcphdr {
	u16 source, dest;
	u32 seq;
};

struct sk_buff;
struct tcphdr *tcp_hdr(struct sk_buff *skb);

#endif
