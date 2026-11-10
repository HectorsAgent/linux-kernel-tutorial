#ifndef TUTORIAL_NET_TCP_H
#define TUTORIAL_NET_TCP_H

#include <net/sock.h>
#include <tutorial/limits.h>

struct sk_buff;

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

static inline struct tcp_sock *tcp_sk(struct sock *sk)
{
	return (struct tcp_sock *)sk;
}

/* Labbet förskapar anslutna sockets utan handshake. */
struct sock *tcp_socket(struct net *net, u32 local_ip, u16 local_port,
	u32 remote_ip, u16 remote_port);
int tcp_sendmsg(struct sock *sk, const void *data, size_t len);
size_t tcp_recvmsg(struct sock *sk, void *data, size_t len);
int tcp_write_xmit(struct sock *sk);
int tcp_v4_rcv(struct sk_buff *skb);
int tcp_rcv_established(struct sock *sk, struct sk_buff *skb);

#endif
