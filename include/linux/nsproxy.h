#ifndef TUTORIAL_LINUX_NSPROXY_H
#define TUTORIAL_LINUX_NSPROXY_H

struct net;

struct nsproxy {
	struct net *net_ns;
};

#endif
