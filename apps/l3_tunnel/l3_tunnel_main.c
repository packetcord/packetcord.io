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
    CordL2RawSocketFlowPoint_dtor(g_app_ctx.l2_eth);
    CordL3StackInjectFlowPoint_dtor(g_app_ctx.l3_si);
    CordL4UdpFlowPoint_dtor(g_app_ctx.l4_udp);
    CordLinuxApiEventHandler_dtor(g_app_ctx.linux_evh);
}

void sigint_callback(int sig)
{
    CORD_LOG("\nCaught signal %d (Ctrl+C). Closing sockets and exiting...\n", sig);
    
    CordL2RawSocketFlowPoint_dtor(g_app_ctx.l2_eth);
    CordL3StackInjectFlowPoint_dtor(g_app_ctx.l3_si);
    CordL4UdpFlowPoint_dtor(g_app_ctx.l4_udp);

    CORD_ASYNC_SAFE_EXIT(CORD_OK);
}

int main(void)
{
    CORD_LOG("Launching the PacketCord Tunnel App!\n");

    signal(SIGINT, sigint_callback);

    CordFlowPoint *l2_eth = (CordFlowPoint *) NEW(CordL2RawSocketFlowPoint,     'A', MTU_SIZE, "eth0");
    CordFlowPoint *l3_si  = (CordFlowPoint *) NEW(CordL3StackInjectFlowPoint,   'B', MTU_SIZE);
    CordFlowPoint *l4_udp = (CordFlowPoint *) NEW(CordL4UdpFlowPoint,           'C', MTU_SIZE, inet_addr("0.0.0.0"), inet_addr("0.0.0.0"), 50000, 60000);
    CordEventHandler *linux_evh = (CordEventHandler *) NEW(CordLinuxApiEventHandler, 'E', -1);

    g_app_ctx.l2_eth    = (CordL2RawSocketFlowPoint *)l2_eth;
    g_app_ctx.l3_si     = (CordL3StackInjectFlowPoint *)l3_si;
    g_app_ctx.l4_udp    = (CordL4UdpFlowPoint *)l4_udp;
    g_app_ctx.linux_evh = (CordLinuxApiEventHandler *)linux_evh;

    CORDEVENTHANDLER_REGISTER_FLOW_POINT(linux_evh, (void *)&((CordL2RawSocketFlowPoint *)l2_eth)->fd); // Rewrite via get_fd() method
    CORDEVENTHANDLER_REGISTER_FLOW_POINT(linux_evh, (void *)&((CordL4UdpFlowPoint *)l4_udp)->fd);       // Rewrite via getter as well

    //
    // Loop
    //
    int wait_ret = CORDEVENTHANDLER_WAIT(linux_evh);

    CORD_LOG("Destroying all objects!\n");
    cord_destroy();                                                                               // Rewrite via a destoy() method, eliminate the need for the global context pointers

    CORD_LOG("Terminating the PacketCord Tunnel App!\n");
    return CORD_OK;
}
