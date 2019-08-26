#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
 * Reads the next segment marker into target
 *
 * returns 0 on success, and nonzero if there is no segment marker to read, or if there was an
 * error reading the segment marker.
 */
static int read_segment_marker(FILE* fp, uint8_t* marker)
{
    // read 2 bytes.
    uint8_t buf[2];
    if ((fread(buf, 2, 1, fp) != 1) || (buf[0] != 0xff)) {
        return -1;
    }

    // if both of the bytes were ff, we need to keep reading until we get a byte that's not ff or 00
    while (((buf[0] == 0xff) && (buf[1] == 0xff)) && fread(&buf[1], 1, 1, fp) != 0);

    // check and see if we failed a read.
    if (buf[1] == 0xff) {
        return -1;
    }
    if (buf[1] == 0x00) {
        return -1;
    }

    *marker = buf[1];
    return 0;
}

/**
 *
 */
static int marker_is_SOF_marker(uint8_t marker)
{
    return (((marker >= SOF_0)  && (marker <= SOF_3))  ||
            ((marker >= SOF_5)  && (marker <= SOF_7))  ||
            ((marker >= SOF_9)  && (marker <= SOF_11)) ||
            ((marker >= SOF_13) && (marker <= SOF_15)));
}

static int marker_is_APP_marker(uint8_t marker)
{
    return ((marker >= APP_0) && (marker <= APP_15));
}

static int marker_is_JPG_marker(uint8_t marker)
{
    return ((marker >= JPG_0) && (marker <= JPG_13));
}

/**
 * Returns 1 if the given marker denotes any jpeg segment other than a frame or scan segment.
 * These segments must be treated differently as they can contain other segments.
 */
static int marker_is_miscellaneous_marker(uint8_t marker)
{
    return (marker_is_SOF_marker(marker) ||
            marker_is_APP_marker(marker) ||
            marker_is_JPG_marker(marker) ||
            (marker == DHT)           ||
            (marker == JPG_EXT)       ||
            (marker == DAC)           ||
            (marker == SOI)           ||
            (marker == EOI)           ||
            (marker == DQT)           ||
            (marker == DNL)           ||
            (marker == DRI)           ||
            (marker == DHP)           ||
            (marker == EXP)           ||
            (marker == COM));
}

/**
 *
 */
static int decode_scan(uint8_t marker, FILE* fp, jpeg_scan_t* dest)
{
    int retval = -1;

    dest->jpeg_scan_header.header.segment_marker = marker;

    uint8_t Ls_buf[2];
    if (fread(Ls_buf, 2, 1, fp) != 1) {
        goto cleanup_0;
    }
    uint16_t Ls = (Ls_buf[0] << 8) | Ls_buf[1];
    dest->jpeg_scan_header.header.Ls = Ls;

    const uint16_t remaining_bytes = dest->jpeg_scan_header.header.Ls - 2;
    uint8_t* buf = calloc(1, remaining_bytes);
    if (fread(buf, remaining_bytes, 1, fp) != 1) {
        goto cleanup_0;
    }

    int idx = 0;
    dest->jpeg_scan_header.num_components = buf[idx++];
    for (int i = 0; i < dest->jpeg_scan_header.num_components; i++) {
        dest->jpeg_scan_header.csps[i].scan_component_selector = buf[idx++];
        dest->jpeg_scan_header.csps[i].dc_ac_entropy_coding_table = buf[idx++];
    }
    dest->jpeg_scan_header.selection_start = buf[idx++];
    dest->jpeg_scan_header.selection_end = buf[idx++];
    dest->jpeg_scan_header.approximation_high_approximation_low = buf[idx++];

    if (idx != remaining_bytes) {
        goto cleanup_0;
    }

    dest->num_ecs = 1;
    dest->entropy_coded_segments = calloc(1, sizeof(entropy_coded_segment_t*));

    // just dump into a buffer until we hit a non-stuffed ff byte
    // we are going to assume that our image has no RST segments.
    uint32_t ecs_capacity = 1024;
    dest->entropy_coded_segments[0] = calloc(1, sizeof(entropy_coded_segment_t));
    entropy_coded_segment_t* ecs = dest->entropy_coded_segments[0];
    ecs->data = malloc(ecs_capacity);
    bool bitstuff_just_happened = false;
    while (1) {
        if (ecs->size == ecs_capacity) {
            ecs_capacity *= 2;
            ecs->data = realloc(ecs->data, ecs_capacity);
        }
        if (fread(&ecs->data[ecs->size], 1, 1, fp) != 1) {
            goto cleanup_0;
        }
        ecs->size += 1;

        // check the last 2 bytes.
        if ((ecs->size > 2) && (ecs->data[ecs->size - 2] == 0xff)) {
            if (bitstuff_just_happened) {
                bitstuff_just_happened = false;
            } else if (ecs->data[ecs->size - 1] == 0x00) {
                // throw away the stuffed 0.
                ecs->size -= 1;
                bitstuff_just_happened = true;
            } else {
                // we've reached a segment marker and need to rewind the file 2 bytes and pass
                // control back up.
                retval = 0;
                ecs->size -= 2;
                fseek(fp, -2, SEEK_CUR);
                goto cleanup_0;
            }
        }
    }

cleanup_0:
    free(buf);

    return retval;
}

/**
 *
 */
static int decode_frame_header(uint8_t marker, FILE* fp, jpeg_frame_header_t* dest)
{
    // read in the frame header
    dest->header.segment_marker = marker;

    uint8_t Ls_buf[2];
    if (fread(Ls_buf, 2, 1, fp) != 1) {
        return -1;
    }
    dest->header.Ls = (Ls_buf[0] << 8) | Ls_buf[1];

    const uint16_t remaining_bytes = dest->header.Ls - 2;
    uint8_t* buf = calloc(1, remaining_bytes);
    if (fread(buf, remaining_bytes, 1, fp) != 1) {
        free(buf);
        return -1;
    }

    dest->sample_precision = buf[0];
    dest->number_of_lines = (buf[1] << 8) | buf[2];
    dest->samples_per_line = (buf[3] << 8) | buf[4];
    dest->number_of_image_components = buf[5];

    dest->csps = calloc(dest->number_of_image_components, sizeof(*dest->csps));
    for (int i = 0; i < dest->number_of_image_components; i++) {
        dest->csps[i].component_identifier = buf[6 + (3 * i)];
        dest->csps[i].horizontal_sampling_factor = buf[7 + (3 * i)];
        dest->csps[i].vertical_sampling_factor = buf[8 + (3 * i)];
    }

    free(buf);
    return 0;
}


static int decode_huffman_tables(uint8_t marker, FILE* fp, jpeg_image_t* jpeg)
{
    uint8_t Ls_buf[2];
    if (fread(Ls_buf, 2, 1, fp) != 1) {
        return -1;
    }
    uint16_t Ls = (Ls_buf[0] << 8) | Ls_buf[1];
    const uint16_t remaining_bytes = Ls - 2;

    uint8_t* buf = calloc(1, remaining_bytes);
    if (fread(buf, remaining_bytes, 1, fp) != 1) {
        free(buf);
        return -1;
    }

    int idx = 0;
    do {
        uint8_t tc_td = buf[idx];
        idx += 1;
        uint8_t table_class = tc_td >> 4;
        uint8_t table_dest = tc_td & 0x0f;
        jpeg_huffman_table_t* dest;
        if (table_class == 0) {
            dest = &jpeg->dc_huffman_tables[table_dest];
        } else {
            dest = &jpeg->ac_huffman_tables[table_dest];
        }

        dest->header.segment_marker = marker;
        dest->header.Ls = Ls;
        dest->tc_td = tc_td;
        memcpy(dest->number_of_codes_with_length, &buf[idx], 16);
        idx += 16;

        uint16_t huffman_sum = 0;
        for (int i = 0; i < 16; i++) {
            huffman_sum += dest->number_of_codes_with_length[i];
        }

        memcpy(dest->huffman_codes, &buf[idx], huffman_sum);
        idx += huffman_sum;
    } while (idx < remaining_bytes);

    free(buf);

    if (idx != remaining_bytes) {
        return -1;
    }
    return 0;
}


static int decode_quantization_tables(uint8_t marker, FILE* fp, jpeg_image_t* jpeg)
{
    uint8_t Ls_buf[2];
    if (fread(Ls_buf, 2, 1, fp) != 1) {
        return -1;
    }
    uint16_t Ls = (Ls_buf[0] << 8) | Ls_buf[1];
    const uint16_t remaining_bytes = Ls - 2;

    uint8_t* buf = calloc(1, remaining_bytes);
    if (fread(buf, remaining_bytes, 1, fp) != 1) {
        free(buf);
        return -1;
    }

    int idx = 0;
    do {
        uint8_t pq_tq = buf[idx];
        idx += 1;
        uint8_t table_precision = pq_tq >> 4;
        uint8_t table_dest = pq_tq & 0x0f;
        jpeg_quantization_table_t* dest = &jpeg->jpeg_quantization_tables[table_dest];

        dest->pq_tq = pq_tq;
        for (int i = 0; i < 64; i++) {
            if (table_precision == 0) {
                dest->Q[i]._8 = buf[idx];
                idx += 1;
            } else {
                dest->Q[i]._16 = (buf[idx] << 8) | (buf[idx + 1]);
                idx += 2;
            }
        }
    } while(idx < remaining_bytes);

    free(buf);
    if (idx != remaining_bytes) {
        return -1;
    }

    return 0;
}

jpeg_image_t* jpeg_image_load_from_file(const char* filename)
{
    jpeg_image_t* jpeg = NULL;

    // open the file.
    FILE* fp = fopen(filename, "rb");
    if (fp == NULL) {
        return NULL;
    }

    // read the SOI marker, just to be sure. We are assuming that there are no 0xff pads before SOI.
    uint8_t SOI_marker[2] = { 0 };
    fread(SOI_marker, 2, 1, fp);
    if ((SOI_marker[0] != 0xff) || (SOI_marker[1] != 0xd8)) {
        return NULL;
    }

    // allocate a new structure.
    jpeg = calloc(1, sizeof(jpeg_image_t));

    // handle each segment as it comes up.
    while (1) {
        uint8_t marker;
        if (read_segment_marker(fp, &marker)) {
            printf("jpeg decoding error reading marker.\n");
            goto cleanup_on_fail;
        }

        if (marker_is_SOF_marker(marker)) {
            printf("jpeg decoding trace:    decoding SOF segment.\n");
            if (decode_frame_header(marker, fp, &jpeg->frame_header)) {
                goto cleanup_on_fail;
            }
        } else if (marker == SOS) {
            printf("jpeg decoding trace:    decoding scan segment.\n");
            if (decode_scan(marker, fp, &jpeg->scan)) {
                goto cleanup_on_fail;
            }
        } else if (marker == DHT) {
            printf("jpeg decoding trace:    decoding huffman table.\n");
            if (decode_huffman_tables(marker, fp, jpeg)) {
                goto cleanup_on_fail;
            }
        } else if (marker == DQT) {
            printf("jpeg decoding trace:    decoding quantization table.\n");
            if (decode_quantization_tables(marker, fp, jpeg)) {
                goto cleanup_on_fail;
            }
        } else if (marker == EOI) {
            if (fgetc(fp) != -1) {
                printf("jpeg decoding warn :    EOI marker found but not at end-of-file.\n");
            }
            break;
        } else {
            // For all segments that this code doesn't use, we will just drop them into the
            // misc_segments array.
            jpeg_generic_segment_t* gs = calloc(1, sizeof(jpeg_generic_segment_t));

            gs->header.segment_marker = marker;

            // get the length of the segment
            uint8_t Ls_buf[2];
            if (fread(Ls_buf, 2, 1, fp) == 0) {
                free(gs);
                goto cleanup_on_fail;
            }
            gs->header.Ls = (Ls_buf[0] << 8) | Ls_buf[1];

            printf("jpeg decoding trace:    decoding misc segment with length %04x "
                   "and marker %02x.\n", gs->header.Ls, marker);

            uint16_t payload_size = gs->header.Ls - 2;
            if (payload_size > 0) {
                gs->data = malloc((gs->header.Ls - 2) * sizeof(uint8_t));
                if (fread(gs->data, payload_size, 1, fp) == 0) {
                    free(gs);
                    goto cleanup_on_fail;
                }
            }

            jpeg->num_misc_segments++;
            uint32_t bytes_to_realloc = jpeg->num_misc_segments * sizeof(jpeg_generic_segment_t*);
            jpeg->misc_segments = realloc(jpeg->misc_segments, bytes_to_realloc);
            jpeg->misc_segments[jpeg->num_misc_segments - 1] = gs;
        }
    }

    fclose(fp);
    return jpeg;

cleanup_on_fail:
    fclose(fp);
    jpeg_image_destroy(jpeg);
    return NULL;
}

void jpeg_image_destroy(jpeg_image_t* jpeg)
{
    for (int i = 0; i < jpeg->num_misc_segments; i++) {
        free(jpeg->misc_segments[i]->data);
        free(jpeg->misc_segments[i]);
    }
    free(jpeg->misc_segments);

    free(jpeg->frame_header.csps);

    free(jpeg);
}


//static void parse_marker
