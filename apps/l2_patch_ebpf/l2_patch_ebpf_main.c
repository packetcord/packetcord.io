//
// # 1. Compile the eBPF C code into BPF bytecode using Clang
// clang -g -O2 -target bpf -c l2_patch_ebpf.bpf.c -o l2_patch_ebpf.bpf.o
//
// # 2. Generate the user-space skeleton header from that bytecode
// bpftool gen skeleton l2_patch_ebpf.bpf.o > l2_patch_ebpf.skel.h
//

#include <cord_flow/event_handler/cord_linux_api_event_handler.h>
#include <cord_flow/flow_point/cord_l2_raw_socket_flow_point.h>
#include <cord_flow/memory/cord_memory.h>
#include <cord_flow/match/cord_match.h>
#include <cord_error.h>

#include <bpf/libbpf.h>
#include "cord_type.h"
#include "l2_patch_ebpf.h"
#include "l2_patch_ebpf.skel.h"

#define MTU_SIZE 1500
#define ETHERNET_HEADER_SIZE 14
#define DOT1Q_TAG_SIZE 4

#define BUFFER_SIZE (MTU_SIZE + ETHERNET_HEADER_SIZE)

#define ETH_IFACE_A_NAME "veth1"
#define ETH_IFACE_B_NAME "veth2"

#define LOG_MAC_ADDRESSES_FROM_USER_SPACE 0

static struct
{
    CordFlowPoint *l2_eth_a;
    CordFlowPoint *l2_eth_b;
    CordEventHandler *evh;
    struct ring_buffer *rb;
	struct l2_patch_ebpf_bpf *skel;
} cord_app_context;

static void cord_app_setup(void)
{
    CORD_LOG("[CordApp] No manual additional setup required.\n");
}

static void cord_app_cleanup(void)
{
    CORD_LOG("[CordApp] Destroying all objects!\n");
    ring_buffer__free(cord_app_context.rb);
    l2_patch_ebpf_bpf__destroy(cord_app_context.skel);
    CORD_DESTROY_FLOW_POINT(cord_app_context.l2_eth_a);
    CORD_DESTROY_FLOW_POINT(cord_app_context.l2_eth_b);
    CORD_DESTROY_EVENT_HANDLER(cord_app_context.evh);
}

static void cord_app_sigint_callback(int sig)
{
    cord_app_cleanup();
    CORD_LOG("[CordApp] Terminating the PacketCord Patch App!\n");
    CORD_ASYNC_SAFE_EXIT(CORD_OK);
}

static int sample_callback(void *ctx, void *data, size_t data_size)
{
    const struct so_event *e = data;

    // Access the packet (header) data and metadata from user-space (after the ring buffer has been polled)
    if (e->pkt_type == PACKET_HOST)
        return 0;

#if (LOG_MAC_ADDRESSES_FROM_USER_SPACE == 1)
    // Log the MAC addresses
    CORD_LOG("[CordApp] eth.src_addr: %02X:%02X:%02X:%02X:%02X:%02X\n",
             e->src_mac[0], e->src_mac[1], e->src_mac[2], e->src_mac[3], e->src_mac[4], e->src_mac[5]);
    CORD_LOG("[CordApp] eth.dst_addr: %02X:%02X:%02X:%02X:%02X:%02X\n", 
             e->dst_mac[0], e->dst_mac[1], e->dst_mac[2], e->dst_mac[3], e->dst_mac[4], e->dst_mac[5]);
#endif

    return CORD_OK;
}

int main(void)
{
    cord_retval_t cord_retval;
    CORD_BUFFER(buffer, BUFFER_SIZE);
    size_t rx_bytes = 0;
    size_t tx_bytes = 0;

    CORD_LOG("[CordApp] Launching the PacketCord Patch App!\n");

    signal(SIGINT, cord_app_sigint_callback);

    cord_app_context.l2_eth_a = CORD_CREATE_L2_RAW_SOCKET_FLOW_POINT('A', ETH_IFACE_A_NAME);
    cord_app_context.l2_eth_b = CORD_CREATE_L2_RAW_SOCKET_FLOW_POINT('B', ETH_IFACE_B_NAME);

    cord_app_context.evh = CORD_CREATE_LINUX_API_EVENT_HANDLER('E', -1);

    cord_retval = CORD_EVENT_HANDLER_REGISTER_FLOW_POINT(cord_app_context.evh, cord_app_context.l2_eth_a);
    cord_retval = CORD_EVENT_HANDLER_REGISTER_FLOW_POINT(cord_app_context.evh, cord_app_context.l2_eth_b);

    // eBPF-related part
	cord_app_context.skel = l2_patch_ebpf_bpf__open_and_load();
	if (cord_app_context.skel == NULL)
    {
        CORD_ERROR("[CordApp] Error: ebpf_filter_bpf__open_and_load()");
	}

    // Set up ring buffer polling
	cord_app_context.rb = ring_buffer__new(bpf_map__fd(cord_app_context.skel->maps.rb), sample_callback, NULL, NULL);
	if (cord_app_context.rb == NULL)
    {
        // Cleanup
        ring_buffer__free(cord_app_context.rb);
	    l2_patch_ebpf_bpf__destroy(cord_app_context.skel);
        CORD_ERROR("[CordApp] Error: ring_buffer__new()");
	}

    // Attach BPF program to raw socket
	int ebpf_prog_fd = bpf_program__fd(cord_app_context.skel->progs.socket_handler);
    cord_filter_type_t filter_type = EBPF_FILTER;
    cord_retval = CORD_FLOW_POINT_ATTACH_FILTER(cord_app_context.l2_eth_a, (void *)&ebpf_prog_fd, (void *)&filter_type);
    if (cord_retval == CORD_OK)
    {
        CORD_LOG("[CordApp] eBPF filter attached successfully!\n");
    }

    while (1)
    {
        // The Linux event
        int nb_fds = CORD_EVENT_HANDLER_WAIT(cord_app_context.evh);

        if (nb_fds == -1)
        {
            if (errno == EINTR)
                continue;
            else
            {
                CORD_ERROR("[CordApp] Error: CORD_EVENT_HANDLER_WAIT()");
                CORD_EXIT(CORD_ERR);
            }
        }

        for (uint8_t n = 0; n < nb_fds; n++)
        {
            //
            // A ---> B
            //
            if (cord_app_context.evh->events[n].data.fd == cord_app_context.l2_eth_a->io_handle)
            {
                cord_retval = CORD_FLOW_POINT_RX(cord_app_context.l2_eth_a, 0, buffer, BUFFER_SIZE, &rx_bytes);
                if (cord_retval != CORD_OK)
                    continue; // Raw socket receive error

                if (rx_bytes < sizeof(cord_eth_hdr_t))
                    continue; // Packet too short to contain Ethernet header

                if (CORD_L2_RAW_SOCKET_FLOW_POINT_ENSURE_INBOUD(cord_app_context.l2_eth_a) != CORD_OK)
                    continue; // Ensure this is not an outgoing packet

                cord_retval = CORD_FLOW_POINT_TX(cord_app_context.l2_eth_b, 0, buffer, rx_bytes, &tx_bytes);
                if (cord_retval != CORD_OK)
                {
                    // Handle the error
                }
            }

            //
            // B ---> A
            //
            if (cord_app_context.evh->events[n].data.fd == cord_app_context.l2_eth_b->io_handle)
            {
                cord_retval = CORD_FLOW_POINT_RX(cord_app_context.l2_eth_b, 0, buffer, BUFFER_SIZE, &rx_bytes);
                if (cord_retval != CORD_OK)
                    continue; // Raw socket receive error

                if (rx_bytes < sizeof(cord_eth_hdr_t))
                    continue; // Packet too short to contain Ethernet header

                if (CORD_L2_RAW_SOCKET_FLOW_POINT_ENSURE_INBOUD(cord_app_context.l2_eth_b) != CORD_OK)
                    continue; // Ensure this is not an outgoing packet

                cord_retval = CORD_FLOW_POINT_TX(cord_app_context.l2_eth_a, 0, buffer, rx_bytes, &tx_bytes);
                if (cord_retval != CORD_OK)
                {
                    // Handle the error
                }
            }
        }

        // The BPF buffer event
        cord_retval = ring_buffer__poll(cord_app_context.rb, 0);
        if (cord_retval < 0)
        {
            CORD_ERROR("[CordApp] Error: ring_buffer__poll()");
        }
    }

    cord_app_cleanup();

    return CORD_OK;
}
