#ifndef BIT_DISPENSER_H
#define BIT_DISPENSER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct bit_dispenser bit_dispenser_t;

bit_dispenser_t* bit_dispenser_create(const uint8_t* data, int datalen);
void bit_dispenser_destroy(bit_dispenser_t* bd);

/**
 * Consumes bits from the bit dispenser and right-shifts them into the given target.
 *
 * The bit-dispenser pulls from the msb of its target data and pushes into the lsb of the given
 * target.
 */
void  bit_dispenser_dispense_u8(uint8_t*  target, int n, bit_dispenser_t* dispenser);
void bit_dispenser_dispense_u16(uint16_t* target, int n, bit_dispenser_t* dispenser);
void bit_dispenser_dispense_u32(uint32_t* target, int n, bit_dispenser_t* dispenser);

bool bit_dispenser_empty(bit_dispenser_t* dispenser);

#endif
