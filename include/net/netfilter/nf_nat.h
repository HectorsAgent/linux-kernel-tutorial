#ifndef TUTORIAL_NET_NETFILTER_NF_NAT_H
#define TUTORIAL_NET_NETFILTER_NF_NAT_H



struct net;
struct nf_conn;
struct sk_buff;
struct nft_rule;
struct nf_hook_state;

void nf_nat_enable(struct net *net);
unsigned int nf_nat_setup_info(struct nf_conn *ct, struct sk_buff *skb,
	const struct nft_rule *rule, const struct nf_hook_state *state);

#endif
