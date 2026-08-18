#ifndef SIGNATURE_OCL_MATRIX_H
#define SIGNATURE_OCL_MATRIX_H

#include <stdint.h>
#include <stdlib.h>

uint8_t** generate_pattern_match_matrix(uint8_t* pattern,          // Pattern buffer
                                        uint16_t pattern_len,      // Pattern buffer length
                                        uint16_t payload_len,      // Packet payload buffer - use the network interface MTU (for UDP) or MSS (for TCP) values
                                        uint16_t* rows_count,      // Generated matrix rows count
                                        uint16_t* columns_count);  // Generated matrix columns count

void print_pattern_match_matrix(uint8_t** matrix,                  // Generated matrix 
                                uint16_t rows_count,               // Rows count
                                uint16_t columns_count);           // Columns count

uint8_t* generate_pattern_match_array(uint8_t** matrix,            // Generated matrix 
                                      uint16_t rows_count,         // Rows count
                                      uint16_t columns_count);     // Columns count
 
void print_pattern_match_array(uint8_t* array,                     // Generated array 
                               uint32_t len);                      // Array length

#endif
