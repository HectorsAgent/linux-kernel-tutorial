#include "learn_net.h"

int ip_finish_output(struct net *net, struct sock *sk, struct sk_buff *skb)
{
	trace_skb(net, "ip_finish_output", skb);
	return neigh_output(net, sk, skb);
}

int ip_output(struct net *net, struct sock *sk, struct sk_buff *skb)
{
	struct net_device *indev = skb->dev;
	struct net_device *outdev = skb->dst->dev;

	trace_skb(net, "ip_output", skb);
	skb->dev = outdev;
	return NF_HOOK(NFPROTO_IPV4, NF_INET_POST_ROUTING, net, sk, skb,
		       indev, outdev, ip_finish_output);
}

static int ip_local_out_finish(struct net *net, struct sock *sk, struct sk_buff *skb)
{
	/* OUTPUT kan ha gjort DNAT. Vi gör alltid om uppslaget för tydlighet. */
	if (ip_route_output_flow(net, skb) < 0)
		return drop_skb(net, skb, "OUTPUT: ingen route");
	return dst_output(net, sk, skb);
}

int ip_local_out(struct net *net, struct sock *sk, struct sk_buff *skb)
{
	trace_skb(net, "ip_local_out", skb);
	return NF_HOOK(NFPROTO_IPV4, NF_INET_LOCAL_OUT, net, sk, skb,
		       NULL, skb->dst->dev, ip_local_out_finish);
}

int ip_queue_xmit(struct sock *sk, struct sk_buff *skb)
{
	struct net *net = sock_net(sk);

	/* Endast lokalt skapade paket får en NY IP-header med TTL 64. */
	skb->sk = sk;
	skb->iph = (struct iphdr) {
		.saddr = sk->sk_rcv_saddr, .daddr = sk->sk_daddr,
		.ttl = 64, .protocol = sk->sk_protocol
	};
	trace_skb(net, "ip_queue_xmit", skb);
	if (ip_route_output_flow(net, skb) < 0)
		return drop_skb(net, skb, "TX: ingen route");
	return ip_local_out(net, sk, skb);
}
