#include <cord_craft/injector/cord_l3_stack_injector.h>
#include <cord_craft/protocols/cord_protocols.h>
#include <cord_craft/cord_retval.h>

#define MTU_SIZE            1420

#define OUTER_SOURCE_IP     "192.168.100.3"
#define OUTER_DEST_IP       "38.242.203.214"
#define OUTER_SOURCE_PORT   60000
#define OUTER_DEST_PORT     50000

#define INNER_SOURCE_IP     "192.168.100.3"
#define INNER_DEST_IP       "11.11.11.100"
#define INNER_SOURCE_PORT   1234
#define INNER_DEST_PORT     8765

#define PAYLOAD             "PacketCord.io Tunneled Hello!!!\n"
#define PAYLOAD_LEN         32

static struct
{
    CordInjector *l3_ci;
} cord_app_context;

static void cord_app_setup(void)
{
    CORD_LOG("[CordApp] No additional setup needed.\n");
}

static void cord_app_cleanup(void)
{
    CORD_LOG("[CordApp] Destroying all objects!\n");
    CORD_DESTROY_INJECTOR(cord_app_context.l3_ci);
}

int main()
{
    cord_retval_t cord_retval;
    uint8_t buffer[MTU_SIZE];
    size_t tx_bytes = 0;

    cord_app_context.l3_ci = CORD_CREATE_L3_STACK_INJECTOR('I');

    for (uint16_t n = 0; n < MTU_SIZE; n++) buffer[n] = 0x00;

    // Calculate payload and packet sizes
    const int inner_payload_len = strlen(PAYLOAD);
    const int inner_total_len = sizeof(cord_ipv4_hdr_t) + sizeof(cord_udp_hdr_t) + inner_payload_len;
    const int outer_payload_len = inner_total_len;
    const int outer_total_len = sizeof(cord_ipv4_hdr_t) + sizeof(cord_udp_hdr_t) + outer_payload_len;

    // Layer offsets using cord-craft structures
    cord_ipv4_hdr_t *outer_ip = (cord_ipv4_hdr_t *) buffer;
    cord_udp_hdr_t *outer_udp = (cord_udp_hdr_t *) (buffer + sizeof(cord_ipv4_hdr_t));
    uint8_t *encapsulated = buffer + sizeof(cord_ipv4_hdr_t) + sizeof(cord_udp_hdr_t);

    // Inner IP/UDP setup
    cord_ipv4_hdr_t *inner_ip = (cord_ipv4_hdr_t *) encapsulated;
    cord_udp_hdr_t *inner_udp = (cord_udp_hdr_t *) (encapsulated + sizeof(cord_ipv4_hdr_t));
    char *inner_payload = (char *) (encapsulated + sizeof(cord_ipv4_hdr_t) + sizeof(cord_udp_hdr_t));

    // Copy payload
    for (uint8_t n = 0; n < PAYLOAD_LEN; n++) inner_payload[n] = PAYLOAD[n];

    // Build inner IP header using cord-craft structure
    inner_ip->version = 4;
    inner_ip->ihl = 5; // Base header
    inner_ip->tos = 0;
    inner_ip->tot_len = cord_htons(inner_total_len);
    inner_ip->id = cord_htons(123);
    inner_ip->frag_off = 0;
    inner_ip->ttl = 5;
    inner_ip->protocol = CORD_IPPROTO_UDP;
    inner_ip->saddr.addr = inet_addr(INNER_SOURCE_IP);
    inner_ip->daddr.addr = inet_addr(INNER_DEST_IP);
    inner_ip->check = 0;
    inner_ip->check = cord_htons(cord_ipv4_checksum(inner_ip));

    // Build inner UDP header using cord-craft structure
    inner_udp->source = cord_htons(INNER_SOURCE_PORT);
    inner_udp->dest = cord_htons(INNER_DEST_PORT);
    inner_udp->len = cord_htons(sizeof(cord_udp_hdr_t) + inner_payload_len);
    inner_udp->check = 0;
    inner_udp->check = cord_htons(cord_udp_checksum_ipv4(inner_ip));

    // Build outer IP header using cord-craft structure
    outer_ip->version = 4;
    outer_ip->ihl = 5; // Base header as well
    outer_ip->tos = 0;
    outer_ip->tot_len = cord_htons(outer_total_len);
    outer_ip->id = cord_htons(321);
    outer_ip->frag_off = 0;
    outer_ip->ttl = 10;
    outer_ip->protocol = CORD_IPPROTO_UDP;
    outer_ip->saddr.addr = inet_addr(OUTER_SOURCE_IP);
    outer_ip->daddr.addr = inet_addr(OUTER_DEST_IP);
    outer_ip->check = 0;
    outer_ip->check = cord_htons(cord_ipv4_checksum(outer_ip));

    // Build outer UDP header using cord-craft structure
    outer_udp->source = cord_htons(OUTER_SOURCE_PORT);
    outer_udp->dest = cord_htons(OUTER_DEST_PORT);
    outer_udp->len = cord_htons(sizeof(cord_udp_hdr_t) + outer_payload_len);
    outer_udp->check = 0;
    outer_udp->check = cord_htons(cord_udp_checksum_ipv4(outer_ip));

    cord_ipv4_hdr_t *outer_ip_hdr = cord_get_ipv4_hdr_l3(buffer);
    CORD_L3_STACK_INJECTOR_SET_TARGET_IPV4(cord_app_context.l3_ci, cord_get_ipv4_dst_addr_l3(outer_ip_hdr));

    CORD_LOG("[CordApp] Transmitting the crafted pseudo-tunnel packet...\n");
    cord_retval = CORD_INJECTOR_TX(cord_app_context.l3_ci, buffer, cord_get_ipv4_total_length_ntohs(outer_ip_hdr), &tx_bytes);
    if (cord_retval != CORD_OK)
    {
        // Handle the error
    }

    cord_app_cleanup();

    return CORD_OK;
}
