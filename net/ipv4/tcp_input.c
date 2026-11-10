#include <linux/skbuff.h>
#include <net/tcp.h>
#include <tutorial/trace.h>
#include <string.h>

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

int tcp_rcv_established(struct sock *sk, struct sk_buff *skb)
{
	if (tcp_data_queue(tcp_sk(sk), skb) < 0)
		return drop_skb(sock_net(sk), skb, "TCP: buffert full");
	kfree_skb(skb);
	return 0;
}
