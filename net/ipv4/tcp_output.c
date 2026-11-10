#include <linux/skbuff.h>
#include <net/tcp.h>
#include <net/ip.h>
#include <string.h>

/* Avgränsning: ingen handshake, ACK, congestion control eller omsändning. */
int tcp_write_xmit(struct sock *sk)
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
