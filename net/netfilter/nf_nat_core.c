#include <linux/skbuff.h>
#include <net/net_namespace.h>
#include <net/netfilter/nf_nat.h>

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
