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

#define XDP_NUM_FRAMES 32
#define XDP_FRAME_SIZE 2048
#define XDP_RX_RING_SIZE (XDP_NUM_FRAMES / 2)
#define XDP_TX_RING_SIZE (XDP_NUM_FRAMES / 2)
#define XDP_FILL_RING_SIZE (XDP_NUM_FRAMES / 2)
#define XDP_COMP_RING_SIZE (XDP_NUM_FRAMES / 2)

#define BATCH_SIZE (XDP_NUM_FRAMES / 2)

static struct
{
    uint64_t frame_offsets[BATCH_SIZE];
    size_t frame_lengths[BATCH_SIZE];
} umem_frame_tracker = {
    .frame_offsets = { 0 },
    .frame_lengths = { 0 },
}; // Or probably a FIFO over this...

const uint8_t *payload_arr[BATCH_SIZE] = {
    "Payload #0.\n",
    "Payload #1..\n",
    "Payload #2...\n",
    "Payload #3....\n",
    "Payload #4.....\n",
    "Payload #5......\n",
    "Payload #6.......\n",
    "Payload #7........\n",
    "Payload #8.........\n",
    "Payload #9..........\n",
    "Payload #10..........\n",
    "Payload #11...........\n",
    "Payload #12............\n",
    "Payload #13.............\n",
    "Payload #14..............\n",
    "Payload #15...............\n"
}; // GTP-U will reside here as payload, but keep it the strings for easier logging...

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

static void af_xdp_send_single(struct cord_xdp_socket_info *xdp_sock, uint64_t frame_offset, size_t frame_len)
{
    // Send packet
    CORD_LOG("[CordApp] Send UDP Packet...\n");

    uint32_t idx_tx = 0;

    // Reserve 1 descriptor slot in the TX ring
    uint32_t reserved = xsk_ring_prod__reserve(&xdp_sock->tx, 1, &idx_tx);
    if (reserved < 1)
    {
        CORD_LOG("[CordApp] Error: Failed to reserve TX ring slot!\n");
    }
    else
    {
        // Populate the TX descriptor pointing to frame_offset
        struct xdp_desc *tx_desc = xsk_ring_prod__tx_desc(&xdp_sock->tx, idx_tx);
        tx_desc->addr = frame_offset;
        tx_desc->len = frame_len;

        // Submit the reserved slot to the kernel TX ring
        xsk_ring_prod__submit(&xdp_sock->tx, 1);

        // Kick the kernel driver to start transmission
        if (xsk_ring_prod__needs_wakeup(&xdp_sock->tx))
        {
            sendto(xsk_socket__fd(xdp_sock->xsk), NULL, 0, MSG_DONTWAIT, NULL, 0);
        }

        CORD_LOG("[CordApp] Packet enqueued to TX ring and kernel kicked!\n");
    }

    // Reclaim frame from Completion Ring once transmission finishes
    uint32_t idx_cq = 0;
    uint32_t completed = xsk_ring_cons__peek(&xdp_sock->cq, 1, &idx_cq);
    if (completed > 0)
    {
        uint64_t completed_addr = *xsk_ring_cons__comp_addr(&xdp_sock->cq, idx_cq);
        xsk_ring_cons__release(&xdp_sock->cq, completed);
        CORD_LOG("[CordApp] TX completed for frame address: 0x%" PRIx64 "\n", completed_addr);
    }
}

static void af_xdp_send_batch(struct cord_xdp_socket_info *xdp_sock, const uint64_t *frame_offsets, const size_t *frame_lens, uint32_t nb)
{
    if (nb == 0)
        return;

    CORD_LOG("[CordApp] Sending batch of %u UDP Packets...\n", nb);

    // Drain Completion Ring first to free up space/buffers from previous transmissions
    uint32_t idx_cq = 0;
    uint32_t completed = xsk_ring_cons__peek(&xdp_sock->cq, xdp_sock->comp_ring_size, &idx_cq);
    if (completed > 0)
    {
        for (uint32_t i = 0; i < completed; i++)
        {
            uint64_t completed_addr = *xsk_ring_cons__comp_addr(&xdp_sock->cq, idx_cq++);
            CORD_LOG("[CordApp] TX completed for frame address: 0x%" PRIx64 "\n", completed_addr);
        }
        xsk_ring_cons__release(&xdp_sock->cq, completed);
    }

    // Reserve 'nb' descriptor slots in the TX ring
    uint32_t idx_tx = 0;
    uint32_t reserved = xsk_ring_prod__reserve(&xdp_sock->tx, nb, &idx_tx);

    if (reserved < nb)
    {
        CORD_LOG("[CordApp] Error: Could only reserve %u out of %u requested TX slots!\n", reserved, nb);
        if (reserved == 0)
            return;
    }

    // Populate ALL reserved TX descriptors in a loop
    for (uint32_t i = 0; i < reserved; i++)
    {
        struct xdp_desc *tx_desc = xsk_ring_prod__tx_desc(&xdp_sock->tx, idx_tx++);
        tx_desc->addr = frame_offsets[i];
        tx_desc->len  = frame_lens[i];
    }

    // Submit the exact number of populated descriptors
    xsk_ring_prod__submit(&xdp_sock->tx, reserved);

    // Single sendto() kick for the entire batch)
    if (xsk_ring_prod__needs_wakeup(&xdp_sock->tx))
    {
        sendto(xsk_socket__fd(xdp_sock->xsk), NULL, 0, MSG_DONTWAIT, NULL, 0);
    }

    CORD_LOG("[CordApp] Batch of %u packets enqueued and kernel kicked!\n", reserved);
}

int main(void)
{
    cord_retval_t cord_retval;
    struct cord_xdp_socket_info *xsk_a;
    ssize_t rx_packets = 0;
    ssize_t tx_packets = 0;

    CORD_LOG("[CordApp] Launching the PacketCord AF_XDP UDP App!\n");

    signal(SIGINT, cord_app_sigint_callback);

    //
    // Initialisation
    //
    xsk_a = cord_xdp_socket_alloc(ETH_IFACE_A_NAME, 0, XDP_NUM_FRAMES, XDP_FRAME_SIZE,
                                  XDP_RX_RING_SIZE, XDP_TX_RING_SIZE,
                                  XDP_FILL_RING_SIZE, XDP_COMP_RING_SIZE);
    cord_xdp_socket_init(&xsk_a, true); // We can also consider using shared UMEM

    cord_app_context.l2_xdp_a = CORD_CREATE_XDP_FLOW_POINT('A', &xsk_a);

    CORD_XDP_FLOW_POINT_FILL(cord_app_context.l2_xdp_a);

    uint8_t *umem_base = (uint8_t *)xsk_umem__get_data(xsk_a->umem_area, umem_frame_tracker.frame_offsets[0]); // Attemp this for zero-copy and change the TX() method of the XDP FlowPoint to skip memcpy()
    uint8_t *current_frame_base = 0;

    //
    // Preload crafted UDP packets inside the UMEM
    //
    for (uint16_t idx = 0; idx < BATCH_SIZE; idx++)
    {
        // Calculate offsets and populate the offsets array (used for batch TX)
        umem_frame_tracker.frame_offsets[idx] = (idx % XDP_NUM_FRAMES) * XDP_FRAME_SIZE;

        // Move the base pointer
        current_frame_base = umem_base + umem_frame_tracker.frame_offsets[idx];

        cord_eth_hdr_t *eth = (cord_eth_hdr_t *)current_frame_base;
        cord_ipv4_hdr_t *ip = (cord_ipv4_hdr_t *)(current_frame_base + sizeof(cord_eth_hdr_t));
        cord_udp_hdr_t *udp = (cord_udp_hdr_t *)(current_frame_base + sizeof(cord_eth_hdr_t) + sizeof(cord_ipv4_hdr_t));

        // Layer 2 Ethernet Header
        uint8_t source_mac[CORD_ETH_ALEN] = { 0x02, 0x42, 0xAC, 0xAA, 0xAA, 0xAA }; // Could be obtained in multiple ways programmatically
        uint8_t   dest_mac[CORD_ETH_ALEN] = { 0x02, 0x42, 0xAC, 0xBB, 0xBB, 0xBB }; // Could be obtained by (crafting) ARP request (for IPv4) / Neigh Solic (for IPv6), or the CAM table (probably via netlink socket) programmatically

        memcpy((void *)&(eth->h_source), (void *)&source_mac, CORD_ETH_ALEN);
        memcpy((void *)&(eth->h_dest), (void *)&dest_mac, CORD_ETH_ALEN);
        eth->h_proto = cord_htons(CORD_ETH_P_IP);

        // Layer 3 IPv4 Header (could support IPv6 in the future)
        ip->version = 4;
        ip->ihl = 5; // Base header
        ip->tos = 0;
        ip->id = cord_htons(idx);          // Or other logic to populate the ID
        ip->frag_off = cord_htons(0x4000); // No IP fragmentation
        ip->ttl = 32;
        ip->protocol = CORD_IPPROTO_UDP;
        ip->saddr.addr = inet_addr("172.16.16.1");
        ip->daddr.addr = inet_addr("172.16.16.2");

        // Layer 4 UDP Header
        udp->source = cord_htons(5050);
        udp->dest = cord_htons(6060);
    }

    //
    // Foreach packet we want to send - set payload and length, calculate checksum; move the UMEM pointer to the next container
    //
    for (uint16_t idx = 0; idx < BATCH_SIZE; idx++)
    {
        // Move the base pointer
        current_frame_base = umem_base + umem_frame_tracker.frame_offsets[idx];

        cord_ipv4_hdr_t *ip = (cord_ipv4_hdr_t *)(current_frame_base + sizeof(cord_eth_hdr_t));
        cord_udp_hdr_t *udp = (cord_udp_hdr_t *)(current_frame_base + sizeof(cord_eth_hdr_t) + sizeof(cord_ipv4_hdr_t));

        // Calculate payload/packet/frame sizes
        const int payload_len = strlen((const char *)payload_arr[idx]);
        const int ip_total_len = sizeof(cord_ipv4_hdr_t) + sizeof(cord_udp_hdr_t) + payload_len;
        umem_frame_tracker.frame_lengths[idx] = sizeof(cord_eth_hdr_t) + ip_total_len;

        udp->len = cord_htons(sizeof(cord_udp_hdr_t) + payload_len);

        // Payload
        uint8_t *payload = (uint8_t *) (current_frame_base + sizeof(cord_eth_hdr_t) + sizeof(cord_ipv4_hdr_t) + sizeof(cord_udp_hdr_t));
        memcpy((void *)payload, (void *)payload_arr[idx], payload_len);

        // Checksums
        ip->tot_len = cord_htons(ip_total_len);
        ip->check = cord_htons(cord_calculate_ipv4_checksum(ip));      // Could be off-loaded to NIC or use Linux kernel implementation
        udp->check = cord_htons(cord_calculate_udp_checksum_ipv4(ip)); // for both IP(v4) and UDP header checksums...

        // Send a single UDP packet over AF_XDP
        //
        // af_xdp_send_single(xsk_a, umem_frame_tracker.frame_offsets[idx], umem_frame_tracker.frame_lengths[idx]);
        //
    }

    af_xdp_send_batch(xsk_a, umem_frame_tracker.frame_offsets, umem_frame_tracker.frame_lengths, BATCH_SIZE);

    cord_app_cleanup();

    return CORD_OK;
}

#endif // ENABLE_XDP_DATAPLANE
