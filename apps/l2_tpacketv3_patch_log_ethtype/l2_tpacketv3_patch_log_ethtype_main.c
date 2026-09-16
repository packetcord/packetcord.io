#include <cord_flow/event_handler/cord_linux_api_event_handler.h>
#include <cord_flow/flow_point/cord_l2_tpacketv3_flow_point.h>
#include <cord_flow/memory/cord_memory.h>
#include <cord_flow/match/cord_match.h>
#include <cord_error.h>
#include <signal.h>
#include <errno.h>

#define ETH_IFACE_A_NAME "veth1"
#define ETH_IFACE_B_NAME "veth2"

#define TPACKET_V3_BLOCK_SIZE (1 << 18)
#define TPACKET_V3_FRAME_SIZE 2048
#define TPACKET_V3_BLOCK_NUM  256

static struct
{
    CordFlowPoint *l2_eth_a;
    CordFlowPoint *l2_eth_b;
    CordEventHandler *evh;
} cord_app_context;

static void cord_app_setup(void)
{
    CORD_LOG("[CordApp] No manual additional setup required.\n");
}

static void cord_app_cleanup(void)
{
    CORD_LOG("[CordApp] Destroying all objects!\n");
    CORD_DESTROY_FLOW_POINT(cord_app_context.l2_eth_a);
    CORD_DESTROY_FLOW_POINT(cord_app_context.l2_eth_b);
    CORD_DESTROY_EVENT_HANDLER(cord_app_context.evh);
}

static void cord_app_sigint_callback(int sig)
{
    cord_app_cleanup();
    CORD_LOG("[CordApp] Terminating the PacketCord TPACKET_V3 Patch App!\n");
    CORD_ASYNC_SAFE_EXIT(CORD_OK);
}

int main(void)
{
    cord_retval_t cord_retval;
    struct cord_tpacketv3_ring *rx_ring_a;
    struct cord_tpacketv3_ring *rx_ring_b;
    ssize_t rx_packets = 0;
    ssize_t tx_packets = 0;

    CORD_LOG("[CordApp] Launching the PacketCord TPACKET_V3 Patch App!\n");

    signal(SIGINT, cord_app_sigint_callback);

    rx_ring_a = cord_tpacketv3_ring_alloc(TPACKET_V3_BLOCK_SIZE, TPACKET_V3_FRAME_SIZE, TPACKET_V3_BLOCK_NUM);
    rx_ring_b = cord_tpacketv3_ring_alloc(TPACKET_V3_BLOCK_SIZE, TPACKET_V3_FRAME_SIZE, TPACKET_V3_BLOCK_NUM);

    cord_app_context.l2_eth_a = CORD_CREATE_L2_TPACKETV3_FLOW_POINT('A', ETH_IFACE_A_NAME, &rx_ring_a);
    cord_app_context.l2_eth_b = CORD_CREATE_L2_TPACKETV3_FLOW_POINT('B', ETH_IFACE_B_NAME, &rx_ring_b);

    cord_app_context.evh = CORD_CREATE_LINUX_API_EVENT_HANDLER('E', -1);

    cord_retval = CORD_EVENT_HANDLER_REGISTER_FLOW_POINT(cord_app_context.evh, cord_app_context.l2_eth_a);
    cord_retval = CORD_EVENT_HANDLER_REGISTER_FLOW_POINT(cord_app_context.evh, cord_app_context.l2_eth_b);

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
            // A ---> B
            if (cord_app_context.evh->events[n].data.fd == cord_app_context.l2_eth_a->io_handle)
            {
                CORD_FLOW_POINT_RX(cord_app_context.l2_eth_a, UNUSED_ARG, &rx_ring_a, UNUSED_ARG, &rx_packets);
                if (rx_packets > 0)
                {
                    struct tpacket_block_desc *block_desc = (struct tpacket_block_desc *)rx_ring_a->iov_ring[rx_ring_a->block_idx].iov_base;
                    struct tpacket3_hdr *frame_hdr = (struct tpacket3_hdr *)((uint8_t *)block_desc + block_desc->hdr.bh1.offset_to_first_pkt);

                    for (ssize_t p = 0; p < rx_packets; p++)
                    {
                        uint8_t *pkt_data = (uint8_t *)frame_hdr + frame_hdr->tp_mac;

                        cord_eth_hdr_t *eth = cord_header_eth(pkt_data);
                        uint16_t eth_type_field = cord_get_field_eth_type_ntohs(eth);

                        CORD_LOG("[CordApp] Log (EthType): 0x%04X (Block %u, Pkt %zu/%lu, Len: %u)\n", 
                                eth_type_field, rx_ring_a->block_idx, p + 1, rx_packets, frame_hdr->tp_snaplen);

                        if (frame_hdr->tp_next_offset == 0)
                            break;

                        frame_hdr = (struct tpacket3_hdr *)((uint8_t *)frame_hdr + frame_hdr->tp_next_offset);
                    }

                    CORD_FLOW_POINT_TX(cord_app_context.l2_eth_b, UNUSED_ARG, &rx_ring_a, rx_packets, &tx_packets);
                }
            }

            // B ---> A
            if (cord_app_context.evh->events[n].data.fd == cord_app_context.l2_eth_b->io_handle)
            {
                CORD_FLOW_POINT_RX(cord_app_context.l2_eth_b, UNUSED_ARG, &rx_ring_b, UNUSED_ARG, &rx_packets);
                if (rx_packets > 0)
                {
                    CORD_FLOW_POINT_TX(cord_app_context.l2_eth_a, UNUSED_ARG, &rx_ring_b, rx_packets, &tx_packets);
                }
            }
        }
    }

    cord_app_cleanup();

    return CORD_OK;
}
