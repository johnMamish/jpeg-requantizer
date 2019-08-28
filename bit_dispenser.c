#include "bit_dispenser.h"

#include <stdlib.h>

struct bit_dispenser
{
    int datalen;
    const uint8_t* data;

    uint8_t currval;
    int bitcount;
    int curidx;
};

bit_dispenser_t* bit_dispenser_create(const uint8_t* data, int datalen)
{
    bit_dispenser_t* result = calloc(1, sizeof(bit_dispenser_t));

    result->data = data;
    result->datalen = datalen;
    result->currval = data[0];

    return result;
}

void bit_dispenser_destroy(bit_dispenser_t* bd)
{
    free(bd);
}

/**
 *
 */
static uint8_t bit_dispenser_dispense_u1(bit_dispenser_t* bd)
{
    if (bit_dispenser_empty(bd) == true) {
        return 0xff;
    }

    uint8_t result = (bd->currval & 0x80) >> 7;
    bd->currval <<= 1;
    bd->bitcount++;
    if (bd->bitcount == 8) {
        bd->bitcount = 0;
        bd->curidx++;
        bd->currval = bd->data[bd->curidx];
    }

    return result;
}

void bit_dispenser_dispense_u8(uint8_t*  target, int n, bit_dispenser_t* bd)
{
    for (int i = 0; i < n; i++) {
        *target <<= 1;
        uint8_t result = bit_dispenser_dispense_u1(bd);
        if (result == 0xff) {
            return;
        }
        *target |= result;
    }
}

void bit_dispenser_dispense_u16(uint16_t* target, int n, bit_dispenser_t* bd)
{
    for (int i = 0; i < n; i++) {
        *target <<= 1;
        uint8_t result = bit_dispenser_dispense_u1(bd);
        if (result == 0xff) {
            return;
        }
        *target |= result;
    }
}

void bit_dispenser_dispense_u32(uint32_t* target, int n, bit_dispenser_t* bd)
{
    for (int i = 0; i < n; i++) {
        *target <<= 1;
        uint8_t result = bit_dispenser_dispense_u1(bd);
        if (result == 0xff) {
            return;
        }
        *target |= result;
    }
}

bool bit_dispenser_empty(const bit_dispenser_t* bd)
{
    return (bd->curidx == bd->datalen);
}
