#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

#define ENABLE_LOG (0)

#define INSPECT_TCP (0)
#define INSPECT_UDP (1)

#define SIGNATURE_LEN (14)
uint8_t signature[SIGNATURE_LEN] = {'E', 'x', 'p', 'l', 'o', 'i', 't', 'B', 'y', 't', 'e', 's', '0', 'P'};
#define ALERT_MSG "\nSIGNATURE DETECTED!"

#endif
