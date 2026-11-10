#ifndef TUTORIAL_TUTORIAL_LAB_H
#define TUTORIAL_TUTORIAL_LAB_H

#include <linux/netdevice.h>
#include <linux/sched.h>

struct sock;

/* Labbstöd utan exakt Linux-motsvarighet. IP-adresser/portar är i host byte order. */
void net_init(struct net *net, const char *name);
void net_cleanup(struct net *net);
struct net_device *netdev_add(struct net *net, const char *name,
	enum link_kind kind, u32 address, u8 prefix_len, const u8 mac[6]);
void veth_pair(struct net_device *a, struct net_device *b);
struct sk_buff *netdev_dequeue(struct net_device *dev);
void loopback_setup(struct net_device *dev);
void veth_setup(struct net_device *dev);
void tutorial_ethernet_setup(struct net_device *dev);

/* Processen får en egen FD-tabell. Inga standardströmmar installeras. */
int task_init(struct task_struct *task, int pid, const char *name, struct net *net);
void task_cleanup(struct task_struct *task);
int task_install_socket(struct task_struct *task, struct sock *sk);
/* Ersätter libc + CPU:ns syscall-inträde. Ingen riktig trap används. */
int tutorial_send(struct task_struct *task, int fd, const void *buf,
	size_t len, unsigned int flags);

#endif
