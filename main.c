/**
 * "Bad JPEG Quality Control"
 *
 * This software recodes a high-quality jpeg to have multiple quality levels in different regions
 * of interest
 */

#include <stdio.h>

#include "jpeg.h"
#include "jpeg-requantizer.h"
#include "bit_dispenser.h"

void printbits_u8(uint8_t bits) {
    for (int i = 0; i < 8; i++) {
        if (bits & 0x80) {
            printf("1");
        } else {
            printf("0");
        }
        bits <<= 1;
    }
}

void printbits_u32(uint32_t bits) {
    for (int i = 0; i < 32; i++) {
        if (bits & 0x80000000) {
            printf("1");
        } else {
            printf("0");
        }
        bits <<= 1;
    }
}

int main(int argc, char** argv)
{
/*    if (argc != 2) {
        printf("needs an arg\n");
        return -1;
    }

    jpeg_image_t* jpeg = jpeg_image_load_from_file(argv[1]);
    if (jpeg == NULL) {
        printf("error reading jpeg\n");
        return -1;
    }
*/

    printf("testing bit dispenser\n");

    uint8_t test_data[] = { 0x4a, 0xff, 0x37, 0x92 };

    printf("test_data is ");
    for (int i = 0; i < 4; i++) {
        printbits_u8(test_data[i]);
        printf(" ");
    }
    printf("\n");

    uint32_t target = 0;
    bit_dispenser_t* bd = bit_dispenser_create(test_data, 4);

    for (int i = 0; i < 14; i++) {
        bit_dispenser_dispense_u32(&target, 1, bd);
    }

    uint32_t target2 = 0;
    bit_dispenser_dispense_u32(&target2, 13, bd);

    printf("target1 = ");
    printbits_u32(target);
    printf("\ntarget2 = ");
    printbits_u32(target2);
    printf("\n");

    return 0;
}
