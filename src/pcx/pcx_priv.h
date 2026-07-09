/*
 * pcx_priv.h 
 * structure definitions for a Truevision TGA file
 * 
 * This code is offered without warranty under the MIT License. Use it as you will 
 * personally or commercially, just give credit if you do.
 */
#include <stdint.h>
#include <image_pcx.h>

#ifndef CA_IMG_PCX_INTERNAL
#define CA_IMG_PCX_INTERNAL

#define fclose_s(A) if(A) fclose(A); A=NULL
#define free_s(A) if(A) free(A); A=NULL

#define PCX_MAGIC 0x0A 
#define PCX_PAL_MAGIC 0x0C 

#pragma pack(push,1)

typedef struct { // 24 bit palette
    uint8_t r;
    uint8_t g;
    uint8_t b;
} pcx_rgb_palette_entry_t;

typedef enum pcx_version : uint8_t {
    PCX_V1  = 0, // (PC Paintbrush 2.5) Fixed EGA Palette info
    PCX_V2  = 2, // (PC Paintbrush 2.8) modifiable EGA Palette
    PCX_V3  = 3, // (PC Paintbrush 2.8) No palette info
    PCX_V4  = 4, // (PC Paintbrush for Windows) 
    PCX_V5  = 5  // (PC Paintbrush 3.0) 256 colour and 24bit image files supported
} pcx_version_t;

typedef enum pcx_encoding : uint8_t {
    PCX_NONE = 0, // No compression (non-standard)
    PCX_RLE  = 1  // PCX-RLE Compressed
} pcx_encoding_t;

typedef enum pcx_pal_info : uint16_t {
    PCX_PAL_LEGACY = 0, // old palette encoding method for CGA info
    PCX_PAL_COLOR  = 1, // Color/BW
    PCX_PAL_GREY   = 2  // Greyscale Palette 
} pcx_pal_info_t;

/* Typical values for plans vs pits per pixel to number of colours
 *
 *  num_planes  bits_per_pixel   Max Colours   Video Mode
 *     1              1                   2    Monochrome
 *     1              2                   4    CGA
 *     3              1                   8    EGA
 *     4              1                  16    EGA/VGA
 *     1              8                 256    MCGA
 *     1              8                 256    Greyscale
 *     3              8          16,777,216    SVGA
 * 
 *  Calculation:
 *  num_colours = (1L << (bits_per_pixel * num_planes))
 */

// 128 byte header
typedef struct {
    uint8_t magic;           // always 0x0A (technically this is "manufacturer" and 10 denotes Zsoft)
    pcx_version_t  version;  // version of PC PAintbrush that created teh image / capabilites encoded (typically 5)
    pcx_encoding_t encoding; // 1 = RLE (PCX does not support any other encodings) [unofficially 0=uncompressed, but not widely supported]
    uint8_t bits_per_pixel;  // bits per pixel per plane
    uint16_t x_start;        // starting x coordinate (typically 0)
    uint16_t y_start;        // starting y coordinate (typically 0)
    uint16_t x_end;          // ending x coordinate (xres = x_end - x_start + 1)
    uint16_t y_end;          // ending y coordinate (yres = y_end - y_start + 1)
    uint16_t horiz_dpi;      // horizontal dpi of device that created the image, or x_res if made by program [info only]
    uint16_t vert_dpi;       // vertical dpi of device that created the image, or y_res if made by program [info only]
    union {                  // 16 colour RGB palette
        uint8_t pal_bytes[48];
        pcx_rgb_palette_entry_t pal_ega[16];
    };
    uint8_t RESERVED1;       // must be 0 for max compatability
    uint8_t num_planes;      // number of bit planes
    uint16_t bytes_per_line; // number of bytes per line per bitplane (may include padding, must be even)
    pcx_pal_info_t pal_type; // 1 = colour or mono, 2 = grey scale, 0 = legacy CGA palette encoding. [info only]
    uint16_t horiz_screen;   // horizontal screen resolution for which the image was created [info only]
    uint16_t vert_screen;    // vertical screen resolution for which the image was created [info only]
    uint8_t RESERVED2[54];   // should be all 0's
} pcx_header_t;

typedef struct { // always located as last 769 bytes in file, if present. [PCX V5]
    uint8_t marker;                   // 0x0c 
    pcx_rgb_palette_entry_t pal[256]; // 256 RGB entries
} pcx_pal256_t;

#pragma pack(pop)

#endif
