#include "learn_net.h"
#include <string.h>

struct sock *tcp_socket(struct net *net, u32 local_ip, u16 local_port,
	u32 remote_ip, u16 remote_port)
{
	struct tcp_sock *available = NULL;
	bool local = false;

	for (size_t i = 0; i < net->ipv4.local.count; i++) {
		struct fib_route *r = &net->ipv4.local.routes[i];
		if (prefix_match(local_ip, r->prefix, r->prefix_len))
			local = true;
	}
	if (!local)
		return NULL;
	for (size_t i = 0; i < MAX_SOCKS; i++) {
		struct tcp_sock *tp = &net->tcp_sockets[i];
		struct sock *sk = &tp->sk;

		if (!tp->in_use) {
			available = tp;
			continue;
		}
		if (sk->sk_rcv_saddr == local_ip && sk->sk_num == local_port &&
		    sk->sk_daddr == remote_ip && sk->sk_dport == remote_port)
			return NULL;
	}
	if (!available)
		return NULL;
	available->in_use = true;
	available->sk = (struct sock) {
		.sk_net = net, .sk_protocol = IPPROTO_TCP,
		.sk_rcv_saddr = local_ip, .sk_num = local_port,
		.sk_daddr = remote_ip, .sk_dport = remote_port
	};
	return &available->sk;
}

/* Avgränsning: ingen handshake, ACK, congestion control eller omsändning. */
static int tcp_write_xmit(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);
	size_t sent = 0;

	while (sent < tp->send_len) {
		struct sk_buff *skb = alloc_skb();
		size_t len = tp->send_len - sent;

		if (!skb) {
			tp->send_len = 0;
			return -1;
		}
		if (len > PACKET_DATA_SIZE)
			len = PACKET_DATA_SIZE;
		skb->th = (struct tcphdr) {
			.source = sk->sk_num, .dest = sk->sk_dport, .seq = tp->write_seq
		};
		memcpy(skb->data, tp->send_buffer + sent, len);
		skb->len = len;
		tp->write_seq += (u32)len;
		sent += len;
		if (ip_queue_xmit(sk, skb) < 0) {
			/* Labbet rapporterar paketfel direkt; riktig TCP gör inte så. */
			tp->send_len = 0;
			return -1;
		}
	}
	tp->send_len = 0;
	return 0;
}

int tcp_sendmsg(struct sock *sk, const void *data, size_t len)
{
	struct tcp_sock *tp = tcp_sk(sk);

	if (len > TCP_BUFFER_SIZE - tp->send_len)
		return -1;
	memcpy(tp->send_buffer + tp->send_len, data, len);
	tp->send_len += len;
	if (tcp_write_xmit(sk) < 0)
		return -1;
	return (int)len;
}

size_t tcp_recvmsg(struct sock *sk, void *data, size_t len)
{
	struct tcp_sock *tp = tcp_sk(sk);

	if (len > tp->receive_len)
		len = tp->receive_len;
	memcpy(data, tp->receive_buffer, len);
	tp->receive_len -= len;
	memmove(tp->receive_buffer, tp->receive_buffer + len, tp->receive_len);
	return len;
}

static int tcp_data_queue(struct tcp_sock *tp, struct sk_buff *skb)
{
	size_t slot = 8;

	/* Inga överlappande segment eller sekvensnummer-wrap i denna modell. */
	if (skb->th.seq < tp->rcv_nxt || !skb->len)
		return 0;
	for (size_t i = 0; i < 8; i++) {
		if (tp->pending[i].used && tp->pending[i].seq == skb->th.seq) {
			slot = i;
			break;
		}
		if (!tp->pending[i].used)
			slot = i;
	}
	if (slot == 8)
		return -1;
	tp->pending[slot].used = true;
	tp->pending[slot].seq = skb->th.seq;
	tp->pending[slot].len = skb->len;
	memcpy(tp->pending[slot].data, skb->data, skb->len);
	for (;;) {
		bool progressed = false;

		for (size_t i = 0; i < 8; i++) {
			size_t len = tp->pending[i].len;

			if (!tp->pending[i].used || tp->pending[i].seq != tp->rcv_nxt)
				continue;
			if (len > TCP_BUFFER_SIZE - tp->receive_len)
				return -1;
			memcpy(tp->receive_buffer + tp->receive_len, tp->pending[i].data, len);
			tp->receive_len += len;
			tp->rcv_nxt += (u32)len;
			tp->pending[i].used = false;
			progressed = true;
		}
		if (!progressed)
			return 0;
	}
}

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
			if (tcp_data_queue(tp, skb) < 0)
				return drop_skb(net, skb, "TCP: buffert full");
			kfree_skb(skb);
			return 0;
		}
	}
	/* Riktig Linux skulle normalt svara med RST när ingen socket finns. */
	return drop_skb(net, skb, "TCP: ingen socket");
}
