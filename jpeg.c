#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bit_dispenser.h"
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
    dest->num_components = buf[5];

    dest->csps = calloc(dest->num_components, sizeof(*dest->csps));
    for (int i = 0; i < dest->num_components; i++) {
        dest->csps[i].component_identifier = buf[6 + (3 * i)];
        dest->csps[i].horizontal_sampling_factor = buf[7 + (3 * i)] >> 4;
        dest->csps[i].vertical_sampling_factor = buf[7 + (3 * i)] & 0x0f;
        dest->csps[i].quantization_table_selector = buf[8 + (3 * i)];
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

/**
 * Allocates a new huffman_decoded_jpeg_scan_t with appropriately sized mcu tables given the width,
 * height, and sampling factors of the given jpeg.
 */
static huffman_decoded_jpeg_scan_t* huffman_decoded_jpeg_scan_create(const jpeg_image_t* jpeg)
{
    // check and make sure that the number of components is compliant with our system
    if ((jpeg->frame_header.num_components != 3) &&
        (jpeg->frame_header.num_components != 1)) {
        return NULL;
    }

    huffman_decoded_jpeg_scan_t* result = calloc(1, sizeof(huffman_decoded_jpeg_scan_t));

    for (int i = 0; i < jpeg->frame_header.num_components; i++) {
        if (jpeg->frame_header.csps[i].horizontal_sampling_factor > result->H_max) {
            result->H_max = jpeg->frame_header.csps[i].horizontal_sampling_factor;
        }
        if (jpeg->frame_header.csps[i].vertical_sampling_factor > result->V_max) {
            result->V_max = jpeg->frame_header.csps[i].vertical_sampling_factor;
        }
    }

    for (int i = 0; i < jpeg->frame_header.num_components; i++) {
        // NB: this handles the ceiling operation
        const int component_x = (jpeg->frame_header.csps[i].horizontal_sampling_factor *
                                 jpeg->frame_header.samples_per_line) / result->H_max;
        const int component_y = (jpeg->frame_header.csps[i].vertical_sampling_factor *
                                 jpeg->frame_header.number_of_lines) / result->V_max;

        // calculate number of BLOCKs
        const int blocks_x   = (component_x + (8 - 1)) / 8;
        const int blocks_y   = (component_y + (8 - 1)) / 8;
        result->components[i].num_blocks = blocks_x * blocks_y;
        result->components[i].blocks     = calloc(result->components[i].num_blocks,
                                                  sizeof(jpeg_block_t));
    }

    return result;
}


/**
 * The values coded into the file need to be converted as described in tables F.1 and F.2 of T.81.
 */
static int16_t coded_value_to_coefficient_value(uint16_t coded_value, int bitlen)
{
    int16_t result = 0;
    if (bitlen == 0) {
        return 0;
    }

    // "split" between negative and positive values happens depending on msb.
    uint16_t msb = (coded_value & (1 << (bitlen - 1)));

    // once we have the msb, we should delete it.
    coded_value &= (1 << (bitlen - 1)) - 1;

    if (msb) {
        int16_t basis = (1 << (bitlen - 1));
        result = basis + coded_value;
    } else {
        int16_t basis = 1 - (1 << bitlen);
        result = basis + coded_value;
    }

    return result;
}



/**
 * Valid return values are in the range [0, 255]. A return value of -1 signifies a decoding error.
 */
static int decode_one_huffman(const jpeg_huffman_table_t* htable, bit_dispenser_t* bd)
{
    // dc block decode
    uint16_t huff_reg = 0;
    int num_bits_shifted = 0;
    uint16_t min_exempt_value = 0;
    uint16_t cumsum = 0;
    int huffman_decode = -1;

    while ((num_bits_shifted < 16) && (huffman_decode == -1) && (!bit_dispenser_empty(bd))) {
        // shift in new bit.
        bit_dispenser_dispense_u16(&huff_reg, 1, bd);

        // update tree tracking
        min_exempt_value <<= 1;
        min_exempt_value += htable->number_of_codes_with_length[num_bits_shifted];
        cumsum += htable->number_of_codes_with_length[num_bits_shifted];

        num_bits_shifted++;

        // check
        if (huff_reg < min_exempt_value) {
            int dist_from_end = min_exempt_value - huff_reg;
            huffman_decode = htable->huffman_codes[cumsum - dist_from_end];
        }
    }

    return huffman_decode;
}


/**
 * NB: this assumes that components are in the same order in the scan as they are in the
 * frame header, which is probably true most of the time.
 */
huffman_decoded_jpeg_scan_t* jpeg_image_huffman_decode(const jpeg_image_t* jpeg)
{
    // allocate new structure
    huffman_decoded_jpeg_scan_t* result = huffman_decoded_jpeg_scan_create(jpeg);

    //
    bit_dispenser_t* bd = bit_dispenser_create(jpeg->scan.entropy_coded_segments[0]->data,
                                               jpeg->scan.entropy_coded_segments[0]->size);
    uint16_t huff_reg = 0;

    int num_mcus = result->H_max * result->V_max;
    for (int i = 0; i < num_mcus; i++) {
        printf("jpeg decoding trace:    decoding MCU %i.\n", i);
        for (int j = 0; j < jpeg->frame_header.num_components; j++) {
            printf("jpeg decoding trace:    decoding component %i of MCU %i.\n", j, i);
            int blocks_per_mcu = (jpeg->frame_header.csps[j].horizontal_sampling_factor *
                                  jpeg->frame_header.csps[j].vertical_sampling_factor);
            int block_idx      = blocks_per_mcu * i;

            // TODO find right value for '?'
            uint8_t huff_tables = jpeg->scan.jpeg_scan_header.csps[j].dc_ac_entropy_coding_table;
            int dc_huff_idx = (huff_tables >> 4) & 0x0f;
            int ac_huff_idx = (huff_tables >> 0) & 0x0f;
            const jpeg_huffman_table_t* dc_huff_table = &(jpeg->dc_huffman_tables[dc_huff_idx]);
            const jpeg_huffman_table_t* ac_huff_table = &(jpeg->ac_huffman_tables[ac_huff_idx]);

            // huffman decode!
            for (int k = 0; k < blocks_per_mcu; k++) {
                printf("jpeg decoding trace:    decoding block %i of component %i of MCU %i.\n",
                       k, j, i);
                jpeg_block_t* target_block = &result->components[j].blocks[block_idx + k];

                // DC value
                int dc_raw_length = decode_one_huffman(dc_huff_table, bd);
                if (dc_raw_length == -1) {
                    printf("jpeg decoding error:    error decoding DC huffman value.\n");
                    goto fail_cleanup;
                }

                // special EOB case
                if (dc_raw_length == 0) {
                    printf("jpeg decoding trace:    DC EOB reached.\n");
                } else {
                    printf("jpeg decoding trace:    Decoding %i bits for DC value.\n", dc_raw_length);
                    uint16_t dc_raw_value = 0;
                    bit_dispenser_dispense_u16(&dc_raw_value, dc_raw_length, bd);

                    // TODO: DC differential decoding.
                    target_block->dc_value = coded_value_to_coefficient_value(dc_raw_value,
                                                                              dc_raw_length);
                    printf("jpeg decoding trace:    Value %04x (length %i) decoded to %i.\n",
                           dc_raw_value, dc_raw_length, target_block->dc_value);
                }

                // ac block decode
                int ac_values_decoded = 0;
                while (ac_values_decoded < 63) {
                    // read in RRRRSSSS byte as described in section F.1.2.2.1 of T.81.
                    int ac_huffman_decode = decode_one_huffman(ac_huff_table, bd);
                    if (ac_huffman_decode == -1) {
                        printf("jpeg decoding error:    error decoding AC huffman value.\n");
                        goto fail_cleanup;
                    }
                    uint8_t rrrrssss = (uint8_t)ac_huffman_decode;

                    // special EOB case
                    if (rrrrssss == 0x00) {
                        printf("jpeg decoding trace:    AC EOB reached.\n");
                        break;
                    }

                    uint8_t zeros_before_next_coeff = (rrrrssss >> 4) & 0x0f;
                    printf("jpeg decoding trace:    %i RLE'd zeros in AC table.\n",
                           zeros_before_next_coeff);

                    for (int i = 0; i < zeros_before_next_coeff; i++) {
                        target_block->ac_values[ac_values_decoded] = 0;
                        ac_values_decoded += 1;
                    }

                    // read next AC coefficient
                    uint8_t ac_coefficient_len = (rrrrssss >> 0) & 0x0f;
                    printf("jpeg decoding trace:    Decoding %i bits.\n", ac_coefficient_len);
                    uint16_t ac_raw_value = 0;
                    bit_dispenser_dispense_u16(&ac_raw_value, ac_coefficient_len, bd);
                    int ac_val = coded_value_to_coefficient_value(ac_raw_value, ac_coefficient_len);
                    target_block->ac_values[ac_values_decoded] = ac_val;
                    printf("jpeg decoding trace:    Value %04x (length %i) decoded to %i.\n",
                           ac_raw_value, ac_coefficient_len, ac_val);
                }
            }
        }
    }

    return result;

fail_cleanup:
    huffman_decoded_jpeg_scan_destroy(result);
    bit_dispenser_destroy(bd);
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

void huffman_decoded_jpeg_scan_destroy(huffman_decoded_jpeg_scan_t* decoded_scan)
{
    for (int i = 0; i < 3; i++) {
        free(decoded_scan->components[i].blocks);
    }
    free(decoded_scan);
}

//static void parse_marker
