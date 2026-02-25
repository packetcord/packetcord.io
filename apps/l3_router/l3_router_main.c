#include <cord_flow/event_handler/cord_linux_api_event_handler.h>
#include <cord_flow/flow_point/cord_l2_raw_socket_flow_point.h>
#include <cord_flow/memory/cord_memory.h>
#include <cord_flow/match/cord_match.h>
#include <cord_flow/action/cord_action.h>
#include <cord_flow/table/cord_lpm.h>
#include <cord_error.h>
#include <arpa/inet.h>

#define MTU_SIZE 1500
#define ETHERNET_HEADER_SIZE 14
#define DOT1Q_TAG_SIZE 4

#define BUFFER_SIZE (MTU_SIZE + ETHERNET_HEADER_SIZE)

#define ETH_IFACE_0 "veth5"
#define ETH_IFACE_1 "veth6"
#define ETH_IFACE_2 "veth7"
#define ETH_IFACE_3 "veth8"

#define LPM_MAX_ROUTES 100
#define ARP_TABLE_SIZE 4

// Static ARP entry structure
typedef struct
{
    uint32_t next_hop_ip;       // Next hop IP (host byte order)
    cord_mac_addr_t next_hop_mac;
    uint8_t egress_port;
} static_arp_entry_t;

static struct
{
    CordFlowPoint *port_0;
    CordFlowPoint *port_1;
    CordFlowPoint *port_2;
    CordFlowPoint *port_3;
    CordEventHandler *evh;

    // Routing infrastructure
    cord_ipv4_lpm_t *lpm;
    static_arp_entry_t arp_table[ARP_TABLE_SIZE];

    // Router's own MAC addresses per interface
    cord_mac_addr_t router_macs[4];
} cord_app_context;

static CordFlowPoint* get_port_by_id(uint8_t port_id)
{
    switch (port_id)
    {
        case 0: return cord_app_context.port_0;
        case 1: return cord_app_context.port_1;
        case 2: return cord_app_context.port_2;
        case 3: return cord_app_context.port_3;
        default: return NULL;
    }
}

static void cord_populate_routing_table(void)
{
    CORD_LOG("[CordApp] Populating routing table...\n");

    // Routing table entries: Network/Prefix → Next Hop ID
    struct {
        const char *cidr;
        uint32_t next_hop_id;
    } routes[] = {
        {"192.168.5.0/24", 1},
        {"192.168.6.0/24", 2},
        {"192.168.7.0/24", 3},
        {"192.168.8.0/24", 4},
    };

    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++)
    {
        uint32_t ip;
        uint8_t depth;

        int ret = cord_ipv4_parse_cidr(routes[i].cidr, &ip, &depth);
        if (ret != 0)
        {
            CORD_LOG("[CordApp] ERROR: Failed to parse CIDR %s\n", routes[i].cidr);
            continue;
        }

        ret = cord_ipv4_lpm_add(cord_app_context.lpm, ip, depth, routes[i].next_hop_id);
        if (ret == 0)
        {
            CORD_LOG("[CordApp] Added route: %s -> next_hop_id %u\n",
                     routes[i].cidr, routes[i].next_hop_id);
        }
        else
        {
            CORD_LOG("[CordApp] ERROR: Failed to add route %s\n", routes[i].cidr);
        }
    }

    CORD_LOG("[CordApp] Routing table population complete.\n");
}

static void cord_populate_arp_table(void)
{
    CORD_LOG("[CordApp] Populating static ARP table...\n");

    // Static ARP entries: next_hop_id → (IP, MAC, port)
    struct {
        uint8_t next_hop_id;
        const char *ip_str;
        const char *mac_str;
        uint8_t egress_port;
    } arp_entries[] = {
        {1, "192.168.5.100", "4e:12:e7:42:83:01", 0},
        {2, "192.168.6.100", "4e:12:e7:42:83:03", 1},
        {3, "192.168.7.100", "4e:12:e7:42:83:04", 2},
        {4, "192.168.8.100", "4e:12:e7:42:83:02", 3},
    };

    for (size_t i = 0; i < sizeof(arp_entries) / sizeof(arp_entries[0]); i++)
    {
        uint8_t idx = arp_entries[i].next_hop_id - 1;

        // Parse IP address
        uint32_t ip;
        int ret = cord_ipv4_str_to_binary(arp_entries[i].ip_str, &ip);
        if (ret != 0)
        {
            CORD_LOG("[CordApp] ERROR: Failed to parse IP %s\n", arp_entries[i].ip_str);
            continue;
        }
        cord_app_context.arp_table[idx].next_hop_ip = ip;

        // Parse MAC address
        ret = cord_mac_str_to_binary(arp_entries[i].mac_str,
                                      &cord_app_context.arp_table[idx].next_hop_mac);
        if (ret != 0)
        {
            CORD_LOG("[CordApp] ERROR: Failed to parse MAC %s\n", arp_entries[i].mac_str);
            continue;
        }

        // Set egress port
        cord_app_context.arp_table[idx].egress_port = arp_entries[i].egress_port;

        CORD_LOG("[CordApp] ARP entry %u: %s -> %s (port %u)\n",
                 arp_entries[i].next_hop_id,
                 arp_entries[i].ip_str,
                 arp_entries[i].mac_str,
                 arp_entries[i].egress_port);
    }

    CORD_LOG("[CordApp] ARP table population complete.\n");
}

static void cord_set_router_macs(void)
{
    CORD_LOG("[CordApp] Setting router interface MACs...\n");

    // Router's own MAC addresses per interface
    const char *router_mac_strs[] = {
        "4e:12:e7:42:83:05",  // port_0
        "4e:12:e7:42:83:06",  // port_1
        "4e:12:e7:42:83:07",  // port_2
        "4e:12:e7:42:83:08",  // port_3
    };

    for (size_t i = 0; i < 4; i++)
    {
        int ret = cord_mac_str_to_binary(router_mac_strs[i], &cord_app_context.router_macs[i]);
        if (ret == 0)
        {
            CORD_LOG("[CordApp] Interface %zu MAC: %s\n", i, router_mac_strs[i]);
        }
        else
        {
            CORD_LOG("[CordApp] ERROR: Failed to parse router MAC %s\n", router_mac_strs[i]);
        }
    }

    CORD_LOG("[CordApp] Router MACs configured.\n");
}

static void cord_app_setup(void)
{
    CORD_LOG("[CordApp] Setting up L3 IPv4 Router...\n");

    // Create LPM table
    cord_app_context.lpm = cord_ipv4_lpm_create(LPM_MAX_ROUTES);
    if (!cord_app_context.lpm)
    {
        CORD_LOG("[CordApp] ERROR: Failed to create LPM table!\n");
        CORD_EXIT(CORD_ERR);
    }

    // Populate routing table
    cord_populate_routing_table();

    // Populate static ARP table
    cord_populate_arp_table();

    // Set router interface MACs
    cord_set_router_macs();

    CORD_LOG("[CordApp] L3 Router setup complete.\n");
}

static void cord_app_cleanup(void)
{
    CORD_LOG("[CordApp] Destroying all objects!\n");

    if (cord_app_context.lpm)
    {
        CORD_LOG("\n");
        cord_ipv4_lpm_print_stats(cord_app_context.lpm);
        cord_ipv4_lpm_destroy(cord_app_context.lpm);
    }

    CORD_DESTROY_FLOW_POINT(cord_app_context.port_0);
    CORD_DESTROY_FLOW_POINT(cord_app_context.port_1);
    CORD_DESTROY_FLOW_POINT(cord_app_context.port_2);
    CORD_DESTROY_FLOW_POINT(cord_app_context.port_3);
    CORD_DESTROY_EVENT_HANDLER(cord_app_context.evh);
}

static void cord_app_sigint_callback(int sig)
{
    cord_app_cleanup();
    CORD_LOG("[CordApp] Terminating the PacketCord L3 Router App!\n");
    CORD_ASYNC_SAFE_EXIT(CORD_OK);
}

static void cord_route_packet(uint8_t ingress_port, void *buffer, size_t rx_bytes)
{
    cord_retval_t cord_retval;
    size_t tx_bytes = 0;

    // Parse Ethernet header
    cord_eth_hdr_t *eth = cord_header_eth(buffer);
    if (!eth)
    {
        return; // Invalid packet
    }

    // Check if IPv4 packet (EtherType = 0x0800)
    uint16_t eth_type = cord_get_field_eth_type_ntohs(eth);
    if (eth_type != 0x0800)
    {
        return; // Not IPv4, drop silently
    }

    // Parse IPv4 header
    cord_ipv4_hdr_t *ip = cord_header_ipv4_from_eth(eth);
    if (!ip)
    {
        return; // Invalid IPv4 header
    }

    // LPM lookup to get next_hop_id
    // Note: cord_get_field_ipv4_dst_addr returns network byte order, ntohl converts to host byte order for LPM
    uint32_t next_hop_id = cord_ipv4_lpm_lookup(cord_app_context.lpm, ntohl(cord_get_field_ipv4_dst_addr(ip)));

    if (next_hop_id == CORD_IPV4_LPM_INVALID_NEXT_HOP)
    {
        return; // No route found, drop packet
    }

    // ARP lookup: next_hop_id → (MAC, egress_port)
    uint8_t arp_idx = next_hop_id - 1;
    if (arp_idx >= ARP_TABLE_SIZE)
    {
        return; // Invalid next_hop_id
    }

    uint8_t egress_port = cord_app_context.arp_table[arp_idx].egress_port;
    cord_mac_addr_t *next_hop_mac = &cord_app_context.arp_table[arp_idx].next_hop_mac;

    // Check TTL
    uint8_t ttl = cord_get_field_ipv4_ttl(ip);
    if (ttl <= 1)
    {
        return; // TTL expired, drop (should send ICMP Time Exceeded, but skip for now)
    }

    // Decrement TTL
    cord_set_field_ipv4_ttl(ip, ttl - 1);

    // Recalculate IPv4 checksum
    uint16_t new_checksum = cord_calculate_ipv4_checksum(ip);
    cord_set_field_ipv4_checksum(ip, cord_htons(new_checksum));

    // Rewrite Ethernet header
    // Update destination MAC (next hop's MAC)
    cord_set_field_eth_dst_addr(eth, next_hop_mac);

    // Update source MAC (router's egress interface MAC)
    cord_set_field_eth_src_addr(eth, &cord_app_context.router_macs[egress_port]);

    // Forward to egress port
    CordFlowPoint *out_port = get_port_by_id(egress_port);
    if (out_port && egress_port != ingress_port)
    {
        cord_retval = CORD_FLOW_POINT_TX(out_port, 0, buffer, rx_bytes, &tx_bytes);
        if (cord_retval != CORD_OK)
        {
            // Handle TX error
        }
    }
}

int main(void)
{
    cord_retval_t cord_retval;
    CORD_BUFFER(buffer, BUFFER_SIZE);
    size_t rx_bytes = 0;

    CORD_LOG("[CordApp] Launching the PacketCord L3 IPv4 Router App!\n");

    signal(SIGINT, cord_app_sigint_callback);

    // Create flow points for all 4 ports
    cord_app_context.port_0 = CORD_CREATE_L2_RAW_SOCKET_FLOW_POINT('0', ETH_IFACE_0);
    cord_app_context.port_1 = CORD_CREATE_L2_RAW_SOCKET_FLOW_POINT('1', ETH_IFACE_1);
    cord_app_context.port_2 = CORD_CREATE_L2_RAW_SOCKET_FLOW_POINT('2', ETH_IFACE_2);
    cord_app_context.port_3 = CORD_CREATE_L2_RAW_SOCKET_FLOW_POINT('3', ETH_IFACE_3);

    // Create event handler
    cord_app_context.evh = CORD_CREATE_LINUX_API_EVENT_HANDLER('E', -1);

    // Register all flow points with event handler
    cord_retval = CORD_EVENT_HANDLER_REGISTER_FLOW_POINT(cord_app_context.evh, cord_app_context.port_0);
    cord_retval = CORD_EVENT_HANDLER_REGISTER_FLOW_POINT(cord_app_context.evh, cord_app_context.port_1);
    cord_retval = CORD_EVENT_HANDLER_REGISTER_FLOW_POINT(cord_app_context.evh, cord_app_context.port_2);
    cord_retval = CORD_EVENT_HANDLER_REGISTER_FLOW_POINT(cord_app_context.evh, cord_app_context.port_3);

    // Setup routing table, ARP table, and router MACs
    cord_app_setup();

    // Main event loop
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
            //
            // Port 0 RX
            //
            if (cord_app_context.evh->events[n].data.fd == cord_app_context.port_0->io_handle)
            {
                cord_retval = CORD_FLOW_POINT_RX(cord_app_context.port_0, 0, buffer, BUFFER_SIZE, &rx_bytes);
                if (cord_retval != CORD_OK)
                    continue;

                if (rx_bytes < sizeof(cord_eth_hdr_t))
                    continue;

                if (CORD_L2_RAW_SOCKET_FLOW_POINT_ENSURE_INBOUD(cord_app_context.port_0) != CORD_OK)
                    continue;

                cord_route_packet(0, buffer, rx_bytes);
            }

            //
            // Port 1 RX
            //
            if (cord_app_context.evh->events[n].data.fd == cord_app_context.port_1->io_handle)
            {
                cord_retval = CORD_FLOW_POINT_RX(cord_app_context.port_1, 0, buffer, BUFFER_SIZE, &rx_bytes);
                if (cord_retval != CORD_OK)
                    continue;

                if (rx_bytes < sizeof(cord_eth_hdr_t))
                    continue;

                if (CORD_L2_RAW_SOCKET_FLOW_POINT_ENSURE_INBOUD(cord_app_context.port_1) != CORD_OK)
                    continue;

                cord_route_packet(1, buffer, rx_bytes);
            }

            //
            // Port 2 RX
            //
            if (cord_app_context.evh->events[n].data.fd == cord_app_context.port_2->io_handle)
            {
                cord_retval = CORD_FLOW_POINT_RX(cord_app_context.port_2, 0, buffer, BUFFER_SIZE, &rx_bytes);
                if (cord_retval != CORD_OK)
                    continue;

                if (rx_bytes < sizeof(cord_eth_hdr_t))
                    continue;

                if (CORD_L2_RAW_SOCKET_FLOW_POINT_ENSURE_INBOUD(cord_app_context.port_2) != CORD_OK)
                    continue;

                cord_route_packet(2, buffer, rx_bytes);
            }

            //
            // Port 3 RX
            //
            if (cord_app_context.evh->events[n].data.fd == cord_app_context.port_3->io_handle)
            {
                cord_retval = CORD_FLOW_POINT_RX(cord_app_context.port_3, 0, buffer, BUFFER_SIZE, &rx_bytes);
                if (cord_retval != CORD_OK)
                    continue;

                if (rx_bytes < sizeof(cord_eth_hdr_t))
                    continue;

                if (CORD_L2_RAW_SOCKET_FLOW_POINT_ENSURE_INBOUD(cord_app_context.port_3) != CORD_OK)
                    continue;

                cord_route_packet(3, buffer, rx_bytes);
            }
        }
    }

    cord_app_cleanup();

    return CORD_OK;
}
