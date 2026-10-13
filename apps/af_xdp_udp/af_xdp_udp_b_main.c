#include "flow_point/cord_flow_point.h"
#ifdef ENABLE_XDP_DATAPLANE

#include <event_handler/cord_linux_api_event_handler.h>
#include <flow_point/cord_xdp_flow_point.h>
#include <memory/cord_memory.h>
#include <match/cord_match.h>
#include <cord_error.h>

#define ETH_IFACE_B_NAME "veth2"

#define XDP_NUM_FRAMES 64
#define XDP_FRAME_SIZE 2048
#define XDP_RX_RING_SIZE (XDP_NUM_FRAMES / 2)
#define XDP_TX_RING_SIZE (XDP_NUM_FRAMES / 2)
#define XDP_FILL_RING_SIZE (XDP_NUM_FRAMES / 2)
#define XDP_COMP_RING_SIZE (XDP_NUM_FRAMES / 2)

#define BATCH_SIZE (XDP_NUM_FRAMES / 2)

static struct
{
    CordFlowPoint *l2_xdp_b;
    CordEventHandler *evh;
} cord_app_context;

static void cord_app_setup(void)
{
    CORD_LOG("[CordApp] No manual additional setup required.\n");
}

static void cord_app_cleanup(void)
{
    CORD_LOG("[CordApp] Destroying all objects!\n");
    CORD_DESTROY_FLOW_POINT(cord_app_context.l2_xdp_b);
    CORD_DESTROY_EVENT_HANDLER(cord_app_context.evh);
}

static void cord_app_sigint_callback(int sig)
{
    CORD_LOG("[CordApp] Terminating the PacketCord AF_XDP Patch App!\n");
    cord_app_cleanup();
    CORD_ASYNC_SAFE_EXIT(CORD_OK);
}

static cord_retval_t CordXdpFlowPoint_batch_rx_(CordXdpFlowPoint * const self, uint16_t queue_id, void *buffer, size_t batch_target, ssize_t *rx_packets)
{
#ifdef CORD_FLOW_POINT_LOG
    CORD_LOG("[CordXdpFlowPoint] rx()\n");
#endif

    (void)queue_id;

    struct cord_xdp_pkt_desc *pkt_descs = (struct cord_xdp_pkt_desc *)buffer;
    struct cord_xdp_socket_info *xsk_info = *(self->xsk_info);

    uint32_t total_received = 0;
    uint32_t idx_rx = 0;

    // Accumulate until we hit batch_target or ring is empty
    while (total_received < batch_target)
    {
        uint32_t wanted = batch_target - total_received;
        uint32_t rcvd = xsk_ring_cons__peek(&xsk_info->rx, wanted, &idx_rx);

        if (rcvd == 0)
            break;

        for (uint32_t i = 0; i < rcvd; i++)
        {
            const struct xdp_desc *rx_desc = xsk_ring_cons__rx_desc(&xsk_info->rx, idx_rx++);
            uint32_t slot = total_received + i;

            pkt_descs[slot].addr = rx_desc->addr;
            pkt_descs[slot].len = rx_desc->len;
            pkt_descs[slot].data = xsk_umem__get_data(xsk_info->umem_area, rx_desc->addr);
            pkt_descs[slot].src_socket = xsk_info;
        }

        total_received += rcvd;
    }

    if (total_received > 0)
    {
        // Release all accumulated RX descriptors
        xsk_ring_cons__release(&xsk_info->rx, total_received);

        // Replenish Fill Ring with the exact same addresses
        uint32_t idx_fq = 0;
        uint32_t reserved = xsk_ring_prod__reserve(&xsk_info->fq, total_received, &idx_fq);
        if (reserved == total_received)
        {
            for (uint32_t i = 0; i < total_received; i++)
            {
                *xsk_ring_prod__fill_addr(&xsk_info->fq, idx_fq++) = pkt_descs[i].addr;
            }
            xsk_ring_prod__submit(&xsk_info->fq, total_received);
        }

        // Kernel wakeup check
        if (xsk_ring_prod__needs_wakeup(&xsk_info->fq))
        {
            recvfrom(xsk_socket__fd(xsk_info->xsk), NULL, 0, MSG_DONTWAIT, NULL, NULL);
        }
    }

    *rx_packets = total_received;
    return CORD_OK;
}

int main(void)
{
    cord_retval_t cord_retval;
    struct cord_xdp_socket_info *xsk_b;
    struct cord_xdp_pkt_desc pkt_descs[BATCH_SIZE];
    ssize_t rx_packets = 0;

    CORD_LOG("[CordApp] Launching the PacketCord AF_XDP Patch App!\n");

    signal(SIGINT, cord_app_sigint_callback);

    xsk_b = cord_xdp_socket_alloc(ETH_IFACE_B_NAME, 0, XDP_NUM_FRAMES, XDP_FRAME_SIZE,
                                  XDP_RX_RING_SIZE, XDP_TX_RING_SIZE,
                                  XDP_FILL_RING_SIZE, XDP_COMP_RING_SIZE);
    cord_xdp_socket_init(&xsk_b, true);

    cord_app_context.l2_xdp_b = CORD_CREATE_XDP_FLOW_POINT('B', &xsk_b);

    CORD_XDP_FLOW_POINT_FILL(cord_app_context.l2_xdp_b);

    cord_app_context.evh = CORD_CREATE_LINUX_API_EVENT_HANDLER('E', -1);

    cord_retval = CORD_EVENT_HANDLER_REGISTER_FLOW_POINT(cord_app_context.evh, cord_app_context.l2_xdp_b);

    (void)cord_retval;

    while (1)
    {
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
            if (cord_app_context.evh->events[n].data.fd == cord_app_context.l2_xdp_b->io_handle)
            {
                CordXdpFlowPoint_batch_rx_((CordXdpFlowPoint *)cord_app_context.l2_xdp_b, 0, pkt_descs, BATCH_SIZE, &rx_packets);
                if (rx_packets > 0)
                {
                    for (ssize_t n = 0; n < rx_packets; n++)
                    {
                        uint8_t *pkt_data = (uint8_t *)pkt_descs[n].data;
                        uint32_t pkt_len  = pkt_descs[n].len;

                        if (pkt_len >= sizeof(cord_eth_hdr_t))
                        {
                            cord_eth_hdr_t *eth = cord_header_eth(pkt_data);
                            uint16_t eth_type_field = cord_get_field_eth_type_ntohs(eth);

                            CORD_LOG("[CordApp] Log (EthType): 0x%04X (Pkt %zu/%zu, Len: %u)\n",
                                eth_type_field, n + 1, rx_packets, pkt_len);
                        }
                    }
                }
            }
        }
    }

    cord_app_cleanup();

    return CORD_OK;
}

#endif // ENABLE_XDP_DATAPLANE
