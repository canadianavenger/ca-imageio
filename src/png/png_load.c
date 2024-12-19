#include <stdio.h>
#include <image_png.h>
#include "png_priv.h"
#include <stdbool.h>
#include <errno.h>
#include <png.h>

#define PNG_BPP (1)
pal_image_t *read_png(FILE *fp);

pal_image_t *load_png(const char *fn) {
    int rval = 0;
    pal_image_t *img = NULL;
    FILE *fp = NULL;

    if(NULL == fn) {
        errno = EBADF;
        return NULL;
    }

    // try to open input file
    if(NULL == (fp = fopen(fn,"rb"))) {
        return NULL;  // can't open input file
    }

    img = read_png(fp);
    if(NULL == img) {
        rval = errno;
        goto CLEANUP;
    }

    fclose_s(fp);
    return img;
CLEANUP:
    fclose_s(fp);
    errno = rval;
    return NULL;
}

pal_image_t *read_png(FILE *fp) {
    int rval = 0;
    pal_image_t *img = NULL;
    png_structp png = NULL;
    png_infop info = NULL;
    png_bytep *row_pointers = NULL;

    if(NULL == (png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL))) {
        errno = ENOMEM; 
        return NULL;
    }

    if(NULL == (info = png_create_info_struct(png))) {
        rval = ENOMEM;
        goto CLEANUP;
    }

    // we didn't register callbacks, so we need to set up a point for
    // for longjmp to land on error.
    if (setjmp(png_jmpbuf(png))) {
        rval = EFAULT;
        goto CLEANUP;
    }

    png_init_io(png, fp);
    png_read_info(png, info);

    if(PNG_COLOR_TYPE_PALETTE != png_get_color_type(png, info)) {
        rval = ENOTSUP;
        goto CLEANUP;
    }

    if(8 != png_get_bit_depth(png, info)) {
        rval = ENOTSUP;
        goto CLEANUP;
    }

    size_t height = png_get_image_height(png, info);
    size_t width = png_get_image_width(png, info);

    // allocate an image buffer, assuming a full 256 colour palette
    if(NULL == (img = image_alloc(width, height, 256, 0))) {
        rval = errno;
        goto CLEANUP;
    }

    int colours = 0;
    png_colorp palette = NULL;
    png_get_PLTE(png, info, &palette, &colours);

    for(int i = 0; i < colours; i++) {
        img->pal[i].r = palette[i].red;
        img->pal[i].g = palette[i].green;
        img->pal[i].b = palette[i].blue;
    }
    img->colours = colours; // update the colours value to reflect valid entries

    // process tRNS here when transparency support is added to image
    png_bytep trans = NULL;
    png_color_16p pngcol = NULL;
    int tcols = 0;
    png_uint_32 res = png_get_tRNS(png, info, &trans, &tcols, &pngcol);
    if(PNG_INFO_tRNS == res) {
        // scan and find FIRST transparent colour
        // must be fully transparent
        // blending/partial transparency, and multiple transparency
        // are not supported
        for(int i = 0; i < tcols; i++) {
            if(0 == trans[i]) {
                img->transparent = i;
                break;
            }
        }
    }

    if(NULL == (row_pointers = (png_bytep *)png_calloc(png, height * sizeof(png_bytep)))) {
        rval = ENOMEM;
        goto CLEANUP;
    }

    // point the row pointers to the line starts within the image buffer
    for(unsigned i = 0; i < height; i++) {
        row_pointers[i] = (png_bytep)(img->pixels + (i * width * PNG_BPP));
    }

    // read in the image data
    png_read_image(png, row_pointers);

    png_read_end(png, NULL);

    png_free(png, row_pointers); // free the row pointers
    png_destroy_read_struct(&png, &info, NULL); // free the read and info contexts
    return img;
CLEANUP:
    image_free(img);
    if(NULL != png) {
        if(NULL != row_pointers) png_free(png, row_pointers); // free the row pointers
        png_destroy_read_struct(&png, &info, NULL); // finally free the png and info contexts
    }
    errno = rval;
    return NULL;
}
