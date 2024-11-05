/*
 * png_priv.h 
 * structure definitions for a PNG image file
 * 
 * This code is offered without warranty under the MIT License. Use it as you will 
 * personally or commercially, just give credit if you do.
 */
#include <stdint.h>
#include <image_png.h>

#ifndef CA_IMG_PNG_INTERNAL
#define CA_IMG_PNG_INTERNAL

#define fclose_s(A) if(A) fclose(A); A=NULL
#define free_s(A) if(A) free(A); A=NULL

#define PNG_SIG "PNG"
#define PNG_FULL_SIG "\x89PNG\x0d\x0a\xa1\x0a"

// these are the chunk identifiers we are intersted in
// for the images we work with They must appear in the 
// order as seen below. however other chunks may appear
// between them.
#define PNG_IHDR "IHDR" /* must be fist chunk in file */
#define PNG_PLTE "PLTE" 
#define PNG_tRNS "tRNS" /* only present if transparency used */
#define PNG_IDAT "IDAT"
#define PNG_IEND "IEND" /* must be last chunk in file */
/**
 * PNG notes
 * multi-byte values are network order AKA big endian!
 */

#pragma pack(push,1)
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} png_palette_entry_t;

/// @brief 8 byte file signature header
typedef struct {
    uint8_t highBitByte; // must be 0x89 (used to detect 7 bit transport)
    char signature[3];   // must be "PNG"
    char dos_crlf[2];    // must be 0x0d 0x0a (used to detect translations)
    char dos_eof;        // must be 0x1a (dos soft EOF to stop stream when 'TYPE'd)
    char uinx_lf;        // must be 0x0a (used to detect translations)
} png_header_t;

/// @brief basic chunk structure
typedef struct {
    uint32_t length;     // length of data section of this chunk,
    char     type_id[4]; // type identifier of this chunk
    uint8_t  data[];     // data for the chunk
    // uint32_t CRC         // CRC-32 for the chunk, follows data, is not part of "length"
} png_chunk_t;

/// @brief main file header - must be first chunk in the file
typedef struct {
    uint32_t width;               //
    uint32_t height;              //
    uint8_t  bit_depth;           // bits per sample 1,2,4,8,16 (we can do 1-8)
    uint8_t  colour_type;         // must be type 3 for us (paletted colour)
    uint8_t  compression_method;  // must be 0 for any PNG
    uint8_t  filter_method;       // must be 0 for us
    uint8_t  interlace_method;    // must be 0 for us
} png_ihdr_t;

/// @brief image palette - must appear before any tRNS or IDAT chunks
///        simply an array of rgb data entries, length must be a multiple of 3
// typedef struct {
//     png_palette_entry_t pal[];
// } png_plte_t;

/// @brief image transparency - must appear after a PLTE but beefore IDAT
///        0=transparrent, 255 = opaque, only need as many values until the
///        final index that has transparency. 255 is assumed otherwise
// typedef struct {
//     uint8_t trans[];
// } png_trns_t;

/// @brief image data
// typedef struct {
//     uint8_t data[];
// } png_idat_t;


/// @brief main file footer - must be final chunk in file
///  Note: This chunk may not appear in some older files, 
///        or files made by older software.
// typedef struct {
//     // uint8_t data[]; // this field is empty for IEND
// } png_iend_t;

#pragma pack(pop)


#endif
