#include <linux/skbuff.h>
#include <net/net_namespace.h>
#include <tutorial/trace.h>
#include <tutorial/lab.h>
#include <linux/nsproxy.h>
#include <linux/syscalls.h>
#include <stdio.h>

void trace_skb(struct net *net, const char *where, const struct sk_buff *skb)
{
	u32 s = skb->iph.saddr, d = skb->iph.daddr;

	if (!net->trace)
		return;
	printf("[%-9s] %-24s %u.%u.%u.%u:%u -> %u.%u.%u.%u:%u ttl=%u\n",
	       net->name, where,
	       s >> 24, (s >> 16) & 255, (s >> 8) & 255, s & 255, skb->th.source,
	       d >> 24, (d >> 16) & 255, (d >> 8) & 255, d & 255, skb->th.dest,
	       skb->iph.ttl);
}

int drop_skb(struct net *net, struct sk_buff *skb, const char *reason)
{
	net->drops++;
	trace_skb(net, reason, skb);
	kfree_skb(skb);
	return -1;
}

void trace_task(const char *where, int fd)
{
	if (current && current->nsproxy && current->nsproxy->net_ns->trace)
		printf("[process  ] %-24s pid=%d (%s) fd=%d\n",
		       where, current->pid, current->comm, fd);
}

void trace_socket(const char *where, struct sock *sk)
{
	if (sock_net(sk)->trace)
		printf("[%-9s] %-24s socket port=%u -> port=%u\n",
		       sock_net(sk)->name, where, sk->sk_num, sk->sk_dport);
}

int tutorial_send(struct task_struct *task, int fd, const void *buf,
	size_t len, unsigned int flags)
{
	struct task_struct *previous = current;
	int result;

	/* En riktig CPU/syscall-entry hade redan gjort den här processen current. */
	current = task;
	trace_task("user: send", fd);
	result = __sys_sendto(fd, buf, len, flags, NULL, 0);
	current = previous;
	return result; /* Negativt errno direkt; libc:s errno-översättning saknas. */
}
