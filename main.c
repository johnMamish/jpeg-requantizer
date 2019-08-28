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

int main(int argc, char** argv)
{
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

    // clean up
    jpeg_image_destroy(jpeg);
    huffman_decoded_jpeg_scan_destroy(huffman_decoded_jpeg);

    return 0;
}
