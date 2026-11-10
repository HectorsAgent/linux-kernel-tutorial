#include "learn_net.h"
#include <assert.h>

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

struct nft_chain *nft_add_chain(struct net *net, enum nft_chain_type type,
	unsigned int hook, unsigned int policy)
{
	struct nft_chain *chain;

	assert(hook < NF_INET_NUMHOOKS);
	assert(policy == NF_ACCEPT || policy == NF_DROP);
	chain = type == NFT_CHAIN_NAT ? &net->nft.nat[hook] : &net->nft.filter[hook];
	assert(!chain->active);
	*chain = (struct nft_chain) {
		.active = true, .type = type, .hooknum = hook, .policy = policy
	};
	if (type == NFT_CHAIN_NAT) {
		assert(policy == NF_ACCEPT);
		assert(hook == NF_INET_PRE_ROUTING || hook == NF_INET_LOCAL_OUT ||
		       hook == NF_INET_POST_ROUTING);
		nf_nat_enable(net);
	} else {
		int result = nf_register_net_hook(net, (struct nf_hook_ops) {
			.hook = nft_do_chain, .priv = chain, .hooknum = hook,
			.priority = NF_IP_PRI_FILTER, .name = "  nft filter"
		});
		assert(result == 0);
		(void)result;
	}
	return chain;
}

void nft_add_rule(struct net *net, struct nft_chain *chain, struct nft_rule rule)
{
	assert(chain->active && chain->count < MAX_RULES);
	assert(rule.match.saddr_len <= 32 && rule.match.daddr_len <= 32);
	if (chain->type == NFT_CHAIN_FILTER) {
		assert(rule.action == NFT_ACCEPT || rule.action == NFT_DROP);
	} else {
		assert((rule.action == NFT_DNAT && chain->hooknum != NF_INET_POST_ROUTING) ||
		       (rule.action == NFT_MASQUERADE && chain->hooknum == NF_INET_POST_ROUTING));
	}
	if (rule.match.ct_state)
		nf_conntrack_enable(net);
	chain->rules[chain->count++] = rule;
}
