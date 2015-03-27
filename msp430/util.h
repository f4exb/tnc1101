#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdint.h>
#include "msp430_interface.h"

#define DELAY_US(n) {__delay_cycles((size_t) (MCLK_MHZ * (n)));}

void print_byte_decimal(uint8_t byte, char *byte_str);

#endif