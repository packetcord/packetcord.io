#ifdef ENABLE_XDP_DATAPLANE

#include <cord_flow/event_handler/cord_linux_api_event_handler.h>
#include <cord_flow/flow_point/cord_xdp_flow_point.h>
#include <cord_flow/memory/cord_memory.h>
#include <cord_flow/match/cord_match.h>
#include <cord_flow/action/cord_action.h>
#include <cord_flow/cord_error.h>

#define ETH_IFACE_B_NAME "enp11s0f1np1"

#define XDP_NUM_FRAMES 64 // Set them twice as long as the sender side (A)
#define XDP_FRAME_SIZE 2048
#define XDP_RX_RING_SIZE (XDP_NUM_FRAMES / 2)
#define XDP_TX_RING_SIZE (XDP_NUM_FRAMES / 2)
#define XDP_FILL_RING_SIZE (XDP_NUM_FRAMES / 2)
#define XDP_COMP_RING_SIZE (XDP_NUM_FRAMES / 2)

#define BATCH_SIZE (XDP_NUM_FRAMES / 2)

#define EXTERNAL_EBPF_PROGRAM ("udp_rx_filter.bpf.o")

static struct
{
    uint64_t frame_offsets[BATCH_SIZE];
    size_t frame_lengths[BATCH_SIZE];
} umem_frame_tracker = {
    .frame_offsets = { 0 },
    .frame_lengths = { 0 },
};

static struct
{
    CordFlowPoint *l2_xdp_b;
    CordEventHandler *evh;
#ifdef EXTERNAL_EBPF_PROGRAM
    struct xdp_program *bpf_prog;
#endif
} cord_app_context;

static void cord_app_setup(void)
{
    CORD_LOG("[CordApp] No manual additional setup required.\n");
}

static void cord_app_cleanup(void)
{
    CORD_LOG("[CordApp] Destroying all objects!\n");
    CORD_DESTROY_FLOW_POINT(cord_app_context.l2_xdp_b);
    CORD_DESTROY_EVENT_HANDLER(cord_app_context.evh);
}

static void cord_app_sigint_callback(int sig)
{
    CORD_LOG("[CordApp] Terminating the PacketCord AF_XDP UDP App!\n");
    cord_app_cleanup();
    CORD_ASYNC_SAFE_EXIT(CORD_OK);
}

static uint32_t af_xdp_receive_batch(struct cord_xdp_socket_info *xdp_sock, uint64_t *frame_offsets, size_t *frame_lens, uint32_t max_nb)
{
    if (max_nb == 0)
        return 0;

    uint32_t idx_rx = 0;

    // Peek RX ring
    uint32_t received = xsk_ring_cons__peek(&xdp_sock->rx, max_nb, &idx_rx);
    if (received == 0)
        return 0;

    // Read descriptors into caller arrays
    for (uint32_t i = 0; i < received; i++)
    {
        const struct xdp_desc *rx_desc = xsk_ring_cons__rx_desc(&xdp_sock->rx, idx_rx++);
        frame_offsets[i] = rx_desc->addr;
        frame_lens[i]    = rx_desc->len;
    }

    // Release consumed descriptors from RX ring MUST happen before working on data
    xsk_ring_cons__release(&xdp_sock->rx, received);

    return received;
}

static void af_xdp_release_rx_batch(struct cord_xdp_socket_info *xdp_sock, const uint64_t *frame_offsets, uint32_t nb)
{
    if (nb == 0)
        return;

    uint32_t idx_fq = 0;
    uint32_t reserved = xsk_ring_prod__reserve(&xdp_sock->fq, nb, &idx_fq);
    if (reserved == nb)
    {
        for (uint32_t i = 0; i < nb; i++)
        {
            *xsk_ring_prod__fill_addr(&xdp_sock->fq, idx_fq++) = frame_offsets[i];
        }
        xsk_ring_prod__submit(&xdp_sock->fq, nb);
    }

    // Trigger kernel wakeup if Fill Ring was starving
    if (xsk_ring_prod__needs_wakeup(&xdp_sock->fq))
    {
        recvfrom(xsk_socket__fd(xdp_sock->xsk), NULL, 0, MSG_DONTWAIT, NULL, NULL);
    }
}

int main(void)
{
    cord_retval_t cord_retval;
    struct cord_xdp_socket_info *xsk_b;
    ssize_t rx_frames = 0;

    CORD_LOG("[CordApp] Launching the PacketCord AF_XDP UDP App!\n");

    signal(SIGINT, cord_app_sigint_callback);

    xsk_b = cord_xdp_socket_alloc(ETH_IFACE_B_NAME, 0, XDP_NUM_FRAMES, XDP_FRAME_SIZE,
                                  XDP_RX_RING_SIZE, XDP_TX_RING_SIZE,
                                  XDP_FILL_RING_SIZE, XDP_COMP_RING_SIZE);
#ifdef EXTERNAL_EBPF_PROGRAM
    cord_xdp_socket_init(&xsk_b, false);
#else
    cord_xdp_socket_init(&xsk_b, true);
#endif

    cord_app_context.l2_xdp_b = CORD_CREATE_XDP_FLOW_POINT('B', &xsk_b);

    CORD_XDP_FLOW_POINT_FILL(cord_app_context.l2_xdp_b);

#ifdef EXTERNAL_EBPF_PROGRAM
    cord_app_context.bpf_prog = xdp_program__open_file(EXTERNAL_EBPF_PROGRAM, "xdp_sock", NULL);

    if (libxdp_get_error(cord_app_context.bpf_prog))
    {
        CORD_ERROR("[CordApp] Failed to open eBPF program object\n");
        return CORD_ERR;
    }

    CORD_FLOW_POINT_ATTACH_EBPF_PROGRAM(cord_app_context.l2_xdp_b, cord_app_context.bpf_prog, NULL);
#endif

    cord_app_context.evh = CORD_CREATE_LINUX_API_EVENT_HANDLER('E', -1);

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

        for (uint8_t n = 0; n < nb_fds; n++)
        {
            if (cord_app_context.evh->events[n].data.fd == cord_app_context.l2_xdp_b->io_handle)
            {
                rx_frames = af_xdp_receive_batch(xsk_b, umem_frame_tracker.frame_offsets, umem_frame_tracker.frame_lengths, BATCH_SIZE);
                if (rx_frames > 0)
                {
                    for (ssize_t idx = 0; idx < rx_frames; idx++)
                    {
                        uint8_t *hw_offload_payload_base = (uint8_t *)xsk_umem__get_data(xsk_b->umem_area, umem_frame_tracker.frame_offsets[idx]);

                        for (uint32_t i = 0; i < umem_frame_tracker.frame_lengths[idx]; i++)
                                CORD_LOG("%c", hw_offload_payload_base[i]);
                    }
                    af_xdp_release_rx_batch(xsk_b, umem_frame_tracker.frame_offsets, rx_frames);
                }
            }
        }
    }

    cord_app_cleanup();

    return CORD_OK;
}

#endif // ENABLE_XDP_DATAPLANE
