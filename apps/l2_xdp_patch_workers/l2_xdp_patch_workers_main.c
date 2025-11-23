#ifdef ENABLE_XDP_DATAPLANE

#include <flow_point/cord_xdp_flow_point.h>
#include <memory/cord_memory.h>
#include <match/cord_match.h>
#include <cord_error.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/epoll.h>

#define ETH_IFACE_A_NAME "veth1"
#define ETH_IFACE_B_NAME "veth2"

#define XDP_NUM_FRAMES 4096
#define XDP_FRAME_SIZE 4096
#define XDP_RX_RING_SIZE 512
#define XDP_TX_RING_SIZE 512
#define XDP_FILL_RING_SIZE 512
#define XDP_COMP_RING_SIZE 512

#define BURST_SIZE 64

#define CPU_CORE_A_TO_B 2
#define CPU_CORE_B_TO_A 3

static volatile bool force_quit = false;

static struct
{
    CordFlowPoint *l2_xdp_a;
    CordFlowPoint *l2_xdp_b;
    struct cord_xdp_socket_info *xsk_a;
    struct cord_xdp_socket_info *xsk_b;
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
    cord_xdp_socket_free(&cord_app_context.xsk_a);
    cord_xdp_socket_free(&cord_app_context.xsk_b);
}

static void cord_app_sigint_callback(int sig)
{
    (void)sig;
    CORD_LOG("[CordApp] SIGINT received, initiating graceful shutdown...\n");
    force_quit = true;
}

static int set_cpu_affinity(int cpu_core)
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_core, &cpuset);

    if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0)
    {
        CORD_ERROR("[Worker] pthread_setaffinity_np");
        return -1;
    }

    CORD_LOG("[Worker] Thread pinned to CPU core %d\n", cpu_core);
    return 0;
}

static void *rx_a_tx_b(void *args)
{
    (void)args;

    cord_retval_t cord_retval;
    ssize_t rx_packets = 0;
    ssize_t tx_packets = 0;
    int epoll_fd;
    struct epoll_event ev, events[1];
    struct cord_xdp_pkt_desc pkt_descs[BURST_SIZE];

    set_cpu_affinity(CPU_CORE_A_TO_B);

    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
    {
        CORD_ERROR("[Worker] epoll_create1");
        return NULL;
    }

    ev.events = EPOLLIN;
    ev.data.fd = cord_app_context.l2_xdp_a->io_handle;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, cord_app_context.l2_xdp_a->io_handle, &ev) == -1)
    {
        CORD_ERROR("[Worker] epoll_ctl");
        close(epoll_fd);
        return NULL;
    }

    CORD_LOG("[Worker] rx_a_tx_b started on CPU %d\n", CPU_CORE_A_TO_B);

    while (!force_quit)
    {
        epoll_wait(epoll_fd, events, 1, 1);

        cord_retval = CORD_FLOW_POINT_RX(cord_app_context.l2_xdp_a, 0, pkt_descs, BURST_SIZE, &rx_packets);
        if (cord_retval != CORD_OK)
            continue;

        if (rx_packets == 0)
            continue;

        cord_retval = CORD_FLOW_POINT_TX(cord_app_context.l2_xdp_b, 0, pkt_descs, rx_packets, &tx_packets);
        (void)tx_packets;
    }

    close(epoll_fd);
    CORD_LOG("[Worker] rx_a_tx_b exiting cleanly\n");
    return NULL;
}

static void *rx_b_tx_a(void *args)
{
    (void)args;

    cord_retval_t cord_retval;
    ssize_t rx_packets = 0;
    ssize_t tx_packets = 0;
    int epoll_fd;
    struct epoll_event ev, events[1];
    struct cord_xdp_pkt_desc pkt_descs[BURST_SIZE];

    set_cpu_affinity(CPU_CORE_B_TO_A);

    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
    {
        CORD_ERROR("[Worker] epoll_create1");
        return NULL;
    }

    ev.events = EPOLLIN;
    ev.data.fd = cord_app_context.l2_xdp_b->io_handle;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, cord_app_context.l2_xdp_b->io_handle, &ev) == -1)
    {
        CORD_ERROR("[Worker] epoll_ctl");
        close(epoll_fd);
        return NULL;
    }

    CORD_LOG("[Worker] rx_b_tx_a started on CPU %d\n", CPU_CORE_B_TO_A);

    while (!force_quit)
    {
        epoll_wait(epoll_fd, events, 1, 1);

        cord_retval = CORD_FLOW_POINT_RX(cord_app_context.l2_xdp_b, 0, pkt_descs, BURST_SIZE, &rx_packets);
        if (cord_retval != CORD_OK)
            continue;

        if (rx_packets == 0)
            continue;

        cord_retval = CORD_FLOW_POINT_TX(cord_app_context.l2_xdp_a, 0, pkt_descs, rx_packets, &tx_packets);
        (void)tx_packets;
    }

    close(epoll_fd);
    CORD_LOG("[Worker] rx_b_tx_a exiting cleanly\n");
    return NULL;
}

int main(void)
{
    pthread_t thread_a_to_b, thread_b_to_a;

    CORD_LOG("[CordApp] Launching the PacketCord AF_XDP Patch Workers App!\n");

    signal(SIGINT, cord_app_sigint_callback);

    cord_app_context.xsk_a = cord_xdp_socket_alloc(ETH_IFACE_A_NAME, 0, XDP_NUM_FRAMES, XDP_FRAME_SIZE,
                                                    XDP_RX_RING_SIZE, XDP_TX_RING_SIZE,
                                                    XDP_FILL_RING_SIZE, XDP_COMP_RING_SIZE);
    cord_xdp_socket_init(&cord_app_context.xsk_a);

    cord_app_context.xsk_b = cord_xdp_socket_alloc(ETH_IFACE_B_NAME, 0, XDP_NUM_FRAMES, XDP_FRAME_SIZE,
                                                    XDP_RX_RING_SIZE, XDP_TX_RING_SIZE,
                                                    XDP_FILL_RING_SIZE, XDP_COMP_RING_SIZE);
    cord_xdp_socket_init(&cord_app_context.xsk_b);

    cord_app_context.l2_xdp_a = CORD_CREATE_XDP_FLOW_POINT('A', &cord_app_context.xsk_a);
    cord_app_context.l2_xdp_b = CORD_CREATE_XDP_FLOW_POINT('B', &cord_app_context.xsk_b);

    if (pthread_create(&thread_a_to_b, NULL, rx_a_tx_b, NULL) != 0)
    {
        CORD_ERROR("[CordApp] pthread_create for rx_a_tx_b");
        cord_app_cleanup();
        return CORD_ERR;
    }

    if (pthread_create(&thread_b_to_a, NULL, rx_b_tx_a, NULL) != 0)
    {
        CORD_ERROR("[CordApp] pthread_create for rx_b_tx_a");
        force_quit = true;
        pthread_join(thread_a_to_b, NULL);
        cord_app_cleanup();
        return CORD_ERR;
    }

    CORD_LOG("[CordApp] All worker threads started, waiting for SIGINT...\n");

    pthread_join(thread_a_to_b, NULL);
    pthread_join(thread_b_to_a, NULL);

    CORD_LOG("[CordApp] All worker threads stopped, cleaning up...\n");
    cord_app_cleanup();
    CORD_LOG("[CordApp] Terminating the PacketCord AF_XDP Patch Workers App!\n");

    return CORD_OK;
}

#endif // ENABLE_XDP_DATAPLANE