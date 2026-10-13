#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>

struct {
    __uint(type, BPF_MAP_TYPE_XSKMAP);
    __uint(max_entries, 256);
    __type(key, __u32);
    __type(value, __u32);
} xsk SEC(".maps");

SEC("xdp")
int rx(struct xdp_md *ctx)
{
    void *data, *data_meta, *data_end;
    struct ipv6hdr *ip6h = NULL;
    struct ethhdr *eth = NULL;
    struct udphdr *udp = NULL;
    struct iphdr *iph = NULL;
    struct xdp_meta *meta;
    int ret;

    data = (void *)(long)ctx->data;
    data_end = (void *)(long)ctx->data_end;
    eth = data;
    if (eth + 1 < data_end) {
        if (eth->h_proto == bpf_htons(ETH_P_IP)) {
            iph = (void *)(eth + 1);
            if (iph + 1 < data_end && iph->protocol == IPPROTO_UDP)
                udp = (void *)(iph + 1);
        }
        if (eth->h_proto == bpf_htons(ETH_P_IPV6)) {
            ip6h = (void *)(eth + 1);
            if (ip6h + 1 < data_end && ip6h->nexthdr == IPPROTO_UDP)
                udp = (void *)(ip6h + 1);
        }
        if (udp && udp + 1 > data_end)
            udp = NULL;
    }

    if (!udp)
        return XDP_PASS;

    if (udp->dest != bpf_htons(6060)) // Hard-coding our destination port
        return XDP_PASS;

    return bpf_redirect_map(&xsk, ctx->rx_queue_index, XDP_PASS);
}
