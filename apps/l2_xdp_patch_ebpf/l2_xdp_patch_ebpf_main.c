//
// # 1. Compile the eBPF C code into BPF bytecode using Clang
// clang -g -O2 -target bpf -c xdp_shared_umem.bpf.c -o xdp_shared_umem.bpf.o
//

#ifdef ENABLE_XDP_DATAPLANE

#include <event_handler/cord_linux_api_event_handler.h>
#include <flow_point/cord_xdp_flow_point.h>
#include <memory/cord_memory.h>
#include <match/cord_match.h>
#include <cord_error.h>
#include <signal.h>
#include <errno.h>

#define ETH_IFACE_A_NAME "veth1"
#define ETH_IFACE_B_NAME "veth2"

//#define ETH_IFACE_A_NAME "enp11s0f0np0"
//#define ETH_IFACE_B_NAME "enp11s0f1np1"

#define XDP_NUM_FRAMES 4096
#define XDP_FRAME_SIZE 4096
#define XDP_RX_RING_SIZE 512
#define XDP_TX_RING_SIZE 512
#define XDP_FILL_RING_SIZE 512
#define XDP_COMP_RING_SIZE 512

#define BURST_SIZE 64

static struct
{
    CordFlowPoint *l2_xdp_a;
    CordFlowPoint *l2_xdp_b;
    CordEventHandler *evh;
    struct xdp_program *bpf_prog;
} cord_app_context;

static void cord_app_setup(void)
{
    CORD_LOG("[CordApp] No manual additional setup required.\n");
}

static void cord_app_cleanup(void)
{
    CORD_LOG("[CordApp] Destroying all objects!\n");
    CORD_DESTROY_FLOW_POINT(cord_app_context.l2_xdp_a);
    CORD_DESTROY_FLOW_POINT(cord_app_context.l2_xdp_b);
    CORD_DESTROY_EVENT_HANDLER(cord_app_context.evh);
}

static void cord_app_sigint_callback(int sig)
{
    (void)sig;
    CORD_LOG("[CordApp] Terminating the PacketCord AF_XDP Patch App!\n");
    cord_app_cleanup();
    CORD_ASYNC_SAFE_EXIT(CORD_OK);
}

int main(void)
{
    cord_retval_t cord_retval;
    struct cord_xdp_socket_info *xsk_a;
    struct cord_xdp_socket_info *xsk_b;
    struct cord_xdp_pkt_desc pkt_descs[BURST_SIZE];
    ssize_t rx_packets = 0;
    ssize_t tx_packets = 0;

    CORD_LOG("[CordApp] Launching the PacketCord AF_XDP Patch App (Shared UMEM)!\n");

    signal(SIGINT, cord_app_sigint_callback);

    cord_app_context.bpf_prog = xdp_program__open_file("xdp_shared_umem.bpf.o", "xdp_sock", NULL);
    if (libxdp_get_error(cord_app_context.bpf_prog))
    {
        CORD_ERROR("[CordApp] Failed to open eBPF program object\n");
        return CORD_ERR;
    }

    xsk_a = cord_xdp_socket_alloc(ETH_IFACE_A_NAME, 0, XDP_NUM_FRAMES, XDP_FRAME_SIZE,
                                  XDP_RX_RING_SIZE, XDP_TX_RING_SIZE,
                                  XDP_FILL_RING_SIZE, XDP_COMP_RING_SIZE);
    cord_xdp_socket_init(&xsk_a, false);

    xsk_b = cord_xdp_socket_alloc(ETH_IFACE_B_NAME, 0, XDP_NUM_FRAMES, XDP_FRAME_SIZE,
                                  XDP_RX_RING_SIZE, XDP_TX_RING_SIZE,
                                  XDP_FILL_RING_SIZE, XDP_COMP_RING_SIZE);
    cord_xdp_socket_init_shared(&xsk_b, &xsk_a, false);

    cord_app_context.l2_xdp_a = CORD_CREATE_XDP_FLOW_POINT('A', &xsk_a);
    cord_app_context.l2_xdp_b = CORD_CREATE_XDP_FLOW_POINT('B', &xsk_b);

    CORD_FLOW_POINT_ATTACH_EBPF_PROGRAM(cord_app_context.l2_xdp_a, cord_app_context.bpf_prog, NULL);
    CORD_FLOW_POINT_ATTACH_EBPF_PROGRAM(cord_app_context.l2_xdp_b, cord_app_context.bpf_prog, NULL);

    CORD_XDP_FLOW_POINT_FILL(cord_app_context.l2_xdp_a);
    CORD_XDP_FLOW_POINT_FILL(cord_app_context.l2_xdp_b);

    cord_app_context.evh = CORD_CREATE_LINUX_API_EVENT_HANDLER('E', -1);

    cord_retval = CORD_EVENT_HANDLER_REGISTER_FLOW_POINT(cord_app_context.evh, cord_app_context.l2_xdp_a);
    cord_retval = CORD_EVENT_HANDLER_REGISTER_FLOW_POINT(cord_app_context.evh, cord_app_context.l2_xdp_b);
    (void)cord_retval;

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

        CORD_FLOW_POINT_RX(cord_app_context.l2_xdp_a, 0, pkt_descs, BURST_SIZE, &rx_packets);
        if (rx_packets > 0)
        {
            CORD_FLOW_POINT_TX(cord_app_context.l2_xdp_b, 0, pkt_descs, rx_packets, &tx_packets);
        }

        CORD_FLOW_POINT_RX(cord_app_context.l2_xdp_b, 0, pkt_descs, BURST_SIZE, &rx_packets);
        if (rx_packets > 0)
        {
            CORD_FLOW_POINT_TX(cord_app_context.l2_xdp_a, 0, pkt_descs, rx_packets, &tx_packets);
        }
    }
    
    cord_app_cleanup();

    return CORD_OK;
}

#endif // ENABLE_XDP_DATAPLANE
