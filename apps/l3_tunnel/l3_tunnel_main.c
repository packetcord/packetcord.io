#include <cord_flow/event_handler/cord_linux_api_event_handler.h>
#include <cord_flow/flow_point/cord_l2_raw_socket_flow_point.h>
#include <cord_flow/flow_point/cord_l3_stack_inject_flow_point.h>
#include <cord_flow/flow_point/cord_l4_udp_flow_point.h>
#include <cord_flow/memory/cord_memory.h>
#include <cord_flow/match/cord_match.h>
#include <cord_flow/action/cord_action.h>
#include <cord_error.h>

#define MTU_SIZE 1420
#define ETHERNET_HEADER_SIZE 14
#define DOT1Q_TAG_SIZE 4

#define BUFFER_SIZE (MTU_SIZE + ETHERNET_HEADER_SIZE)

#define MATCH_IP_TO_TUNNEL  "11.11.11.100"
#define MATCH_NETMASK       "255.255.255.255"

static struct
{
    CordFlowPoint *l2_eth;
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
    CORD_DESTROY_FLOW_POINT(cord_app_context.l2_eth);
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
    struct in_addr prefix_ip, netmask;
    inet_pton(AF_INET, MATCH_IP_TO_TUNNEL, &prefix_ip);
    inet_pton(AF_INET, MATCH_NETMASK, &netmask);

    cord_retval_t cord_retval;
    CORD_BUFFER(buffer, BUFFER_SIZE);
    size_t rx_bytes = 0;
    size_t tx_bytes = 0;

    cord_ipv4_hdr_t *ip = NULL;
    cord_udp_hdr_t *udp = NULL;

    CORD_LOG("[CordApp] Launching the PacketCord Tunnel App!\n");

    signal(SIGINT, cord_app_sigint_callback);

    cord_app_context.l2_eth = CORD_CREATE_L2_RAW_SOCKET_FLOW_POINT('A', "enp6s0");
    cord_app_context.l3_si  = CORD_CREATE_L3_STACK_INJECT_FLOW_POINT('I');
    cord_app_context.l4_udp = CORD_CREATE_L4_UDP_FLOW_POINT('B', inet_addr("192.168.100.5"), inet_addr("37.60.250.192"), 60000, 50000);

    cord_app_context.evh = CORD_CREATE_LINUX_API_EVENT_HANDLER('E', -1);

    cord_retval = CORD_EVENT_HANDLER_REGISTER_FLOW_POINT(cord_app_context.evh, cord_app_context.l2_eth);
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
            if (cord_app_context.evh->events[n].data.fd == cord_app_context.l2_eth->io_handle)
            {
                cord_retval = CORD_FLOW_POINT_RX(cord_app_context.l2_eth, 0, buffer, BUFFER_SIZE, &rx_bytes);
                if (cord_retval != CORD_OK)
                    continue; // Raw socket receive error

                if (rx_bytes < sizeof(cord_eth_hdr_t))
                    continue; // Packet too short to contain Ethernet header

                cord_eth_hdr_t *eth = cord_header_eth(buffer);
                if (cord_get_field_eth_type(eth) != CORD_ETH_P_IP)
                    continue; // Only handle IPv4 packets

                if (rx_bytes < sizeof(cord_eth_hdr_t) + sizeof(cord_ipv4_hdr_t))
                    continue; // Too short for IP header

                ip = cord_header_ipv4_from_eth(eth);
                if (cord_get_field_ipv4_version(ip) != 4)
                    continue; // Not IPv4

                int iphdr_len = cord_get_field_ipv4_header_length(ip);

                if (rx_bytes < sizeof(cord_eth_hdr_t) + iphdr_len)
                    continue; // IP header incomplete

                if (CORD_L2_RAW_SOCKET_FLOW_POINT_ENSURE_INBOUD(cord_app_context.l2_eth) != CORD_OK)
                    continue; // Ensure this is not an outgoing packet

                if (rx_bytes < sizeof(cord_eth_hdr_t) + iphdr_len + sizeof(cord_udp_hdr_t))
                    continue; // Too short for UDP header

                udp = cord_header_udp_ipv4(ip);

                uint32_t src_ip = cord_get_field_ipv4_src_addr_ntohl(ip);
                uint32_t dst_ip = cord_get_field_ipv4_dst_addr_ntohl(ip);

                if (cord_compare_ipv4_dst_subnet(ip, cord_ntohl(prefix_ip.s_addr), cord_ntohl(netmask.s_addr)))
                {
                    uint16_t total_len = cord_get_field_ipv4_total_length_ntohs(ip);

                    cord_retval = CORD_FLOW_POINT_TX(cord_app_context.l4_udp, 0, ip, total_len, &tx_bytes);
                    if (cord_retval != CORD_OK)
                    {
                        // Handle the error
                    }
                }
            }

            if (cord_app_context.evh->events[n].data.fd == cord_app_context.l4_udp->io_handle)
            {
                cord_retval = CORD_FLOW_POINT_RX(cord_app_context.l4_udp, 0, buffer, BUFFER_SIZE, &rx_bytes);
                if (cord_retval != CORD_OK)
                    continue; // Raw socket receive error

                cord_ipv4_hdr_t *ip_inner = cord_header_ipv4(buffer);

                if (rx_bytes != cord_get_field_ipv4_total_length_ntohs(ip_inner))
                    continue; // Packet partially received

                if (cord_get_field_ipv4_version(ip_inner) != 4)
                    continue;

                int ip_inner_hdrlen = cord_get_field_ipv4_header_length(ip_inner);

                CORD_L3_STACK_INJECT_FLOW_POINT_SET_TARGET_IPV4(cord_app_context.l3_si, cord_get_field_ipv4_dst_addr(ip_inner));

                cord_retval = CORD_FLOW_POINT_TX(cord_app_context.l3_si, 0, buffer, cord_get_field_ipv4_total_length_ntohs(ip_inner), &tx_bytes);
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
