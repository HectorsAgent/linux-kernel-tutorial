#include <linux/skbuff.h>
#include <net/net_namespace.h>
#include <tutorial/lab.h>
#include <linux/net.h>
#include <linux/fs.h>
#include <linux/fdtable.h>
#include <linux/nsproxy.h>
#include <linux/syscalls.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Inga nätverksrättigheter, riktiga sockets eller nätverksinterface används. */
static const u8 mac_a[6] = {2, 0, 0, 0, 0, 1};
static const u8 mac_b[6] = {2, 0, 0, 0, 0, 2};

/* Lågnivåtesterna börjar fortfarande vid TCP, utan process/syscall. */
static int send_tcp(struct sock *sk, const void *data, size_t len)
{
	struct msghdr msg = {0};
	int result = import_ubuf(data, len, &msg.msg_iter);

	return result < 0 ? result : tcp_sendmsg(sk, &msg, len);
}


static struct sk_buff *packet(u32 src, u16 sport, u32 dst, u16 dport, u8 proto)
{
	struct sk_buff *skb = alloc_skb();

	assert(skb);
	skb->iph = (struct iphdr) { .saddr = src, .daddr = dst, .ttl = 64, .protocol = proto };
	skb->th = (struct tcphdr) { .source = sport, .dest = dport };
	skb->data[0] = 'x';
	skb->len = 1;
	return skb;
}

static int inject(struct net_device *in, struct sk_buff *skb)
{
	skb->dev = in;
	memcpy(skb->eth.h_dest, in->dev_addr, 6);
	return netif_receive_skb(skb);
}

static void test_local_tcp_and_loopback(void)
{
	struct net net;
	struct net_device *eth;
	struct sock *a, *b;
	char text[8] = {0};

	net_init(&net, "local");
	eth = netdev_add(&net, "eth0", LINK_ETHERNET, IP4(10, 0, 0, 1), 24, mac_a);
	a = tcp_socket(&net, eth->address, 5000, eth->address, 80);
	b = tcp_socket(&net, eth->address, 80, eth->address, 5000);
	assert(a && b);
	assert(send_tcp(a, "ab", 2) == 2);
	assert(send_tcp(a, "cd", 2) == 2);
	/* Två sendmsg blir en byteström, inte två meddelanden. */
	assert(tcp_recvmsg(b, text, 3) == 3);
	assert(memcmp(text, "abc", 3) == 0);
	assert(tcp_recvmsg(b, text, sizeof(text)) == 1 && text[0] == 'd');
	assert(eth->tx_packets == 0 && net.loopback_dev->tx_packets == 2);
	assert(net.hook_hits[NF_INET_LOCAL_OUT] == 2);
	assert(net.hook_hits[NF_INET_POST_ROUTING] == 2);
	assert(net.hook_hits[NF_INET_PRE_ROUTING] == 2);
	assert(net.hook_hits[NF_INET_LOCAL_IN] == 2);
	assert(net.hook_hits[NF_INET_FORWARD] == 0 && !net.ct.enabled);
	net_cleanup(&net);
}

static void test_routing_and_neighbour_scope(void)
{
	struct net net;
	struct net_device *a, *b;
	struct sock *sk;
	struct sk_buff *skb;

	net_init(&net, "routing");
	a = netdev_add(&net, "a", LINK_ETHERNET, IP4(10, 0, 0, 1), 24, mac_a);
	b = netdev_add(&net, "b", LINK_ETHERNET, IP4(10, 1, 0, 1), 24, mac_b);
	fib_add_route(&net.ipv4.main, 0, 0, a, IP4(10, 0, 0, 254), false);
	fib_add_route(&net.ipv4.main, IP4(8, 8, 8, 8), 32, b, IP4(10, 0, 0, 254), false);
	/* Samma next-hop-IP, olika MAC på olika interface. */
	neigh_add(a, IP4(10, 0, 0, 254), mac_a);
	neigh_add(b, IP4(10, 0, 0, 254), mac_b);
	sk = tcp_socket(&net, a->address, 5000, IP4(8, 8, 8, 8), 80);
	assert(sk && send_tcp(sk, "x", 1) == 1);
	skb = netdev_dequeue(b);
	assert(skb && !netdev_dequeue(a));
	assert(memcmp(skb->eth.h_dest, mac_b, 6) == 0);
	assert(skb->iph.daddr == IP4(8, 8, 8, 8)); /* IP-målet är inte gateway. */
	kfree_skb(skb);
	net_cleanup(&net);
}

static void test_forwarding_without_conntrack(void)
{
	struct net net;
	struct net_device *lan, *wan;
	struct sk_buff *skb;

	net_init(&net, "router");
	lan = netdev_add(&net, "lan", LINK_ETHERNET, IP4(10, 0, 0, 1), 24, mac_a);
	wan = netdev_add(&net, "wan", LINK_ETHERNET, IP4(203, 0, 113, 10), 24, mac_b);
	neigh_add(wan, IP4(203, 0, 113, 20), mac_a);
	net.ipv4.ip_forward = true;
	assert(inject(lan, packet(IP4(10, 0, 0, 2), 5000, IP4(203, 0, 113, 20), 80, IPPROTO_TCP)) == 0);
	skb = netdev_dequeue(wan);
	assert(skb && skb->iph.ttl == 63 && skb->iph.saddr == IP4(10, 0, 0, 2));
	kfree_skb(skb);
	assert(net.hook_hits[NF_INET_FORWARD] == 1 && net.hook_hits[NF_INET_LOCAL_OUT] == 0);
	assert(!net.ct.enabled && net.ct.count == 0);
	skb = packet(IP4(10, 0, 0, 2), 5000, IP4(203, 0, 113, 20), 80, IPPROTO_TCP);
	skb->iph.ttl = 1;
	assert(inject(lan, skb) < 0 && wan->tx_count == 0);
	net.ipv4.ip_forward = false;
	assert(inject(lan, packet(IP4(10, 0, 0, 2), 5000, IP4(203, 0, 113, 20), 80, IPPROTO_TCP)) < 0);
	net_cleanup(&net);
}

struct lab {
	struct net container, host, server;
	struct net_device *ceth, *lan, *wan, *seth;
	struct sock *client, *service;
};

static void lab_init(struct lab *lab, bool container_tracking)
{
	struct nft_chain *chain;

	net_init(&lab->container, "container");
	net_init(&lab->host, "host");
	net_init(&lab->server, "server");
	lab->ceth = netdev_add(&lab->container, "eth0", LINK_VETH, IP4(10, 0, 0, 2), 24, mac_a);
	lab->lan = netdev_add(&lab->host, "lan", LINK_VETH, IP4(10, 0, 0, 1), 24, mac_b);
	lab->wan = netdev_add(&lab->host, "wan", LINK_VETH, IP4(203, 0, 113, 10), 24, mac_a);
	lab->seth = netdev_add(&lab->server, "eth0", LINK_VETH, IP4(203, 0, 113, 20), 24, mac_b);
	veth_pair(lab->ceth, lab->lan);
	veth_pair(lab->wan, lab->seth);
	lab->host.ipv4.ip_forward = true;
	fib_add_route(&lab->container.ipv4.main, 0, 0, lab->ceth, lab->lan->address, false);
	neigh_add(lab->ceth, lab->lan->address, mac_b);
	neigh_add(lab->lan, lab->ceth->address, mac_a);
	neigh_add(lab->wan, lab->seth->address, mac_b);
	neigh_add(lab->seth, lab->wan->address, mac_a);
	chain = nft_add_chain(&lab->host, NFT_CHAIN_NAT, NF_INET_POST_ROUTING, NF_ACCEPT);
	nft_add_rule(&lab->host, chain, (struct nft_rule) {
		.match = { .saddr = IP4(10, 0, 0, 0), .saddr_len = 24, .oif = lab->wan },
		.action = NFT_MASQUERADE
	});
	if (container_tracking)
		nf_conntrack_enable(&lab->container);
	lab->client = tcp_socket(&lab->container, lab->ceth->address, 50000, lab->seth->address, 443);
	lab->service = tcp_socket(&lab->server, lab->seth->address, 443, lab->wan->address, 50000);
	assert(lab->client && lab->service);
}

static void lab_cleanup(struct lab *lab)
{
	net_cleanup(&lab->container);
	net_cleanup(&lab->host);
	net_cleanup(&lab->server);
}

static void test_snat_round_trip(bool container_tracking)
{
	struct lab lab;
	struct nf_conn *ct;
	char text[16] = {0};

	lab_init(&lab, container_tracking);
	assert(send_tcp(lab.client, "request", 7) == 7);
	assert(tcp_recvmsg(lab.service, text, sizeof(text)) == 7);
	assert(memcmp(text, "request", 7) == 0);
	assert(send_tcp(lab.service, "reply", 5) == 5);
	assert(tcp_recvmsg(lab.client, text, sizeof(text)) == 5);
	assert(memcmp(text, "reply", 5) == 0);
	assert(lab.host.ct.count == 1 && lab.server.ct.count == 0);
	assert(lab.container.ct.count == (container_tracking ? 1u : 0u));
	ct = &lab.host.ct.entries[0];
	assert(ct->status & IPS_SRC_NAT);
	assert(ct->status & IPS_SEEN_REPLY);
	assert(ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.ip == lab.ceth->address);
	assert(ct->tuplehash[IP_CT_DIR_REPLY].tuple.dst.ip == lab.wan->address);
	assert(lab.host.hook_hits[NF_INET_LOCAL_IN] == 0); /* Svaret routades efter de-NAT. */
	assert(lab.host.hook_hits[NF_INET_LOCAL_OUT] == 0);
	assert(lab.wan->last_tx_iph.ttl == 63 && lab.lan->last_tx_iph.ttl == 63);
	if (container_tracking) {
		assert(lab.container.ct.entries[0].status & IPS_SEEN_REPLY);
		assert(!(lab.container.ct.entries[0].status & IPS_SRC_NAT));
	}
	lab_cleanup(&lab);
}

static void test_stateful_firewall_and_confirmation(void)
{
	struct lab lab;
	struct nft_chain *forward;
	struct sk_buff *skb;

	lab_init(&lab, false);
	forward = nft_add_chain(&lab.host, NFT_CHAIN_FILTER, NF_INET_FORWARD, NF_DROP);
	nft_add_rule(&lab.host, forward, (struct nft_rule) {
		.match = { .iif = lab.lan }, .action = NFT_ACCEPT
	});
	nft_add_rule(&lab.host, forward, (struct nft_rule) {
		.match = { .ct_state = NFT_CT_ESTABLISHED }, .action = NFT_ACCEPT
	});
	/* Ny anslutning från utsidan till containern får inte skapa en sparad post. */
	skb = packet(lab.seth->address, 9999, lab.ceth->address, 8080, IPPROTO_TCP);
	assert(inject(lab.wan, skb) < 0 && lab.host.ct.count == 0);
	assert(send_tcp(lab.client, "x", 1) == 1);
	assert(send_tcp(lab.service, "y", 1) == 1);
	assert(lab.host.ct.count == 1 && lab.host.drops == 1);
	lab_cleanup(&lab);
}

static void test_udp_port_collision_and_saved_nat_decision(void)
{
	struct net net;
	struct net_device *lan, *wan;
	struct nft_chain *nat;
	struct sk_buff *a, *b, *c;
	u32 remote = IP4(203, 0, 113, 20);

	net_init(&net, "nat");
	lan = netdev_add(&net, "lan", LINK_ETHERNET, IP4(10, 0, 0, 1), 24, mac_a);
	wan = netdev_add(&net, "wan", LINK_ETHERNET, IP4(203, 0, 113, 10), 24, mac_b);
	neigh_add(wan, remote, mac_a);
	net.ipv4.ip_forward = true;
	nat = nft_add_chain(&net, NFT_CHAIN_NAT, NF_INET_POST_ROUTING, NF_ACCEPT);
	nft_add_rule(&net, nat, (struct nft_rule) {
		.match = { .oif = wan }, .action = NFT_MASQUERADE
	});
	assert(inject(lan, packet(IP4(10, 0, 0, 2), 5000, remote, 53, IPPROTO_UDP)) == 0);
	assert(inject(lan, packet(IP4(10, 0, 0, 3), 5000, remote, 53, IPPROTO_UDP)) == 0);
	a = netdev_dequeue(wan);
	b = netdev_dequeue(wan);
	assert(a && b && a->th.source == 5000 && b->th.source != 5000);
	assert(a->iph.saddr == wan->address && b->iph.saddr == wan->address);
	/* Ändra reglerna: det befintliga flödet behåller sin mappning. */
	nat->count = 0;
	assert(inject(lan, packet(IP4(10, 0, 0, 3), 5000, remote, 53, IPPROTO_UDP)) == 0);
	c = netdev_dequeue(wan);
	assert(c && c->th.source == b->th.source && c->iph.saddr == wan->address);
	assert(net.ct.count == 2);
	/* Samma UDP-svar från servern hittar rätt intern IP och originalport. */
	neigh_add(lan, IP4(10, 0, 0, 2), mac_a);
	neigh_add(lan, IP4(10, 0, 0, 3), mac_b);
	assert(inject(wan, packet(remote, 53, wan->address, a->th.source, IPPROTO_UDP)) == 0);
	assert(inject(wan, packet(remote, 53, wan->address, b->th.source, IPPROTO_UDP)) == 0);
	{
		struct sk_buff *reply_a = netdev_dequeue(lan);
		struct sk_buff *reply_b = netdev_dequeue(lan);

		assert(reply_a && reply_b);
		assert(reply_a->iph.daddr == IP4(10, 0, 0, 2) && reply_a->th.dest == 5000);
		assert(reply_b->iph.daddr == IP4(10, 0, 0, 3) && reply_b->th.dest == 5000);
		kfree_skb(reply_a);
		kfree_skb(reply_b);
	}
	kfree_skb(a);
	kfree_skb(b);
	kfree_skb(c);
	net_cleanup(&net);
}

static void test_dnat_and_reverse_translation(void)
{
	struct lab lab;
	struct nft_chain *nat;
	struct sock *client, *backend;
	char text[8];

	lab_init(&lab, false);
	/* Serverns namespace agerar nu extern klient till hostens port 8080. */
	client = tcp_socket(&lab.server, lab.seth->address, 51000, lab.wan->address, 8080);
	backend = tcp_socket(&lab.container, lab.ceth->address, 80, lab.seth->address, 51000);
	assert(client && backend);
	nat = nft_add_chain(&lab.host, NFT_CHAIN_NAT, NF_INET_PRE_ROUTING, NF_ACCEPT);
	nft_add_rule(&lab.host, nat, (struct nft_rule) {
		.match = { .daddr = lab.wan->address, .daddr_len = 32, .dport = 8080 },
		.action = NFT_DNAT, .nat_addr = lab.ceth->address, .nat_port = 80
	});
	assert(send_tcp(client, "req", 3) == 3);
	assert(tcp_recvmsg(backend, text, sizeof(text)) == 3);
	assert(send_tcp(backend, "res", 3) == 3);
	assert(tcp_recvmsg(client, text, sizeof(text)) == 3);
	assert(memcmp(text, "res", 3) == 0);
	assert(lab.wan->last_tx_iph.saddr == lab.wan->address);
	assert(lab.wan->last_tx_th.source == 8080);
	assert(lab.host.hook_hits[NF_INET_LOCAL_IN] == 0);
	assert(lab.host.ct.entries[0].status & IPS_DST_NAT);
	lab_cleanup(&lab);
}

static void test_output_dnat_reroutes_to_local(void)
{
	struct net net;
	struct net_device *eth;
	struct nft_chain *nat;
	struct sock *a, *b;
	char text[4];

	net_init(&net, "output");
	eth = netdev_add(&net, "eth0", LINK_ETHERNET, IP4(10, 0, 0, 1), 24, mac_a);
	nat = nft_add_chain(&net, NFT_CHAIN_NAT, NF_INET_LOCAL_OUT, NF_ACCEPT);
	nft_add_rule(&net, nat, (struct nft_rule) {
		.match = { .daddr = IP4(10, 0, 0, 99), .daddr_len = 32, .dport = 80 },
		.action = NFT_DNAT, .nat_addr = eth->address, .nat_port = 8080
	});
	a = tcp_socket(&net, eth->address, 5000, IP4(10, 0, 0, 99), 80);
	b = tcp_socket(&net, eth->address, 8080, eth->address, 5000);
	assert(a && b && send_tcp(a, "x", 1) == 1);
	assert(tcp_recvmsg(b, text, sizeof(text)) == 1);
	assert(eth->tx_packets == 0 && net.loopback_dev->tx_packets == 1);
	assert(send_tcp(b, "y", 1) == 1);
	assert(tcp_recvmsg(a, text, sizeof(text)) == 1 && text[0] == 'y');
	assert(net.ct.count == 1 && net.hook_hits[NF_INET_FORWARD] == 0);
	net_cleanup(&net);
}

static unsigned int drop_callback(void *priv, struct sk_buff *skb,
				  const struct nf_hook_state *state)
{
	int *hits = priv;
	(void)skb;
	(void)state;
	(*hits)++;
	return NF_DROP;
}

static void test_drop_stops_later_hooks(void)
{
	struct net net;
	int hits = 0;
	int result;

	net_init(&net, "drop");
	nf_conntrack_enable(&net);
	/* Registrerad sist men lägre prioritetstal: körs före conntrack. */
	result = nf_register_net_hook(&net, (struct nf_hook_ops) {
		.hook = drop_callback, .priv = &hits, .hooknum = NF_INET_PRE_ROUTING,
		.priority = -300, .name = "test drop"
	});
	assert(result == 0);
	assert(inject(net.loopback_dev,
		packet(IP4(127, 0, 0, 1), 5000, IP4(127, 0, 0, 1), 80, IPPROTO_TCP)) < 0);
	assert(hits == 1 && net.ct.count == 0 && net.hook_hits[NF_INET_LOCAL_IN] == 0);
	net_cleanup(&net);
}

static void test_tcp_reassembles_out_of_order_segments(void)
{
	struct net net;
	struct sock *sk;
	struct sk_buff *skb;
	u32 local = IP4(127, 0, 0, 1);
	char text[8] = {0};

	net_init(&net, "tcp");
	sk = tcp_socket(&net, local, 80, local, 5000);
	assert(sk);
	skb = packet(local, 5000, local, 80, IPPROTO_TCP);
	skb->th.seq = 2;
	memcpy(skb->data, "cd", 2);
	skb->len = 2;
	assert(inject(net.loopback_dev, skb) == 0);
	assert(tcp_recvmsg(sk, text, sizeof(text)) == 0);
	skb = packet(local, 5000, local, 80, IPPROTO_TCP);
	memcpy(skb->data, "ab", 2);
	skb->len = 2;
	assert(inject(net.loopback_dev, skb) == 0);
	assert(tcp_recvmsg(sk, text, sizeof(text)) == 4);
	assert(memcmp(text, "abcd", 4) == 0);
	net_cleanup(&net);
}

static void test_packetization_preserves_byte_stream(void)
{
	struct net net;
	struct sock *a, *b;
	u32 local = IP4(127, 0, 0, 1);
	char sent[1300], received[1300];

	net_init(&net, "segments");
	a = tcp_socket(&net, local, 5000, local, 80);
	b = tcp_socket(&net, local, 80, local, 5000);
	memset(sent, 'x', sizeof(sent));
	assert(a && b && send_tcp(a, sent, sizeof(sent)) == (int)sizeof(sent));
	assert(tcp_recvmsg(b, received, sizeof(received)) == sizeof(received));
	assert(memcmp(sent, received, sizeof(sent)) == 0);
	assert(net.loopback_dev->tx_packets == 3);
	net_cleanup(&net);
}

int main(void)
{
	test_local_tcp_and_loopback();
	test_routing_and_neighbour_scope();
	test_forwarding_without_conntrack();
	test_snat_round_trip(false);
	test_snat_round_trip(true);
	test_stateful_firewall_and_confirmation();
	test_udp_port_collision_and_saved_nat_decision();
	test_dnat_and_reverse_translation();
	test_output_dnat_reroutes_to_local();
	test_drop_stops_later_hooks();
	test_tcp_reassembles_out_of_order_segments();
	test_packetization_preserves_byte_stream();
	puts("12 scenarier OK: routing, TCP, hooks, namespace-isolering, conntrack och NAT.");
	return 0;
}
