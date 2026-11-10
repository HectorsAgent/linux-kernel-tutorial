#include <linux/skbuff.h>
#include <net/net_namespace.h>
#include <tutorial/lab.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
	struct net container, host, server;
	const u8 cmac[6] = {2, 0, 0, 0, 0, 2};
	const u8 lmac[6] = {2, 0, 0, 0, 0, 1};
	const u8 wmac[6] = {2, 0, 0, 0, 1, 1};
	const u8 smac[6] = {2, 0, 0, 0, 1, 2};
	struct net_device *ceth, *lan, *wan, *seth;
	struct nft_chain *forward, *postrouting;
	struct sock *client, *service;
	char buf[32] = {0};
	int result;
	size_t received;

	net_init(&container, "container");
	net_init(&host, "host");
	net_init(&server, "server");
	container.trace = host.trace = server.trace = true;

	/* Två simulerade kablar. Även internetlänken använder veth i labbet. */
	ceth = netdev_add(&container, "eth0", LINK_VETH, IP4(10, 0, 0, 2), 24, cmac);
	lan = netdev_add(&host, "veth0", LINK_VETH, IP4(10, 0, 0, 1), 24, lmac);
	wan = netdev_add(&host, "eth0", LINK_VETH, IP4(203, 0, 113, 10), 24, wmac);
	seth = netdev_add(&server, "eth0", LINK_VETH, IP4(203, 0, 113, 20), 24, smac);
	veth_pair(ceth, lan);
	veth_pair(wan, seth);
	host.ipv4.ip_forward = true;
	fib_add_route(&container.ipv4.main, 0, 0, ceth, lan->address, false);

	/* Ersätter ARP i denna version. Nyckeln innehåller både interface och IP. */
	neigh_add(ceth, lan->address, lmac);
	neigh_add(lan, ceth->address, cmac);
	neigh_add(wan, seth->address, smac);
	neigh_add(seth, wan->address, wmac);

	/* Konfigurationsvägen: motsvarar en liten del av vad nft/Netlink gör. */
	forward = nft_add_chain(&host, NFT_CHAIN_FILTER, NF_INET_FORWARD, NF_DROP);
	nft_add_rule(&host, forward, (struct nft_rule) {
		.match = { .iif = lan, .oif = wan }, .action = NFT_ACCEPT
	});
	nft_add_rule(&host, forward, (struct nft_rule) {
		.match = { .iif = wan, .oif = lan, .ct_state = NFT_CT_ESTABLISHED },
		.action = NFT_ACCEPT
	});
	postrouting = nft_add_chain(&host, NFT_CHAIN_NAT, NF_INET_POST_ROUTING, NF_ACCEPT);
	nft_add_rule(&host, postrouting, (struct nft_rule) {
		.match = { .saddr = IP4(10, 0, 0, 0), .saddr_len = 24, .oif = wan },
		.action = NFT_MASQUERADE
	});

	/* Förskapade anslutna sockets: vi simulerar inte TCP-handshaken. */
	client = tcp_socket(&container, ceth->address, 50000, seth->address, 443);
	service = tcp_socket(&server, seth->address, 443, wan->address, 50000);
	assert(client && service);

	puts("\n=== Containern skickar; hosten forwardar och gör SNAT ===");
	result = tcp_sendmsg(client, "hej", 3);
	assert(result == 3);
	received = tcp_recvmsg(service, buf, sizeof(buf));
	assert(received == 3);
	assert(memcmp(buf, "hej", 3) == 0);

	puts("\n=== Servern svarar; hosten återställer destinationen FÖRE routing ===");
	result = tcp_sendmsg(service, "svar", 4);
	assert(result == 4);
	memset(buf, 0, sizeof(buf));
	received = tcp_recvmsg(client, buf, sizeof(buf));
	assert(received == 4);
	printf("\nContainerns receive_buffer gav: %s\n", buf);
	printf("Conntrack-poster: container=%zu, host=%zu, server=%zu\n",
	       container.ct.count, host.ct.count, server.ct.count);
	printf("Hostens hooks: INPUT=%u, FORWARD=%u, OUTPUT=%u\n",
	       host.hook_hits[NF_INET_LOCAL_IN], host.hook_hits[NF_INET_FORWARD],
	       host.hook_hits[NF_INET_LOCAL_OUT]);
	puts("Hosten har inga TCP-sockets för denna anslutning.");
	assert(host.ct.count == 1 && container.ct.count == 0 && server.ct.count == 0);
	assert(host.hook_hits[NF_INET_LOCAL_IN] == 0);
	assert(host.hook_hits[NF_INET_LOCAL_OUT] == 0);
	(void)result;
	(void)received;

	net_cleanup(&container);
	net_cleanup(&host);
	net_cleanup(&server);
	return 0;
}
