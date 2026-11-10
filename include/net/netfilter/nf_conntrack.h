#ifndef TUTORIAL_NET_NETFILTER_NF_CONNTRACK_H
#define TUTORIAL_NET_NETFILTER_NF_CONNTRACK_H

#include <linux/types.h>

struct net;
struct sk_buff;

enum ip_conntrack_dir { IP_CT_DIR_ORIGINAL, IP_CT_DIR_REPLY, IP_CT_DIR_MAX };
enum ip_conntrack_info { IP_CT_NEW, IP_CT_ESTABLISHED, IP_CT_ESTABLISHED_REPLY };
enum {
	IPS_SEEN_REPLY = 1 << 0,
	IPS_CONFIRMED = 1 << 1,
	IPS_SRC_NAT = 1 << 2,
	IPS_DST_NAT = 1 << 3
};

struct nf_conntrack_tuple {
	struct { u32 ip; u16 port; } src, dst;
	u8 protonum;
};

struct nf_conntrack_tuple_hash {
	struct nf_conntrack_tuple tuple;
};

struct nf_conn {
	struct nf_conntrack_tuple_hash tuplehash[IP_CT_DIR_MAX];
	unsigned int status;
	/* Motsvarar förenklat NAT-metadata som Linux knyter till conntrack. */
	bool src_initialized, dst_initialized;
};

void nf_reset_ct(struct sk_buff *skb);
void nf_conntrack_enable(struct net *net);
struct nf_conntrack_tuple nf_ct_get_tuple(const struct sk_buff *skb);
struct nf_conntrack_tuple nf_ct_invert_tuple(struct nf_conntrack_tuple tuple);
bool nf_ct_tuple_equal(struct nf_conntrack_tuple a, struct nf_conntrack_tuple b);
bool nf_ct_tuple_taken(struct net *net, struct nf_conntrack_tuple tuple,
	const struct nf_conn *ignore);
enum ip_conntrack_dir CTINFO2DIR(enum ip_conntrack_info info);

#endif
