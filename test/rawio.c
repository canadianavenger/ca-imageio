#include <stdio.h>
#include <stdint.h>
#include <image.h>
#include <errno.h>
#include "rawio.h"

#define fclose_s(A) if(A) fclose(A); A=NULL
#define free_s(A) if(A) free(A); A=NULL

// strucuture definition for the file on disk
typedef struct {
    uint16_t width;       // width of image in pixels
    uint16_t height;      // height of image in pixels
    uint16_t colours;     // number of colours in the palette (size is 3x)
    int16_t  transparent; // transparent colour index
    uint16_t img_offset;  // absolute file offset of image data
    uint8_t  data[];      // raw uncompressed palette data followed by image data
} raw_image_file_t;

/// @brief saves the image pointed to by src as a BMP
/// @param fn name of the file to create and write to
/// @param src pointer to a basic_image_t structure containing the image
/// @return 0 on success, otherwise an error code
int save_raw(const char *fn, pal_image_t *img) {
    int rval = EFAULT;
    FILE *fp = NULL;

    if((NULL == img) || (NULL == fn)) return EBADF;

    if((0 == img->width) || (0 == img->height) || (0 == img->colours)) return EINVAL;

    raw_image_file_t raw;
    raw.width = img->width;
    raw.height = img->height;
    raw.colours = img->colours;
    raw.transparent = img->transparent;
    raw.img_offset = (img->colours * 3) + sizeof(raw_image_file_t);
    
    // try to open/create output file
    if(NULL == (fp = fopen(fn,"wb"))) {
        rval = errno;  // can't open/create output file
        goto CLEANUP;
    }

    // write the header
    int nw = fwrite(&raw, sizeof(raw_image_file_t), 1, fp);
    if(1 != nw) {
        rval = errno;  // can't write file
        goto CLEANUP;
    }

    // write the palette
    nw = fwrite(img->pal, sizeof(img_pal_entry_t), img->colours, fp);
    if(nw != img->colours) {
        rval = errno;  // can't write file
        goto CLEANUP;
    }

    // write the image
    nw = fwrite(img->pixels, img->width, img->height, fp);
    if(nw != img->height) {
        rval = errno;  // can't write file
        goto CLEANUP;
    }

    rval = 0;
CLEANUP:
    fclose_s(fp);
    return rval;
}

/// @brief loads the BMP image from a file
/// @param fn name of file to load
/// @return  pointer to a basic_image_t structure containing the image, or null on error (errno is set)
pal_image_t *load_raw(const char *fn) {
    int rval = EFAULT;
    FILE *fp = NULL;
    pal_image_t *img = NULL;

    if(NULL == fn) {
        rval = EBADF;
        goto CLEANUP;
    }

    // try to open/create output file
    if(NULL == (fp = fopen(fn,"rb"))) {
        rval = errno;  // can't open input file
        goto CLEANUP;
    }

    raw_image_file_t raw = {0,0,0,0,0};

    // read the header
    int nr = fread(&raw, sizeof(raw_image_file_t), 1, fp);
    if(1 != nr) {
        rval = errno;  // can't read file
        goto CLEANUP;
    }

    // only up to 256 colours supported
    if((1 > raw.colours) || (raw.colours > 256)) {
        rval = EFTYPE;
        goto CLEANUP;
    }

    // make sure the file size isn't ridiculous.
    size_t imgsz = raw.width;
    imgsz *= raw.height;
    if(imgsz > (1024 * 1024)) { // 1M would be huge for the images we are dealing with
        rval = EFBIG;
        goto CLEANUP;
    }

    if(NULL == (img = image_alloc(raw.width, raw.height, raw.colours, 0))) {
        rval = errno;
        goto CLEANUP;
    }

    img->transparent = raw.transparent;

    // read the palette
    nr = fread(img->pal, sizeof(img_pal_entry_t), img->colours, fp);
    if(nr != img->colours) {
        rval = errno;  // can't read file
        goto CLEANUP;
    }

    // read the image
    nr = fread(img->pixels, img->width, img->height, fp);
    if(nr != img->height) {
        rval = errno;  // can't read file
        goto CLEANUP;
    }

    fclose_s(fp);

    return img;
CLEANUP:
    image_free(img);
    fclose_s(fp);
    errno = rval;
    return NULL;
}
