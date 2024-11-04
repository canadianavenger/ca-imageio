#include <stdio.h>
#include <image_bmp.h>
#include "bmp_priv.h"
#include <stdbool.h>
#include <errno.h>

/// @brief loads ONLY the image portion of a BMP file with 4bpp encoding
/// @param img pointer to an allocated basic_image_t structure large enough for the image
/// @param bmp pointer to a bmp header struct (filled in by calling code)
/// @param fp  file handle to an open file for the BMP
/// @return 0 on sucess, otherwise an error code
static int load_bmp4(pal_image_t *img, bmp_header_t *bmp, FILE *fp);

/// @brief loads ONLY the image portion of a BMP file with 8bpp encoding
/// @param img pointer to an allocated basic_image_t structure large enough for the image
/// @param bmp pointer to a bmp header struct (filled in by calling code)
/// @param fp  file handle to an open file for the BMP
/// @return 0 on sucess, otherwise an error code
static int load_bmp8(pal_image_t *img, bmp_header_t *bmp, FILE *fp);

/// @brief loads a Windows BMP file into  memory. Must be a uncompressed palletted
///        4 bit per pixel or 8 bit per pixel image
/// @param fn pointer to the filename of the BMP to read
/// @return pointer to a pal_image_t structure containing the image, or null on error with errno set/
pal_image_t *load_bmp(const char *fn) {
    int rval = 0;
    FILE *fp = NULL;
    bmp_header_t *bmp = NULL;
    pal_image_t *img = NULL;
    bmp_palette_entry_t *pal = NULL;

    // do some basic error checking on the inputs
    if(NULL == fn) {
        rval = BMP_NULL_POINTER;
        goto bmp_cleanup;
    }

    // try to open input file
    if(NULL == (fp = fopen(fn,"rb"))) {
        rval = errno;  // can't open input file
        goto bmp_cleanup;
    }

    bmp_signature_t sig = 0;
    int nr = fread(&sig, sizeof(bmp_signature_t), 1, fp);
    if(1 != nr) {
        rval = errno;  // unable to read file
        goto bmp_cleanup;
    }
    if(BMPFILESIG != sig) { // not a BMP file
        rval = BMP_INVALID;
        goto bmp_cleanup;
    }

    // allocate a buffer to hold the header 
    if(NULL == (bmp = calloc(1, sizeof(bmp_header_t)))) {
        rval = errno;  // unable to allocate mem
        goto bmp_cleanup;
    }
    nr = fread(bmp, sizeof(bmp_header_t), 1, fp);
    if(1 != nr) {
        rval = errno;  // unable to read file
        goto bmp_cleanup;
    }

    // check some basic header vitals to make sure it's in a format we can work with
    if((1 != bmp->bmi.num_planes) || 
       (sizeof(bmi_header_t) != bmp->bmi.header_size) || 
       (0 != bmp->dib.RES)) {  // invalid header
        rval = BMP_INVALID;
        goto bmp_cleanup;
    }

    // basic checking for supported BMP formats
    if((0 != bmp->bmi.compression) || (1 != bmp->bmi.num_planes)) { // we only support uncompressed single plane images
        rval = BMP_UNSUPPORTED;
        goto bmp_cleanup;
    }

    if(((4 != bmp->bmi.bits_per_pixel) && 
       (8 != bmp->bmi.bits_per_pixel)) ||
       (0 == bmp->bmi.num_colors)) { // we only support 4 and 8 BPP paletted images
        rval = BMP_UNSUPPORTED;
        goto bmp_cleanup;
    }

    // allocate image struct here
    if(NULL == (img = image_alloc(bmp->bmi.image_width, bmp->bmi.image_height, (1 << bmp->bmi.bits_per_pixel)) )) {
        rval = errno;
        goto bmp_cleanup;
    }

    // load palette here
    if(NULL == (pal = calloc(bmp->bmi.num_colors, sizeof(bmp_palette_entry_t)))) {
        rval = errno;  // unable to allocate mem
        goto bmp_cleanup;
    }

    // read the palette from the file
    nr = fread(pal, sizeof(bmp_palette_entry_t), bmp->bmi.num_colors, fp);
    if(bmp->bmi.num_colors != nr) {
        rval = errno;  // can't read file
        goto bmp_cleanup;
    }

    // copy the  BMP BGRA palette to the external RGB palette
    for(int i = 0; i < bmp->bmi.num_colors; i++) {
        img->pal[i].r = pal[i].r;
        img->pal[i].g = pal[i].g;
        img->pal[i].b = pal[i].b;
    }

    // load in the image data here
    rval = BMP_UNSUPPORTED;
    if(4 == bmp->bmi.bits_per_pixel) rval = load_bmp4(img, bmp, fp);
    if(8 == bmp->bmi.bits_per_pixel) rval = load_bmp8(img, bmp, fp);
    if(BMP_NOERROR != rval) goto bmp_cleanup;


    fclose_s(fp);
    free_s(pal);
    free_s(bmp);
    return img;
bmp_cleanup:
    fclose_s(fp);
    free_s(pal);
    free_s(bmp);
    image_free(img);
    errno = rval;
    return NULL;
}

static int load_bmp4(pal_image_t *img, bmp_header_t *bmp, FILE *fp) {
    int rval = BMP_NOERROR;
    uint8_t *buf = NULL; // line buffer

    // do some basic error checking on the inputs
    if((NULL == fp) || (NULL == img) || (NULL == bmp)) {
        rval = BMP_NULL_POINTER;  // NULL pointer error
        goto bmp_cleanup;
    }

    // if height is negative, flip the render order
    bool flip = (bmp->bmi.image_height < 0); 
    bmp->bmi.image_height = abs(bmp->bmi.image_height);

    uint16_t lw = bmp->bmi.image_width;
    uint16_t lh = bmp->bmi.image_height;

    // stride is the bytes per line in the BMP file, which are padded to 32 bit boundary
    // we get 2 pixels per byte for being 16 colour
    uint32_t stride = ((lw + 3) & (~0x0003)) / 2; 

    // allocate our line buffer
    if(NULL == (buf = calloc(1, stride))) {
        rval = errno;  // unable to allocate mem
        goto bmp_cleanup;
    }

    // seek to the start of the image data
    fseek(fp, bmp->dib.image_offset, SEEK_SET);

    // now we need to read the image scanlines. 
    // start by pointing to start of last line of data
    size_t img_len = lw * lh;
    uint8_t *px = &img->data[img_len - lw]; 
    if(flip) px = img->data; // if flipped, start at beginning
    // loop through the lines
    for(int y = 0; y < lh; y++) {
        int nr = fread(buf, stride, 1, fp); // read a line
        if(1 != nr) {
            rval = errno;  // unable to read file
            goto bmp_cleanup;
        }

        // loop through all the pixels for a line
        // we are packing 2 pixels per byte, so width is half
        for(int x = 0; x < ((lw + 1) / 2); x++) {
            uint8_t sp = buf[x];      // get the pixel pair
            *px++ = (sp >> 4) & 0x0f; // write the 1st pixel
            if((x * 2 + 1) < lw) {    // test for odd pixel end
                *px++ = sp & 0x0f;    // write the 2nd pixel
            }
        }
        if(!flip) { // if not flipped, we have to walk backwards
            px -= (lw * 2); // move back to start of previous line
        }
    }

    img->width = lw;
    img->height = lh;

bmp_cleanup:
    free_s(buf);
    return rval;
}

static int load_bmp8(pal_image_t *img, bmp_header_t *bmp, FILE *fp) {
    int rval = BMP_NOERROR;
    uint8_t *buf = NULL; // line buffer

    // do some basic error checking on the inputs
    if((NULL == fp) || (NULL == img) || (NULL == bmp)) {
        rval = BMP_NULL_POINTER;  // NULL pointer error
        goto bmp_cleanup;
    }

    // if height is negative, flip the render order
    bool flip = (bmp->bmi.image_height < 0); 
    bmp->bmi.image_height = abs(bmp->bmi.image_height);

    uint16_t lw = bmp->bmi.image_width;
    uint16_t lh = bmp->bmi.image_height;

    // stride is the bytes per line in the BMP file, which are padded to 32 bit boundary
    // we get 1 pixels per byte for being 256 colour
    uint32_t stride = ((lw + 3) & (~0x0003)); 

    // allocate our line buffer
    if(NULL == (buf = calloc(1, stride))) {
        rval = errno;  // unable to allocate mem
        goto bmp_cleanup;
    }

    // seek to the start of the image data
    fseek(fp, bmp->dib.image_offset, SEEK_SET);

    // now we need to read the image scanlines. 
    // start by pointing to start of last line of data
    size_t img_len = lw * lh;
    uint8_t *px = &img->data[img_len - lw]; 
    if(flip) px = img->data; // if flipped, start at beginning
    // loop through the lines
    for(int y = 0; y < lh; y++) {
        int nr = fread(buf, stride, 1, fp); // read a line
        if(1 != nr) {
            rval = errno;  // unable to read file
            goto bmp_cleanup;
        }

        // loop through all the pixels for a line
        for(int x = 0; x < lw; x++) {
            *px++ = buf[x];
        }
        if(!flip) { // if not flipped, we have to walk backwards
            px -= (lw * 2); // move back to start of previous line
        }
    }

    img->width = lw;
    img->height = lh;

bmp_cleanup:
    free_s(buf);
    return rval;
}