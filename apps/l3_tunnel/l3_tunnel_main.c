#include <cord_flow/event_handler/cord_linux_api_event_handler.h>
#include <cord_flow/flow_point/cord_l2_raw_socket_flow_point.h>
#include <cord_flow/flow_point/cord_l3_stack_inject_flow_point.h>
#include <cord_flow/flow_point/cord_l4_udp_flow_point.h>
#include <cord_flow/memory/cord_memory.h>
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
{ }

static void cord_app_cleanup(void)
{
    CORD_LOG("[CordApp] Destroying all objects!\n");
    CORD_DESTROY_FLOW_POINT(cord_app_context.l2_eth);
    CORD_DESTROY_FLOW_POINT(cord_app_context.l3_si);
    CORD_DESTROY_FLOW_POINT(cord_app_context.l4_udp);
    CORD_DESTROY_EVENT_HANDLER(cord_app_context.evh);
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

    struct iphdr *ip = NULL;
    struct udphdr *udp = NULL;

    CORD_LOG("[CordApp] Launching the PacketCord Tunnel App!\n");

    signal(SIGINT, cord_app_sigint_callback);

    cord_app_context.l2_eth = CORD_CREATE_L2_RAW_SOCKET_FLOW_POINT('A', "enp6s0");
    cord_app_context.l3_si  = CORD_CREATE_L3_STACK_INJECT_FLOW_POINT('I');
    cord_app_context.l4_udp = CORD_CREATE_L4_UDP_FLOW_POINT('B', inet_addr("192.168.100.6"), inet_addr("38.242.203.214"), 60000, 50000);

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
                cord_retval = CORD_FLOW_POINT_RX(cord_app_context.l2_eth, buffer, BUFFER_SIZE, &rx_bytes);
                if (cord_retval != CORD_OK)
                    continue; // Raw socket receive error

                if (rx_bytes < sizeof(struct ethhdr))
                    continue; // Packet too short to contain Ethernet header

                struct ethhdr *eth = (struct ethhdr *)buffer;
                if (ntohs(eth->h_proto) != ETH_P_IP)
                    continue; // Only handle IPv4 packets

                if (rx_bytes < sizeof(struct ethhdr) + sizeof(struct iphdr))
                    continue; // Too short for IP header

                ip = (struct iphdr *)(buffer + sizeof(struct ethhdr));
                int iphdr_len = ip->ihl << 2;

                if (rx_bytes < sizeof(struct ethhdr) + iphdr_len)
                    continue; // IP header incomplete

                if (CORD_L2_RAW_SOCKET_FLOW_POINT_ENSURE_INBOUD(cord_app_context.l2_eth) != CORD_OK)
                    continue; // Ensure this is not an outgoing packet

                if (rx_bytes < sizeof(struct ethhdr) + iphdr_len + sizeof(struct udphdr))
                    continue; // Too short for UDP header

                udp = (struct udphdr *)((char *)ip + iphdr_len);

                struct in_addr src_ip = { .s_addr = ip->saddr };
                struct in_addr dst_ip = { .s_addr = ip->daddr };

                if ((dst_ip.s_addr & netmask.s_addr) == prefix_ip.s_addr)
                {
                    uint16_t total_len = ntohs(ip->tot_len);

                    cord_retval = CORD_FLOW_POINT_TX(cord_app_context.l4_udp, ip, total_len, &tx_bytes);
                    if (cord_retval != CORD_OK)
                    {
                        // Handle the error
                    }
                }
            }

            if (cord_app_context.evh->events[n].data.fd == cord_app_context.l4_udp->io_handle)
            {
                cord_retval = CORD_FLOW_POINT_RX(cord_app_context.l4_udp, buffer, BUFFER_SIZE, &rx_bytes);
                if (cord_retval != CORD_OK)
                    continue; // Raw socket receive error

                struct iphdr *ip_inner = (struct iphdr *)buffer;

                if (rx_bytes != ntohs(ip_inner->tot_len))
                    continue; // Packet partially received

                if (ip_inner->version != 4)
                    continue;

                int ip_inner_hdrlen = ip_inner->ihl << 2;

                CORD_L3_STACK_INJECT_FLOW_POINT_SET_TARGET_IPV4(cord_app_context.l3_si, ip_inner->daddr);
                
                cord_retval = CORD_FLOW_POINT_TX(cord_app_context.l3_si, buffer, ntohs(ip_inner->tot_len), &tx_bytes);
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
