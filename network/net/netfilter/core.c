#include "learn_net.h"

const char *nf_hook_name(unsigned int hook)
{
	static const char *const names[] = {
		"PREROUTING", "INPUT", "FORWARD", "OUTPUT", "POSTROUTING"
	};
	return hook < NF_INET_NUMHOOKS ? names[hook] : "okänd hook";
}

int nf_register_net_hook(struct net *net, struct nf_hook_ops ops)
{
	size_t index;

	if (ops.hooknum >= NF_INET_NUMHOOKS ||
	    net->nf.count[ops.hooknum] == MAX_HOOKS)
		return -1;
	index = net->nf.count[ops.hooknum]++;
	/* Lägre prioritetstal körs först; lika prioritet behåller ordningen. */
	while (index && net->nf.hooks[ops.hooknum][index - 1].priority > ops.priority) {
		net->nf.hooks[ops.hooknum][index] = net->nf.hooks[ops.hooknum][index - 1];
		index--;
	}
	net->nf.hooks[ops.hooknum][index] = ops;
	return 0;
}

int nf_hook_slow(struct sk_buff *skb, const struct nf_hook_state *state)
{
	struct net *net = state->net;

	for (size_t i = 0; i < net->nf.count[state->hook]; i++) {
		struct nf_hook_ops *ops = &net->nf.hooks[state->hook][i];
		unsigned int verdict = ops->hook(ops->priv, skb, state);

		trace_skb(net, ops->name, skb);
		if (verdict != NF_ACCEPT)
			return drop_skb(net, skb, "NF_DROP");
	}
	return 1; /* Alla accepterade; anroparen får köra fortsättningen. */
}

int NF_HOOK(u8 pf, unsigned int hook, struct net *net, struct sock *sk,
	struct sk_buff *skb, struct net_device *in, struct net_device *out,
	nf_okfn okfn)
{
	struct nf_hook_state state = {
		.pf = pf, .hook = (u8)hook, .net = net, .sk = sk,
		.in = in, .out = out, .okfn = okfn
	};
	int result;

	if (hook >= NF_INET_NUMHOOKS || pf != NFPROTO_IPV4)
		return drop_skb(net, skb, "ogiltig hook");
	net->hook_hits[hook]++;
	trace_skb(net, nf_hook_name(hook), skb);
	result = nf_hook_slow(skb, &state);
	return result == 1 ? okfn(net, sk, skb) : result;
}
