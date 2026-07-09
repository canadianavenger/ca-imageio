#include <stdio.h>
#include <string.h>
#include "pcx_priv.h"
#include <image_pcx.h>
#include <stdbool.h>
#include <errno.h>
#include <memstream.h>

static int pcx_rle_encode(int bpl, memstream_buf_t *dst, memstream_buf_t *src);

/// @brief saves an image as a 4 bit or 8 bit PCX image, always as single-plane image
/// @param fn pointer to the name of the file to save the image as
/// @param img pointer to the pal_image_t structure containing the image
/// @return 0 on success otherwise an error value
int save_pcx(const char *fn, pal_image_t *img) {
    int rval = ENOTSUP;
    FILE *fp = NULL;
    pcx_pal256_t *pal = NULL;
    uint8_t *pcx_buf = NULL;

    if((NULL == img) || (NULL == fn)) return EBADF;

    if((0 == img->width) || (0 == img->height) || (0 == img->colours)) return EINVAL;

    // we support 16 and 256 colour modes. Anything greater than 16 is considered 256
    if(img->colours < 16) return EINVAL;

    // try to open/create output file
    if(NULL == (fp = fopen(fn,"wb"))) {
        rval = errno;  // can't open/create output file
        goto CLEANUP;
    }

    pcx_header_t pcx;
    memset(&pcx, 0, sizeof(pcx_header_t));

    pcx.magic = PCX_MAGIC;
    pcx.version = PCX_V5;
    pcx.encoding = PCX_RLE;

    // currently we only encode at 1 plane, and 4 or 8 bits epr pixel
    if(img->colours == 16) {
        pcx.bits_per_pixel = 4;
        pcx.bytes_per_line = (img->width + 1) / 2; // we pack 2 pixels per byte

        // copy the palette in
        // we caan get away with memcpy here because the PCX palette format is the same as our internal one
        memcpy(pcx.pal_bytes, img->pal, sizeof(pcx_rgb_palette_entry_t) * 16);

    } else {
        pcx.bits_per_pixel = 8;
        pcx.bytes_per_line = img->width;
        // palette is appended after the image data
    }
    pcx.num_planes = 1;
    if(pcx.bytes_per_line & 1) pcx.bytes_per_line++; // bytes per line must be even
    
    // set the resolution
    pcx.x_start = 0;
    pcx.y_start = 0;
    pcx.x_end = img->width-1;
    pcx.y_end = img->height-1;
    pcx.horiz_dpi = img->width;
    pcx.vert_dpi = img->height;

    // write the header
    int nw = fwrite(&pcx, sizeof(pcx_header_t), 1, fp);
    if(1 != nw) {
        rval = errno;  // can't write file
        goto CLEANUP;
    }

    // now we need to repackage the image data according to our configuration
    // create a buffer based on the bytes per line value + some headroom
    pcx_buf = calloc(img->height + 32, pcx.bytes_per_line);
    if(NULL == pcx_buf) {
        rval = errno;  // unable to allocate mem
        goto CLEANUP;
    }

    uint8_t *sp = img->pixels;
    uint8_t *dp = pcx_buf + (pcx.bytes_per_line * 32);
    if(pcx.bytes_per_line == img->width) { // we have a 1:1, just copy it
        memcpy(dp, sp, img->image_size);
    } else if(pcx.bits_per_pixel == 8) { // we have 1 byte per pixel, but padding at the end of each line
        for(int y = 0; y < img->height; y++) {
            memcpy(dp, sp, img->width);
            sp += img->width;
            dp += pcx.bytes_per_line;
        }
    } else { // must be a 4 bit image, pack it 2:1
        for(int y = 0; y < img->height; y++) {
            for(int x = 0; x < img->width; x++) {
                uint8_t px = 0;
                px |= (*sp++) & 0x0f;
                px <<= 4;
                px |= (*sp++) & 0x0f;
                dp[x] = px;
            }
            dp += pcx.bytes_per_line;
        }
    }

    // now we can RLE encode the data
    memstream_buf_t dst = {.pos = 0, .len = (img->height + 32) * pcx.bytes_per_line, .data = pcx_buf};
    memstream_buf_t src = {.pos = 0, .len = pcx.bytes_per_line * img->height, .data = pcx_buf + (pcx.bytes_per_line * 32)};

    rval = pcx_rle_encode(pcx.bytes_per_line, &dst, &src);
    if(rval != 0) {
        goto CLEANUP;
    }

    nw = fwrite(dst.data, dst.pos, 1, fp);
    if(nw != 1) {
        rval = errno;  // can't write file
        goto CLEANUP;
    }

    // write the 256 colour palette if necessary
    if(img->colours > 16) {
        pal = calloc(img->colours, sizeof(pcx_pal256_t));
        if(NULL == pal) {
            rval = errno;  // unable to allocate mem
            goto CLEANUP;
        }
        pal->marker = PCX_PAL_MAGIC;
        // we caan get away with memcpy here because the PCX palette format is the same as our internal one
        memcpy(pal->pal, img->pal, img->colours * sizeof(pcx_rgb_palette_entry_t));

        // write the palette
        nw = fwrite(pal, sizeof(pcx_pal256_t), 1, fp);
        if(nw != 1) {
            rval = errno;  // can't write file
            goto CLEANUP;
        }
    }

    rval = 0;
CLEANUP:
    free_s(pcx_buf); 
    free_s(pal);
    fclose_s(fp);
    return rval;
}

/// @brief performes the PCX RLE compression on the input stream on a line by line basis
/// @param bpl bytes per line for the input data, the input buffer must be a multiple of this
/// @param dst pointer to a memstream buffer holding the RLE compressed result data 
/// @param src pointer to a memstream buffer for holding the decompressed source data
/// @return 
static int pcx_rle_encode(int bpl, memstream_buf_t *dst, memstream_buf_t *src) {
    if(0 != (src->len % bpl)) return EINVAL;
    int lines = src->len / bpl;

    for(int y = 0; y < lines; y++) {
        if(src->pos == src->len) return EFAULT;
        uint8_t count = 1;
        uint8_t last = src->data[src->pos++];
        for(int x = 1; x < bpl; x++) {
            if(src->pos == src->len) return EFAULT;
            uint8_t cur = src->data[src->pos++];
            if(cur == last) { // we're in a run
                count++;
                if(count == 64) {
                    if((dst->pos+2) > dst->len) return ENOBUFS;
                    dst->data[dst->pos++] = 0xff; // 0xC0 + 63
                    dst->data[dst->pos++] = cur;
                    count = 1; // we retained 1 to continue
                }
            } else { // we reached the end of a run
                if((count == 1) && (last < 0xc0)) { // encode as a single literal
                    if((dst->pos + 1) > dst->len) return ENOBUFS;
                    dst->data[dst->pos++] = last;
                } else { // encode the run
                    if((dst->pos+2) > dst->len) return ENOBUFS;
                    dst->data[dst->pos++] = 0xc0 + count; 
                    dst->data[dst->pos++] = last;
                }
                last = cur;
                count = 1;
            }
        }
        // encode what remains
        if((count == 1) && (last < 0xc0)) { // encode as a single literal
            if((dst->pos + 1) > dst->len) return ENOBUFS;
            dst->data[dst->pos++] = last;
        } else { // encode the run
            if((dst->pos+2) > dst->len) return ENOBUFS;
            dst->data[dst->pos++] = 0xc0 + count; 
            dst->data[dst->pos++] = last;
        }
    }

    return 0;
}
