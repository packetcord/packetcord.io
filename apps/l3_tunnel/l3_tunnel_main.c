#include <stdio.h>
#include <arpa/inet.h>
#include <cord_flow/flow_point/cord_l2_raw_socket_flow_point.h>
#include <cord_flow/flow_point/cord_l4_udp_flow_point.h>
#include <cord_flow/flow_point/cord_l3_stack_inject_flow_point.h>

#define MTU 2048

/* Compile-time role: 1=server, 0=client */
#define SERVER_SIDE 1

#if SERVER_SIDE
  #define IFACE_NAME "veth0"
  #define PEER_IP     "10.0.0.2"
  #define PEER_PORT   50000
#else
  #define IFACE_NAME "veth1"
  #define PEER_IP     "10.0.0.1"
  #define PEER_PORT   50000
#endif

int main(void)
{
    uint8_t buffer[MTU];

    /* 1) Construct flow-points (raw FD creation/binding done inside ctor) */
    CordL2RawSocketFlowPoint   l2_fp;
    CordL4UdpFlowPoint         udp_fp;
    CordL3StackInjectFlowPoint inj_fp;

    /* Sample ctor signatures; adjust to match your headers */
    CordL2RawSocketFlowPoint_ctor(&l2_fp, 1, MTU, IFACE_NAME, /*other args*/);
    CordL4UdpFlowPoint_ctor   (&udp_fp, 2, MTU, "0.0.0.0", PEER_PORT,
                                          PEER_IP,     PEER_PORT, /*other args*/);
    CordL3StackInjectFlowPoint_ctor(&inj_fp, 3, MTU /*, fd */);

    /* 2) Main loop: L2 → UDP → Stack */
    for (;;) {
        if (CORDFLOWPOINT_RX_VCALL(&l2_fp.base, buffer) == CORD_OK)
            CORDFLOWPOINT_TX_VCALL(&udp_fp.base, buffer);

        if (CORDFLOWPOINT_RX_VCALL(&udp_fp.base, buffer) == CORD_OK)
            CORDFLOWPOINT_TX_VCALL(&inj_fp.base, buffer);
    }
    return 0;
}

