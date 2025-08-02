#include <cord_flow/flow_point/cord_l2_raw_socket_flow_point.h>
#include <cord_flow/flow_point/cord_l4_udp_flow_point.h>
#include <cord_flow/flow_point/cord_l3_stack_inject_flow_point.h>

#define MTU_SIZE 1420

int main(void)
{
    CordL2RawSocketFlowPoint l2_eth = NEW_ON_STACK(CordL2RawSocketFlowPoint, 1, MTU_SIZE, "eth0");
    CordL3StackInjectFlowPoint l3_si = NEW_ON_STACK(CordL3StackInjectFlowPoint, 2, MTU_SIZE);
    CordL4UdpFlowPoint l4_udp = NEW_ON_STACK(CordL4UdpFlowPoint, 3, MTU_SIZE, inet_addr("0.0.0.0"), inet_addr("0.0.0.0"), 50000, 60000);

    close(l2_eth.fd); // Put it inside the dtor()
    close(l3_si.fd);  // Put it inside the dtor()
    close(l4_udp.fd); // Put it inside the dtor()

    CORD_LOG("Hello from the PacketCord App!\n");

    return CORD_OK;
}
