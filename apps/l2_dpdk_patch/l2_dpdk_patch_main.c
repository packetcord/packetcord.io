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

#define LCORE_MASK 0x00

#define RX_TX_RING_SIZE 1024

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

#define VETH1_DPDK_PORT_ID 0
#define VETH2_DPDK_PORT_ID 1

static struct
{
    CordFlowPoint *l2_dpdk_a;
} cord_app_context;

static void cord_app_setup(void)
{
    CORD_LOG("[CordApp] No manual additional setup required.\n");
}

static void cord_app_cleanup(void)
{
    CORD_LOG("[CordApp] Destroying all objects!\n");
    CORD_DESTROY_FLOW_POINT(cord_app_context.l2_dpdk_a);
    CORD_LOG("[CordApp] DPDK EAL cleanup.\n");
    cord_eal_cleanup();
}

static void cord_app_sigint_callback(int sig)
{
    cord_app_cleanup();
    CORD_LOG("[CordApp] Terminating the PacketCord Patch App!\n");
    CORD_ASYNC_SAFE_EXIT(CORD_OK);
}

int main(void)
{
    cord_retval_t cord_retval;
    struct rte_mempool *cord_pktmbuf_mpool_common;
    struct rte_mbuf *cord_mbufs[BURST_SIZE];
    size_t rx_packets = 0;
    size_t tx_packets = 0;

    signal(SIGINT, cord_app_sigint_callback);

    char *cord_eal_argv[] = {
        "l2_dpdk_patch_app",                // Program name (argv[0])
        "--lcores", "0-1",                  // Master logical cores to use (0-1)
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

    cord_pktmbuf_mpool_common = cord_pktmbuf_mpool_alloc("MBUF_POOL", NUM_MBUFS * nb_ports, MBUF_CACHE_SIZE);
    cord_app_context.l2_dpdk_a = CORD_CREATE_DPDK_FLOW_POINT('A',                           // FlowPoint object ID
                                                             VETH1_DPDK_PORT_ID,            // DPDK Port ID
                                                             1,                             // Queue count
                                                             RX_TX_RING_SIZE,               // Queue size
                                                             LCORE_MASK,                    // Logic core (CPU) mask
                                                             cord_pktmbuf_mpool_common);    // DPDK memory pool

    uint16_t port;
    while (1)
    {
        RTE_ETH_FOREACH_DEV(port)
        {
            CORD_LOG("[CordApp] Port: %u\n", port);
            cord_retval = CORD_FLOW_POINT_RX(cord_app_context.l2_dpdk_a, 0, cord_mbufs, BURST_SIZE, &rx_packets);
            if (cord_retval != CORD_OK)
                continue; // Raw socket receive error
        }
    }

    cord_app_cleanup();

    return CORD_OK;
}
