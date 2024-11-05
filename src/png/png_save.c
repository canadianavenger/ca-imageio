#include <stdio.h>
#include <image_png.h>
#include "png_priv.h"
#include <stdbool.h>
#include <errno.h>
#include <png.h>

int write_png(FILE *fp, pal_image_t *img);

int save_png(const char *fn, pal_image_t *img) {
    int rval = 0;
    FILE *fp = NULL;

    if((NULL == img) || (NULL == fn)) return EBADF;

    if((0 == img->width) || (0 == img->height) || (0 == img->colours)) return EINVAL;

    // try to open/create output file
    if(NULL == (fp = fopen(fn,"wb"))) {
        rval = errno;  // can't open/create output file
        goto CLEANUP;
    }

    rval = write_png(fp, img);

    rval = 0;
CLEANUP:
    fclose_s(fp);
    return rval;
}

#define PNG_BPP (1)
int write_png(FILE *fp, pal_image_t *img) {
    int rval = 0;
    png_structp png;
    png_infop info;
    png_colorp palette;

    // make sure we have an open file
    if(NULL == fp) {
        return EBADF;
    }

    // initialize the PNG stuct
    if(NULL == (png = png_create_write_struct(PNG_LIBPNG_VER_STRING,NULL,NULL,NULL))) {
        return ENOMEM;
    }

    // allocate+initialize the info struct
    if(NULL == (info = png_create_info_struct(png))) {
        rval = ENOMEM;
        goto CLEANUP;
    }

    // we didn't register callbacks, so we need to set up a point for
    // for longjmp to land on error.
    if(setjmp(png_jmpbuf(png))) {
        rval = EFAULT;
        goto CLEANUP;
    }

    // use stdio stream
    png_init_io(png, fp);

    // set the image info here
    png_set_IHDR(png, info, img->width, img->height, 8,
        PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    /* Set the palette if there is one.  REQUIRED for indexed-color images. */
    palette = (png_colorp)png_malloc(png, img->colours * (sizeof (png_color)));
    
    for(int i=0; i<img->colours; i++) {
        palette[i].red = img->pal[i].r;
        palette[i].green = img->pal[i].g;
        palette[i].blue = img->pal[i].b;
    }

    png_set_PLTE(png, info, palette, img->colours);

    // png_byte trans[1] = {0};
    // png_set_tRNS(png_ptr, info_ptr, trans, 1, NULL);

    png_write_info(png, info);

    if (img->height > (PNG_SIZE_MAX / (img->width * PNG_BPP))) {
        rval = EINVAL;
        goto CLEANUP;
    }

    if (img->height > (PNG_UINT_32_MAX / (sizeof (png_bytep)))) {
        rval = EINVAL;
        goto CLEANUP;
    }

    png_bytep *row_pointers;
    if(NULL == (row_pointers = (png_bytep *)png_calloc(png, img->height * sizeof(png_bytep)))) {
        rval = ENOMEM;
        goto CLEANUP;
    }

    for(unsigned i = 0; i < img->height; i++) {
        row_pointers[i] = (png_bytep)(img->data + (i * img->width * PNG_BPP));
    }

    png_write_image(png, row_pointers);
    png_write_end(png, info);

    rval = 0;
CLEANUP:
    if(NULL != png) {
        if(NULL != row_pointers) png_free(png, row_pointers);
        if(NULL != palette) png_free(png, palette);
        png_destroy_write_struct(&png, &info);
    }
    return rval;
}
