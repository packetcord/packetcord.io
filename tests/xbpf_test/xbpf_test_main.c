#ifdef ENABLE_DPDK_DATAPLANE
#include <cord_flow/flow_point/cord_dpdk_flow_point.h>
#endif

#ifdef ENABLE_XDP_DATAPLANE
#include <cord_flow/flow_point/cord_xdp_flow_point.h>
#endif

#include <cord_flow/flow_point/cord_l2_custom_flow_point.h>
#include <cord_flow/flow_point/cord_l2_tpacketv3_flow_point.h>
#include <cord_flow/flow_point/cord_l2_raw_socket_flow_point.h>
#include <cord_flow/flow_point/cord_l3_raw_socket_flow_point.h>
#include <cord_flow/flow_point/cord_l3_stack_inject_flow_point.h>
#include <cord_flow/flow_point/cord_l4_tcp_flow_point.h>
#include <cord_flow/flow_point/cord_l4_udp_flow_point.h>
#include <cord_flow/flow_point/cord_l4_sctp_flow_point.h>

int main(void)
{
    CORD_LOG("xBPF attach test!\n");
    
    return CORD_OK;
}
