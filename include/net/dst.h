#ifndef TUTORIAL_NET_DST_H
#define TUTORIAL_NET_DST_H



struct net;
struct net_device;
struct sock;
struct sk_buff;

/* Routingresultatet anger också nästa funktion i paketvägen. */
struct dst_entry {
	struct net_device *dev;
	int (*input)(struct net *, struct sock *, struct sk_buff *);
	int (*output)(struct net *, struct sock *, struct sk_buff *);
};

int dst_input(struct sk_buff *skb);
int dst_output(struct net *net, struct sock *sk, struct sk_buff *skb);

#endif
