#include <cord_flow/event_handler/cord_linux_api_event_handler.h>
#include <cord_flow/flow_point/cord_l2_raw_socket_flow_point.h>
#include <cord_flow/memory/cord_memory.h>
#include <cord_flow/match/cord_match.h>
#include <cord_flow/action/cord_action.h>
#include <cord_error.h>
#include <CL/cl.h>
#include <stdint.h>
#include <sys/types.h>

#include "protocol_headers/cord_protocol_common.h"
#include "signature_ocl_matrix.h"
#include "ids_ips.h"

#define BUFFER_SIZE 9000

#define ETH_IFACE_A_NAME "eno1"

static struct
{
    CordFlowPoint *l2_eth;
    CordEventHandler *evh;
} cord_app_context;

static void cord_app_setup(void)
{
    CORD_LOG("[CordApp] No manual additional setup required.\n");
}

static void cord_app_cleanup(void)
{
    CORD_LOG("[CordApp] Destroying all objects!\n");
    CORD_DESTROY_FLOW_POINT(cord_app_context.l2_eth);
    CORD_DESTROY_EVENT_HANDLER(cord_app_context.evh);
}

static void cord_app_sigint_callback(int sig)
{
    cord_app_cleanup();
    CORD_LOG("[CordApp] Terminating the PacketCord IDS App!\n");
    CORD_ASYNC_SAFE_EXIT(CORD_OK);
}

const uint32_t PATTERN_LEN = SIGNATURE_LEN;
const uint32_t CL_PAYLOAD_CHUNK_LEN = 256;

uint16_t rows = 0;
uint16_t cols = 0;

volatile uint32_t c = 0;

size_t globalItemSize = CL_PAYLOAD_CHUNK_LEN;

cl_uint clDimensions = 1;
cl_command_queue commandQueue = NULL;
cl_kernel kernel = NULL;

cl_mem aMemObj = NULL;
cl_mem bMemObj = NULL;
cl_mem cMemObj = NULL;
cl_mem dMemObj = NULL;
cl_mem eMemObj = NULL;
cl_mem fMemObj = NULL;

static void handle_packet(uint8_t *buffer, size_t len)
{
    cord_eth_hdr_t *eth = cord_header_eth(buffer);
    if (cord_get_field_eth_type_ntohs(eth) != CORD_ETH_P_IP)
        return; // Only handle IPv4 packets

    cord_ipv4_hdr_t *ip = cord_header_ipv4_from_eth(eth);
    if (cord_get_field_ipv4_version(ip) != 4)
        return; // Not IPv4

    uint32_t source_ip = cord_get_field_ipv4_src_addr(ip);
    int iphdr_len = cord_get_field_ipv4_header_length(ip);
    uint8_t *payload_ptr = NULL;
    uint32_t payload_len = 0;
    uint8_t payload_offset = 0;

#if (INSPECT_UDP == 1)
    if (cord_compare_ipv4_protocol(ip, CORD_IPPROTO_UDP))
    {
        cord_udp_hdr_t *udp = cord_header_udp_ipv4(ip);
        payload_offset = sizeof(cord_eth_hdr_t) + iphdr_len + sizeof(cord_udp_hdr_t);

        payload_ptr = buffer + payload_offset;
        payload_len = cord_get_field_udp_length_ntohs(udp) - sizeof(cord_udp_hdr_t);

#if (ENABLE_LOG == 1)
        CORD_LOG("UDP %u bytes payload from IP %s \n", payload_len, inet_ntoa(*(struct in_addr *) &source_ip));
        for (uint32_t i = 0; i < payload_len; i++)
            CORD_LOG("%c", payload_ptr[i]); // %.2X

        CORD_LOG("_______________________________\n");
#endif

        goto inspect_pkt;
    }
#endif

#if (INSPECT_TCP == 1)
    if (cord_compare_ipv4_protocol(ip, CORD_IPPROTO_TCP))
    {
        cord_tcp_hdr_t *tcp = (cord_tcp_hdr_t *) ((uint8_t *) ip + iphdr_len);
        uint16_t tcphdr_len = cord_get_field_tcp_doff(tcp) * 4;

        payload_ptr = buffer + sizeof(cord_eth_hdr_t) + iphdr_len + tcphdr_len;

        uint16_t ip_total_len = cord_get_field_ipv4_total_length_ntohs(ip);
        payload_len = ip_total_len - iphdr_len - tcphdr_len;

#if (ENABLE_LOG == 1)
        CORD_LOG("TCP %u bytes payload from IP %s \n", payload_len, inet_ntoa(*(struct in_addr *) &source_ip));
        for (uint32_t i = 0; i < payload_len; i++)
            CORD_LOG("%c", payload_ptr[i]); // %.2X

        CORD_LOG("_______________________________\n");
#endif

        goto inspect_pkt;
    }
#endif

inspect_pkt:
    cl_int ret;
    ELEMENT_TYPE *a = payload_ptr;

    uint32_t loop_payload_len = payload_len;
    uint32_t offset = 0;

    if ((payload_len / CL_PAYLOAD_CHUNK_LEN) > 0)
    {
        do
        {
#if (ENABLE_LOG == 1)
            for (uint32_t i = offset; i < offset + CL_PAYLOAD_CHUNK_LEN; i++)
                CORD_LOG("%c", payload_ptr[i]);
#endif

            OPENCL_MATCH_LOGIC();

            offset += CL_PAYLOAD_CHUNK_LEN;
            loop_payload_len -= CL_PAYLOAD_CHUNK_LEN;

        } while (loop_payload_len >= CL_PAYLOAD_CHUNK_LEN);

#if (ENABLE_LOG == 1)
        CORD_LOG("loop_payload_len: %u\n", loop_payload_len);
        CORD_LOG("payload_len: %u\n", payload_len);
#endif

        if (loop_payload_len > 0)
        {
            // Set the offset to point to the last chuck
            offset = payload_len - CL_PAYLOAD_CHUNK_LEN;

#if (ENABLE_LOG == 1)
            CORD_LOG("end offset: %u\n", offset);

            for (uint32_t i = offset; i < offset + CL_PAYLOAD_CHUNK_LEN; i++)
                CORD_LOG("%c", payload_ptr[i]);
#endif

            OPENCL_MATCH_LOGIC();
        }
    }

    else if (payload_len > 0)
    {
        offset = 0;

#if (ENABLE_LOG == 1)
        for (uint32_t i = 0; i < payload_len; i++)
            CORD_LOG("%c", payload_ptr[i + offset]);
#endif

        OPENCL_MATCH_LOGIC();
    }

    return;
}

int main(int argc, char **argv)
{
    if (PATTERN_LEN >= CL_PAYLOAD_CHUNK_LEN)
    {
        CORD_LOG("Fatal: PATTERN_LEN is greater than or equal to the CL_PAYLOAD_CHUNK_LEN!\n");
    }

    ELEMENT_TYPE *pattern = (ELEMENT_TYPE *) malloc(sizeof(ELEMENT_TYPE) * CL_PAYLOAD_CHUNK_LEN);
    for (uint32_t i = 0; i < CL_PAYLOAD_CHUNK_LEN; i++)
        pattern[i] = 0x00;

    for (uint32_t i = 0; i < SIGNATURE_LEN; i++)
        pattern[i] = signature[i];

    ELEMENT_TYPE **pattern_match_matrix =
        generate_pattern_match_matrix(pattern, PATTERN_LEN, CL_PAYLOAD_CHUNK_LEN, &rows, &cols);

#if (ENABLE_LOG == 1)
    print_pattern_match_matrix(pattern_match_matrix, rows, cols);
#endif

    ELEMENT_TYPE *pattern_match_buffer = generate_pattern_match_array(pattern_match_matrix, rows, cols);

    CORD_LOG("Generated pattern match buffer len: %u\n", (rows * cols));

    ELEMENT_TYPE *b = pattern_match_buffer;
    uint32_t d = PATTERN_LEN; // Singature length
    uint32_t e = rows;        // Parts  (rows in the pattern tables foreach signature)
    uint32_t f = cols;

    if (cols != CL_PAYLOAD_CHUNK_LEN)
    {
        CORD_LOG("Pattern Matrix column count (%u) does not match CL_PAYLOAD_CHUNK_LEN!\n", cols);
        return -1;
    }

    // Load kernel from file
    FILE *kernelFile;
    char *kernelSource;
    size_t kernelSize;

    kernelFile = fopen("vecMatchKernel.cl", "r");

    if (!kernelFile)
    {
        CORD_ERROR("No file named vecMatchKernel.cl was found\n");
        exit(-1);
    }

    kernelSource = (char *) malloc(MAX_SOURCE_SIZE);
    kernelSize = fread(kernelSource, 1, MAX_SOURCE_SIZE, kernelFile);
    fclose(kernelFile);

    // Getting platform and device information
    cl_platform_id platformId = NULL;
    cl_device_id deviceID = NULL;
    cl_uint retNumDevices;
    cl_uint retNumPlatforms;
    cl_int ret = clGetPlatformIDs(1, &platformId, &retNumPlatforms);
    ret = clGetDeviceIDs(platformId, CL_DEVICE_TYPE_DEFAULT, 1, &deviceID, &retNumDevices);

    // Check the OpenCL version
    char cBuffer[1024];
    clGetDeviceInfo(deviceID, CL_DEVICE_OPENCL_C_VERSION, sizeof(cBuffer), &cBuffer, NULL);
    CORD_LOG("%s\n", cBuffer);

    // Creating context.
    cl_context context = clCreateContext(NULL, 1, &deviceID, NULL, NULL, &ret);

    // Creating command queue
    commandQueue = clCreateCommandQueue(context, deviceID, 0, &ret);

    // Memory buffers for each array - CL_MEM_ALLOC_HOST_PTR
    aMemObj = clCreateBuffer(context, CL_MEM_ALLOC_HOST_PTR | CL_MEM_READ_ONLY,
                             CL_PAYLOAD_CHUNK_LEN * sizeof(ELEMENT_TYPE), NULL, &ret);
    bMemObj = clCreateBuffer(context, CL_MEM_ALLOC_HOST_PTR | CL_MEM_READ_ONLY, rows * cols * sizeof(ELEMENT_TYPE),
                             NULL, &ret);
    dMemObj = clCreateBuffer(context, CL_MEM_ALLOC_HOST_PTR | CL_MEM_READ_ONLY, sizeof(uint32_t), NULL, &ret);
    eMemObj = clCreateBuffer(context, CL_MEM_ALLOC_HOST_PTR | CL_MEM_READ_ONLY, sizeof(uint32_t), NULL, &ret);
    fMemObj = clCreateBuffer(context, CL_MEM_ALLOC_HOST_PTR | CL_MEM_READ_ONLY, sizeof(uint32_t), NULL, &ret);

    cMemObj = clCreateBuffer(context, CL_MEM_ALLOC_HOST_PTR | CL_MEM_READ_WRITE, sizeof(uint32_t), NULL, &ret);

    // Create program from kernel source
    cl_program program =
        clCreateProgramWithSource(context, 1, (const char **) &kernelSource, (const size_t *) &kernelSize, &ret);

    // Build program
    ret = clBuildProgram(program, 1, &deviceID, "-cl-std=CL1.2", NULL, NULL);

    // Create kernel
    kernel = clCreateKernel(program, "vectorMatch", &ret);

    // Set arguments for kernel
    ret = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *) &aMemObj);
    ret = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void *) &bMemObj);
    ret = clSetKernelArg(kernel, 2, sizeof(cl_mem), (void *) &cMemObj);
    ret = clSetKernelArg(kernel, 3, sizeof(cl_mem), (void *) &dMemObj);
    ret = clSetKernelArg(kernel, 4, sizeof(cl_mem), (void *) &eMemObj);
    ret = clSetKernelArg(kernel, 5, sizeof(cl_mem), (void *) &fMemObj);

    // Buffer 'a' is enqued inside handle_packet
    ret = clEnqueueWriteBuffer(commandQueue, bMemObj, CL_TRUE, 0, rows * cols * sizeof(ELEMENT_TYPE), b, 0, NULL, NULL);
    // Buffer 'c' is enqued inside handle_packet
    ret = clEnqueueWriteBuffer(commandQueue, dMemObj, CL_TRUE, 0, sizeof(uint32_t), (const void *) (&d), 0, NULL, NULL);
    ret = clEnqueueWriteBuffer(commandQueue, eMemObj, CL_TRUE, 0, sizeof(uint32_t), (const void *) (&e), 0, NULL, NULL);
    ret = clEnqueueWriteBuffer(commandQueue, fMemObj, CL_TRUE, 0, sizeof(uint32_t), (const void *) (&f), 0, NULL, NULL);
    ret = clEnqueueWriteBuffer(commandQueue, cMemObj, CL_TRUE, 0, sizeof(uint32_t), (const void *) (&c), 0, NULL, NULL);

    cord_retval_t cord_retval;
    CORD_BUFFER(packet_buffer, BUFFER_SIZE);
    size_t rx_bytes = 0;
    size_t tx_bytes = 0;

    CORD_LOG("[CordApp] Launching the PacketCord IDS GPU App!\n");

    signal(SIGINT, cord_app_sigint_callback);

    cord_app_context.l2_eth = CORD_CREATE_L2_RAW_SOCKET_FLOW_POINT('A', ETH_IFACE_A_NAME);

    cord_app_context.evh = CORD_CREATE_LINUX_API_EVENT_HANDLER('E', -1);

    cord_retval = CORD_EVENT_HANDLER_REGISTER_FLOW_POINT(cord_app_context.evh, cord_app_context.l2_eth);

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
            if (cord_app_context.evh->events[n].data.fd == cord_app_context.l2_eth->io_handle)
            {
                cord_retval = CORD_FLOW_POINT_RX(cord_app_context.l2_eth, 0, packet_buffer, BUFFER_SIZE, &rx_bytes);
                if (cord_retval != CORD_OK)
                    continue; // Raw socket receive error

                if (rx_bytes < sizeof(cord_eth_hdr_t))
                    continue; // Packet too short to contain Ethernet header

                if (CORD_L2_RAW_SOCKET_FLOW_POINT_ENSURE_INBOUD(cord_app_context.l2_eth) != CORD_OK)
                    continue; // Ensure this is not an outgoing packet

                handle_packet(packet_buffer, rx_bytes);
            }
        }
    }

    ret = clFlush(commandQueue);
    ret = clFinish(commandQueue);
    ret = clReleaseCommandQueue(commandQueue);
    ret = clReleaseKernel(kernel);
    ret = clReleaseProgram(program);

    ret = clReleaseMemObject(aMemObj);
    ret = clReleaseMemObject(bMemObj);
    ret = clReleaseMemObject(cMemObj);
    ret = clReleaseMemObject(dMemObj);
    ret = clReleaseMemObject(eMemObj);
    ret = clReleaseMemObject(fMemObj);

    ret = clReleaseContext(context);

    free(pattern);
    free(pattern_match_matrix);
    free(pattern_match_buffer);

    cord_app_cleanup();
    return CORD_OK;
}
