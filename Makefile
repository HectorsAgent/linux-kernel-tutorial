CC = cc
AR = ar
CFLAGS = -std=c11 -Wall -Wextra -Werror -Wpedantic -g -O0
CPPFLAGS = -Iinclude
BUILD = build

SOURCES = kernel/fork.c fs/file.c net/socket.c net/ipv4/af_inet.c lib/iov_iter.c \
	net/core/dev.c net/core/neighbour.c net/core/skbuff.c \
	net/core/net_namespace.c net/ipv4/route.c \
	net/ipv4/ip_input.c net/ipv4/ip_forward.c net/ipv4/ip_output.c \
	net/ipv4/tcp.c net/ipv4/tcp_input.c net/ipv4/tcp_output.c net/ipv4/tcp_ipv4.c \
	net/netfilter/core.c net/netfilter/nf_conntrack_core.c \
	net/netfilter/nf_nat_core.c net/netfilter/nf_nat_proto.c \
	net/netfilter/nf_tables_core.c net/netfilter/nf_tables_api.c \
	drivers/net/loopback.c drivers/net/veth.c drivers/net/tutorial.c lib/tutorial.c
OBJECTS = $(SOURCES:%.c=$(BUILD)/%.o)

.PHONY: all demo nat-demo test clean
all: $(BUILD)/liblearnnet.a $(BUILD)/send_demo $(BUILD)/nat_demo

$(BUILD)/liblearnnet.a: $(OBJECTS)
	$(AR) rcs $@ $^

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/nat_demo: tools/net/nat_demo.c $(BUILD)/liblearnnet.a
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP $< $(BUILD)/liblearnnet.a -o $@

$(BUILD)/send_demo: tools/net/send_demo.c $(BUILD)/liblearnnet.a
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP $< $(BUILD)/liblearnnet.a -o $@

$(BUILD)/test_stack: tools/testing/selftests/net/test_stack.c $(BUILD)/liblearnnet.a
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP $< $(BUILD)/liblearnnet.a -o $@

$(BUILD)/test_syscall: tools/testing/selftests/net/test_syscall.c $(BUILD)/liblearnnet.a
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP $< $(BUILD)/liblearnnet.a -o $@

demo: $(BUILD)/send_demo
	./$(BUILD)/send_demo

nat-demo: $(BUILD)/nat_demo
	./$(BUILD)/nat_demo

test: $(BUILD)/test_stack $(BUILD)/test_syscall
	./$(BUILD)/test_stack
	./$(BUILD)/test_syscall

-include $(OBJECTS:.o=.d) $(BUILD)/send_demo.d $(BUILD)/nat_demo.d \
	$(BUILD)/test_stack.d $(BUILD)/test_syscall.d

clean:
	rm -rf build build-asan
