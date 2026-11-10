#ifndef TUTORIAL_NET_IP_H
#define TUTORIAL_NET_IP_H



struct net;
struct sock;
struct sk_buff;

int ip_rcv(struct sk_buff *skb);
int ip_local_deliver(struct net *net, struct sock *sk, struct sk_buff *skb);
int ip_forward(struct net *net, struct sock *sk, struct sk_buff *skb);
int ip_queue_xmit(struct sock *sk, struct sk_buff *skb);
int ip_local_out(struct net *net, struct sock *sk, struct sk_buff *skb);
int ip_output(struct net *net, struct sock *sk, struct sk_buff *skb);
int ip_finish_output(struct net *net, struct sock *sk, struct sk_buff *skb);

#endif
