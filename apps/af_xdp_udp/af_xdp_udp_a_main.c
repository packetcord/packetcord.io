#include <stddef.h>
#ifdef ENABLE_XDP_DATAPLANE

#include <cord_flow/event_handler/cord_linux_api_event_handler.h>
#include <cord_flow/flow_point/cord_xdp_flow_point.h>
#include <cord_flow/memory/cord_memory.h>
#include <cord_flow/match/cord_match.h>
#include <cord_flow/protocol_headers/cord_l2_protocols.h>
#include <cord_flow/protocol_headers/cord_l3_protocols.h>
#include <cord_flow/protocol_headers/cord_l4_protocols.h>
#include <cord_flow/cord_error.h>
#include <cord_craft/protocols/cord_protocols.h>

#define ETH_IFACE_A_NAME "veth1"

#define XDP_NUM_FRAMES 4096
#define XDP_FRAME_SIZE 4096
#define XDP_RX_RING_SIZE 512
#define XDP_TX_RING_SIZE 512
#define XDP_FILL_RING_SIZE 512
#define XDP_COMP_RING_SIZE 512

#define SINGLE_PACKET 1
#define PAYLOAD "PacketCord.io Crafted Packet!!!\n"
#define PAYLOAD_LEN 32

static struct
{
    CordFlowPoint *l2_xdp_a;
    CordEventHandler *evh;
} cord_app_context;

static void cord_app_setup(void)
{
    CORD_LOG("[CordApp] No manual additional setup required.\n");
}

static void cord_app_cleanup(void)
{
    CORD_LOG("[CordApp] Destroying all objects!\n");
    CORD_DESTROY_FLOW_POINT(cord_app_context.l2_xdp_a);
    CORD_DESTROY_EVENT_HANDLER(cord_app_context.evh);
}

static void cord_app_sigint_callback(int sig)
{
    CORD_LOG("[CordApp] Terminating the PacketCord AF_XDP Patch App!\n");
    cord_app_cleanup();
    CORD_ASYNC_SAFE_EXIT(CORD_OK);
}

int main(void)
{
    cord_retval_t cord_retval;
    struct cord_xdp_socket_info *xsk_a;
    ssize_t rx_packets = 0;
    ssize_t tx_packets = 0;

    CORD_LOG("[CordApp] Launching the PacketCord AF_XDP UDP App!\n");

    signal(SIGINT, cord_app_sigint_callback);

    xsk_a = cord_xdp_socket_alloc(ETH_IFACE_A_NAME, 0, XDP_NUM_FRAMES, XDP_FRAME_SIZE,
                                  XDP_RX_RING_SIZE, XDP_TX_RING_SIZE,
                                  XDP_FILL_RING_SIZE, XDP_COMP_RING_SIZE);
    cord_xdp_socket_init(&xsk_a, true);

    cord_app_context.l2_xdp_a = CORD_CREATE_XDP_FLOW_POINT('A', &xsk_a);

    CORD_XDP_FLOW_POINT_FILL(cord_app_context.l2_xdp_a);
    
    uint64_t frame_offset = 0;
    uint8_t *pkt = (uint8_t *)xsk_umem__get_data(xsk_a->umem_area, frame_offset); // Attemp this for zero-copy and change the TX() method of the XDP FlowPoint to skip memcpy()

    // Calculate payload and packet sizes
    const int payload_len = PAYLOAD_LEN;
    const int ip_total_len = sizeof(cord_ipv4_hdr_t) + sizeof(cord_udp_hdr_t) + payload_len;
    size_t frame_len = sizeof(cord_eth_hdr_t) + ip_total_len;

    // Layer offsets
    cord_eth_hdr_t *eth = (cord_eth_hdr_t *) pkt;
    cord_ipv4_hdr_t *ip = (cord_ipv4_hdr_t *) (pkt + sizeof(cord_eth_hdr_t));
    cord_udp_hdr_t *udp = (cord_udp_hdr_t *) (pkt + sizeof(cord_eth_hdr_t) + sizeof(cord_ipv4_hdr_t));

    // Payload
    uint8_t *payload = (uint8_t *) (pkt + sizeof(cord_eth_hdr_t) + sizeof(cord_ipv4_hdr_t) + sizeof(cord_udp_hdr_t));
    memcpy((void *)payload, (void *)PAYLOAD, PAYLOAD_LEN);

    // Layer 2 Ethernet Header
    uint8_t source_mac[CORD_ETH_ALEN] = { 0x02, 0x42, 0xAC, 0xAA, 0xAA, 0xAA };
    uint8_t   dest_mac[CORD_ETH_ALEN] = { 0x02, 0x42, 0xAC, 0xBB, 0xBB, 0xBB };

    memcpy((void *)&(eth->h_source), (void *)&source_mac, CORD_ETH_ALEN);
    memcpy((void *)&(eth->h_dest), (void *)&dest_mac, CORD_ETH_ALEN);
    eth->h_proto = cord_htons(CORD_ETH_P_IP);

    // Layer 3 IPv4 Header (could support IPv6 in the future)
    ip->version = 4;
    ip->ihl = 5; // Base header
    ip->tos = 0;
    ip->tot_len = cord_htons(ip_total_len);
    ip->id = cord_htons(123);
    ip->frag_off = cord_htons(0x4000); // No IP fragmentation
    ip->ttl = 32;
    ip->protocol = CORD_IPPROTO_UDP;
    ip->saddr.addr = inet_addr("172.16.16.1");
    ip->daddr.addr = inet_addr("172.16.16.2");
    ip->check = cord_htons(cord_calculate_ipv4_checksum(ip));      // Could be off-loaded to NIC

    // Layer 4 UDP Header
    udp->source = cord_htons(5050);
    udp->dest = cord_htons(6060);
    udp->len = cord_htons(sizeof(cord_udp_hdr_t) + payload_len);
    udp->check = cord_htons(cord_calculate_udp_checksum_ipv4(ip)); // Could be off-loaded to NIC, too

    //
    // Alternatively - use an eBPF programme like https://docs.ebpf.io/linux/concepts/af_xdp/
    //

    // Send packet
    CORD_LOG("[CordApp] Send UDP Packet...\n");

    uint32_t idx_tx = 0;

    // Reserve 1 descriptor slot in the TX ring
    uint32_t reserved = xsk_ring_prod__reserve(&xsk_a->tx, SINGLE_PACKET, &idx_tx);
    if (reserved < 1)
    {
        CORD_LOG("[CordApp] Error: Failed to reserve TX ring slot!\n");
    }
    else
    {
        // Populate the TX descriptor pointing to frame_offset
        struct xdp_desc *tx_desc = xsk_ring_prod__tx_desc(&xsk_a->tx, idx_tx);
        tx_desc->addr = frame_offset;
        tx_desc->len = frame_len;

        // Submit the reserved slot to the kernel TX ring
        xsk_ring_prod__submit(&xsk_a->tx, SINGLE_PACKET);

        // Kick the kernel driver to start transmission
        if (xsk_ring_prod__needs_wakeup(&xsk_a->tx))
        {
            sendto(xsk_socket__fd(xsk_a->xsk), NULL, 0, MSG_DONTWAIT, NULL, 0);
        }
        else
        {
            // Call sendto unconditionally if XDP_USE_NEED_WAKEUP isn't set
            sendto(xsk_socket__fd(xsk_a->xsk), NULL, 0, MSG_DONTWAIT, NULL, 0);
        }

        CORD_LOG("[CordApp] Packet enqueued to TX ring and kernel kicked!\n");
    }

    // Reclaim frame from Completion Ring once transmission finishes
    uint32_t idx_cq = 0;
    uint32_t completed = xsk_ring_cons__peek(&xsk_a->cq, 1, &idx_cq);
    if (completed > 0)
    {
        uint64_t completed_addr = *xsk_ring_cons__comp_addr(&xsk_a->cq, idx_cq);
        xsk_ring_cons__release(&xsk_a->cq, completed);
        CORD_LOG("[CordApp] TX completed for frame address: 0x%" PRIx64 "\n", completed_addr);
    }

    cord_app_cleanup();

    return CORD_OK;
}

#endif // ENABLE_XDP_DATAPLANE
