#include <stddef.h>
#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/socket.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>
#include "l2_patch_ebpf.h"

char LICENSE[] SEC("license") = "MIT";

struct
{
	__uint(type, BPF_MAP_TYPE_RINGBUF);
	__uint(max_entries, 1024 * 1024);
} rb SEC(".maps");

SEC("socket")
int socket_handler(struct __sk_buff *skb)
{
    // Accept IPv4 traffic only
    if (skb->protocol != bpf_htons(ETH_P_IP))
        return 0;

    struct so_event *e;
    e = bpf_ringbuf_reserve(&rb, sizeof(*e), 0);
    if (!e)
        return 0;

    __builtin_memset(e, 0, sizeof(*e));

    bpf_skb_load_bytes(skb, 0, e->dst_mac, 6);
    // Offset 6 = Source MAC (6 bytes)
    bpf_skb_load_bytes(skb, 6, e->src_mac, 6);

    e->ifindex = skb->ifindex;
    e->pkt_type = skb->pkt_type;

    bpf_ringbuf_submit(e, 0);

    return skb->len;
}
