CC = cc
AR = ar
CFLAGS = -std=c11 -Wall -Wextra -Werror -Wpedantic -g -O0
CPPFLAGS = -Iinclude
BUILD = build

SOURCES = net/core/dev.c net/core/skbuff.c net/core/net_namespace.c \
	net/ipv4/route.c net/ipv4/ip_input.c net/ipv4/ip_forward.c \
	net/ipv4/ip_output.c net/ipv4/tcp.c \
	net/netfilter/core.c net/netfilter/nf_conntrack_core.c \
	net/netfilter/nf_nat_core.c net/netfilter/nf_tables.c
OBJECTS = $(SOURCES:%.c=$(BUILD)/%.o)

.PHONY: all demo test clean
all: $(BUILD)/liblearnnet.a $(BUILD)/nat_demo

$(BUILD)/liblearnnet.a: $(OBJECTS)
	$(AR) rcs $@ $^

$(BUILD)/%.o: %.c include/learn_net.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/nat_demo: examples/nat_demo.c $(BUILD)/liblearnnet.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD)/liblearnnet.a -o $@

$(BUILD)/test_stack: tests/test_stack.c $(BUILD)/liblearnnet.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD)/liblearnnet.a -o $@

demo: $(BUILD)/nat_demo
	./$(BUILD)/nat_demo

test: $(BUILD)/test_stack
	./$(BUILD)/test_stack

clean:
	rm -rf build build-asan
