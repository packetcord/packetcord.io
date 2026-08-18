#ifndef IDS_IPS_H
#define IDS_IPS_H

#include "config.h"

#define MAX_SOURCE_SIZE (0x100000)
#define ELEMENT_TYPE uint8_t

#define OPENCL_MATCH_LOGIC()                                                                                      \
    ret = clEnqueueWriteBuffer(commandQueue, aMemObj, CL_TRUE, 0, CL_PAYLOAD_CHUNK_LEN * sizeof(ELEMENT_TYPE),    \
                               a + offset, 0, NULL, NULL);                                                        \
    if (ret != CL_SUCCESS)                                                                                        \
        return;                                                                                                   \
    ret = clEnqueueNDRangeKernel(commandQueue, kernel, clDimensions, NULL, &globalItemSize, NULL, 0, NULL, NULL); \
    if (ret != CL_SUCCESS)                                                                                        \
        return;                                                                                                   \
    ret = clEnqueueReadBuffer(commandQueue, cMemObj, CL_TRUE, 0, sizeof(c), (void *) &c, 0, NULL, NULL);          \
    if (ret != CL_SUCCESS)                                                                                        \
        return;                                                                                                   \
    if (c == PATTERN_LEN)                                                                                         \
        printf("%s from IP %s \n", ALERT_MSG, inet_ntoa(*(struct in_addr *) &ip->saddr));

#endif
