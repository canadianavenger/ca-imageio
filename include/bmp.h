/*
 * bmp.h 
 * interface definitions for a writing an indexed 256 colour Windows BMP file
 * 
 * This code is offered without warranty under the MIT License. Use it as you will 
 * personally or commercially, just give credit if you do.
 */
#include <image.h>

#ifndef CA_IMG_BMP
#define CA_IMG_BMP

/// @brief additional possible return/errno values beyond what 
///        the C standard library provides
// TODO: remove some of these and favour defined ERRNO values
enum bmp_errors {
    BMP_NOERROR      = 0,
    BMP_NULL_POINTER = -1,   // passed pointer is null
    BMP_INVALID      = -2,   // invalid parameter value
    BMP_UNSUPPORTED  = -3,   // valid BMP, but unsupported format
};

/// @brief saves the image pointed to by src as a BMP
/// @param fn name of the file to create and write to
/// @param src pointer to a basic_image_t structure containing the image
/// @return 0 on success, otherwise an error code
int save_bmp(const char *fn, pal_image_t *src);

/// @brief loads the BMP image from a file
/// @param fn name of file to load
/// @return  pointer to a basic_image_t structure containing the image, or null on error (errno is set)
pal_image_t *load_bmp(const char *fn);

#endif
