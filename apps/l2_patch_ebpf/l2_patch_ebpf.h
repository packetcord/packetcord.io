#ifndef __EBPF_FILTER_H
#define __EBPF_FILTER_H

#include <linux/types.h>

#define MAX_BUF_SIZE 64


struct so_event {
	__u8 src_mac[6];
    __u8 dst_mac[6];

    __be32 src_ipv4;
    __be32 dst_ipv4;
    union {
        __be32 ports;
        __be16 port16[2];
    };
    __u32 ip_proto;
    __u32 pkt_type;
    __u32 ifindex;
    __u32 payload_length;
    __u8 payload[MAX_BUF_SIZE];
};

#endif /* __EBPF_FILTER_H */
