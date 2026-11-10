#include <linux/skbuff.h>
#include <net/net_namespace.h>
#include <net/netfilter/nf_nat.h>

/* Här finns bara vanliga matchningar, ingen riktig nftables-VM eller parser. */
static bool nft_match_packet(const struct nft_match *m, const struct sk_buff *skb,
			     const struct nf_hook_state *state)
{
	return prefix_match(skb->iph.saddr, m->saddr, m->saddr_len) &&
	       prefix_match(skb->iph.daddr, m->daddr, m->daddr_len) &&
	       (!m->protocol || m->protocol == skb->iph.protocol) &&
	       (!m->dport || m->dport == skb->th.dest) &&
	       (!m->iif || m->iif == state->in) &&
	       (!m->oif || m->oif == state->out) &&
	       (!m->ct_state || (m->ct_state & nft_ct_state(skb)));
}

unsigned int nft_do_chain(void *priv, struct sk_buff *skb,
	const struct nf_hook_state *state)
{
	struct nft_chain *chain = priv;

	for (size_t i = 0; i < chain->count; i++) {
		struct nft_rule *rule = &chain->rules[i];

		if (!nft_match_packet(&rule->match, skb, state))
			continue;
		switch (rule->action) {
		case NFT_ACCEPT: return NF_ACCEPT;
		case NFT_DROP: return NF_DROP;
		case NFT_DNAT:
		case NFT_MASQUERADE:
			return nf_nat_setup_info(skb->nfct, skb, rule, state);
		}
	}
	return chain->policy;
}
