#include <cord_flow/event_handler/cord_linux_api_event_handler.h>
#include <cord_flow/flow_point/cord_l2_tpacketv3_flow_point.h>
#include <cord_flow/memory/cord_memory.h>
#include <cord_flow/match/cord_match.h>
#include <cord_error.h>

#define MTU_SIZE 1500
#define ETHERNET_HEADER_SIZE 14
#define DOT1Q_TAG_SIZE 4

#define BUFFER_SIZE (MTU_SIZE + ETHERNET_HEADER_SIZE)

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
    CORD_BUFFER(buffer, BUFFER_SIZE);
    size_t rx_bytes = 0;
    size_t tx_bytes = 0;

    CORD_LOG("[CordApp] Launching the PacketCord TPACKET_V3 Patch App!\n");

    signal(SIGINT, cord_app_sigint_callback);

    cord_app_context.l2_eth_a = CORD_CREATE_L2_TPACKETV3_FLOW_POINT('A', ETH_IFACE_A_NAME, TPACKET_V3_BLOCK_SIZE, TPACKET_V3_FRAME_SIZE, TPACKET_V3_BLOCK_NUM);
    cord_app_context.l2_eth_b = CORD_CREATE_L2_TPACKETV3_FLOW_POINT('B', ETH_IFACE_B_NAME, TPACKET_V3_BLOCK_SIZE, TPACKET_V3_FRAME_SIZE, TPACKET_V3_BLOCK_NUM);

    cord_app_context.evh = CORD_CREATE_LINUX_API_EVENT_HANDLER('E', -1);

    cord_retval = CORD_EVENT_HANDLER_REGISTER_FLOW_POINT(cord_app_context.evh, cord_app_context.l2_eth_a);
    cord_retval = CORD_EVENT_HANDLER_REGISTER_FLOW_POINT(cord_app_context.evh, cord_app_context.l2_eth_b);

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
            if (cord_app_context.evh->events[n].data.fd == cord_app_context.l2_eth_a->io_handle)
            {

            }

            if (cord_app_context.evh->events[n].data.fd == cord_app_context.l2_eth_b->io_handle)
            {

            }
        }
    }

    cord_app_cleanup();

    return CORD_OK;
}
