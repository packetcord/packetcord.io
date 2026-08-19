#include <stdio.h>
#include "signature_ocl_matrix.h"

uint8_t **generate_pattern_match_matrix(uint8_t *pattern, uint16_t pattern_len, uint16_t payload_len,
                                        uint16_t *rows_count, uint16_t *columns_count)
{
    uint16_t matrix_rows = ((payload_len - pattern_len) + 1);
    uint8_t **matrix = (uint8_t **) malloc(sizeof(uint8_t *) * matrix_rows);

    uint8_t *normalized_pattern_buffer = (uint8_t *) malloc(sizeof(uint8_t) * payload_len);

    uint16_t i = 0;
    for (i = 0; i < payload_len; i++)
        *(normalized_pattern_buffer + i) = 'X';

    for (i = 0; i < pattern_len; i++)
        *(normalized_pattern_buffer + i) = pattern[i];


#if (ENABLE_LOG)
    printf("Single line from the configuration file: ");
    for (i = 0; i < pattern_len; i++)
    {
        *(normalized_pattern_buffer + i) = pattern[i];
        printf("%c", *(normalized_pattern_buffer + i));
    }
    printf("\n");


    printf("Line transformed to packet buffer: ");
    for (i = 0; i < payload_len; i++)
        printf("%c", *(normalized_pattern_buffer + i));
    printf("\n");
#endif

    //
    // Populate the matrix
    //
    uint16_t offset = 0;
    for (i = 0; i < matrix_rows; i++)
    {
        matrix[i] = (uint8_t *) malloc(sizeof(uint8_t) * payload_len);
        for (uint16_t j = 0; j < payload_len; j++)
        {
            *(*(matrix + i) + ((j + offset) % payload_len)) = normalized_pattern_buffer[j];

            // Alternative to: matrix[i][(j+offset) % payload_len] = normalized_pattern_buffer[j];
        }
        offset++;
    }

    *rows_count = matrix_rows;
    *columns_count = payload_len;

    free(normalized_pattern_buffer);

    return matrix;
}

void print_pattern_match_matrix(uint8_t **matrix, uint16_t rows_count, uint16_t columns_count)
{
    uint16_t i = 0;

    printf("===========================\n");
    for (i = 0; i < rows_count; i++)
    {
        printf("Row %u: ", i);

        for (uint16_t j = 0; j < columns_count; j++)
        {
            printf("%c", *(*(matrix + i) + j));

            // Alternative to: printf("%c", matrix[i][j]);
        }
        printf("\n");
    }
    printf("===========================\n");

    return;
}

uint8_t *generate_pattern_match_array(uint8_t **matrix,       // Generated matrix
                                      uint16_t rows_count,    // Rows count
                                      uint16_t columns_count) // Columns count
{
    if ((rows_count < 1) || (columns_count < 1))
        return NULL;

    // Calculate the new length
    uint32_t len = rows_count * columns_count;
    uint8_t *array = (uint8_t *) malloc(sizeof(uint8_t) * (len));

    uint32_t n = 0;

    for (uint32_t i = 0; i < rows_count; i++)
        for (uint16_t j = 0; j < columns_count; j++)
            array[n++] = *(*(matrix + i) + j);

    free(matrix);

    return array;
}

void print_pattern_match_array(uint8_t *array, // Generated array
                               uint32_t len)   // Array length
{
    printf("===========================\n");

    for (uint32_t i = 0; i < len; i++)
        printf("%c", array[i]);
    printf("\n");

    printf("===========================\n");
}
