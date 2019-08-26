#include <stdio.h>
#include "jpeg.h"

////////////////////////////////////////////////////////////////
// Marker symbol definitions
const static uint8_t SOF_0 = 0xc0;
const static uint8_t SOF_1 = 0xc1;
const static uint8_t SOF_2 = 0xc2;
const static uint8_t SOF_3 = 0xc3;
const static uint8_t SOF_5 = 0xc5;
const static uint8_t SOF_6 = 0xc6;
const static uint8_t SOF_7 = 0xc7;
const static uint8_t SOF_9 = 0xc9;
const static uint8_t SOF_10 = 0xca;
const static uint8_t SOF_11 = 0xcb;
const static uint8_t SOF_13 = 0xcd;
const static uint8_t SOF_14 = 0xce;
const static uint8_t SOF_15 = 0xcf;

const static uint8_t DHT = 0xc4;
const static uint8_t JPG_EXT = 0xc8;
const static uint8_t DAC = 0xcc;

const static uint8_t SOI = 0xd8;
const static uint8_t EOI = 0xd9;
const static uint8_t SOS = 0xda;
const static uint8_t DQT = 0xdb;
const static uint8_t DNL = 0xdc;
const static uint8_t DRI = 0xdd;
const static uint8_t DHP = 0xde;
const static uint8_t EXP = 0xdf;

const static uint8_t APP_0 = 0xe0;
const static uint8_t APP_15 = 0xef;

const static uint8_t JPG_0 = 0xf0;
const static uint8_t JPG_13 = 0xfd;

const static uint8_t COM = 0xfe;


/**
 *
 */
static int marker_is_SOF_marker(uint8_t marker[2])
{
    return (((marker[1] >= SOF_0)  && (marker[1] <= SOF_3))  ||
            ((marker[1] >= SOF_5)  && (marker[1] <= SOF_7))  ||
            ((marker[1] >= SOF_9)  && (marker[1] <= SOF_11)) ||
            ((marker[1] >= SOF_13) && (marker[1] <= SOF_15)));
}

static int marker_is_APP_marker(uint8_t marker[2])
{
    return ((marker[1] >= APP_0) && (marker[1] <= APP_15));
}

static int marker_is_JPG_marker(uint8_t marker[2])
{
    return ((marker[1] >= JPG_0) && (marker[1] <= JPG_13));
}

/**
 * Returns 1 if the given marker denotes any jpeg segment other than a frame or scan segment.
 * These segments must be treated differently as they can contain other segments.
 */
static int marker_is_miscellaneous_marker(uint8_t marker[2])
{
    return (marker_is_SOF_marker(marker) ||
            marker_is_APP_marker(marker) ||
            marker_is_JPG_marker(marker) ||
            (marker[1] == DHT)           ||
            (marker[1] == JPG_EXT)       ||
            (marker[1] == DAC)           ||
            (marker[1] == SOI)           ||
            (marker[1] == EOI)           ||
            (marker[1] == DQT)           ||
            (marker[1] == DNL)           ||
            (marker[1] == DRI)           ||
            (marker[1] == DHP)           ||
            (marker[1] == EXP)           ||
            (marker[1] == COM));
}

/**
 * Decodes the next segment in the file and places it as appropriate in the jpeg_t.
 *
 * on failure, returns a nonzero value.
 */
static int decode_miscellaneous_segment(FILE* fp, uint8_t marker[2], jpeg_image_t* jpeg)
{
    if (!marker_is_miscellaneous_marker(marker)) {
        return -1;
    }

    // figure out if there is a special struct that we need to decode this segment into.
    if (marker[1] == DHT) {
        decode_huffman_table_segment(fp, marker, jpeg);
    } else if (marker[1] == DQT) {
        decode_quantization_table_segment(fp, marker, jpeg);
    } else {
        jpeg_generic_segment_t* gs = calloc(1, sizeof(jpeg_generic_segment_t));

        gs->header.segment_marker[0] = marker[0];
        gs->header.segment_marker[1] = marker[1];

        // get the length of the segment
        uint8_t Ls_buf[2];
        if (fread(fp, 2, 1, Ls_buf) == 0) {
            goto cleanup_failed_read;
        }
        gs->header.Ls = (Ls_buf[0] << 8) | Ls_buf[1];

        uint16_t payload_size = gs->header.Ls - 2;
        if (payload_size > 0) {
            gs->data = malloc((gs->header.Ls - 2) * sizeof(uint8_t));
            if (fread(fp, payload_size, 1, gs->data) == 0) {
                goto cleanup_failed_read;
            }
        }

        jpeg->num_jpeg_segments++;
        jpeg->frame_segment

        return 0;

    cleanup_failed_read:

    }
}

/**
 *
 */
static jpeg_scan_t* decode_scan(uint8_t marker[2], FILE* fp, jpeg_image_t* jpeg)
{

}

/**
 *
 */
static int decode_frame(uint8_t frame_marker[2], FILE* fp, jpeg_image_t* jpeg)
{
    // read in the frame header


    // read in scans
    uint8_t marker[2];
    while (fread(marker, 2, 1, fp) != 0) {
        if (marker[0] != 0xff) {
            return -1;
        }

        // decode huffman table
        } else if (marker[1] == DQT) {
            // decode quantization table
        }
    }
}

jpeg_image_t* load_jpeg(const char* filename)
{
    jpeg_image_t* jpeg = NULL;

    // open the file.
    FILE* fp = fopen(filename, "rb");
    if (fp == NULL) {
        return NULL;
    }

    // read the SOI marker, just to be sure.
    uint8_t SOI_marker[2] = { 0 };
    fread(marker, 2, 1, fp);
    if ((SOI_marker[0] != 0xff) || (SOI_marker[1] != 0xd8)) {
        return NULL;
    }

    // allocate a new structure.
    jpeg = calloc(1, sizeof(jpeg_image_t));

    // handle each segment as it comes up.
    uint8_t marker[2];
    while (fread(marker, 2, 1, fp) != 0) {
        // The first byte of every marker is 0xff.
        if (marker[0] != 0xff) {
            jpeg_image_destroy(jpeg);
            return NULL;
        }

        // The second byte of a marker determines
        switch (marker[1]) {
            // Start-of-frame markers
            case 0xc0 ... 0xc3:
            case 0xc5 ... 0xc7:
            case 0xc9 ... 0xcb:
            case 0xcd ... 0xcf: {

                break;
            }

            // Huffman table
            case 0xc4: {
                break;
            }

            // Arithmetic coding table
            case 0xcc: {
                break;
            }

            // application segment
            case 0xe0 ... 0xef: {

                break;
            }

        }

    }

    fclose(fp);
    return jpeg;
}

void jpeg_image_destroy(jpeg_image_t* jpeg)
{
    for (int i = 0; i < jpeg->num_application_segments; i++) {
        free(jpeg->application_segments[i]->data);
        free(jpeg->application_segments[i]);
    }
    free(application_segments);

    for (int i = 0; i < jpeg->nscans; i++) {
        jpeg_scan_destroy(jpeg->scans[i]);
    }

    free(jpeg->numlines);
    free(jpeg);
}


static void parse_marker
