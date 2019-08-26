#ifndef JPEG_REQUANTIZER_H
#define JPEG_REQUANTIZER_H

#include "jpeg.h"

/**
 *
 * rois should have the same dimensions as the image stored in jpg.
 * every value of rois should be in [1, 100]. If there are conflicting values for one 8x8 MCU, the
 * higher value is taken for that MCU.
 */
void recode_jpeg(jpeg_image_t *jpg, unsigned char *rois);


#endif
