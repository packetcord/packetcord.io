#include <cord_flow/memory/cord_memory.h>
#include <cord_flow/match/cord_match.h>
#include <cord_flow/eal_initer/cord_eal_initer.h>
#include <cord_flow/flow_point/cord_dpdk_flow_point.h>
#include <cord_error.h>
#include <cord_retval.h>
#include <cord_type.h>
#include <rte_eal.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#include <stdbool.h>

#define RX_TX_RING_SIZE 1024

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

#define VETH1_DPDK_PORT_ID 0
#define VETH2_DPDK_PORT_ID 1

#define LCORE_2_ID 2
#define LCORE_3_ID 3

static volatile bool force_quit = false;

static struct
{
    CordFlowPoint *l2_dpdk_a;
    CordFlowPoint *l2_dpdk_b;
} cord_app_context;

static void cord_app_setup(void)
{
    CORD_LOG("[CordApp] No manual additional setup required.\n");
}

static void cord_app_cleanup(void)
{
    CORD_LOG("[CordApp] Destroying all objects!\n");
    CORD_DESTROY_FLOW_POINT(cord_app_context.l2_dpdk_a);
    CORD_DESTROY_FLOW_POINT(cord_app_context.l2_dpdk_b);
    CORD_LOG("[CordApp] DPDK EAL cleanup.\n");
    cord_eal_cleanup();
}

static void cord_app_sigint_callback(int sig)
{
    (void)sig;
    CORD_LOG("[CordApp] SIGINT received, initiating graceful shutdown...\n");
    force_quit = true;
}

// A ---> B
static int rx_a_tx_b(void *args)
{
    (void)args;

    cord_retval_t cord_retval;
    size_t rx_packets = 0;
    size_t tx_packets = 0;
    struct rte_mbuf *cord_mbufs[BURST_SIZE];

    while (!force_quit)
    {
        // RX
        cord_retval = CORD_FLOW_POINT_RX(cord_app_context.l2_dpdk_a, 0, cord_mbufs, BURST_SIZE, &rx_packets);
        if (cord_retval != CORD_OK)
            continue; // Raw socket receive error

        if (unlikely(rx_packets == 0))
            continue;

        if (rx_packets > 0)
        {
            // TX (CordDpdkFlowPoint_tx_ already frees unsent packets internally)
            cord_retval = CORD_FLOW_POINT_TX(cord_app_context.l2_dpdk_b, 0, cord_mbufs, rx_packets, &tx_packets);
            (void)tx_packets; // Used by CORD_FLOW_POINT_TX but not needed here
        }
    }

    CORD_LOG("[Worker] rx_a_tx_b exiting cleanly on lcore %u\n", rte_lcore_id());
    return CORD_OK;
}

// B ---> A
static int rx_b_tx_a(void *args)
{
    (void)args;

    cord_retval_t cord_retval;
    size_t rx_packets = 0;
    size_t tx_packets = 0;
    struct rte_mbuf *cord_mbufs[BURST_SIZE];

    while (!force_quit)
    {
        // RX
        cord_retval = CORD_FLOW_POINT_RX(cord_app_context.l2_dpdk_b, 0, cord_mbufs, BURST_SIZE, &rx_packets);
        if (cord_retval != CORD_OK)
            continue; // Raw socket receive error

        if (unlikely(rx_packets == 0))
            continue;

        if (rx_packets > 0)
        {
            // TX (CordDpdkFlowPoint_tx_ already frees unsent packets internally)
            cord_retval = CORD_FLOW_POINT_TX(cord_app_context.l2_dpdk_a, 0, cord_mbufs, rx_packets, &tx_packets);
            (void)tx_packets; // Used by CORD_FLOW_POINT_TX but not needed here
        }
    }

    CORD_LOG("[Worker] rx_b_tx_a exiting cleanly on lcore %u\n", rte_lcore_id());
    return CORD_OK;
}

int main(void)
{
    struct rte_mempool *cord_pktmbuf_mpool_a;
    struct rte_mempool *cord_pktmbuf_mpool_b;

    signal(SIGINT, cord_app_sigint_callback);

    char *cord_eal_argv[] = {
        "l2_dpdk_patch_app",                // Program name (argv[0])
        "--lcores", "0,2,3",                // Master lcore 0, worker lcores 2 and 3
        "--proc-type=auto",                 // Process type (auto-detect primary/secondary)
        "--no-pci",                         // Do not probe for physical or virtual PCIe devices
        "--vdev=net_af_xdp0,iface=veth1",   // Bind via AF_XDP PMD to veth1
        "--vdev=net_af_xdp1,iface=veth2",   // Bind via AF_XDP PMD to veth2
        "--log-level=8",                    // Log level (8 = debug)
    };

    int cord_eal_argc = sizeof(cord_eal_argv) / sizeof(cord_eal_argv[0]);
    cord_eal_init(cord_eal_argc, cord_eal_argv);

    uint16_t nb_ports = rte_eth_dev_count_avail();
    CORD_LOG("[CordApp] Total DPDK ports: %u\n", nb_ports);

    // Create separate mempools for each port (avoids double-free and improves cache locality)
    cord_pktmbuf_mpool_a = cord_pktmbuf_mpool_alloc("MBUF_POOL_A", NUM_MBUFS, MBUF_CACHE_SIZE);
    cord_pktmbuf_mpool_b = cord_pktmbuf_mpool_alloc("MBUF_POOL_B", NUM_MBUFS, MBUF_CACHE_SIZE);

    cord_app_context.l2_dpdk_a = CORD_CREATE_DPDK_FLOW_POINT('A',                           // FlowPoint object ID
                                                             VETH1_DPDK_PORT_ID,            // DPDK Port ID
                                                             1,                             // Queue count
                                                             RX_TX_RING_SIZE,               // Queue size
                                                             cord_pktmbuf_mpool_a);         // DPDK memory pool

    cord_app_context.l2_dpdk_b = CORD_CREATE_DPDK_FLOW_POINT('B',                           // FlowPoint object ID
                                                             VETH2_DPDK_PORT_ID,            // DPDK Port ID
                                                             1,                             // Queue count
                                                             RX_TX_RING_SIZE,               // Queue size
                                                             cord_pktmbuf_mpool_b);         // DPDK memory pool


    rte_eal_remote_launch(rx_a_tx_b, NULL, LCORE_2_ID);
    rte_eal_remote_launch(rx_b_tx_a, NULL, LCORE_3_ID);

    // Wait for all worker lcores to finish (they exit when force_quit is set)
    rte_eal_mp_wait_lcore();

    CORD_LOG("[CordApp] All worker lcores stopped, cleaning up...\n");
    cord_app_cleanup();
    CORD_LOG("[CordApp] Terminating the PacketCord Patch App!\n");

    return CORD_OK;
}
