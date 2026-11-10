#ifndef LEARN_NET_H
#define LEARN_NET_H

/*
 * En körbar undervisningsmodell, inte Linux ABI eller riktiga wire-format.
 * Namnen följer Linux; signaturer och fält är medvetet förenklade.
 * Adresser och portar lagras i HOST byte order i hela modellen.
 * Alla paketvägar tar över ägandet av skb, även när de returnerar ett fel.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

#define IP4(a, b, c, d) (((u32)(a) << 24) | ((u32)(b) << 16) | \
			 ((u32)(c) << 8) | (u32)(d))
#define MAX_DEVS 8
#define MAX_ROUTES 32
#define MAX_NEIGH 32
#define MAX_SOCKS 8
#define MAX_CT 32
#define MAX_HOOKS 8
#define MAX_RULES 16
#define TCP_BUFFER_SIZE 4096
#define PACKET_DATA_SIZE 512

enum { NFPROTO_IPV4 = 2, IPPROTO_TCP = 6, IPPROTO_UDP = 17 };
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

struct iphdr {
	u32 saddr, daddr;
	u8 ttl, protocol;
};

/* Samma två portfält används för UDP i denna modell. */
struct tcphdr {
	u16 source, dest;
	u32 seq;
};

struct ethhdr {
	u8 h_source[6], h_dest[6];
};

/* En route innehåller funktionerna för nästa steg, likt Linux dst_entry. */
struct dst_entry {
	struct net_device *dev;
	nf_okfn input, output;
};

struct rtable {
	struct dst_entry dst;
	u32 gateway;
	bool local;
};

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

struct sk_buff {
	struct iphdr iph;
	struct tcphdr th;
	struct ethhdr eth;
	u8 data[PACKET_DATA_SIZE];
	size_t len; /* Bara payload-längd här, inte hela paketets längd. */
	struct net_device *dev;
	struct sock *sk;
	struct rtable rt; /* Route per paket; inga route-cache/refcounts. */
	struct dst_entry *dst;
	struct nf_conn *nfct;
	enum ip_conntrack_info nfctinfo;
	bool nfct_owned; /* skb äger en ännu obekräftad conntrack-post. */
};

struct net_device_ops {
	int (*ndo_start_xmit)(struct sk_buff *, struct net_device *);
};

enum link_kind { LINK_LOOPBACK, LINK_VETH, LINK_ETHERNET };
struct net_device {
	char name[16];
	struct net *nd_net;
	const struct net_device_ops *netdev_ops;
	enum link_kind kind;
	u32 address;
	u8 dev_addr[6];
	struct net_device *peer;
	/* En simulerad Ethernet-drivrutin lämnar paket i denna utkorg. */
	struct sk_buff *tx_queue[32];
	size_t tx_count;
	unsigned int tx_packets;
	struct iphdr last_tx_iph;
	struct tcphdr last_tx_th;
};

struct fib_route {
	u32 prefix;
	u8 prefix_len;
	struct net_device *dev;
	u32 gateway;
	bool local;
};
struct fib_table {
	struct fib_route routes[MAX_ROUTES];
	size_t count;
};
struct neighbour {
	struct net_device *dev;
	u32 primary_key;
	u8 ha[6];
};

struct sock {
	struct net *sk_net;
	u32 sk_rcv_saddr, sk_daddr;
	u16 sk_num, sk_dport;
	u8 sk_protocol;
};
struct tcp_sock {
	/* Linux har fler mellanlager: inet_connection_sock och inet_sock. */
	struct sock sk;
	bool in_use;
	u32 write_seq, rcv_nxt;
	u8 send_buffer[TCP_BUFFER_SIZE], receive_buffer[TCP_BUFFER_SIZE];
	size_t send_len, receive_len;
	/* Små, icke överlappande segment kan buffras i fel ordning. */
	struct { u32 seq; size_t len; u8 data[PACKET_DATA_SIZE]; bool used; } pending[8];
};

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

struct sk_buff *alloc_skb(void);
void kfree_skb(struct sk_buff *skb);
void nf_reset_ct(struct sk_buff *skb);
struct net *dev_net(const struct net_device *dev);
struct net *sock_net(const struct sock *sk);
struct tcp_sock *tcp_sk(struct sock *sk);
struct iphdr *ip_hdr(struct sk_buff *skb);
struct tcphdr *tcp_hdr(struct sk_buff *skb);
void trace_skb(struct net *net, const char *where, const struct sk_buff *skb);
int drop_skb(struct net *net, struct sk_buff *skb, const char *reason);
const char *nf_hook_name(unsigned int hook);

void net_init(struct net *net, const char *name);
void net_cleanup(struct net *net);
struct net_device *netdev_add(struct net *net, const char *name,
	enum link_kind kind, u32 address, u8 prefix_len, const u8 mac[6]);
void veth_pair(struct net_device *a, struct net_device *b);
void neigh_add(struct net_device *dev, u32 ip, const u8 mac[6]);
int neigh_output(struct net *net, struct sock *sk, struct sk_buff *skb);
int dev_queue_xmit(struct sk_buff *skb);
int netif_receive_skb(struct sk_buff *skb);
struct sk_buff *netdev_dequeue(struct net_device *dev);

bool prefix_match(u32 ip, u32 prefix, u8 len);
void fib_add_route(struct fib_table *table, u32 prefix, u8 len,
	struct net_device *dev, u32 gateway, bool local);
int ip_route_input_noref(struct sk_buff *skb);
int ip_route_output_flow(struct net *net, struct sk_buff *skb);
int dst_input(struct sk_buff *skb);
int dst_output(struct net *net, struct sock *sk, struct sk_buff *skb);

int ip_rcv(struct sk_buff *skb);
int ip_local_deliver(struct net *net, struct sock *sk, struct sk_buff *skb);
int ip_forward(struct net *net, struct sock *sk, struct sk_buff *skb);
int ip_queue_xmit(struct sock *sk, struct sk_buff *skb);
int ip_local_out(struct net *net, struct sock *sk, struct sk_buff *skb);
int ip_output(struct net *net, struct sock *sk, struct sk_buff *skb);
int ip_finish_output(struct net *net, struct sock *sk, struct sk_buff *skb);

/* Skapa en redan etablerad TCP-socket; inget SYN/SYN-ACK/ACK i modellen. */
struct sock *tcp_socket(struct net *net, u32 local_ip, u16 local_port,
	u32 remote_ip, u16 remote_port);
int tcp_sendmsg(struct sock *sk, const void *data, size_t len);
size_t tcp_recvmsg(struct sock *sk, void *data, size_t len);
int tcp_v4_rcv(struct sk_buff *skb);

int nf_register_net_hook(struct net *net, struct nf_hook_ops ops);
int nf_hook_slow(struct sk_buff *skb, const struct nf_hook_state *state);
int NF_HOOK(u8 pf, unsigned int hook, struct net *net, struct sock *sk,
	struct sk_buff *skb, struct net_device *in, struct net_device *out,
	nf_okfn okfn);
void nf_conntrack_enable(struct net *net);
struct nf_conntrack_tuple nf_ct_get_tuple(const struct sk_buff *skb);
struct nf_conntrack_tuple nf_ct_invert_tuple(struct nf_conntrack_tuple tuple);
bool nf_ct_tuple_equal(struct nf_conntrack_tuple a, struct nf_conntrack_tuple b);
bool nf_ct_tuple_taken(struct net *net, struct nf_conntrack_tuple tuple,
	const struct nf_conn *ignore);
enum ip_conntrack_dir CTINFO2DIR(enum ip_conntrack_info info);
unsigned int nft_ct_state(const struct sk_buff *skb);
void nf_nat_enable(struct net *net);
unsigned int nf_nat_setup_info(struct nf_conn *ct, struct sk_buff *skb,
	const struct nft_rule *rule, const struct nf_hook_state *state);

/* Direkt C-konfiguration ersätter nft-kommandot/Netlink och en riktig VM. */
struct nft_chain *nft_add_chain(struct net *net, enum nft_chain_type type,
	unsigned int hook, unsigned int policy);
void nft_add_rule(struct net *net, struct nft_chain *chain, struct nft_rule rule);
unsigned int nft_do_chain(void *priv, struct sk_buff *skb,
	const struct nf_hook_state *state);

#endif
