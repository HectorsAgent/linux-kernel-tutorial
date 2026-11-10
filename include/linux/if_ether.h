#ifndef TUTORIAL_LINUX_IF_ETHER_H
#define TUTORIAL_LINUX_IF_ETHER_H

#include <linux/types.h>

#define ETH_P_IP 0x0800

struct ethhdr {
	u8 h_source[6], h_dest[6];
	u16 h_proto;
};

#endif
