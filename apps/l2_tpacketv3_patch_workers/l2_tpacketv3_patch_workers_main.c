#define _GNU_SOURCE
#include <cord_flow/event_handler/cord_linux_api_event_handler.h>
#include <cord_flow/flow_point/cord_l2_tpacketv3_flow_point.h>
#include <cord_flow/memory/cord_memory.h>
#include <cord_flow/match/cord_match.h>
#include <cord_error.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <sys/poll.h>
#include <linux/if_packet.h>

#define MTU_SIZE 1500
#define ETHERNET_HEADER_SIZE 14
#define DOT1Q_TAG_SIZE 4

#define BUFFER_SIZE (MTU_SIZE + ETHERNET_HEADER_SIZE)

#define ETH_IFACE_A_NAME "veth1"
#define ETH_IFACE_B_NAME "veth2"

#define TPACKET_V3_BLOCK_SIZE (1 << 18)
#define TPACKET_V3_FRAME_SIZE 2048
#define TPACKET_V3_BLOCK_NUM  256
#define BURST_SIZE (TPACKET_V3_BLOCK_SIZE / TPACKET_V3_FRAME_SIZE)

#define CPU_CORE_A_TO_B 2
#define CPU_CORE_B_TO_A 3

static volatile bool force_quit = false;

static struct
{
    CordFlowPoint *l2_eth_a;
    CordFlowPoint *l2_eth_b;
    struct cord_tpacketv3_ring *rx_ring_a;
    struct cord_tpacketv3_ring *rx_ring_b;
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
    ssize_t rx_packets = 0;
    ssize_t tx_packets = 0;
    struct pollfd pfd;
    struct tpacket_block_desc *pbd;

    set_cpu_affinity(CPU_CORE_A_TO_B);

    pfd.fd = cord_app_context.l2_eth_a->io_handle;
    pfd.events = POLLIN | POLLERR;
    pfd.revents = 0;

    CORD_LOG("[Worker] rx_a_tx_b started on CPU %d\n", CPU_CORE_A_TO_B);

    while (!force_quit)
    {
        // Check if block is ready FIRST (tight spin)
        pbd = (struct tpacket_block_desc *)cord_app_context.rx_ring_a->iov_ring[cord_app_context.rx_ring_a->block_idx].iov_base;

        if (!(pbd->hdr.bh1.block_status & TP_STATUS_USER))
        {
            // No block ready, poll with short timeout
            poll(&pfd, 1, 1);
            continue;
        }

        // RX - process ready blocks
        cord_retval = CORD_FLOW_POINT_RX(cord_app_context.l2_eth_a, 0, &cord_app_context.rx_ring_a, BURST_SIZE, &rx_packets);
        if (cord_retval != CORD_OK)
            continue;

        if (rx_packets == 0)
            continue;

        // TX
        cord_retval = CORD_FLOW_POINT_TX(cord_app_context.l2_eth_b, 0, &cord_app_context.rx_ring_a, rx_packets, &tx_packets);
        (void)tx_packets;
    }

    CORD_LOG("[Worker] rx_a_tx_b exiting cleanly\n");
    return NULL;
}

// B ---> A
static void *rx_b_tx_a(void *args)
{
    (void)args;

    cord_retval_t cord_retval;
    ssize_t rx_packets = 0;
    ssize_t tx_packets = 0;
    struct pollfd pfd;
    struct tpacket_block_desc *pbd;

    set_cpu_affinity(CPU_CORE_B_TO_A);

    pfd.fd = cord_app_context.l2_eth_b->io_handle;
    pfd.events = POLLIN | POLLERR;
    pfd.revents = 0;

    CORD_LOG("[Worker] rx_b_tx_a started on CPU %d\n", CPU_CORE_B_TO_A);

    while (!force_quit)
    {
        // Check if block is ready FIRST (tight spin)
        pbd = (struct tpacket_block_desc *)cord_app_context.rx_ring_b->iov_ring[cord_app_context.rx_ring_b->block_idx].iov_base;

        if (!(pbd->hdr.bh1.block_status & TP_STATUS_USER))
        {
            // No block ready, poll with short timeout
            poll(&pfd, 1, 1);
            continue;
        }

        // RX - process ready blocks
        cord_retval = CORD_FLOW_POINT_RX(cord_app_context.l2_eth_b, 0, &cord_app_context.rx_ring_b, BURST_SIZE, &rx_packets);
        if (cord_retval != CORD_OK)
            continue;

        if (rx_packets == 0)
            continue;

        // TX
        cord_retval = CORD_FLOW_POINT_TX(cord_app_context.l2_eth_a, 0, &cord_app_context.rx_ring_b, rx_packets, &tx_packets);
        (void)tx_packets;
    }

    CORD_LOG("[Worker] rx_b_tx_a exiting cleanly\n");
    return NULL;
}

int main(void)
{
    pthread_t thread_a_to_b, thread_b_to_a;

    CORD_LOG("[CordApp] Launching the PacketCord TPACKET_V3 Patch Workers App!\n");

    signal(SIGINT, cord_app_sigint_callback);

    cord_app_context.rx_ring_a = cord_tpacketv3_ring_alloc(TPACKET_V3_BLOCK_SIZE, TPACKET_V3_FRAME_SIZE, TPACKET_V3_BLOCK_NUM);
    cord_app_context.rx_ring_b = cord_tpacketv3_ring_alloc(TPACKET_V3_BLOCK_SIZE, TPACKET_V3_FRAME_SIZE, TPACKET_V3_BLOCK_NUM);

    cord_app_context.l2_eth_a = CORD_CREATE_L2_TPACKETV3_FLOW_POINT('A', ETH_IFACE_A_NAME, &cord_app_context.rx_ring_a);
    cord_app_context.l2_eth_b = CORD_CREATE_L2_TPACKETV3_FLOW_POINT('B', ETH_IFACE_B_NAME, &cord_app_context.rx_ring_b);

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

    // Wait for all worker threads to finish (they exit when force_quit is set)
    pthread_join(thread_a_to_b, NULL);
    pthread_join(thread_b_to_a, NULL);

    CORD_LOG("[CordApp] All worker threads stopped, cleaning up...\n");
    cord_app_cleanup();
    CORD_LOG("[CordApp] Terminating the PacketCord TPACKET_V3 Patch Workers App!\n");

    return CORD_OK;
}
