#include <cord_flow/flow_point/cord_l2_raw_socket_flow_point.h>
#include <cord_flow/flow_point/cord_l4_udp_flow_point.h>
#include <cord_flow/flow_point/cord_l3_stack_inject_flow_point.h>

#define MTU_SIZE 1420

static struct {
    CordL2RawSocketFlowPoint  *l2_eth;
    CordL3StackInjectFlowPoint *l3_si;
    CordL4UdpFlowPoint        *l4_udp;
} g_app_ctx;

void sigint_callback(int sig)
{
    CORD_LOG("\nCaught signal %d (Ctrl+C). Closing sockets and exiting...\n", sig);
    
    CordL2RawSocketFlowPoint_dtor(g_app_ctx.l2_eth);
    CordL3StackInjectFlowPoint_dtor(g_app_ctx.l3_si);
    CordL4UdpFlowPoint_dtor(g_app_ctx.l4_udp);

    _Exit(0);  // async‐safe exit
}

int main(void)
{
    CordL2RawSocketFlowPoint *l2_eth = NEW(CordL2RawSocketFlowPoint, 1, MTU_SIZE, "eth0");
    CordL3StackInjectFlowPoint *l3_si = NEW(CordL3StackInjectFlowPoint, 2, MTU_SIZE);
    CordL4UdpFlowPoint *l4_udp = NEW(CordL4UdpFlowPoint, 3, MTU_SIZE, inet_addr("0.0.0.0"), inet_addr("0.0.0.0"), 50000, 60000);

    g_app_ctx.l2_eth = l2_eth;
    g_app_ctx.l3_si = l3_si;
    g_app_ctx.l4_udp = l4_udp;

    close(l2_eth->fd); // Put it inside the dtor()
    close(l3_si->fd);  // Put it inside the dtor()
    close(l4_udp->fd); // Put it inside the dtor()

    CORD_LOG("Hello from the PacketCord App!\n");

    return CORD_OK;
}
