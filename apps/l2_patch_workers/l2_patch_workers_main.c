#define _GNU_SOURCE
#include <cord_flow/event_handler/cord_linux_api_event_handler.h>
#include <cord_flow/flow_point/cord_l2_raw_socket_flow_point.h>
#include <cord_flow/memory/cord_memory.h>
#include <cord_flow/match/cord_match.h>
#include <cord_error.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <sys/poll.h>

#define MTU_SIZE 1500
#define ETHERNET_HEADER_SIZE 14
#define DOT1Q_TAG_SIZE 4

#define BUFFER_SIZE (MTU_SIZE + ETHERNET_HEADER_SIZE)

#define ETH_IFACE_A_NAME "veth1"
#define ETH_IFACE_B_NAME "veth2"

#define CPU_CORE_A_TO_B 2
#define CPU_CORE_B_TO_A 3

static volatile bool force_quit = false;

static struct
{
    CordFlowPoint *l2_eth_a;
    CordFlowPoint *l2_eth_b;
} cord_app_context;

static void cord_app_setup(void)
{
    CORD_LOG("[CordApp] No manual additional setup required.\n");
}

static void cord_app_cleanup(void)
{
    CORD_LOG("[CordApp] Destroying all objects!\n");
    CORD_DESTROY_FLOW_POINT(cord_app_context.l2_eth_a);
    CORD_DESTROY_FLOW_POINT(cord_app_context.l2_eth_b);
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

// A ---> B
static void *rx_a_tx_b(void *args)
{
    (void)args;

    cord_retval_t cord_retval;
    CORD_BUFFER(buffer, BUFFER_SIZE);
    ssize_t rx_bytes = 0;
    ssize_t tx_bytes = 0;
    struct pollfd pfd;

    set_cpu_affinity(CPU_CORE_A_TO_B);

    pfd.fd = cord_app_context.l2_eth_a->io_handle;
    pfd.events = POLLIN | POLLERR;
    pfd.revents = 0;

    CORD_LOG("[Worker] rx_a_tx_b started on CPU %d\n", CPU_CORE_A_TO_B);

    while (!force_quit)
    {
        if (poll(&pfd, 1, 1) <= 0)
            continue;

        cord_retval = CORD_FLOW_POINT_RX(cord_app_context.l2_eth_a, 0, buffer, BUFFER_SIZE, &rx_bytes);
        if (cord_retval != CORD_OK)
            continue;

        if (rx_bytes < (ssize_t)sizeof(cord_eth_hdr_t))
            continue;

        if (CORD_L2_RAW_SOCKET_FLOW_POINT_ENSURE_INBOUD(cord_app_context.l2_eth_a) != CORD_OK)
            continue;

        cord_retval = CORD_FLOW_POINT_TX(cord_app_context.l2_eth_b, 0, buffer, rx_bytes, &tx_bytes);
        (void)tx_bytes;
    }

    CORD_LOG("[Worker] rx_a_tx_b exiting cleanly\n");
    return NULL;
}

// B ---> A
static void *rx_b_tx_a(void *args)
{
    (void)args;

    cord_retval_t cord_retval;
    CORD_BUFFER(buffer, BUFFER_SIZE);
    ssize_t rx_bytes = 0;
    ssize_t tx_bytes = 0;
    struct pollfd pfd;

    set_cpu_affinity(CPU_CORE_B_TO_A);

    pfd.fd = cord_app_context.l2_eth_b->io_handle;
    pfd.events = POLLIN | POLLERR;
    pfd.revents = 0;

    CORD_LOG("[Worker] rx_b_tx_a started on CPU %d\n", CPU_CORE_B_TO_A);

    while (!force_quit)
    {
        if (poll(&pfd, 1, 1) <= 0)
            continue;

        cord_retval = CORD_FLOW_POINT_RX(cord_app_context.l2_eth_b, 0, buffer, BUFFER_SIZE, &rx_bytes);
        if (cord_retval != CORD_OK)
            continue;

        if (rx_bytes < (ssize_t)sizeof(cord_eth_hdr_t))
            continue;

        if (CORD_L2_RAW_SOCKET_FLOW_POINT_ENSURE_INBOUD(cord_app_context.l2_eth_b) != CORD_OK)
            continue;

        cord_retval = CORD_FLOW_POINT_TX(cord_app_context.l2_eth_a, 0, buffer, rx_bytes, &tx_bytes);
        (void)tx_bytes;
    }

    CORD_LOG("[Worker] rx_b_tx_a exiting cleanly\n");
    return NULL;
}

int main(void)
{
    pthread_t thread_a_to_b, thread_b_to_a;

    CORD_LOG("[CordApp] Launching the PacketCord Patch Workers App!\n");

    signal(SIGINT, cord_app_sigint_callback);

    cord_app_context.l2_eth_a = CORD_CREATE_L2_RAW_SOCKET_FLOW_POINT('A', ETH_IFACE_A_NAME);
    cord_app_context.l2_eth_b = CORD_CREATE_L2_RAW_SOCKET_FLOW_POINT('B', ETH_IFACE_B_NAME);

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
    CORD_LOG("[CordApp] Terminating the PacketCord Patch Workers App!\n");

    return CORD_OK;
}
