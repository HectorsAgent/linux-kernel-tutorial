#ifndef TUTORIAL_LINUX_NETFILTER_H
#define TUTORIAL_LINUX_NETFILTER_H

#include <linux/types.h>

enum { NFPROTO_IPV4 = 2 };
enum { NF_DROP = 0, NF_ACCEPT = 1 };
enum nf_inet_hooks {
	NF_INET_PRE_ROUTING,
	NF_INET_LOCAL_IN,
	NF_INET_FORWARD,
	NF_INET_LOCAL_OUT,
	NF_INET_POST_ROUTING,
	NF_INET_NUMHOOKS
};
enum {
	NF_IP_PRI_CONNTRACK = -200,
	NF_IP_PRI_NAT_DST = -100,
	NF_IP_PRI_FILTER = 0,
	NF_IP_PRI_NAT_SRC = 100,
	NF_IP_PRI_CONNTRACK_CONFIRM = 1000 /* Linux använder INT_MAX. */
};

struct net;
struct net_device;
struct sock;
struct sk_buff;
struct nf_hook_state;
typedef int (*nf_okfn)(struct net *, struct sock *, struct sk_buff *);

struct nf_hook_state {
	u8 hook, pf;
	struct net_device *in, *out;
	struct sock *sk;
	struct net *net;
	nf_okfn okfn;
};
struct nf_hook_ops {
	unsigned int (*hook)(void *, struct sk_buff *, const struct nf_hook_state *);
	void *priv;
	unsigned int hooknum;
	int priority;
	const char *name; /* Extra fält för läsbar utskrift. */
};

int nf_register_net_hook(struct net *net, struct nf_hook_ops ops);
int nf_hook_slow(struct sk_buff *skb, const struct nf_hook_state *state);
int NF_HOOK(u8 pf, unsigned int hook, struct net *net, struct sock *sk,
	struct sk_buff *skb, struct net_device *in, struct net_device *out,
	nf_okfn okfn);
const char *nf_hook_name(unsigned int hook);

#endif
