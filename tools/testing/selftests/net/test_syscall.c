#include <linux/skbuff.h>
#include <linux/net.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/fdtable.h>
#include <linux/nsproxy.h>
#include <net/net_namespace.h>
#include <tutorial/lab.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const u8 our_mac[6] = {2, 0, 0, 0, 0, 10};
static const u8 gateway_mac[6] = {2, 0, 0, 0, 0, 1};

struct lab {
	struct net net;
	struct task_struct task;
	struct net_device *eth;
	struct sock *sk;
	int fd;
};

static void setup(struct lab *lab)
{
	net_init(&lab->net, "syscall");
	lab->eth = netdev_add(&lab->net, "eth0", LINK_ETHERNET,
		IP4(192, 0, 2, 10), 24, our_mac);
	fib_add_route(&lab->net.ipv4.main, 0, 0, lab->eth, IP4(192, 0, 2, 1), false);
	neigh_add(lab->eth, IP4(192, 0, 2, 1), gateway_mac);
	lab->sk = tcp_socket(&lab->net, lab->eth->address, 50000, IP4(203, 0, 113, 20), 443);
	assert(lab->sk);
	assert(task_init(&lab->task, 100, "client", &lab->net) == 0);
	lab->fd = task_install_socket(&lab->task, lab->sk);
	assert(lab->fd >= 0);
}

static void cleanup(struct lab *lab)
{
	assert(current == NULL);
	task_cleanup(&lab->task);
	net_cleanup(&lab->net);
}

static void test_syscall_to_ethernet_and_process_fd_scope(void)
{
	struct lab lab;
	struct task_struct second;
	struct sock *second_sk;
	struct sk_buff *first, *other;
	char user_buffer[] = "hej";
	int second_fd;

	setup(&lab);
	second_sk = tcp_socket(&lab.net, lab.eth->address, 50001, IP4(203, 0, 113, 20), 443);
	assert(second_sk && task_init(&second, 200, "second", &lab.net) == 0);
	second_fd = task_install_socket(&second, second_sk);
	assert(second_fd == lab.fd); /* Samma nummer, olika processers filobjekt. */
	assert(second.files->fdt.fd[second_fd] != lab.task.files->fdt.fd[lab.fd]);
	current = &second;
	assert(tutorial_send(&lab.task, lab.fd, user_buffer, 3, 0) == 3);
	assert(current == &second); /* Den simulerade syscall-kontexten återställs. */
	current = NULL;
	memset(user_buffer, 'x', 3);
	assert(tutorial_send(&second, second_fd, "b", 1, 0) == 1);
	first = netdev_dequeue(lab.eth);
	other = netdev_dequeue(lab.eth);
	assert(first && other && first->th.source == 50000 && other->th.source == 50001);
	assert(first->len == 3 && memcmp(first->data, "hej", 3) == 0);
	assert(first->iph.daddr == IP4(203, 0, 113, 20) && first->iph.ttl == 64);
	assert(first->eth.h_proto == ETH_P_IP);
	assert(memcmp(first->eth.h_source, our_mac, 6) == 0);
	assert(memcmp(first->eth.h_dest, gateway_mac, 6) == 0);
	assert(lab.task.files->fdt.fd[lab.fd]->f_count == 1);
	assert(lab.net.hook_hits[NF_INET_LOCAL_OUT] == 2);
	assert(lab.net.hook_hits[NF_INET_POST_ROUTING] == 2);
	kfree_skb(first);
	kfree_skb(other);
	task_cleanup(&second);
	cleanup(&lab);
}

static void test_invalid_fd_wrong_file_and_user_buffer(void)
{
	struct lab lab;
	struct file *regular;

	setup(&lab);
	assert(tutorial_send(&lab.task, -1, "x", 1, 0) == -EBADF);
	assert(tutorial_send(&lab.task, MAX_FDS, "x", 1, 0) == -EBADF);
	assert(tutorial_send(&lab.task, MAX_FDS - 1, "x", 1, 0) == -EBADF);
	regular = calloc(1, sizeof(*regular));
	assert(regular);
	regular->f_count = 1;
	current = &lab.task;
	fd_install(1, regular);
	current = NULL;
	assert(tutorial_send(&lab.task, 1, "x", 1, 0) == -ENOTSOCK);
	assert(regular->f_count == 1);
	assert(tutorial_send(&lab.task, lab.fd, NULL, 1, 0) == -EFAULT);
	assert(tutorial_send(&lab.task, lab.fd, "x", 1, 1) == -EOPNOTSUPP);
	assert(tutorial_send(&lab.task, lab.fd, NULL, 0, 0) == 0);
	assert(lab.eth->tx_count == 0);
	cleanup(&lab);
}

static void test_socket_keeps_original_network_namespace(void)
{
	struct lab lab;
	struct net other;

	setup(&lab);
	net_init(&other, "other");
	/* Motsvarar att processen bytt netns efter att socketen skapades. */
	lab.task.nsproxy->net_ns = &other;
	assert(tutorial_send(&lab.task, lab.fd, "x", 1, 0) == 1);
	assert(lab.eth->tx_count == 1 && lab.sk->sk_net == &lab.net);
	assert(other.hook_hits[NF_INET_LOCAL_OUT] == 0);
	cleanup(&lab);
	net_cleanup(&other);
}

static unsigned int probe_calls;

static int probe_sendmsg(struct sock *sk, struct msghdr *msg, size_t len)
{
	(void)sk;
	assert(len == 3 && msg->msg_iter.count == 3);
	probe_calls++;
	return (int)len;
}

static void test_family_dispatches_through_protocol_operations(void)
{
	struct lab lab;
	const struct proto probe = { .name = "probe", .sendmsg = probe_sendmsg };

	setup(&lab);
	lab.sk->sk_prot = &probe;
	probe_calls = 0;
	assert(tutorial_send(&lab.task, lab.fd, "hej", 3, 0) == 3);
	assert(probe_calls == 1 && lab.eth->tx_count == 0);
	lab.sk->sk_prot = &tcp_prot;
	cleanup(&lab);
}

static void test_fdget_reference_survives_close(void)
{
	struct lab lab;
	struct fd held;
	struct msghdr msg = {0};

	setup(&lab);
	current = &lab.task;
	held = fdget((unsigned int)lab.fd);
	assert(held.file && held.file->f_count == 2);
	assert(close_fd((unsigned int)lab.fd) == 0);
	assert(held.file->f_count == 1);
	assert(close_fd((unsigned int)lab.fd) == -EBADF);
	current = NULL;
	assert(tutorial_send(&lab.task, lab.fd, "x", 1, 0) == -EBADF);
	assert(import_ubuf("x", 1, &msg.msg_iter) == 0);
	assert(sock_sendmsg(sock_from_file(held.file), &msg) == 1);
	fdput(held);
	cleanup(&lab);
}

static void test_syscall_packetization_and_fd_table_limit(void)
{
	struct lab lab;
	u8 bytes[1300];
	size_t total = 0;

	setup(&lab);
	memset(bytes, 'a', sizeof(bytes));
	assert(tutorial_send(&lab.task, lab.fd, bytes, sizeof(bytes), 0) == (int)sizeof(bytes));
	assert(lab.eth->tx_count == 3);
	for (size_t i = 0; i < lab.eth->tx_count; i++) {
		struct sk_buff *skb = lab.eth->tx_queue[i];

		assert(skb->th.seq == total && skb->len <= PACKET_DATA_SIZE);
		assert(memcmp(skb->data, bytes + total, skb->len) == 0);
		total += skb->len;
	}
	assert(total == sizeof(bytes));
	current = &lab.task;
	for (unsigned int i = 1; i < MAX_FDS; i++) {
		struct file *file = calloc(1, sizeof(*file));

		assert(file);
		file->f_count = 1;
		fd_install(i, file);
	}
	current = NULL;
	assert(task_install_socket(&lab.task, lab.sk) == -EMFILE);
	cleanup(&lab);
}

int main(void)
{
	test_syscall_to_ethernet_and_process_fd_scope();
	test_invalid_fd_wrong_file_and_user_buffer();
	test_socket_keeps_original_network_namespace();
	test_family_dispatches_through_protocol_operations();
	test_fdget_reference_survives_close();
	test_syscall_packetization_and_fd_table_limit();
	puts("6 syscall-scenarier OK: FD, filreferenser, protokollval och Ethernet.");
	return 0;
}
