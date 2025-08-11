#include <cord_flow/event_handler/cord_linux_api_event_handler.h>
#include <cord_flow/flow_point/cord_l2_raw_socket_flow_point.h>
#include <cord_flow/flow_point/cord_l3_stack_inject_flow_point.h>
#include <cord_flow/flow_point/cord_l4_udp_flow_point.h>
#include <cord_error.h>

#define MTU_SIZE 1420

static struct
{
    CordFlowPoint *l2_eth;
    CordFlowPoint *l3_si;
    CordFlowPoint *l4_udp;
    CordEventHandler *linux_evh;
} cord_app_context;

void cord_app_cleanup(void)
{
    CORD_LOG("[CordApp] Destroying all objects!\n");
    CORD_DESTROY_FLOW_POINT(cord_app_context.l2_eth);
    CORD_DESTROY_FLOW_POINT(cord_app_context.l3_si);
    CORD_DESTROY_FLOW_POINT(cord_app_context.l4_udp);
    CORD_DESTROY_EVENT_HANDLER(cord_app_context.linux_evh);
}

void cord_app_sigint_callback(int sig)
{
    cord_app_cleanup();
    CORD_LOG("[CordApp] Terminating the PacketCord Tunnel App!\n");
    CORD_ASYNC_SAFE_EXIT(CORD_OK);
}

int main(void)
{
    CORD_LOG("[CordApp] Launching the PacketCord Tunnel App!\n");

    signal(SIGINT, cord_app_sigint_callback);

    cord_app_context.l2_eth = CORD_CREATE_L2_RAW_SOCKET_FLOW_POINT('A', "enp6s0");
    cord_app_context.l3_si  = CORD_CREATE_L3_STACK_INJECT_FLOW_POINT('B');
    cord_app_context.l4_udp = CORD_CREATE_L4_UDP_FLOW_POINT('C', inet_addr("0.0.0.0"), inet_addr("0.0.0.0"), 50000, 60000);
    cord_app_context.linux_evh = CORD_CREATE_LINUX_API_EVENT_HANDLER('E', -1);

    CORD_EVENT_HANDLER_REGISTER_FLOW_POINT(cord_app_context.linux_evh, cord_app_context.l2_eth);
    CORD_EVENT_HANDLER_REGISTER_FLOW_POINT(cord_app_context.linux_evh, cord_app_context.l4_udp);

    // print the counter
    CORD_LOG("Registered FPs: %u\n", cord_app_context.linux_evh->nb_registered_fps);

    while (1)
    {
        int nb_fds = CORD_EVENT_HANDLER_WAIT(cord_app_context.linux_evh);

        //
        // Logic TBD
        //
    }

    cord_app_cleanup();

    return CORD_OK;
}
