#include "learn_net.h"
#include <assert.h>
#include <stdlib.h>

struct nf_conntrack_tuple nf_ct_get_tuple(const struct sk_buff *skb)
{
	return (struct nf_conntrack_tuple) {
		.src = { .ip = skb->iph.saddr, .port = skb->th.source },
		.dst = { .ip = skb->iph.daddr, .port = skb->th.dest },
		.protonum = skb->iph.protocol
	};
}

struct nf_conntrack_tuple nf_ct_invert_tuple(struct nf_conntrack_tuple t)
{
	return (struct nf_conntrack_tuple) {
		.src = { .ip = t.dst.ip, .port = t.dst.port },
		.dst = { .ip = t.src.ip, .port = t.src.port },
		.protonum = t.protonum
	};
}

bool nf_ct_tuple_equal(struct nf_conntrack_tuple a, struct nf_conntrack_tuple b)
{
	return a.src.ip == b.src.ip && a.src.port == b.src.port &&
	       a.dst.ip == b.dst.ip && a.dst.port == b.dst.port &&
	       a.protonum == b.protonum;
}

bool nf_ct_tuple_taken(struct net *net, struct nf_conntrack_tuple tuple,
	const struct nf_conn *ignore)
{
	for (size_t i = 0; i < net->ct.count; i++) {
		struct nf_conn *ct = &net->ct.entries[i];

		if (ct == ignore)
			continue;
		for (size_t dir = 0; dir < IP_CT_DIR_MAX; dir++)
			if (nf_ct_tuple_equal(tuple, ct->tuplehash[dir].tuple))
				return true;
	}
	return false;
}

enum ip_conntrack_dir CTINFO2DIR(enum ip_conntrack_info info)
{
	return info == IP_CT_ESTABLISHED_REPLY ? IP_CT_DIR_REPLY : IP_CT_DIR_ORIGINAL;
}

unsigned int nft_ct_state(const struct sk_buff *skb)
{
	if (!skb->nfct)
		return NFT_CT_UNTRACKED;
	return skb->nfctinfo == IP_CT_NEW ? NFT_CT_NEW : NFT_CT_ESTABLISHED;
}

static unsigned int nf_conntrack_in(void *priv, struct sk_buff *skb,
				    const struct nf_hook_state *state)
{
	struct net *net = state->net;
	struct nf_conntrack_tuple tuple = nf_ct_get_tuple(skb);
	struct nf_conn *ct;
	(void)priv;

	if (skb->nfct) /* Exempelvis ett lokalt paket återfört av lo. */
		return NF_ACCEPT;
	for (size_t i = 0; i < net->ct.count; i++) {
		ct = &net->ct.entries[i];
		for (size_t dir = 0; dir < IP_CT_DIR_MAX; dir++) {
			if (!nf_ct_tuple_equal(tuple, ct->tuplehash[dir].tuple))
				continue;
			skb->nfct = ct;
			if (dir == IP_CT_DIR_REPLY) {
				ct->status |= IPS_SEEN_REPLY;
				skb->nfctinfo = IP_CT_ESTABLISHED_REPLY;
			} else {
				skb->nfctinfo = ct->status & IPS_SEEN_REPLY ?
					IP_CT_ESTABLISHED : IP_CT_NEW;
			}
			return NF_ACCEPT;
		}
	}
	ct = calloc(1, sizeof(*ct));
	if (!ct)
		return NF_DROP;
	ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple = tuple;
	ct->tuplehash[IP_CT_DIR_REPLY].tuple = nf_ct_invert_tuple(tuple);
	skb->nfct = ct;
	skb->nfct_owned = true;
	skb->nfctinfo = IP_CT_NEW;
	return NF_ACCEPT;
}

static unsigned int nf_conntrack_confirm(void *priv, struct sk_buff *skb,
					 const struct nf_hook_state *state)
{
	struct net *net = state->net;
	struct nf_conn *ct = skb->nfct;
	(void)priv;

	if (!ct || !skb->nfct_owned)
		return NF_ACCEPT;
	if (net->ct.count == MAX_CT)
		return NF_DROP;
	for (size_t dir = 0; dir < IP_CT_DIR_MAX; dir++)
		if (nf_ct_tuple_taken(net, ct->tuplehash[dir].tuple, NULL))
			return NF_DROP;
	/* Spara först efter brandväggen. Droppade nya paket lämnar ingen post. */
	ct->status |= IPS_CONFIRMED;
	net->ct.entries[net->ct.count] = *ct;
	skb->nfct = &net->ct.entries[net->ct.count++];
	skb->nfct_owned = false;
	free(ct);
	return NF_ACCEPT;
}

void nf_conntrack_enable(struct net *net)
{
	const unsigned int tracking[] = { NF_INET_PRE_ROUTING, NF_INET_LOCAL_OUT };
	const unsigned int confirming[] = { NF_INET_LOCAL_IN, NF_INET_POST_ROUTING };

	if (net->ct.enabled)
		return;
	net->ct.enabled = true;
	for (size_t i = 0; i < 2; i++) {
		int result = nf_register_net_hook(net, (struct nf_hook_ops) {
			.hook = nf_conntrack_in, .hooknum = tracking[i],
			.priority = NF_IP_PRI_CONNTRACK, .name = "  conntrack lookup"
		});
		assert(result == 0);
		result = nf_register_net_hook(net, (struct nf_hook_ops) {
			.hook = nf_conntrack_confirm, .hooknum = confirming[i],
			.priority = NF_IP_PRI_CONNTRACK_CONFIRM, .name = "  conntrack confirm"
		});
		assert(result == 0);
		(void)result;
	}
}
