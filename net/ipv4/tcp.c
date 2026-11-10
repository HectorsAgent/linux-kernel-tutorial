#include <linux/skbuff.h>
#include <linux/socket.h>
#include <net/net_namespace.h>
#include <net/tcp.h>
#include <tutorial/trace.h>
#include <errno.h>
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
		.sk_net = net, .sk_prot = &tcp_prot, .sk_protocol = IPPROTO_TCP,
		.sk_rcv_saddr = local_ip, .sk_num = local_port,
		.sk_daddr = remote_ip, .sk_dport = remote_port
	};
	return &available->sk;
}

int tcp_sendmsg(struct sock *sk, struct msghdr *msg, size_t len)
{
	struct tcp_sock *tp = tcp_sk(sk);

	trace_socket("tcp_sendmsg", sk);
	if (len > msg->msg_iter.count)
		return -EINVAL;
	if (len > TCP_BUFFER_SIZE - tp->send_len)
		return -ENOBUFS;
	if (copy_from_iter(tp->send_buffer + tp->send_len, len, &msg->msg_iter) != len)
		return -EFAULT;
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
