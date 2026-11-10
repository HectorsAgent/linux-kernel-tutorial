#include "learn_net.h"
#include <assert.h>

unsigned int nf_nat_setup_info(struct nf_conn *ct, struct sk_buff *skb,
	const struct nft_rule *rule, const struct nf_hook_state *state)
{
	struct nf_conntrack_tuple reply = ct->tuplehash[IP_CT_DIR_REPLY].tuple;
	(void)skb;

	if (rule->action == NFT_DNAT) {
		/* Svaret förväntas komma FRÅN den nya destinationen. */
		reply.src.ip = rule->nat_addr;
		if (rule->nat_port)
			reply.src.port = rule->nat_port;
		if (nf_ct_tuple_taken(state->net, reply, ct))
			return NF_DROP;
		ct->status |= IPS_DST_NAT;
	} else if (rule->action == NFT_MASQUERADE) {
		if (!state->out)
			return NF_DROP;
		/* Svaret förväntas komma TILL utgående interfacets adress. */
		reply.dst.ip = state->out->address;
		/* Behåll originalporten om hela svarstupeln är ledig. */
		if (nf_ct_tuple_taken(state->net, reply, ct)) {
			bool found = false;

			for (u32 port = 40000; port <= UINT16_MAX; port++) {
				reply.dst.port = (u16)port;
				if (!nf_ct_tuple_taken(state->net, reply, ct)) {
					found = true;
					break;
				}
			}
			if (!found)
				return NF_DROP;
		}
		ct->status |= IPS_SRC_NAT;
	}
	ct->tuplehash[IP_CT_DIR_REPLY].tuple = reply;
	return NF_ACCEPT;
}

static unsigned int nf_nat_ipv4_fn(void *priv, struct sk_buff *skb,
				   const struct nf_hook_state *state)
{
	struct nf_conn *ct = skb->nfct;
	struct nf_conntrack_tuple original, reply;
	bool source = state->hook == NF_INET_POST_ROUTING;
	bool *initialized;
	(void)priv;

	if (!ct)
		return NF_ACCEPT;
	initialized = source ? &ct->src_initialized : &ct->dst_initialized;
	/* NAT-regler väljer mappningen en gång per riktning av manipulation. */
	if (CTINFO2DIR(skb->nfctinfo) == IP_CT_DIR_ORIGINAL && !*initialized) {
		struct nft_chain *chain = &state->net->nft.nat[state->hook];

		if (chain->active && nft_do_chain(chain, skb, state) == NF_DROP)
			return NF_DROP;
		*initialized = true; /* Även beslutet "ingen NAT" sparas. */
	}
	original = ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple;
	reply = ct->tuplehash[IP_CT_DIR_REPLY].tuple;
	if (CTINFO2DIR(skb->nfctinfo) == IP_CT_DIR_ORIGINAL) {
		if (source && (ct->status & IPS_SRC_NAT)) {
			skb->iph.saddr = reply.dst.ip;
			skb->th.source = reply.dst.port;
		}
		if (!source && (ct->status & IPS_DST_NAT)) {
			skb->iph.daddr = reply.src.ip;
			skb->th.dest = reply.src.port;
		}
	} else {
		/* Invers SNAT blir ändrad destination, före routing av svaret. */
		if (!source && (ct->status & IPS_SRC_NAT)) {
			skb->iph.daddr = original.src.ip;
			skb->th.dest = original.src.port;
		}
		/* Invers DNAT blir ändrad källa, på svarets väg ut. */
		if (source && (ct->status & IPS_DST_NAT)) {
			skb->iph.saddr = original.dst.ip;
			skb->th.source = original.dst.port;
		}
	}
	return NF_ACCEPT;
}

void nf_nat_enable(struct net *net)
{
	const unsigned int hooks[] = {
		NF_INET_PRE_ROUTING, NF_INET_LOCAL_OUT, NF_INET_POST_ROUTING
	};

	if (net->nft.nat_enabled)
		return;
	nf_conntrack_enable(net);
	net->nft.nat_enabled = true;
	/* Svar måste översättas även vid hooks där användaren inte lagt regler. */
	for (size_t i = 0; i < 3; i++) {
		int result = nf_register_net_hook(net, (struct nf_hook_ops) {
			.hook = nf_nat_ipv4_fn, .hooknum = hooks[i],
			.priority = hooks[i] == NF_INET_POST_ROUTING ?
				NF_IP_PRI_NAT_SRC : NF_IP_PRI_NAT_DST,
			.name = "  NAT"
		});
		assert(result == 0);
		(void)result;
	}
}
