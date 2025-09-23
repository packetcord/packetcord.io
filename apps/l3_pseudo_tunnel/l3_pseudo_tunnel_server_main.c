#include <cord_flow/event_handler/cord_linux_api_event_handler.h>
#include <cord_flow/flow_point/cord_l2_raw_socket_flow_point.h>
#include <cord_flow/flow_point/cord_l3_stack_inject_flow_point.h>
#include <cord_flow/flow_point/cord_l4_udp_flow_point.h>
#include <cord_flow/memory/cord_memory.h>
#include <cord_flow/match/cord_match.h>
#include <cord_error.h>

#define MTU_SIZE 1420
#define ETHERNET_HEADER_SIZE 14
#define DOT1Q_TAG_SIZE 4

#define BUFFER_SIZE (MTU_SIZE + ETHERNET_HEADER_SIZE)

static struct
{
    CordFlowPoint *l3_si;
    CordFlowPoint *l4_udp;
    CordEventHandler *evh;
} cord_app_context;

static void cord_app_setup(void)
{
    CORD_LOG("[CordApp] Expecting manual additional setup - blackhole routes, interface MTU.\n");
}

static void cord_app_cleanup(void)
{
    CORD_LOG("[CordApp] Destroying all objects!\n");
    CORD_DESTROY_FLOW_POINT(cord_app_context.l3_si);
    CORD_DESTROY_FLOW_POINT(cord_app_context.l4_udp);
    CORD_DESTROY_EVENT_HANDLER(cord_app_context.evh);

    CORD_LOG("[CordApp] Expecting manual additional cleanup.\n");
}

static void cord_app_sigint_callback(int sig)
{
    cord_app_cleanup();
    CORD_LOG("[CordApp] Terminating the PacketCord Tunnel App!\n");
    CORD_ASYNC_SAFE_EXIT(CORD_OK);
}

int main(void)
{
    cord_retval_t cord_retval;
    CORD_BUFFER(buffer, BUFFER_SIZE);
    size_t rx_bytes = 0;
    size_t tx_bytes = 0;

    cord_ipv4_hdr_t *ip = NULL;
    cord_udp_hdr_t *udp = NULL;

    CORD_LOG("[CordApp] Launching the PacketCord Pseudo Tunnel App!\n");

    signal(SIGINT, cord_app_sigint_callback);

    cord_app_context.l3_si  = CORD_CREATE_L3_STACK_INJECT_FLOW_POINT('I');
    cord_app_context.l4_udp = CORD_CREATE_L4_UDP_FLOW_POINT('A', inet_addr("38.242.203.214"), inet_addr("78.83.207.86"), 50000, 60000);

    cord_app_context.evh = CORD_CREATE_LINUX_API_EVENT_HANDLER('E', -1);

    cord_retval = CORD_EVENT_HANDLER_REGISTER_FLOW_POINT(cord_app_context.evh, cord_app_context.l4_udp);

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
            if (cord_app_context.evh->events[n].data.fd == cord_app_context.l4_udp->io_handle)
            {
                cord_retval = CORD_FLOW_POINT_RX(cord_app_context.l4_udp, buffer, BUFFER_SIZE, &rx_bytes);
                if (cord_retval != CORD_OK)
                    continue; // Raw socket receive error

                cord_ipv4_hdr_t *ip_inner = cord_get_ipv4_hdr_l3(buffer);

                if (rx_bytes != cord_get_ipv4_total_length_ntohs(ip_inner))
                    continue; // Packet partially received

                if (!cord_match_ipv4_version(ip_inner))
                    continue;

                int ip_inner_hdrlen = cord_get_ipv4_header_length(ip_inner);

                CORD_L3_STACK_INJECT_FLOW_POINT_SET_TARGET_IPV4(cord_app_context.l3_si, cord_get_ipv4_dst_addr_l3(ip_inner));

                cord_retval = CORD_FLOW_POINT_TX(cord_app_context.l3_si, buffer, cord_get_ipv4_total_length_ntohs(ip_inner), &tx_bytes);
                if (cord_retval != CORD_OK)
                {
                    // Handle the error
                }
            }
        }
    }

    cord_app_cleanup();

    return CORD_OK;
}
