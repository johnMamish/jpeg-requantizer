/**
 * "Bad JPEG Quality Control"
 *
 * This software recodes a high-quality jpeg to have multiple quality levels in different regions
 * of interest
 */

#include <stdio.h>

#include "jpeg.h"
#include "jpeg-requantizer.h"

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

    return 0;
}
