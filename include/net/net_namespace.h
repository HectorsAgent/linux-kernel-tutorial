#ifndef TUTORIAL_NET_NET_NAMESPACE_H
#define TUTORIAL_NET_NET_NAMESPACE_H

#include <linux/netdevice.h>
#include <linux/netfilter.h>
#include <net/ip_fib.h>
#include <net/neighbour.h>
#include <net/tcp.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_tables.h>
#include <tutorial/limits.h>

struct net {
	char name[24];
	bool trace;
	struct net_device devices[MAX_DEVS];
	size_t dev_count;
	struct net_device *loopback_dev;
	struct {
		bool ip_forward;
		struct fib_table local, main;
	} ipv4;
	struct neighbour neighbours[MAX_NEIGH];
	size_t neighbour_count;
	struct tcp_sock tcp_sockets[MAX_SOCKS];
	struct {
		bool enabled;
		struct nf_conn entries[MAX_CT];
		size_t count;
	} ct;
	struct {
		struct nf_hook_ops hooks[NF_INET_NUMHOOKS][MAX_HOOKS];
		size_t count[NF_INET_NUMHOOKS];
	} nf;
	struct {
		struct nft_chain filter[NF_INET_NUMHOOKS], nat[NF_INET_NUMHOOKS];
		bool nat_enabled;
	} nft;
	/* Undervisningsinstrumentering, inte Linux fältnamn. */
	unsigned int hook_hits[NF_INET_NUMHOOKS], drops, udp_received;
};

#endif
