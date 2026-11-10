#include <linux/skbuff.h>
#include <net/net_namespace.h>
#include <tutorial/lab.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
	struct net host;
	struct task_struct process;
	struct net_device *eth0;
	struct sock *sk;
	struct sk_buff *frame;
	const u8 our_mac[6] = {2, 0, 0, 0, 0, 10};
	const u8 gateway_mac[6] = {2, 0, 0, 0, 0, 1};
	u32 server = IP4(203, 0, 113, 20);
	int fd, result;

	/* Labbförberedelser: anslutningen och neighbor-posten finns redan. */
	net_init(&host, "host");
	host.trace = true;
	eth0 = netdev_add(&host, "eth0", LINK_ETHERNET, IP4(192, 0, 2, 10), 24, our_mac);
	fib_add_route(&host.ipv4.main, 0, 0, eth0, IP4(192, 0, 2, 1), false);
	neigh_add(eth0, IP4(192, 0, 2, 1), gateway_mac);
	sk = tcp_socket(&host, eth0->address, 50000, server, 443);
	assert(sk);
	result = task_init(&process, 100, "client", &host);
	assert(result == 0);
	if (result < 0)
		return 1;
	fd = task_install_socket(&process, sk);
	assert(fd >= 0);
	if (fd < 0) {
		task_cleanup(&process);
		return 1;
	}

	puts("\n=== Börja läsa här: processen anropar send(fd, \"hej\", 3, 0) ===");
	result = tutorial_send(&process, fd, "hej", 3, 0);
	assert(result == 3);

	/* Drivrutinens utkorg representerar överlämningen till Ethernet-kabeln. */
	frame = netdev_dequeue(eth0);
	assert(frame);
	if (!frame) {
		task_cleanup(&process);
		net_cleanup(&host);
		return 1;
	}
	assert(frame->iph.daddr == server && frame->iph.ttl == 64);
	assert(frame->eth.h_proto == ETH_P_IP);
	assert(memcmp(frame->eth.h_dest, gateway_mac, 6) == 0);
	printf("\nVid den simulerade kabeln: EtherType=0x%04x, payload=\"%.*s\"\n",
	       frame->eth.h_proto, (int)frame->len, (const char *)frame->data);
	puts("Ethernet-destinationen är gatewayens MAC; IP-destinationen är servern.");
	kfree_skb(frame);
	task_cleanup(&process);
	net_cleanup(&host);
	return 0;
}
