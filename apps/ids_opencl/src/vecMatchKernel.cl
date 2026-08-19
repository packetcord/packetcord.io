#define BLANK_SPACE 'X'

#define CL_LOG (0)
#define CL_LOG_VERBOSE (0)

__kernel void vectorMatch(__global const uchar *a, __global const uchar *b, volatile __global uint *c,
                          __global uint *signature_length, __global uint *parts, __global uint *payload_length)
{
    uint n = get_global_id(0); // or get_global_id
    *c = 0;

#if (CL_LOG_VERBOSE != 0)
    printf("CL, signature_length: %u\n", *signature_length);
    printf("CL, parts: %u\n", *parts);
    printf("CL, payload_length: %u\n", *payload_length);
    printf("CL, get_global_id: %u\n", n);
#endif

    for (int i = 0; i < *parts; i++)
    {
        uint p = n;
        uint q = ((*payload_length) * i) + n;

#if (CL_LOG != 0)
        printf("a[%u]=%c, b[%u]=%c", p, a[p], q, b[q]);
#endif

        if (p == 0)
        {
            *c = 0;
        }

        if ((a[p] ^ b[q]) == 0)
        {
            if (b[q] != BLANK_SPACE)
            {
                atomic_inc(c);

#if (CL_LOG != 0)
// printf( "a[%u]=%c, b[%u]=%c", p, a[p], q, b[q] );
#endif
            }
            else        // Consider removing this and using the sum of two consequent executions - in case
                *c = 0; // the signature is divided between the end of packet N and the begging of packet N+1
        }
        barrier(CLK_GLOBAL_MEM_FENCE);

        if (*c == *signature_length)
            return;
    }
}

/*
	Not used yet. Consider adding a barrier and re-test. Don't forget to change the
	size_t globalItemSize = rows * cols; instead of PAYLOAD_LEN
*/
__kernel void vectorMatchAlternative(__global const uchar *a, __global const uchar *b, volatile __global uint *c,
                                     __global uint *signature_length, __global uint *parts,
                                     __global uint *payload_length)
{
    uint n = get_global_id(0);

    uint p = n % (*payload_length);
    uint q = n;

    if (p == 0)
        *c = 0;

    if ((a[p] ^ b[q]) == 0)
    {
        if (b[q] != BLANK_SPACE)
        {
            atomic_inc(c);

#if (CL_LOG != 0)
// printf( "a[%u]=%c, b[%u]=%c", p, a[p], q, b[q] );
#endif
        }
    }

    if (*c == *signature_length)
        return;

    // printf( "NG CL, p: %u | q: %u\n", p, q );
}
