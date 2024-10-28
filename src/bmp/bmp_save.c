#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bmp_priv.h"
#include <bmp.h>
//#include "util.h"
#include <stdbool.h>
#include <errno.h>

// allocate a header buffer large enough for all 3 parts, plus 16 bit padding at the start to 
// maintian 32 bit alignment after the 16 bit signature.
#define HDRBUFSZ (sizeof(bmp_signature_t) + sizeof(bmp_header_t))

/// @brief saves an image as a 4 bit or 8 bit Windows BMP image
/// @param fn pointer to the name of the file to save the image as
/// @param img pointer to the quick_image_t structure containing the image
/// @return 0 on success otherwise an error value
int save_bmp(const char *fn, quick_image_t *img) {
    if((NULL == img) || (NULL == fn)) return BMP_NULL_POINTER;

    if((0 == img->width) || (0 == img->height)) return BMP_INVALID;

    if(16 == img->colours) return save_bmp4(fn, img);
    if(256 == img->colours) return save_bmp8(fn, img);
    return BMP_INVALID;
}

/// @brief loads a Windows BMP file into  memory. Must be a uncompressed palletted
///        4 bit per pixel or 8 bit per pixel image
/// @param fn pointer ot the filename of the BMP to read
/// @return pointer to a quick_image_t structure containing the image, or null on error with errno set/
quick_image_t *load_bmp(const char *fn) {
    int rval = 0;
    FILE *fp = NULL;
    bmp_header_t *bmp = NULL;
    quick_image_t *img = NULL;
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
    img->width = bmp->bmi.image_width; 
    img->height = abs(bmp->bmi.image_height);       // height can be specified as a negative value
    img->colours = (1 << bmp->bmi.bits_per_pixel);  // we use the max colours as the number
    img->palette_offset = img->width * img->height; // palette starts after the image

    // load palette here
    // do palette read here if xpal is ! null
    if(NULL == (pal = calloc(bmp->bmi.num_colors, sizeof(bmp_palette_entry_t)))) {
        rval = errno;  // unable to allocate mem
        goto bmp_cleanup;
    }
    rgb_pal_entry_t *xpal = (rgb_pal_entry_t *)&img->data[img->palette_offset];

    // read the palette from the file
    nr = fread(pal, sizeof(bmp_palette_entry_t), bmp->bmi.num_colors, fp);
    if(bmp->bmi.num_colors != nr) {
        rval = errno;  // can't read file
        goto bmp_cleanup;
    }

    // copy the  BMP BGRA palette to the external RGB palette
    for(int i = 0; i < bmp->bmi.num_colors; i++) {
        xpal[i].r = pal[i].r;
        xpal[i].g = pal[i].g;
        xpal[i].b = pal[i].b;
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

int save_bmp8(const char *fn, quick_image_t *img) {
    int rval = 0;
    FILE *fp = NULL;
    uint8_t *buf = NULL; // line buffer, also holds header info

    // do some basic error checking on the inputs
    if((NULL == fn) || (NULL == img)) {
        rval = BMP_NULL_POINTER;
        goto bmp_cleanup;
    }

    // try to open/create output file
    if(NULL == (fp = fopen(fn,"wb"))) {
        rval = errno;  // can't open/create output file
        goto bmp_cleanup;
    }

    // stride is the bytes per line in the BMP file, which are padded
    // out to 32 bit boundaries
    uint32_t stride = ((img->width + 3) & (~0x0003)); 
    uint32_t bmp_img_sz = (stride) * img->height;

    // allocate a buffer to hold the header and a single scanline of data
    // this could be optimized if necessary to only allocate the larger of
    // the line buffer, or the header + padding as they are used at mutually
    // exclusive times
    if(NULL == (buf = calloc(1, HDRBUFSZ + stride + 2))) {
        rval = errno;  // unable to allocate mem
        goto bmp_cleanup;
    }

    // signature starts after padding to maintain 32bit alignment for the rest of the header
    bmp_signature_t *sig = (bmp_signature_t *)&buf[stride + 2];

    // bmp header starts after signature
    bmp_header_t *bmp = (bmp_header_t *)&buf[stride + 2 + sizeof(bmp_signature_t)];

    // setup the signature and DIB header fields
    *sig = BMPFILESIG;
    size_t palsz = sizeof(bmp_palette_entry_t) * 256;
    bmp->dib.image_offset = HDRBUFSZ + palsz;
    bmp->dib.file_size = bmp->dib.image_offset + bmp_img_sz;

    // setup the bmi header fields
    bmp->bmi.header_size = sizeof(bmi_header_t);
    bmp->bmi.image_width = img->width;
    bmp->bmi.image_height = img->height;
    bmp->bmi.num_planes = 1;           // always 1
    bmp->bmi.bits_per_pixel = 8;       // 256 colour image
    bmp->bmi.compression = 0;          // uncompressed
    bmp->bmi.bitmap_size = bmp_img_sz;
    bmp->bmi.horiz_res = BMP96DPI;
    bmp->bmi.vert_res = BMP96DPI;
    bmp->bmi.num_colors = 256;         // palette has 256 colours
    bmp->bmi.important_colors = 0;     // all colours are important

    // write out the header
    int nr = fwrite(sig, HDRBUFSZ, 1, fp);
    if(1 != nr) {
        rval = errno;  // unable to write file
        goto bmp_cleanup;
    }

    bmp_palette_entry_t *pal = calloc(256, sizeof(bmp_palette_entry_t));
    if(NULL == pal) {
        rval = errno;  // unable to allocate mem
        goto bmp_cleanup;
    }

    // copy the external RGB palette to the BMP BGRA palette
    rgb_pal_entry_t *xpal = (rgb_pal_entry_t *)&img->data[img->palette_offset];
    for(int i = 0; i < 256; i++) {
        pal[i].r = xpal[i].r;
        pal[i].g = xpal[i].g;
        pal[i].b = xpal[i].b;
    }

    // write out the palette
    nr = fwrite(pal, palsz, 1, fp);
    if(1 != nr) {
        rval = errno;  // can't write file
        goto bmp_cleanup;
    }
    // we can free the BMP palette now as we don't need it anymore
    free(pal);

    // now we need to output the image scanlines. For maximum
    // compatibility we do so in the natural order for BMP
    // which is from bottom to top. 
    // start by pointing to start of last line of data
    size_t img_len = img->width * img->height;
    uint8_t *px = &img->data[img_len - img->width];
    // loop through the lines
    for(int y = 0; y < img->height; y++) {
        memset(buf, 0, stride); // zero out the line in the output buffer
        // loop through all the pixels for a line
        for(int x = 0; x < img->width; x++) {
            buf[x] = *px++;
        }
        nr = fwrite(buf, stride, 1, fp); // write out the line
        if(1 != nr) {
            rval = errno;  // unable to write file
            goto bmp_cleanup;
        }
        px -= (img->width * 2); // move back to start of previous line
    }

bmp_cleanup:
    fclose_s(fp);
    free_s(buf);
    return rval;
}

int save_bmp4(const char *fn, quick_image_t *img) {
    int rval = 0;
    FILE *fp = NULL;
    uint8_t *buf = NULL; // line buffer, also holds header info

    // do some basic error checking on the inputs
    if((NULL == fn) || (NULL == img)) {
        rval = BMP_NULL_POINTER;
        goto bmp_cleanup;
    }

    // try to open/create output file
    if(NULL == (fp = fopen(fn,"wb"))) {
        rval = errno;  // can't open/create output file
        goto bmp_cleanup;
    }

    // stride is the bytes per line in the BMP file, which are padded
    // out to 32 bit boundaries
    uint32_t stride = ((((img->width + 1) / 2) + 3) & (~0x0003)); // we get 2 pixels per byte for being 16 colour
    uint32_t bmp_img_sz = (stride) * img->height;

    // allocate a buffer to hold the header and a single scanline of data
    // this could be optimized if necessary to only allocate the larger of
    // the line buffer, or the header + padding as they are used at mutually
    // exclusive times
    if(NULL == (buf = calloc(1, HDRBUFSZ + stride + 2))) {
        rval = errno;  // unable to allocate mem
        goto bmp_cleanup;
    }

    // signature starts after padding to maintain 32bit alignment for the rest of the header
    bmp_signature_t *sig = (bmp_signature_t *)&buf[stride + 2];

    // bmp header starts after signature
    bmp_header_t *bmp = (bmp_header_t *)&buf[stride + 2 + sizeof(bmp_signature_t)];

    // setup the signature and DIB header fields
    *sig = BMPFILESIG;
    size_t palsz = sizeof(bmp_palette_entry_t) * 16;
    bmp->dib.image_offset = HDRBUFSZ + palsz;
    bmp->dib.file_size = bmp->dib.image_offset + bmp_img_sz;

    // setup the bmi header fields
    bmp->bmi.header_size = sizeof(bmi_header_t);
    bmp->bmi.image_width = img->width;
    bmp->bmi.image_height = img->height;
    bmp->bmi.num_planes = 1;           // always 1
    bmp->bmi.bits_per_pixel = 4;       // 16 colour image
    bmp->bmi.compression = 0;          // uncompressed
    bmp->bmi.bitmap_size = bmp_img_sz;
    bmp->bmi.horiz_res = BMP96DPI;
    bmp->bmi.vert_res = BMP96DPI;
    bmp->bmi.num_colors = 16;          // palette has 16 colours
    bmp->bmi.important_colors = 0;     // all colours are important

    // write out the header
    int nr = fwrite(sig, HDRBUFSZ, 1, fp);
    if(1 != nr) {
        rval = errno;  // unable to write file
        goto bmp_cleanup;
    }

    bmp_palette_entry_t *pal = calloc(16, sizeof(bmp_palette_entry_t));
    if(NULL == pal) {
        rval = errno;  // unable to allocate mem
        goto bmp_cleanup;
    }

    // copy the external RGB palette to the BMP BGRA palette
    rgb_pal_entry_t *xpal = (rgb_pal_entry_t *)&img->data[img->palette_offset];
    for(int i = 0; i < 16; i++) {
        pal[i].r = xpal[i].r;
        pal[i].g = xpal[i].g;
        pal[i].b = xpal[i].b;
    }

    // write out the palette
    nr = fwrite(pal, palsz, 1, fp);
    if(1 != nr) {
        rval = errno;  // can't write file
        goto bmp_cleanup;
    }
    // we can free the palette now as we don't need it anymore
    free(pal);

    // now we need to output the image scanlines. For maximum
    // compatibility we do so in the natural order for BMP
    // which is from bottom to top. For 16 colour/4 bit image
    // the pixels are packed two per byte, left most pixel in
    // the most significant nibble.
    // start by pointing to start of last line of data
    size_t img_len = img->width*img->height;
    uint8_t *px = &img->data[img_len - img->width];
    // loop through the lines
    for(int y = 0; y < img->height; y++) {
        memset(buf, 0, stride); // zero out the line in the output buffer
        // loop through all the pixels for a line
        // we are packing 2 pixels per byte, so width is half
        for(int x = 0; x < ((img->width + 1) / 2); x++) {
            uint8_t sp = *px++;          // get the first pixel
            sp <<= 4;                    // shift to make room
            if((x * 2 + 1) < img->width) {    // test for odd pixel end
                sp |= (*px++) & 0x0f;    // get the next pixel
            }
            buf[x] = sp;                 // write it to the line buffer
        }
        nr = fwrite(buf, stride, 1, fp); // write out the line
        if(1 != nr) {
            rval = errno;  // unable to write file
            goto bmp_cleanup;
        }
        px -= (img->width * 2); // move back to start of previous line
    }

bmp_cleanup:
    fclose_s(fp);
    free_s(buf);
    return rval;
}


int load_bmp4(quick_image_t *img, bmp_header_t *bmp, FILE *fp) {
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

int load_bmp8(quick_image_t *img, bmp_header_t *bmp, FILE *fp) {
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