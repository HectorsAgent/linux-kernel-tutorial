#ifndef TUTORIAL_LINUX_IF_ETHER_H
#define TUTORIAL_LINUX_IF_ETHER_H

#include <linux/types.h>

struct ethhdr {
	u8 h_source[6], h_dest[6];
};

#endif
