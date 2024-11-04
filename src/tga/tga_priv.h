/*
 * tga_priv.h 
 * structure definitions for a Truevision TGA file
 * 
 * This code is offered without warranty under the MIT License. Use it as you will 
 * personally or commercially, just give credit if you do.
 */
#include <stdint.h>
#include <image_tga.h>

#ifndef CA_IMG_TGA_INTERNAL
#define CA_IMG_TGA_INTERNAL

#define fclose_s(A) if(A) fclose(A); A=NULL
#define free_s(A) if(A) free(A); A=NULL

/**
 * TGA Notes:
 * Multi-byte values are stored in little-endian order
 * meaning that despite the spec showing colours as ARGB
 * in the file they would apepar as BGRA
 * 
 * We only support TGA V2 [TGA with footer+sig)] as that is the most likely case
 * these days, though all we use from the footer is the identifying signature.
 * this signature check could be bypassed if we find we need to.
 * 
 * MacOS Note: Preview app (and tuumbnails) on MacOS appears to have a bug
 * where it interprets colour data as RGBA (file) ABGR (32bit)
 */

#define TGA_HAS_CMAP (1)
#define TGA_NO_CMAP  (0)

enum tga_image_type {
                          // PAL RLE
    TGA_NO_IMAGE   = 0,   //  N   N  file has no image data
    TGA_PALETTED   = 1,   //  Y   x  colour mapped image data
    TGA_TRUECOLOUR = 2,   //  N   x  Truecolour (direct colour) image data
    TGA_MONOCHROME = 3,   //  N   x  Monochrome image data
    TGA_COMPRESSED = 0x08 //  x   Y  flag indicating image data is RLE compressed
};

#define TGA_SIG "TRUEVISION-XFILE." // "version 2" signatire

#pragma pack(push,1)

typedef struct { // 24 bit palette
    uint8_t b;
    uint8_t g;
    uint8_t r;
} tga_rgb_palette_entry_t;

typedef struct { // 32 bit palette
    uint8_t b;
    uint8_t g;
    uint8_t r;
    uint8_t a;
} tga_argb_palette_entry_t;

typedef union {
    tga_rgb_palette_entry_t rgb;
    tga_argb_palette_entry_t argb;
} tga_palette_entry_t;

typedef struct {
    uint16_t colour_map_start;  // offset of first cmap entry in the palette
    uint16_t colour_map_length; // number of cmap entries
    uint8_t  colour_map_depth;  // bit width of each cmap entry (15,16,24,32)
} tga_cmap_info;

typedef struct {
    uint16_t x_offset;          // x origin point of image (typically bot-left, see image_descriptor)
    uint16_t y_offset;          // y origin point of image (typically bot-left, see image_descriptor)
    uint16_t width;             // image width in pixels
    uint16_t height;            // image height in pixels
    uint8_t  pixel_depth;       // bits per pixel (typically 8,16,24,32)
    uint8_t  image_descriptor;  // bits 0-3 indicate number of attribute bits (alpha) per pixel. 
                                // bit 4 X anchor 0 = left, 1 = right (always 0)
                                // bit 5 Y anchor 0 = bottom, 1 = top (typically 0)
                                // bits 6 & 7 are reserved, and should be 0
                                // bits 6 & 7 may indicate data interleave
                                // 00 = none, 01 = even/odd, 10 = 4-way, 11, reserved
} tga_image_info;

typedef struct {
    uint8_t  id_length;         // length if image id data that follows the header
    uint8_t  colour_map_type;   // 1 = contains palette, 0 = no palette
    uint8_t  image_type;        // see tga_image_type
    tga_cmap_info cmap;         // colour map (palette) specifications
    tga_image_info image;       // image specifications
    uint8_t  id[];              // length defined by id_length
} tga_header_t;

// V2 extended header (aka footer, placed at end of file)
typedef struct {
    uint32_t extension_offset; // can be 0 for not present
    uint32_t developer_offset; // can be 0 for not present
    char sig[18];
} tga_footer_t;

// cmap and image data entries are byte aligned, so if boundary falls on a non-byte boundary, remaining bits are
// assumed to be attribute bits

#pragma pack(pop)

#endif
