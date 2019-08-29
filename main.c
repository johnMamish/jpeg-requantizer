/**
 * "Bad JPEG Quality Control"
 *
 * This software recodes a high-quality jpeg to have multiple quality levels in different regions
 * of interest
 */

#include <stdio.h>
#include <stdlib.h>

#include "jpeg.h"
#include "jpeg-requantizer.h"
#include "bit_dispenser.h"
#include "bit_packer.h"

static void print_block(jpeg_block_t* block)
{
    printf("DCT Matrix=");

    printf("[  DC ");
    int acidx = 0;
    for (int i = 0; i < 7; i++) {
        printf(" %5d", block->ac_values[acidx]);
        acidx++;
    }
    printf("]\n");

    for (int i = 0; i < 7; i++) {
        printf("           [");
        for (int j = 0; j < 8; j++) {
            printf("%5d ", block->ac_values[acidx]);
            acidx++;
        }
        printf("\b]\n");
    }
}

static void print_block_unzigged(jpeg_block_t* block)
{
    jpeg_block_t* unzigged = calloc(1, sizeof(jpeg_block_t));

    // zigzag starts on [1, 0] and moves in the downwards direction
    int zigdir = -1;
    int zig_x = 1;
    int zig_y = 0;
    for (int i = 0; i < 63; i++) {
        // copy data over
        int zigidx = (zig_x + (8 * zig_y)) - 1;
        unzigged->ac_values[zigidx] = block->ac_values[i];
//        printf("zigidx = %3i, i = %3i, zig_x = %2i, zig_y = %2i, zigdir = %2i\n",
//               zigidx, i, zig_x, zig_y, zigdir);

        // update zig coordinates
        // check and see if moving in the zig direction would put us out of bounds
        int nx = zig_x + zigdir;
        int ny = zig_y - zigdir;
        if ((nx < 0) && (ny > 7)) {
            zig_x++;
            zigdir = -zigdir;
        } else if ((nx < 0) && (zigdir == -1)) {
            zig_y++;
            zigdir = -zigdir;
        } else if ((ny < 0) && (zigdir == 1)) {
            zig_x++;
            zigdir = -zigdir;
        } else if ((nx > 7) && (zigdir == 1)) {
            zig_y++;
            zigdir = -zigdir;
        } else if ((ny > 7) && (zigdir == -1)) {
            zig_x++;
            zigdir = -zigdir;
        } else {
            zig_x = nx;
            zig_y = ny;
        }
    }

    print_block(unzigged);
    free(unzigged);
}

int main(int argc, char** argv)
{
#if 0
    // bit packer test code
    bit_packer_t* bp = bit_packer_create();

    bit_packer_pack_u8(0b1111110, 7, bp);
    bit_packer_pack_u16(0b0111111111, 10, bp);
    bit_packer_pack_u8(0b1100, 4, bp);
    bit_packer_pack_u8(0b01, 2, bp);
    bit_packer_pack_u8(0b01, 2, bp);
    bit_packer_fill_endbits(bp);

    // print bit packer
    for (int i = 0; i < bp->curidx; i++) {
        printf("%02x ", bp->data[i]);
    }
    printf("\n");

    bit_packer_destroy(bp);
#endif

    if (argc != 2) {
        printf("needs an arg\n");
        return -1;
    }

    jpeg_image_t* jpeg = jpeg_image_load_from_file(argv[1]);
    if (jpeg == NULL) {
        printf("error reading jpeg\n");
        return -1;
    }

    huffman_decoded_jpeg_scan_t* huffman_decoded_jpeg = jpeg_image_huffman_decode(jpeg);
    if (huffman_decoded_jpeg == NULL) {
        printf("error during huffman decoding\n");
        return -1;
    }

    jpeg_image_t* recompress = jpeg_image_huffman_recode_with_tables(huffman_decoded_jpeg, jpeg);
    huffman_decoded_jpeg_scan_t* redecompress = jpeg_image_huffman_decode(recompress);

    if (redecompress == NULL) {
        printf("error during huffman redcompression\n");
        return -1;
    }

    // print
    for (int MCU = 0; MCU < 10; MCU++) {
        for (int component = 0; component < 3; component++) {
            const int MCUs_per_line   = ((jpeg->frame_header.csps[component].horizontal_sampling_factor *
                                          jpeg->frame_header.samples_per_line) /
                                         huffman_decoded_jpeg->H_max);

            const int MCU_x = MCU % MCUs_per_line;
            const int MCU_y = MCU / MCUs_per_line;
            const int blocks_per_mcu =
                (jpeg->frame_header.csps[component].horizontal_sampling_factor *
                 jpeg->frame_header.csps[component].vertical_sampling_factor);
            const int block_idx = blocks_per_mcu * MCU;
            for (int block = 0; block < blocks_per_mcu; block++) {
                printf("Component = %i, MCU = [%i,%i]\n", component, MCU_x, MCU_y);
                print_block_unzigged(&huffman_decoded_jpeg->components[component].blocks[block_idx + block]);
                printf("\n\n");
            }
        }
    }

    // clean up
    jpeg_image_destroy(jpeg);
    huffman_decoded_jpeg_scan_destroy(huffman_decoded_jpeg);

    return 0;
}
