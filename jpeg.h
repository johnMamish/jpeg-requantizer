/**
 * Notable differences from the standard:
 *   * Doesn't support more than 1 scan.
 *   * If duplicate Huffman tables are defined, only the ones defined last in the file are used.
 */


#ifndef MAMISH_JPEG_H
#define MAMISH_JPEG_H

#include <stdint.h>


////////////////////////////////////////////////////////////////
// miscellaneous, non-nesting jpeg segment types
////////////////////////////////////////////////////////////////
typedef struct jpeg_segment
{
    uint8_t segment_marker[2];

    // Length of the following segment
    uint16_t Ls;
} jpeg_segment_t;

/**
 * A generic jpeg segment containing arbitrary, unstructured data.
 */
typedef struct jpeg_generic_segment
{
    jpeg_segment_t header;
    uint8_t* data;
} jpeg_generic_segment_t;

////////////////////////////////////////////////////////////////
// jpeg scan segment
////////////////////////////////////////////////////////////////
typedef struct scan_component_specification
{
    uint8_t scan_component_selector;

    // Specifies one of four possible DC and AC entropy coding table destinations from which the
    // entropy table needed for decoding of the coefficients of component Csj is retrieved. DC and
    // AC coding table indicies are packed into one byte, with the DC index occupying the most-
    // significant nibble.
    uint8_t dc_ac_entropy_coding_table;
} scan_component_specification_t;

typedef struct entropy_coded_segment
{
    uint32_t size;
    uint8_t *data;
} entropy_coded_segment_t;

typedef jpeg_scan_t
{
    jpeg_segment_t header;

    uint8_t n_components;
    scan_component_specification_t scan_component_specifications[4];
}

////////////////////////////////////////////////////////////////
// jpeg frame members
////////////////////////////////////////////////////////////////
typedef struct jpeg_huffman_table
{
    jpeg_segment_t header;
    uint8_t table_class;
    uint8_t table_dest;
    uint8_t number_of_codes_with_length[16];
    uint8_t huffman_codes[256];
} jpeg_huffman_table_t;

typedef struct jpeg_quantization_table
{
    // this structure doesn't have a header, as one header can hold several quantization tables.

    // pq_tq<7:4> specify the bit-depth of samples. 0 for 8-bit, 1 for 16-bit.
    // pq_tq<3:0> specify the quantization table destination identifier.
    uint8_t pq_tq;
    union {
        uint8_t _8;
        uint16_t _16;
    } Q[64];
} jpeg_quantization_table_t;

typedef struct component_specification_parameters
{
    uint8_t component_identifier;
    uint8_t horizontal_sampling_factor;
    uint8_t vertical_sampling_factor;
} component_specification_parameters_t;

typedef struct jpeg_scan_header
{
    jpeg_segment_t header;

    // in many (most?) images, this will be 3.
    uint8_t num_components;

    component_specification_parameters_t component_specification_parameters[4];

    uint8_t selection_start;
    uint8_t selection_end;
    uint8_t approximation_high_approximation_low;
} jpeg_scan_header_t;

typedef struct jpeg_frame_header
{
    jpeg_segment_t header;

    // Precision in bits. For baseline, expected to be 8.
    uint8_t  sample_precision;

    // Essentially the height of the image in pixels.
    uint16_t number_of_lines;

    // Essentially the width of the image in pixels.
    uint16_t samples_per_line;

    // Expected to be 3 or 1; one for each color.
    uint8_t  number_of_image_components;

    // The number of component specification parameters is equal to "number_of_image_components".
    component_specification_parameters_t* component_specifications;
} jpeg_frame_header_t;

/**
 * Container for a jpeg image.
 *
 * In order to make porting to an embedded system simpler, this software only supports a single
 * scan.
 */
typedef struct jpeg_image
{
    // An array holding misc. segments.
    uint32_t num_jpeg_segments;
    jpeg_generic_segment_t** misc_segments;

    // no jpeg can have more than 4 huffman tables for either AC or DC.
    jpeg_huffman_table_t dc_huffman_tables[4];
    jpeg_huffman_table_t ac_huffman_tables[4];
    jpeg_quantization_table_t jpeg_quantization_tables[4];

    jpeg_frame_header_t frame_header;

    // This software only supports a single scan
    jpeg_scan_header_t scan;

    uint32_t num_ecs;
    entropy_coded_segment_t** entropy_coded_segments;
} jpeg_image_t;

/**
 * Given a file path, decodes the jpeg's parts into a newly allocated jpeg_t struct.
 *
 * This does not do any huffman, RLE, or ifft decoding, just loads the file.
 *
 * If there's an error while loading the file, returns NULL.
 */
jpeg_image_t* jpeg_image_load_from_file(const char* file);

void jpeg_image_destroy(jpeg_image_t* jpeg_image);

#endif