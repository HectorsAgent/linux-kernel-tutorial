#ifndef TUTORIAL_TUTORIAL_LIMITS_H
#define TUTORIAL_TUTORIAL_LIMITS_H

#include <linux/types.h>

#define IP4(a, b, c, d) (((u32)(a) << 24) | ((u32)(b) << 16) | \
			 ((u32)(c) << 8) | (u32)(d))
#define MAX_DEVS 8
#define MAX_ROUTES 32
#define MAX_NEIGH 32
#define MAX_SOCKS 8
#define MAX_FDS 16
#define MAX_CT 32
#define MAX_HOOKS 8
#define MAX_RULES 16
#define TCP_BUFFER_SIZE 4096
#define PACKET_DATA_SIZE 512

#endif
