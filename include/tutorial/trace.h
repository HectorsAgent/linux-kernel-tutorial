#ifndef TUTORIAL_TUTORIAL_TRACE_H
#define TUTORIAL_TUTORIAL_TRACE_H



struct net;
struct sk_buff;
struct sock;

void trace_skb(struct net *net, const char *where, const struct sk_buff *skb);
int drop_skb(struct net *net, struct sk_buff *skb, const char *reason);
void trace_task(const char *where, int fd);
void trace_socket(const char *where, struct sock *sk);

#endif
