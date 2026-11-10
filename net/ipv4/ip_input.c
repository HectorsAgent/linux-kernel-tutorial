#include <linux/skbuff.h>
#include <net/net_namespace.h>
#include <net/ip.h>
#include <net/tcp.h>
#include <tutorial/trace.h>

static int ip_local_deliver_finish(struct net *net, struct sock *sk,
				   struct sk_buff *skb)
{
	(void)sk;
	if (skb->iph.protocol == IPPROTO_TCP)
		return tcp_v4_rcv(skb);
	/* UDP finns för att visa att conntrack inte är en TCP-sockettabell. */
	if (skb->iph.protocol == IPPROTO_UDP) {
		net->udp_received++;
		kfree_skb(skb);
		return 0;
	}
	return drop_skb(net, skb, "okänt protokoll");
}

int ip_local_deliver(struct net *net, struct sock *sk, struct sk_buff *skb)
{
	trace_skb(net, "ip_local_deliver", skb);
	return NF_HOOK(NFPROTO_IPV4, NF_INET_LOCAL_IN, net, sk, skb,
		       skb->dev, NULL, ip_local_deliver_finish);
}

static int ip_rcv_finish(struct net *net, struct sock *sk, struct sk_buff *skb)
{
	(void)sk;
	/* PREROUTING kan ha ändrat daddr. Först NU väljer vi route. */
	if (ip_route_input_noref(skb) < 0)
		return drop_skb(net, skb, "RX: ingen route");
	/* Routingresultatet väljer ip_local_deliver eller ip_forward. */
	return dst_input(skb);
}

int ip_rcv(struct sk_buff *skb)
{
	struct net *net = dev_net(skb->dev);

	trace_skb(net, "ip_rcv", skb);
	if (skb->len > PACKET_DATA_SIZE || skb->iph.ttl == 0)
		return drop_skb(net, skb, "IP: ogiltigt paket");
	if (skb->iph.protocol != IPPROTO_TCP && skb->iph.protocol != IPPROTO_UDP)
		return drop_skb(net, skb, "IP: protokoll saknas");
	return NF_HOOK(NFPROTO_IPV4, NF_INET_PRE_ROUTING, net, NULL, skb,
		       skb->dev, NULL, ip_rcv_finish);
}
