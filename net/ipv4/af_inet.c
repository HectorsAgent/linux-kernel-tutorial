#include <linux/net.h>
#include <net/inet_common.h>
#include <net/sock.h>
#include <tutorial/trace.h>
#include <errno.h>

int inet_sendmsg(struct socket *sock, struct msghdr *msg, size_t size)
{
	struct sock *sk = sock->sk;

	trace_socket("inet_sendmsg", sk);
	if (!sk->sk_prot || !sk->sk_prot->sendmsg)
		return -EPROTONOSUPPORT;
	/* Familjen IPv4 har valt TCP-protokollets operationer för denna socket. */
	return sk->sk_prot->sendmsg(sk, msg, size);
}

const struct proto_ops inet_stream_ops = {
	.family = AF_INET,
	.sendmsg = inet_sendmsg
};
