# ca-imageio
Basic read/write of some standard paletted graphics file formats for use with my various projects. This repository is meant to be included as a git module and linked into other projects. However if built as a stand-alone project the test code will be generated. Note: `ca-imageio` depends on [`ca-image`](https://github.com/canadianavenger/ca-image), though it is not expressly included as a module here, to avoid code replication wehn included in a project.

## Library contents
- `include/bmp.h`: types, macros, and function declarations for saving and loading Windows BMP formatted images
  - `src/bmp/bmp_load.c`:  code for loading 4 and 8 bit BMP images (16 and 256 colour paletted)
  - `src/bmp/bmp_save.c`: code for saving 4 and 8 bit BMP images (16 and 256 colour paletted)
  - `src/bmp/bmp_priv.h`: private header containing the BMP specific structures and defines

### Test Code
- `test/rawio.h`: types, macros, and function declarations for saving and loading raw ca-image images
  - `test/rawio.c`: read/write code for the raw images
- `test/bmp2raw.c`: code for testing the BMP read code
- `test/raw2bmp.c`: code for testing the BMP save code

## Adding ca-imageio to a project
To add this submodule into a folder perform the following command:

`git submodule add https://github.com/canadianavenger/ca-imageio.git <folder name>`

if `git` does not automatically include the contents into `<folder name>` perfrom the following

`git submodule update --init --recursive`

For more info: [Working with Submodules on GitHub](https://github.blog/open-source/git/working-with-submodules/)

