#include <linux/skbuff.h>
#include <net/net_namespace.h>
#include <net/tcp.h>
#include <tutorial/trace.h>

const struct proto tcp_prot = {
	.name = "TCP",
	.sendmsg = tcp_sendmsg
};

int tcp_v4_rcv(struct sk_buff *skb)
{
	struct net *net = dev_net(skb->dev);

	trace_skb(net, "tcp_v4_rcv", skb);
	for (size_t i = 0; i < MAX_SOCKS; i++) {
		struct tcp_sock *tp = &net->tcp_sockets[i];
		struct sock *sk = &tp->sk;

		if (tp->in_use && sk->sk_rcv_saddr == skb->iph.daddr &&
		    sk->sk_num == skb->th.dest && sk->sk_daddr == skb->iph.saddr &&
		    sk->sk_dport == skb->th.source) {
			return tcp_rcv_established(sk, skb);
		}
	}
	/* Riktig Linux skulle normalt svara med RST när ingen socket finns. */
	return drop_skb(net, skb, "TCP: ingen socket");
}
