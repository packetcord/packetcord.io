#include <cord_flow/event_handler/cord_linux_api_event_handler.h>
#include <cord_flow/flow_point/cord_l2_raw_socket_flow_point.h>
#include <cord_flow/memory/cord_memory.h>
#include <cord_flow/match/cord_match.h>
#include <cord_flow/action/cord_action.h>
#include <cord_flow/table/cord_cam.h>
#include <cord_flow/table/cord_lpm.h>
#include <cord_error.h>

#define MTU_SIZE 1500
#define ETHERNET_HEADER_SIZE 14
#define DOT1Q_TAG_SIZE 4

#define BUFFER_SIZE (MTU_SIZE + ETHERNET_HEADER_SIZE)

#define ETH_PORT_0 "veth5"
#define ETH_PORT_1 "veth6"
#define ETH_PORT_2 "veth7"
#define ETH_PORT_3 "veth8"

#define CAM_NUM_BUCKETS 1024
#define CAM_MAX_ENTRIES 10000

static struct
{
    CordFlowPoint *port_0;
    CordFlowPoint *port_1;
    CordFlowPoint *port_2;
    CordFlowPoint *port_3;
    CordEventHandler *evh;
    cord_l2_cam_t *cam;
} cord_app_context;

static void cord_populate_static_cam(void)
{
    CORD_LOG("[CordApp] Populating static CAM entries...\n");

    // Static MAC-to-Port mappings (all on VLAN 0 / untagged)
    struct {
        const char *mac_str;
        uint32_t port_id;
        uint16_t vlan_id;
    } static_entries[] = {
        {"4e:12:e7:42:83:01", 0, 0},
        {"4e:12:e7:42:83:03", 1, 0},
        {"4e:12:e7:42:83:04", 2, 0},
        {"4e:12:e7:42:83:02", 3, 0},
    };

    for (size_t i = 0; i < sizeof(static_entries) / sizeof(static_entries[0]); i++)
    {
        cord_mac_addr_t mac;
        int ret = cord_mac_str_to_binary(static_entries[i].mac_str, &mac);
        if (ret != 0)
        {
            CORD_LOG("[CordApp] ERROR: Failed to parse MAC %s\n", static_entries[i].mac_str);
            continue;
        }

        ret = cord_l2_cam_add(cord_app_context.cam, &mac,
                              static_entries[i].port_id,
                              static_entries[i].vlan_id);
        if (ret == 0)
        {
            CORD_LOG("[CordApp] Added: %s VLAN %u -> Port %u\n",
                     static_entries[i].mac_str,
                     static_entries[i].vlan_id,
                     static_entries[i].port_id);
        }
        else
        {
            CORD_LOG("[CordApp] ERROR: Failed to add MAC %s to CAM\n", static_entries[i].mac_str);
        }
    }

    CORD_LOG("[CordApp] Static CAM population complete.\n");
}

static void cord_app_setup(void)
{
    CORD_LOG("[CordApp] Setting up L2 Switch...\n");

    // Create CAM table
    cord_app_context.cam = cord_l2_cam_create(CAM_NUM_BUCKETS, CAM_MAX_ENTRIES);
    if (!cord_app_context.cam)
    {
        CORD_LOG("[CordApp] ERROR: Failed to create CAM table!\n");
        CORD_EXIT(CORD_ERR);
    }

    // Populate static entries
    cord_populate_static_cam();

    CORD_LOG("[CordApp] L2 Switch setup complete.\n");
}

static void cord_app_cleanup(void)
{
    CORD_LOG("[CordApp] Destroying all objects!\n");

    if (cord_app_context.cam)
    {
        CORD_LOG("\n");
        cord_l2_cam_print_stats(cord_app_context.cam);
        cord_l2_cam_destroy(cord_app_context.cam);
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
    CORD_LOG("[CordApp] Terminating the PacketCord L2 Switch App!\n");
    CORD_ASYNC_SAFE_EXIT(CORD_OK);
}

static void cord_switch_packet(uint8_t ingress_port, void *buffer, size_t rx_bytes)
{
    cord_retval_t cord_retval;
    size_t tx_bytes = 0;

    // Parse Ethernet header
    cord_eth_hdr_t *eth = cord_header_eth(buffer);
    if (!eth)
    {
        return; // Invalid packet
    }

    // Extract destination MAC address
    cord_mac_addr_t dst_mac;
    cord_get_field_eth_dst_addr(eth, &dst_mac);

    // CAM lookup (VLAN 0 for untagged traffic)
    uint32_t egress_port = cord_l2_cam_lookup(cord_app_context.cam, &dst_mac, 0);

    if (egress_port != CORD_L2_CAM_INVALID_PORT)
    {
        // Unicast: forward to specific port based on CAM lookup
        CordFlowPoint *out_port = NULL;

        switch (egress_port)
        {
            case 0: out_port = cord_app_context.port_0; break;
            case 1: out_port = cord_app_context.port_1; break;
            case 2: out_port = cord_app_context.port_2; break;
            case 3: out_port = cord_app_context.port_3; break;
        }

        if (out_port && egress_port != ingress_port)
        {
            cord_retval = CORD_FLOW_POINT_TX(out_port, 0, buffer, rx_bytes, &tx_bytes);
            if (cord_retval != CORD_OK)
            {
                // Handle TX error
            }
        }
    }
    else
    {
        // Unknown destination MAC: flood to all ports except ingress
        if (ingress_port != 0)
        {
            cord_retval = CORD_FLOW_POINT_TX(cord_app_context.port_0, 0, buffer, rx_bytes, &tx_bytes);
            if (cord_retval != CORD_OK)
            {
                // Handle TX error
            }
        }

        if (ingress_port != 1)
        {
            cord_retval = CORD_FLOW_POINT_TX(cord_app_context.port_1, 0, buffer, rx_bytes, &tx_bytes);
            if (cord_retval != CORD_OK)
            {
                // Handle TX error
            }
        }

        if (ingress_port != 2)
        {
            cord_retval = CORD_FLOW_POINT_TX(cord_app_context.port_2, 0, buffer, rx_bytes, &tx_bytes);
            if (cord_retval != CORD_OK)
            {
                // Handle TX error
            }
        }

        if (ingress_port != 3)
        {
            cord_retval = CORD_FLOW_POINT_TX(cord_app_context.port_3, 0, buffer, rx_bytes, &tx_bytes);
            if (cord_retval != CORD_OK)
            {
                // Handle TX error
            }
        }
    }
}

int main(void)
{
    cord_retval_t cord_retval;
    CORD_BUFFER(buffer, BUFFER_SIZE);
    size_t rx_bytes = 0;

    CORD_LOG("[CordApp] Launching the PacketCord L2 Switch App!\n");

    signal(SIGINT, cord_app_sigint_callback);

    // Create flow points for all 4 ports
    cord_app_context.port_0 = CORD_CREATE_L2_RAW_SOCKET_FLOW_POINT('0', ETH_PORT_0);
    cord_app_context.port_1 = CORD_CREATE_L2_RAW_SOCKET_FLOW_POINT('1', ETH_PORT_1);
    cord_app_context.port_2 = CORD_CREATE_L2_RAW_SOCKET_FLOW_POINT('2', ETH_PORT_2);
    cord_app_context.port_3 = CORD_CREATE_L2_RAW_SOCKET_FLOW_POINT('3', ETH_PORT_3);

    // Create event handler
    cord_app_context.evh = CORD_CREATE_LINUX_API_EVENT_HANDLER('E', -1);

    // Register all flow points with event handler
    cord_retval = CORD_EVENT_HANDLER_REGISTER_FLOW_POINT(cord_app_context.evh, cord_app_context.port_0);
    cord_retval = CORD_EVENT_HANDLER_REGISTER_FLOW_POINT(cord_app_context.evh, cord_app_context.port_1);
    cord_retval = CORD_EVENT_HANDLER_REGISTER_FLOW_POINT(cord_app_context.evh, cord_app_context.port_2);
    cord_retval = CORD_EVENT_HANDLER_REGISTER_FLOW_POINT(cord_app_context.evh, cord_app_context.port_3);

    // Setup CAM table with static entries
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
                    continue; // Raw socket receive error

                if (rx_bytes < sizeof(cord_eth_hdr_t))
                    continue; // Packet too short to contain Ethernet header

                if (CORD_L2_RAW_SOCKET_FLOW_POINT_ENSURE_INBOUD(cord_app_context.port_0) != CORD_OK)
                    continue; // Ensure this is not an outgoing packet

                cord_switch_packet(0, buffer, rx_bytes);
            }

            //
            // Port 1 RX
            //
            if (cord_app_context.evh->events[n].data.fd == cord_app_context.port_1->io_handle)
            {
                cord_retval = CORD_FLOW_POINT_RX(cord_app_context.port_1, 0, buffer, BUFFER_SIZE, &rx_bytes);
                if (cord_retval != CORD_OK)
                    continue; // Raw socket receive error

                if (rx_bytes < sizeof(cord_eth_hdr_t))
                    continue; // Packet too short to contain Ethernet header

                if (CORD_L2_RAW_SOCKET_FLOW_POINT_ENSURE_INBOUD(cord_app_context.port_1) != CORD_OK)
                    continue; // Ensure this is not an outgoing packet

                cord_switch_packet(1, buffer, rx_bytes);
            }

            //
            // Port 2 RX
            //
            if (cord_app_context.evh->events[n].data.fd == cord_app_context.port_2->io_handle)
            {
                cord_retval = CORD_FLOW_POINT_RX(cord_app_context.port_2, 0, buffer, BUFFER_SIZE, &rx_bytes);
                if (cord_retval != CORD_OK)
                    continue; // Raw socket receive error

                if (rx_bytes < sizeof(cord_eth_hdr_t))
                    continue; // Packet too short to contain Ethernet header

                if (CORD_L2_RAW_SOCKET_FLOW_POINT_ENSURE_INBOUD(cord_app_context.port_2) != CORD_OK)
                    continue; // Ensure this is not an outgoing packet

                cord_switch_packet(2, buffer, rx_bytes);
            }

            //
            // Port 3 RX
            //
            if (cord_app_context.evh->events[n].data.fd == cord_app_context.port_3->io_handle)
            {
                cord_retval = CORD_FLOW_POINT_RX(cord_app_context.port_3, 0, buffer, BUFFER_SIZE, &rx_bytes);
                if (cord_retval != CORD_OK)
                    continue; // Raw socket receive error

                if (rx_bytes < sizeof(cord_eth_hdr_t))
                    continue; // Packet too short to contain Ethernet header

                if (CORD_L2_RAW_SOCKET_FLOW_POINT_ENSURE_INBOUD(cord_app_context.port_3) != CORD_OK)
                    continue; // Ensure this is not an outgoing packet

                cord_switch_packet(3, buffer, rx_bytes);
            }
        }
    }

    cord_app_cleanup();

    return CORD_OK;
}
