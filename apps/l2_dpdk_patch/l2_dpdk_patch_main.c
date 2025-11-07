#include <cord_flow/memory/cord_memory.h>
#include <cord_flow/match/cord_match.h>
#include <cord_flow/eal_initer/cord_eal_initer.h>
#include <cord_error.h>
#include <cord_retval.h>
#include <cord_type.h>
#include <rte_eal.h>
#include <rte_errno.h>

#define MTU_SIZE 1500
#define ETHERNET_HEADER_SIZE 14
#define DOT1Q_TAG_SIZE 4

#define BUFFER_SIZE (MTU_SIZE + ETHERNET_HEADER_SIZE)

static struct
{

} cord_app_context;

static void cord_app_setup(void)
{
    CORD_LOG("[CordApp] No manual additional setup required.\n");
}

static void cord_app_cleanup(void)
{
    if (rte_eal_cleanup() == 0)
        CORD_LOG("[CordApp] DPDK EAL cleanup.\n");
    else
        CORD_ERROR("[CordApp] DPDK EAL cleanup failed!\n");
}

static void cord_app_sigint_callback(int sig)
{
    cord_app_cleanup();
    CORD_LOG("[CordApp] Terminating the PacketCord Patch App!\n");
    CORD_ASYNC_SAFE_EXIT(CORD_OK);
}

int main(void)
{
    char *cord_eal_argv[] = {
        "l2_dpdk_patch",      // Program name
        "-l", "0-1",          // Logical cores to use (0-1)
        "--proc-type=auto",   // Process type (auto-detect primary/secondary)
        "--log-level=8",      // Log level (8 = debug)
    };

    // --no-pci --vdev net_af_xdp0,iface=veth1 --vdev net_af_xdp1,iface=veth2 --log-level=pmd.net.af_xdp:debug

    int cord_eal_argc = sizeof(cord_eal_argv) / sizeof(cord_eal_argv[0]);
    cord_eal_init(cord_eal_argc, cord_eal_argv);

    signal(SIGINT, cord_app_sigint_callback);

    while (1)
    {

    }

    cord_app_cleanup();

    return CORD_OK;
}
