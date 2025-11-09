#include <cord_flow/memory/cord_memory.h>
#include <cord_flow/match/cord_match.h>
#include <cord_flow/eal_initer/cord_eal_initer.h>
#include <cord_error.h>
#include <cord_retval.h>
#include <cord_type.h>
#include <rte_eal.h>
#include <rte_errno.h>
#include <rte_ethdev.h>

#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

static struct
{
    struct rte_mempool* cord_pktmbuf_mpool;
} cord_app_context;

static void cord_app_setup(void)
{
    CORD_LOG("[CordApp] No manual additional setup required.\n");
}

static void cord_app_cleanup(void)
{
    CORD_LOG("[CordApp] DPDK Packet Mbuf and Mempool cleanup.\n");
    cord_pktmbuf_mpool_free(cord_app_context.cord_pktmbuf_mpool);

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

    cord_app_context.cord_pktmbuf_mpool = cord_pktmbuf_mpool_alloc("MBUF_POOL", NUM_MBUFS * nb_ports, MBUF_CACHE_SIZE);

    while (1)
    {

    }

    cord_app_cleanup();

    return CORD_OK;
}
