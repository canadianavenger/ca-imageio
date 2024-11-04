#include <stdio.h>
#include <string.h>
#include "tga_priv.h"
#include <image_tga.h>
#include <stdbool.h>
#include <errno.h>

/// @brief saves an image as a 4 bit or 8 bit Windows BMP image
/// @param fn pointer to the name of the file to save the image as
/// @param img pointer to the pal_image_t structure containing the image
/// @return 0 on success otherwise an error value
int save_tga(const char *fn, pal_image_t *img) {
    int rval = 0;
    FILE *fp = NULL;
    tga_palette_entry_t *pal = NULL;

    if((NULL == img) || (NULL == fn)) return EBADF;

    if((0 == img->width) || (0 == img->height) || (0 == img->colours)) return EINVAL;

    // try to open/create output file
    if(NULL == (fp = fopen(fn,"wb"))) {
        rval = errno;  // can't open/create output file
        goto CLEANUP;
    }

    tga_header_t tga;
    memset(&tga, 0, sizeof(tga_header_t));

    tga.colour_map_type = TGA_HAS_CMAP;
    tga.image_type = TGA_PALETTED;

    tga.cmap.colour_map_start  = 0;
    tga.cmap.colour_map_length = img->colours;
    tga.cmap.colour_map_depth  = 24; //32; // 32 if we have transparency

    tga.image.width = img->width;
    tga.image.height = img->height;
    tga.image.pixel_depth = 8;

    // write the header
    int nw = fwrite(&tga, sizeof(tga_header_t), 1, fp);
    if(1 != nw) {
        rval = errno;  // can't write file
        goto CLEANUP;
    }

    // todo switch between RGB and ARGB
    int pal_entry_size = sizeof(tga_rgb_palette_entry_t);

    pal = calloc(img->colours, pal_entry_size);
    if(NULL == pal) {
        rval = errno;  // unable to allocate mem
        goto CLEANUP;
    }

    // copy the external RGB palette to the TGA BGR palette
    tga_rgb_palette_entry_t *ipal = &pal->rgb;
    for(int i = 0; i < img->colours; i++) {
        ipal[i].r = img->pal[i].r;
        ipal[i].g = img->pal[i].g;
        ipal[i].b = img->pal[i].b;
        //ipal[i].a = 255;
    }

    // write the palette
    nw = fwrite(pal, pal_entry_size, img->colours, fp);
    if(nw != img->colours) {
        rval = errno;  // can't write file
        goto CLEANUP;
    }

    // write the image
    nw = fwrite(img->data, img->width, img->height, fp);
    if(nw != img->height) {
        rval = errno;  // can't write file
        goto CLEANUP;
    }

    tga_footer_t tgaf;
    memset(&tgaf, 0, sizeof(tga_footer_t));
    strncpy(tgaf.sig, TGA_SIG, 18);

    // write the footer
    nw = fwrite(&tgaf, sizeof(tga_footer_t), 1, fp);
    if(1 != nw) {
        rval = errno;  // can't write file
        goto CLEANUP;
    }

    rval = 0;
CLEANUP:
    free_s(pal);
    fclose_s(fp);
    return rval;
}
