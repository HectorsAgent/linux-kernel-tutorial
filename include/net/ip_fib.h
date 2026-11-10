#ifndef TUTORIAL_NET_IP_FIB_H
#define TUTORIAL_NET_IP_FIB_H

#include <linux/types.h>
#include <tutorial/limits.h>

struct net_device;

struct fib_route {
	u32 prefix;
	u8 prefix_len;
	struct net_device *dev;
	u32 gateway;
	bool local;
};
struct fib_table {
	struct fib_route routes[MAX_ROUTES];
	size_t count;
};

bool prefix_match(u32 ip, u32 prefix, u8 len);
void fib_add_route(struct fib_table *table, u32 prefix, u8 len,
	struct net_device *dev, u32 gateway, bool local);

#endif
