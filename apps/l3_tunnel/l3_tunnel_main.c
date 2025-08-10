#include <cord_flow/event_handler/cord_linux_api_event_handler.h>
#include <cord_flow/flow_point/cord_l2_raw_socket_flow_point.h>
#include <cord_flow/flow_point/cord_l3_stack_inject_flow_point.h>
#include <cord_flow/flow_point/cord_l4_udp_flow_point.h>
#include <cord_error.h>

#define MTU_SIZE 1420

static struct
{
    CordL2RawSocketFlowPoint    *l2_eth;
    CordL3StackInjectFlowPoint  *l3_si;
    CordL4UdpFlowPoint          *l4_udp;
    CordLinuxApiEventHandler    *linux_evh;
} g_app_ctx;

void cord_destroy(void)
{
    CORD_LOG("Destroying all objects!\n");
    CORD_DESTROY_L2_RAW_SOCKET_FLOW_POINT(g_app_ctx.l2_eth);
    CordL3StackInjectFlowPoint_dtor(g_app_ctx.l3_si);
    CordL4UdpFlowPoint_dtor(g_app_ctx.l4_udp);
}

void sigint_callback(int sig)
{
    cord_destroy();
    CORD_LOG("Terminating the PacketCord Tunnel App!\n");
    CORD_ASYNC_SAFE_EXIT(CORD_OK);
}

int main(void)
{
    CORD_LOG("Launching the PacketCord Tunnel App!\n");

    signal(SIGINT, sigint_callback);

    CordFlowPoint *l2_eth = CORD_CREATE_L2_RAW_SOCKET_FLOW_POINT('A', 1500, "enp6s0");
    CordFlowPoint *l3_si  = (CordFlowPoint *) NEW_ON_HEAP(CordL3StackInjectFlowPoint,   'B', MTU_SIZE);
    CordFlowPoint *l4_udp = (CordFlowPoint *) NEW_ON_HEAP(CordL4UdpFlowPoint,           'C', MTU_SIZE, inet_addr("0.0.0.0"), inet_addr("0.0.0.0"), 50000, 60000);
    CordEventHandler *linux_evh = (CordEventHandler *) NEW_ON_HEAP(CordLinuxApiEventHandler, 'E', -1);

    // To be re-written in a much better manner
    g_app_ctx.l2_eth    = (CordL2RawSocketFlowPoint *)l2_eth;
    g_app_ctx.l3_si     = (CordL3StackInjectFlowPoint *)l3_si;
    g_app_ctx.l4_udp    = (CordL4UdpFlowPoint *)l4_udp;
    g_app_ctx.linux_evh = (CordLinuxApiEventHandler *)linux_evh;

    CORDEVENTHANDLER_REGISTER_FLOW_POINT(linux_evh, l2_eth);
    CORDEVENTHANDLER_REGISTER_FLOW_POINT(linux_evh, l4_udp);

    while (1)
    {
        int nb_fds = CORDEVENTHANDLER_WAIT(linux_evh);

        //
        // Logic TBD
        //
    }

    return CORD_OK;
}
