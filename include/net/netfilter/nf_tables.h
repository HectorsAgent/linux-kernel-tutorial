#ifndef TUTORIAL_NET_NETFILTER_NF_TABLES_H
#define TUTORIAL_NET_NETFILTER_NF_TABLES_H

#include <linux/types.h>
#include <tutorial/limits.h>

struct net;
struct sk_buff;
struct net_device;
struct nf_hook_state;

enum nft_chain_type { NFT_CHAIN_FILTER, NFT_CHAIN_NAT };
enum nft_action { NFT_ACCEPT, NFT_DROP, NFT_DNAT, NFT_MASQUERADE };
enum { NFT_CT_NEW = 1, NFT_CT_ESTABLISHED = 2, NFT_CT_UNTRACKED = 4 };
struct nft_match {
	u32 saddr, daddr;
	u8 saddr_len, daddr_len; /* /0 betyder alla adresser. */
	u8 protocol; /* 0 betyder alla protokoll. */
	u16 dport;   /* 0 betyder alla portar. */
	struct net_device *iif, *oif; /* NULL betyder alla interface. */
	unsigned int ct_state;      /* 0 betyder alla tillstånd. */
};
struct nft_rule {
	struct nft_match match;
	enum nft_action action;
	u32 nat_addr;
	u16 nat_port; /* 0 bevarar porten vid DNAT. */
};
struct nft_chain {
	bool active;
	enum nft_chain_type type;
	unsigned int hooknum, policy;
	struct nft_rule rules[MAX_RULES];
	size_t count;
};

/* C-konfiguration ersätter nft-kommandot, Netlink och en riktig VM. */
struct nft_chain *nft_add_chain(struct net *net, enum nft_chain_type type,
	unsigned int hook, unsigned int policy);
void nft_add_rule(struct net *net, struct nft_chain *chain, struct nft_rule rule);
unsigned int nft_do_chain(void *priv, struct sk_buff *skb,
	const struct nf_hook_state *state);
unsigned int nft_ct_state(const struct sk_buff *skb);

#endif
