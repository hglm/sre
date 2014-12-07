/*

Copyright (c) 2014 Harm Hanemaaijer <fgenfb@yahoo.com>

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

*/


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <malloc.h>
#ifdef __GNUC__
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#ifdef OPENGL_ES2
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#endif
#ifdef OPENGL
#include <GL/glew.h>
#include <GL/gl.h>
#endif
#include <png.h>

#include "sre.h"
#include "sre_internal.h"

static bool checked_texture_formats = false;
static int ETC1_internal_format = - 1;
static int ETC2_RGB8_internal_format = - 1;
static int DXT1_internal_format = - 1;
static int SRGB_DXT1_internal_format = - 1;
static int DXT1A_internal_format = - 1;
static int SRGB_DXT1A_internal_format = - 1;
static int BPTC_internal_format = - 1;
static int SRGB_BPTC_internal_format = - 1;
static int BPTC_float_internal_format = - 1;
static int RGTC1_internal_format = - 1;
static int RGTC2_internal_format = - 1;
static float max_anisotropy, min_anisotropy;

// Standard textures for fall-back when texture doesn't load.
static sreTexture *standard_texture = NULL;
static sreTexture *standard_texture_wrap_repeat = NULL;

static void RegisterTexture(sreTexture *tex);

static void CheckTextureFormats() {
   GLint num = 0;

//   GLEW_OES_compressed_ETC1_RGB8_texture = 0;

   glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &num);
   if (num > 0) {
      GLint* formats = (GLint*)calloc(num, sizeof(GLint));

      glGetIntegerv(GL_COMPRESSED_TEXTURE_FORMATS, formats);

      for (int index = 0; index < num; index++) {
//         printf("Checking format 0x%04X.\n", formats[index]);
#ifdef OPENGL_ES2
         if (formats[index] == GL_ETC1_RGB8_OES) {
             printf("ETC1 texture format supported.\n");
             ETC1_internal_format = GL_ETC1_RGB8_OES;
//            GLEW_OES_compressed_ETC1_RGB8_texture = 1;
         }
#endif
	if (formats[index] == 0x9274) {
		printf("ETC2_RGB8 texture format supported.\n");
		ETC2_RGB8_internal_format = 0x9274;
	}
#if defined(OPENGL)
         if (formats[index] == GL_COMPRESSED_RGB_S3TC_DXT1_EXT) {
             // Assume that alpha and SRGB variants are also supported.
             printf("DXT1 texture format supported.\n");
             DXT1_internal_format = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
             printf("SRGB DXT1 texture format supported.\n");
             SRGB_DXT1_internal_format = GL_COMPRESSED_SRGB_S3TC_DXT1_EXT;
             printf("DXT1A texture format supported.\n");
             DXT1A_internal_format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
             printf("SRGB DXT1A texture format supported.\n");
             SRGB_DXT1A_internal_format = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT;
         }
#endif
      }

      free(formats);
   }

#ifndef OPENGL_ES2
    GLint major, minor;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    printf("OpenGL version %d.%d.\n", major, minor);
    if (GLEW_ARB_texture_compression_bptc) {
        BPTC_internal_format = GL_COMPRESSED_RGBA_BPTC_UNORM_ARB;
        SRGB_BPTC_internal_format = GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB;
        BPTC_float_internal_format = GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB;
        printf("BPTC texture formats supported.\n");
    }
    if (GLEW_ARB_texture_compression_rgtc) {
        RGTC1_internal_format = GL_COMPRESSED_RED_RGTC1;
        RGTC2_internal_format = GL_COMPRESSED_RG_RGTC2;
        printf("RGTC texture formats supported.\n");
    }
#endif

    min_anisotropy = 0;
    max_anisotropy = 0;
#ifndef OPENGL_ES2
    if (GLEW_EXT_texture_filter_anisotropic) {
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_anisotropy);
        printf("Max anisotropy for anisotropic filtering: %.1f.\n",
            max_anisotropy);
    }
#endif

    checked_texture_formats = true;
}

sreTexture::sreTexture() {
    data = NULL;
    largest_level_width = (unsigned int)1 << 30;
}

sreTexture::sreTexture(int w, int h) {
    width = w;
    height = h;
    data = new unsigned int[w * h];
    bytes_per_pixel = 4;
    format = TEXTURE_FORMAT_RAW;
    nu_components = 4;
    bit_depth = 8;
    largest_level_width = (unsigned int)1 << 30;
}

void sreTexture::ClearData() {
    delete [] data;
    data = NULL;
}

sreTexture::~sreTexture() {
    ClearData();
}

static int CountPowersOfTwo(int w, int h) {
    // Count how many of width or height are a power of two.
    int count = 0;
    for (int i = 0; i < 18; i++) {
        if (w == (1 << i))
            count++;
        if (h == (1 << i))
            count++;
        if (count == 2 || (w < (1 << i) && h < (1 << i)))
            break;
    }
    return count;
}

static void SetGLTextureParameters(int type, int flags, int nu_components, int nu_mipmaps_used,
int power_of_two_count) {
#ifndef OPENGL_ES2
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, nu_mipmaps_used - 1);
#endif
    if (nu_mipmaps_used == 1) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    else {
        if (power_of_two_count != 2) {
            sreMessage(SRE_MESSAGE_INFO, "Note: Using non-power-of-two texture with mipmaps.");
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    if (flags & SRE_TEXTURE_TYPE_FLAG_WRAP_REPEAT) {
        if (power_of_two_count != 2) {
            sreFatalError("Repeating textures require power of two texture dimensions.\n");
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }
    else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
#ifndef OPENGL_ES2
    // Replicate single component textures to all components.
    if (nu_components == 1) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
        if (type == TEXTURE_TYPE_TRANSPARENT)
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_RED);
    }
#endif
}

void sreTexture::SelectMipmaps(int nu_mipmaps, int& power_of_two_count, int& nu_mipmaps_used,
int& target_width, int& target_height, int& nu_levels_skipped, int flags) {
    // Count whether width or height is a power of two.
    power_of_two_count = CountPowersOfTwo(width, height);
    nu_mipmaps_used = nu_mipmaps;
    CalculateTargetSize(target_width, target_height, nu_levels_skipped, flags);
    // Adjust the number of levels skipped according to the largest allowed
    // texture width for the texture.
    while ((width >> nu_levels_skipped) > largest_level_width) {
        nu_levels_skipped++;
    }
    nu_mipmaps_used -= nu_levels_skipped;
    if (nu_mipmaps_used < 1) {
        // When there insufficient lower detail compressed mipmap levels, use
        // the single lowest defined mipmap level.
        nu_levels_skipped -= 1 - nu_mipmaps_used;
        nu_mipmaps_used = 1;
        sreMessage(SRE_MESSAGE_WARNING, "Insufficient lower-order compressed texture mipmap "
            "levels, cannot fully apply texture detail reduction settings.");
    }
    if (nu_levels_skipped > 0)
         sreMessage(SRE_MESSAGE_INFO, "Highest-level compressed texture mipmap levels (n = %d) "
             "omitted due to texture detail settings or limitations.",
             nu_levels_skipped);
    if (!(sre_internal_texture_detail_flags & SRE_TEXTURE_DETAIL_NPOT_MIPMAPS)
    && power_of_two_count != 2 && nu_mipmaps_used > 1) {
        // Mipmaps not supported for textures that are not a power of two in width and height.
        sreMessage(SRE_MESSAGE_WARNING,
           "Compressed non-power-of-2 mipmapped textures not supported --"
           "using single mipmap.");
        nu_mipmaps_used = 1;
    }
    else if ((flags & SRE_TEXTURE_TYPE_FLAG_WRAP_REPEAT) &&
    !(sre_internal_texture_detail_flags & SRE_TEXTURE_DETAIL_NPOT_WRAP)
    && power_of_two_count != 2) {
         sreMessage(SRE_MESSAGE_WARNING,
           "Wrap mode non-power-of-2 mipmapped textures not supported --"
           "using single mipmap.");
        nu_mipmaps_used = 1;
    }
    else if (!(sre_internal_texture_detail_flags & SRE_TEXTURE_DETAIL_NPOT_MIPMAPS_COMPRESSED)
    && (format & TEXTURE_FORMAT_COMPRESSED) && power_of_two_count != 2) {
        if (nu_mipmaps_used > 1) {
            sreMessage(SRE_MESSAGE_WARNING,
                "Compressed non-power-of-2 mipmapped textures not supported --"
                "using single mipmap.");
                 nu_mipmaps_used = 1;
        }
        if ((target_width & 3) != 0) {
            sreMessage(SRE_MESSAGE_WARNING,
                "Selected single mipmap of compressed non-power-of-2 texture "
                "is not a multiple of four -- picking a lower (larger) mipmap level.");
            for (int level = nu_levels_skipped;; level--) {
                // Obtain the actual width of this level.
                int w = width >> level;
                if ((w & 3) == 0) {
                    nu_levels_skipped = level;
                    break;
                }
                if (level == 0) {
                    sreMessage(SRE_MESSAGE_WARNING,
                        "No suitable lower-level mipmap found, keeping non-multiple-of-four "
                        "mipmap.");
                    break;
                }
            }
        }
    }
}

void sreTexture::UploadGL(int flags) {
    if (width > sre_internal_max_texture_size || height > sre_internal_max_texture_size) {
        printf("Error - texture size of (%d x %d) is too large (max supported %d x %d).\n",
            width, height, sre_internal_max_texture_size, sre_internal_max_texture_size);
        exit(1);
    }

#ifndef NO_SRGB
    if (format == TEXTURE_FORMAT_RAW) {
        // Regular (image) textures should be handled as SRGB.
        if (type == TEXTURE_TYPE_NORMAL || type == TEXTURE_TYPE_SRGB)
            if (bytes_per_pixel == 4)
                format = TEXTURE_FORMAT_RAW_SRGBA8;
            else if (bytes_per_pixel == 3)
                format = TEXTURE_FORMAT_RAW_SRGB8;
            else
                format = TEXTURE_FORMAT_RAW_R8;
        else // Normal maps etc. are stored stored in linear color space format.
            if (bytes_per_pixel == 4)
                format = TEXTURE_FORMAT_RAW_RGBA8;
            else if (bytes_per_pixel == 3)
                format = TEXTURE_FORMAT_RAW_RGB8;
            else
                format = TEXTURE_FORMAT_RAW_R8;
    }
#endif
    GLuint texid;
    glGenTextures(1, &texid);
    opengl_id = texid;
    glBindTexture(GL_TEXTURE_2D, opengl_id);
    int count = CountPowersOfTwo(width, height);
    bool generate_mipmaps = false;
    sreAbortOnGLError("Error before glTexParameteri.\n");
    if (count != 2 || format == TEXTURE_FORMAT_ETC1) {
        // Mip-maps not supported for textures that are not a power of two in width and height.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        generate_mipmaps = true;
    }
    if (flags & SRE_TEXTURE_TYPE_FLAG_WRAP_REPEAT) {
        if (count != 2) {
            sreFatalError("Repeating textures require power of two texture dimensions.");
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }
    else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
#ifndef OPENGL_ES2
    // Replicate single component textures to all components.
    if (nu_components == 1) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
        if (type == TEXTURE_TYPE_TRANSPARENT)
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_RED);
     }
#endif
    sreAbortOnGLError("Error after glTexParameteri.\n");
    GLint internal_format;
    if (format == TEXTURE_FORMAT_ETC1) {
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, ETC1_internal_format,
            width, height, 0, (width / 4) * (height / 4) * 8, data);
        internal_format = ETC1_internal_format;
    }
    else {
#ifdef OPENGL_ES2
        if (bytes_per_pixel == 4)
            internal_format = GL_RGBA;
        else if (bytes_per_pixel == 3)
            internal_format = GL_RGB;
        else
            internal_format = GL_LUMINANCE;
#else
        if (nu_components == 1)
             internal_format = GL_RED;
        else if (format == TEXTURE_FORMAT_RAW_RGBA8 || format == TEXTURE_FORMAT_RAW_RGB8)
            if (bytes_per_pixel == 4)
                internal_format = GL_RGBA8;
            else
                internal_format = GL_RGB8;
        else if (format == TEXTURE_FORMAT_RAW_SRGBA8 || format == TEXTURE_FORMAT_RAW_SRGB8) {
            if (bytes_per_pixel == 4)
                internal_format = GL_SRGB_ALPHA;
            else
                internal_format = GL_SRGB;
        }
        else {
            sreFatalError("Unknown texture format.");
        }
#endif
        if (bytes_per_pixel == 4)
            glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, data);
        else if (bytes_per_pixel == 3) {
            /* Unpack alignment of 1. */
            GLint previousUnpackAlignment;
            glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousUnpackAlignment);
            if (previousUnpackAlignment != 1)
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0,
                GL_RGB, GL_UNSIGNED_BYTE, data);
            if (previousUnpackAlignment != 1)
                glPixelStorei(GL_UNPACK_ALIGNMENT, previousUnpackAlignment);
        }
        else {
            // Assume single 8-bit component.
            /* Unpack alignment of 1. */
            GLint previousUnpackAlignment;
            glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousUnpackAlignment);
            if (previousUnpackAlignment != 1)
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            int format;
#ifdef OPENGL_ES2
            format = GL_LUMINANCE;
#else
            format = GL_RED;
#endif
            glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0,
                format, GL_UNSIGNED_BYTE, data);
            if (previousUnpackAlignment != 1)
                glPixelStorei(GL_UNPACK_ALIGNMENT, previousUnpackAlignment);
        }
    }
    sreAbortOnGLError("Error after glTexImage2D (internal_format = 0x%04X)\n", internal_format);
    if (generate_mipmaps) {
        glGenerateMipmap(GL_TEXTURE_2D);
        sreAbortOnGLError("Error after glGenerateMipmap (internal format = %d).\n", internal_format);
    }
    if (!(flags & SRE_TEXTURE_TYPE_FLAG_KEEP_DATA))
        delete [] data;
 
    RegisterTexture(this);
}

void sreTexture::ConvertFrom24BitsTo32Bits() {
    int n = width * height;
    unsigned int *data2 = new unsigned int[n];
    unsigned char *data1 = (unsigned char *)data;
    for (int i = 0; i < n; i++) {
        data2[i] = data1[i * 3] + (data1[i * 3 + 1] << 8) + (data1[i * 3 + 2] << 16) + 0xFF000000;
    }
    delete [] data;
    data = data2;
    bytes_per_pixel = 4;
    nu_components = 4;
}

// PNG loading.

void sreTexture::LoadPNG(const char *filename, int flags) {
    int png_width, png_height;
    png_byte color_type;
    png_byte png_bit_depth;
    png_structp png_ptr;
    png_infop info_ptr;
    png_bytep *row_pointers;

        png_byte header[8];    // 8 is the maximum size that can be checked

        /* open file and test for it being a png */
        FILE *fp = fopen(filename, "rb");
        if (!fp) {
            printf("Error - File %s could not be opened for reading\n", filename);
            exit(1);
        }
        fread(header, 1, 8, fp);
        if (png_sig_cmp(header, 0, 8)) {
            printf("Error - File %s is not recognized as a PNG file\n", filename);
            exit(1);
        }

        /* initialize stuff */
        png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

        if (!png_ptr) {
            printf("png_create_read_struct failed\n");
            exit(1);
        }   

        info_ptr = png_create_info_struct(png_ptr);
        if (!info_ptr) {
            printf("png_create_info_struct failed\n");
            exit(1);
        }

        if (setjmp(png_jmpbuf(png_ptr))) {
            printf("Error during init_io");
            exit(1);
        }

        png_init_io(png_ptr, fp);
        png_set_sig_bytes(png_ptr, 8);

        png_read_info(png_ptr, info_ptr);

        png_width = png_get_image_width(png_ptr, info_ptr);
        png_height = png_get_image_height(png_ptr, info_ptr);
        color_type = png_get_color_type(png_ptr, info_ptr);
        png_bit_depth = png_get_bit_depth(png_ptr, info_ptr);

        int number_of_passes = png_set_interlace_handling(png_ptr);
        if (png_bit_depth == 16)
            png_set_swap(png_ptr);
        png_read_update_info(png_ptr, info_ptr);

        /* read file */
        if (setjmp(png_jmpbuf(png_ptr))) {
            printf("Error during read_image\n");
            exit(1);
        }

        row_pointers = (png_bytep *)malloc(sizeof(png_bytep) * png_height);
        for (int y = 0; y < png_height; y++)
            row_pointers[y] = (png_byte *)malloc(png_get_rowbytes(png_ptr, info_ptr));

        png_read_image(png_ptr, row_pointers);

        fclose(fp);

    width = png_width;
    height = png_height;
    if (color_type != PNG_COLOR_TYPE_RGB && color_type != PNG_COLOR_TYPE_RGBA && color_type != PNG_COLOR_TYPE_GRAY) {
        sreFatalError("Error - unexpected color format.\n");
    }
    if (png_bit_depth != 8 && png_bit_depth != 16) {
        sreFatalError("Error - expected bit depth of 8 or 16 in PNG file (depth = %d).\n",
            png_bit_depth);
    }
    bit_depth = png_bit_depth;
    if (color_type == PNG_COLOR_TYPE_RGB) {
        bytes_per_pixel = 3 * (bit_depth / 8);
        nu_components = 3;
    }
    else
    if (color_type == PNG_COLOR_TYPE_RGBA) {
        bytes_per_pixel = 4 * (bit_depth / 8);
        nu_components = 4;
    }
    else
    if (color_type == PNG_COLOR_TYPE_GRAY) {
        bytes_per_pixel = (bit_depth / 8); // Grayscale.
        nu_components = 1;
#ifdef EXPAND_SINGLE_COMPONENT_TEXTURES
        bytes_per_pixel *= 3;
        nu_components *= 3;
#endif
    }
    else
        sreFatalError("Unexpected PNG file color type %d.", color_type);
    sreMessage(SRE_MESSAGE_INFO,
        "Loading uncompressed texture with size (%d x %d), bit depth %d, %d components.",
        width, height, png_bit_depth, nu_components);
    if (color_type == PNG_COLOR_TYPE_GRAY) {
        data = (unsigned int *)new unsigned char[width * height * (bit_depth / 8) *
            nu_components];
        for (int y = 0; y < height; y++)
            for (int x = 0; x < width; x++)
                 for (int i = 0; i < nu_components; i++) {
                     if (bit_depth == 8)
                         *((unsigned char *)data + y * width * nu_components + x * nu_components + i) =
                             *((unsigned char *)row_pointers[y] + x * nu_components + i);
                     else // bit_depth == 16
                         *((unsigned short *)data + y * width * nu_components + x * nu_components + i) =
                         *((unsigned short *)row_pointers[y] + x * nu_components + i);
                 }
    }
    else { // RGB or RGBA
        data = (unsigned int *)new unsigned char[width * height * bytes_per_pixel];
        for (int y = 0; y < height; y++)
            memcpy((unsigned char *)data + y * width * bytes_per_pixel, row_pointers[y],
                width * bytes_per_pixel);
    }
    for (int y = 0; y < height; y++)
        free(row_pointers[y]);
    free(row_pointers);

    format = TEXTURE_FORMAT_RAW;

    ApplyTextureDetailSettings(flags);
    if (type != TEXTURE_TYPE_WILL_MERGE_LATER &&
    !(flags & SRE_TEXTURE_TYPE_FLAG_NO_UPLOAD)) {
        UploadGL(flags);
    }
}

#define KTX_HEADER_SIZE	(64)
/* KTX files require an unpack alignment of 4 */
#define KTX_GL_UNPACK_ALIGNMENT 4

typedef unsigned char khronos_uint8_t;
typedef unsigned int khronos_uint32_t;

/**
 * @internal
 * @brief KTX file header
 *
 * See the KTX specification for descriptions
 * 
 */
typedef struct KTX_header_t {
	khronos_uint8_t  identifier[12];
	khronos_uint32_t endianness;
	khronos_uint32_t glType;
	khronos_uint32_t glTypeSize;
	khronos_uint32_t glFormat;
	khronos_uint32_t glInternalFormat;
	khronos_uint32_t glBaseInternalFormat;
	khronos_uint32_t pixelWidth;
	khronos_uint32_t pixelHeight;
	khronos_uint32_t pixelDepth;
	khronos_uint32_t numberOfArrayElements;
	khronos_uint32_t numberOfFaces;
	khronos_uint32_t numberOfMipmapLevels;
	khronos_uint32_t bytesOfKeyValueData;
} KTX_header;

bool sreTexture::LoadKTX(const char *filename, int flags) {
    FILE *f = fopen(filename, "rb");
    // Read header.
    KTX_header header;
    fread(&header, 1, KTX_HEADER_SIZE, f);
    if (header.endianness != 0x04030201) {
        sreMessage(SRE_MESSAGE_INFO, "Endianness wrong way around in .ktx file.");
	for (int i = 12; i < sizeof(KTX_header); i += 4) {
            unsigned char *h = (unsigned char *)&header;
            unsigned char temp = h[i];
            h[i] = h[i + 3];
            h[i + 3] = temp;
            temp = h[i + 1];
            h[i + 1] = h[i + 2];
            h[i + 2] = temp;
	}
    }
    // Skip metadata
    unsigned char *metadata = (unsigned char *)alloca(header.bytesOfKeyValueData);
    fread(metadata, 1, header.bytesOfKeyValueData, f);
    GLenum glInternalFormat = header.glInternalFormat;
    int supported_format = - 1;
    switch (glInternalFormat) {
#ifdef OPENGL_ES2
    case GL_ETC1_RGB8_OES :
        if (ETC1_internal_format != - 1)
            supported_format = TEXTURE_FORMAT_ETC1;
        break;
#endif
    case 0x9274 :	// ETC2_RGB8
	if (ETC2_RGB8_internal_format != - 1)
            supported_format = TEXTURE_FORMAT_ETC2_RGB8;
        break;
#ifndef OPENGL_ES2
    case GL_COMPRESSED_RGB_S3TC_DXT1_EXT :
#ifndef NO_SRGB
        if (type == TEXTURE_TYPE_NORMAL || type == TEXTURE_TYPE_SRGB) {
    	    if (SRGB_DXT1_internal_format != - 1) {
                supported_format = TEXTURE_FORMAT_SRGB_DXT1;
                glInternalFormat = GL_COMPRESSED_SRGB_S3TC_DXT1_EXT;
            }
        }
        else
#endif
	if (DXT1_internal_format != - 1)
            supported_format = TEXTURE_FORMAT_DXT1;
        break;
#endif
#ifndef OPENGL_ES2
    case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT :
	if (SRGB_DXT1_internal_format != - 1)
            supported_format = TEXTURE_FORMAT_SRGB_DXT1;
        break;
    case GL_COMPRESSED_RGBA_BPTC_UNORM_ARB :
#ifndef NO_SRGB
        if (type == TEXTURE_TYPE_NORMAL || type == TEXTURE_TYPE_SRGB) {
            if (SRGB_BPTC_internal_format != - 1) {
                supported_format = TEXTURE_FORMAT_SRGB_BPTC;
                glInternalFormat = GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB;
            }
        }
        else
#endif
        if (BPTC_internal_format != - 1)
            supported_format = TEXTURE_FORMAT_BPTC;
        break;
    case GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB :
        if (SRGB_BPTC_internal_format != - 1)
            supported_format = TEXTURE_FORMAT_SRGB_BPTC;
        break;
    case GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB :
        if (BPTC_float_internal_format != - 1)
            supported_format = TEXTURE_FORMAT_BPTC_FLOAT;
        break;
#endif
default :
        break;
    }
    if (supported_format == - 1) {
        sreMessage(SRE_MESSAGE_INFO,
            "Texture format in KTX file not supported (glInternalFormat = 0x%04X).",
                glInternalFormat);
	return false;
    }

    width = header.pixelWidth;
    height = header.pixelHeight;
    format = supported_format;

    switch (supported_format) {
    case TEXTURE_FORMAT_BPTC :
    case TEXTURE_FORMAT_SRGB_BPTC :
         nu_components = 4;
         break;
    default :
         nu_components = 3;
         break;
    }

    int power_of_two_count, nu_mipmaps_used, target_width, target_height, nu_levels_skipped;
    SelectMipmaps(header.numberOfMipmapLevels, power_of_two_count, nu_mipmaps_used,
        target_width, target_height, nu_levels_skipped, flags);

    sreMessage(SRE_MESSAGE_INFO,
        "Loading KTX texture with size (%d x %d), using %d mipmap levels starting at %d.",
        header.pixelWidth, header.pixelHeight, nu_mipmaps_used, nu_levels_skipped);

    /* KTX files require an unpack alignment of 4 */
    GLint previousUnpackAlignment;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousUnpackAlignment);
    if (previousUnpackAlignment != KTX_GL_UNPACK_ALIGNMENT)
        glPixelStorei(GL_UNPACK_ALIGNMENT, KTX_GL_UNPACK_ALIGNMENT);
    // Generate the texture.
    GLuint texid;
    glGenTextures(1, &texid);
    opengl_id = texid;
    glBindTexture(GL_TEXTURE_2D, opengl_id);
    SetGLTextureParameters(type, flags, nu_components, nu_mipmaps_used, power_of_two_count);

    khronos_uint32_t faceLodSize;
    khronos_uint32_t faceLodSizeRounded;
    khronos_uint32_t level;
    khronos_uint32_t face;
    khronos_uint32_t dataSize = 0;

    data = NULL;
    for (level = 0; level < nu_mipmaps_used + nu_levels_skipped; level++) {
        GLsizei pixelWidth  = maxi(1, header.pixelWidth  >> level);
        GLsizei pixelHeight = maxi(1, header.pixelHeight >> level);
        GLsizei pixelDepth  = maxi(1, header.pixelDepth  >> level);
        if (level == nu_levels_skipped) {
            // Set the texture size to the first used mipmap level.
            width = pixelWidth;
            height = pixelHeight;
        }

        fread(&faceLodSize, 1, sizeof(khronos_uint32_t), f);
        faceLodSizeRounded = (faceLodSize + 3) & ~(khronos_uint32_t)3;
        if (!data) {
            /* Allocate memory sufficient for the first level. */
	    data = (unsigned int *)malloc(faceLodSizeRounded);
 	    dataSize = faceLodSizeRounded;
        }
        for (face = 0; face < header.numberOfFaces; face++) {
            fread(data, 1, faceLodSizeRounded, f);
            if (header.numberOfArrayElements)
                pixelHeight = header.numberOfArrayElements;
            if (level < nu_levels_skipped)
                continue;
            glCompressedTexImage2D(GL_TEXTURE_2D + face, level - nu_levels_skipped,
                 glInternalFormat, pixelWidth, pixelHeight, 0, faceLodSize, data);
            sreAbortOnGLError("Error uploading compressed KTX texture.\n");
        }
    }
    fclose(f);
    free(data);
    /* restore previous GL state */
    if (previousUnpackAlignment != KTX_GL_UNPACK_ALIGNMENT)
        glPixelStorei(GL_UNPACK_ALIGNMENT, previousUnpackAlignment);

    RegisterTexture(this);
    return true;
}

void sreTexture::LoadDDS(const char *filename, int flags) {
    unsigned char header[124];
 
    FILE *fp; 
    /* try to open the file */
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        sreFatalError("Cannot access texture file %s.", filename);
    }
 
    /* verify the type of file */
    char filecode[4];
    fread(filecode, 1, 4, fp);
    if (strncmp(filecode, "DDS ", 4) != 0) {
        fclose(fp);
        sreFatalError(".dds file is not a DDS file.");
    }
 
    /* get the surface desc */
    fread(&header, 124, 1, fp);
 
    height      = *(unsigned int *)(header + 8);
    width         = *(unsigned int *)(header + 12);
    unsigned int linearSize     = *(unsigned int *)(header + 16);
    unsigned int mipMapCount = *(unsigned int *)(header + 24);

    char four_cc[5];
    strncpy(four_cc, (char *)&header[80], 4);
    four_cc[4] = '\0';
    unsigned int dx10_format = 0;
    if (strncmp(four_cc, "DX10", 4) == 0) {
        unsigned char dx10_header[20];
	fread(dx10_header, 1, 20, fp);
	dx10_format = *(unsigned int *)&dx10_header[0];
	unsigned int resource_dimension = *(unsigned int *)&dx10_header[4];
        if (resource_dimension != 3)
            sreFatalError("Only 2D textures supported for .dds files.\n");
    }

    GLint internal_format;
#ifdef OPENGL_ES2
    // For OpenGL-ES 2.0, only check for DXT1 compression.
    if ((dx10_format == 0 && strncmp(four_cc, "DXT1", 4) != 0)
    || (dx10_format > 0 && dx10_format != 0x83F0))
        sreFatalError("Only DXT1 compression format supported in .dds file with OpenGL-ES 2.0");
    if (DXT1_internal_format >= 0) {
        format = TEXTURE_FORMAT_DXT1;
        internal_format = DXT1_internal_format;
    }
    else
        sreFatalError("DXT1 format not supported by OpenGL-ES 2.0 implementation.");
#else
    if (dx10_format == 0) {
        if (strncmp(four_cc, "DXT1", 4) == 0) {
           format = TEXTURE_FORMAT_DXT1;
           internal_format = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
        }
        else if (strncmp(four_cc, "ATI1", 4) == 0) {
           format = TEXTURE_FORMAT_RGTC1;
           internal_format = GL_COMPRESSED_RED_RGTC1;
        }
        else if (strncmp(four_cc, "ATI2", 4) == 0) {
           format = TEXTURE_FORMAT_RGTC2;
           internal_format = GL_COMPRESSED_RG_RGTC2;
        }
        else
            sreFatalError("Unsupported FOURCC (%s) in .dds file.", four_cc);
    }
    else if (dx10_format == 70 || dx10_format == 71) {
       format = TEXTURE_FORMAT_DXT1;
       internal_format = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
    }
#if 0
    else if (dx10_format == 70) {
       format = TEXTURE_FORMAT_DXT1A;
       internal_format = GL_COMPRESSED_RGB_S3TC_DXT1A_EXT;
    }
#endif
    else if (dx10_format == 79 || dx10_format == 80) {
       format = TEXTURE_FORMAT_RGTC1;
       internal_format = GL_COMPRESSED_RED_RGTC1;
    }
    else if (dx10_format == 81) {
       format = TEXTURE_FORMAT_SIGNED_RGTC1;
       internal_format = GL_COMPRESSED_SIGNED_RED_RGTC1;
    }
    else if (dx10_format == 82 || dx10_format == 83) {
       format = TEXTURE_FORMAT_RGTC2;
       internal_format = GL_COMPRESSED_RG_RGTC2;
    }
    else if (dx10_format == 84) {
       format = TEXTURE_FORMAT_SIGNED_RGTC2;
       internal_format = GL_COMPRESSED_SIGNED_RG_RGTC2;
    }
    else
        sreFatalError("Unsupported DX10 format %d in .dds file.", dx10_format);
#endif

    sreAbortOnGLError("Error before loading DDS texture.\n");

#ifndef OPENGL_ES2
    // When transparency is set, use a DXT1A instead of DXT1 texture format.
    if (type == TEXTURE_TYPE_TRANSPARENT && format == TEXTURE_FORMAT_DXT1) {
        internal_format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
        format = TEXTURE_FORMAT_DXT1A;
    }
#ifndef NO_SRGB
    // Force SRGB mode for color textures.
    if (type == TEXTURE_TYPE_NORMAL || type == TEXTURE_TYPE_SRGB) {
        if (format == TEXTURE_FORMAT_DXT1) {
            internal_format = GL_COMPRESSED_SRGB_S3TC_DXT1_EXT;
            format = TEXTURE_FORMAT_SRGB_DXT1;
        }
        else if (format == TEXTURE_FORMAT_DXT1A) {
            internal_format = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT;
            format = TEXTURE_FORMAT_SRGB_DXT1A;
        }
    }
#endif
    if ((format == TEXTURE_FORMAT_DXT1 && DXT1_internal_format < 0)
    || (format == TEXTURE_FORMAT_SRGB_DXT1 && SRGB_DXT1_internal_format < 0)
    || (format == TEXTURE_FORMAT_DXT1A && DXT1A_internal_format < 0)
    || (format == TEXTURE_FORMAT_SRGB_DXT1A && SRGB_DXT1A_internal_format < 0)
    || (format == TEXTURE_FORMAT_RGTC1 && RGTC1_internal_format < 0)
    || (format == TEXTURE_FORMAT_RGTC2 && RGTC2_internal_format < 0)
    || (format == TEXTURE_FORMAT_SIGNED_RGTC1 && RGTC1_internal_format < 0)
    || (format == TEXTURE_FORMAT_SIGNED_RGTC2 && RGTC2_internal_format < 0))
        sreFatalError("Compressed texture format 0x%04X not supported by GPU.",
            internal_format);
#endif

    switch (format) {
    case TEXTURE_FORMAT_DXT1A :
    case TEXTURE_FORMAT_SRGB_DXT1A :
    case TEXTURE_FORMAT_BPTC :
    case TEXTURE_FORMAT_SRGB_BPTC :
         nu_components = 4;
         break;
    case TEXTURE_FORMAT_RGTC1 :
    case TEXTURE_FORMAT_SIGNED_RGTC1 :
         nu_components = 1;
         break;
    case TEXTURE_FORMAT_RGTC2 :
    case TEXTURE_FORMAT_SIGNED_RGTC2 :
         nu_components = 2;
         break;
    default :
         nu_components = 3;
         break;
    }

    // Check whether the texture dimensions are a power of two.
    int count = CountPowersOfTwo(width, height);

    // Check for buggy dds files that have too many mipmap levels for non-square textures.
    int w = width;
    int h = height;
    for (int i = 0; i < mipMapCount; i++) {
        if (w == 0 || h == 0) {
            mipMapCount = i;
            break;
        }
        w /= 2;
        h /= 2;
    }

    int power_of_two_count, nu_mipmaps_used, target_width, target_height, nu_levels_skipped;
    SelectMipmaps(mipMapCount, power_of_two_count, nu_mipmaps_used, target_width, target_height,
       nu_levels_skipped, flags);

    sreMessage(SRE_MESSAGE_INFO,
        "Loading DDS texture with size (%d x %d), %d mipmap levels starting at %d.",
         width, height, mipMapCount, nu_levels_skipped);

    // Load texture file into buffer.
    unsigned char *buffer;
    unsigned int bufsize;
    // Only RGTC2 (BC5) has a 16-byte block size.
    unsigned int blockSize =
        (format == TEXTURE_FORMAT_RGTC2 || format == TEXTURE_FORMAT_SIGNED_RGTC2) ? 16 : 8;
    unsigned int size = ((width + 3) / 4) * ((height + 3) / 4) * blockSize;
    // Allocate a buffer of sufficient size to hold all the mipmaps.
    bufsize = mipMapCount > 1 ? size * 2 : size;
    buffer = (unsigned char*)malloc(bufsize * sizeof(unsigned char));
    // Read the compressed texture data into the buffer.
    fread(buffer, 1, bufsize, fp);
    fclose(fp);

    // Create one OpenGL texture
    GLuint textureID;
    glGenTextures(1, &textureID);
    opengl_id = textureID;
 
    // "Bind" the newly created texture : all future texture functions will modify this texture
    glBindTexture(GL_TEXTURE_2D, textureID);

    SetGLTextureParameters(type, flags, nu_components, nu_mipmaps_used, power_of_two_count);
    sreAbortOnGLError("Error after setting texture parameters.\n");

    /* Load the mipmaps. */
    w = width;
    h = height;
    unsigned int offset = 0;
    for (unsigned int level = 0; level < nu_mipmaps_used + nu_levels_skipped; level++) {
        if (level == nu_levels_skipped) {
            // Set the texture size to the first used mipmap level.
            width = w;
            height = h;
        }
        unsigned int level_size;
        level_size = ((w + 3) / 4) * ((h + 3) / 4) * blockSize;
        if (level >= nu_levels_skipped) {
            glCompressedTexImage2D(GL_TEXTURE_2D, level - nu_levels_skipped,
                internal_format, w, h, 0, level_size, buffer + offset);
            sreAbortOnGLError("Error loading .dds texture level.\n");
        }
        offset += level_size;
        w /= 2;
        h /= 2;
    }
    free(buffer);
    RegisterTexture(this);
}

static bool FileExists(const char *filename) {
#ifdef __GNUC__
    struct stat stat_buf;
    int r = stat(filename, &stat_buf);
    if (r == - 1)
        return false;
    return true;
#else
    FILE *f = fopen(filename, "rb");
    if (f == NULL)
	return false;
    fclose(f);
    return true;
#endif
}

sreTexture *sreCreateTexture(const char *pathname, int type) {
    return new sreTexture(pathname, type);
}

sreTexture *sreCreateTextureLimitLevelWidth(const char *pathname, int type,
int largest_level_width) {
    sreTexture *tex = new sreTexture;
    tex->largest_level_width = largest_level_width;
    tex->Load(pathname, type);
    return tex;
}

sreTexture::sreTexture(const char *pathname_without_ext, int _type) {
    largest_level_width = (unsigned int)1 << 30;
    Load(pathname_without_ext, _type);
}

void sreTexture::Load(const char *basefilename, int _type) {
    if (!checked_texture_formats)
        CheckTextureFormats();
    char *s;
    s = new char[strlen(basefilename) + 5];
    sprintf(s, "%s.ktx", basefilename);
    bool success = false;
    bool keep_data = false;
    if (_type & SRE_TEXTURE_TYPE_FLAG_KEEP_DATA)
        keep_data = true;
    type = _type & (~SRE_TEXTURE_TYPE_FLAGS_MASK);
    if (!(_type & SRE_TEXTURE_TYPE_FLAG_USE_UNCOMPRESSED_TEXTURE) && FileExists(s)) {
        success = LoadKTX(s, _type & SRE_TEXTURE_TYPE_FLAGS_MASK);
    }
    if (!success) {
        sprintf(s, "%s.dds", basefilename);
        if (DXT1_internal_format != -1 && !(_type & SRE_TEXTURE_TYPE_FLAG_USE_UNCOMPRESSED_TEXTURE)
        && FileExists(s)) {
            LoadDDS(s, _type & SRE_TEXTURE_TYPE_FLAGS_MASK);
            success = true;
        }
    }
    if (!success) {
        sprintf(s, "%s.png", basefilename);
        if (FileExists(s))
            LoadPNG(s, _type & SRE_TEXTURE_TYPE_FLAGS_MASK);
        else {
            sreMessage(SRE_MESSAGE_WARNING,
                "Texture file %s(.png, .ktx, .dds) not found or not supported. "
                "Replacing with internal standard texture.", basefilename);
            sreTexture *tex;
            if (_type & SRE_TEXTURE_TYPE_FLAG_WRAP_REPEAT)
                tex = sreGetStandardTextureWrapRepeat();
            else
                tex = sreGetStandardTexture();
            // Copy the fields of the standard texture, not graceful but it works.
            width = tex->width;
            height = tex->height;
            bytes_per_pixel = tex->bytes_per_pixel;
            format = tex->format;
            // Do not copy the data field because is invalid anyway (previously been freed after upload).
            data = NULL;
            opengl_id = tex->opengl_id;
        }
    }
    delete [] s;
}

void sreTexture::ChangeParameters(int flags, int filtering, float anisotropy) {
    glBindTexture(GL_TEXTURE_2D, opengl_id);
    if (flags & SRE_TEXTURE_FLAG_SET_FILTER)
        switch (filtering) {
        case SRE_TEXTURE_FILTER_NEAREST :
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            break;
        case SRE_TEXTURE_FILTER_LINEAR :
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            break;
        case SRE_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR :
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            break;
         }
    if (flags & SRE_TEXTURE_FLAG_ENABLE_WRAP_REPEAT) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }
    if (flags & SRE_TEXTURE_FLAG_DISABLE_WRAP_REPEAT) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
#ifndef OPENGL_ES2
    if (flags & SRE_TEXTURE_FLAG_SET_ANISOTROPY) {
        if (GLEW_EXT_texture_filter_anisotropic) {
            if (anisotropy > max_anisotropy)
                anisotropy = max_anisotropy;
            if (anisotropy < 1.0)
                anisotropy = 1.0;
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy);
        }
    }
#endif
}

void sreTexture::MergeTransparencyMap(sreTexture *t) {
    if (t->width != width || t->height !=  height)
        sreFatalError("sreTexture::MergeTransparencyMap: Transparency texture does not match"
            "texture size.");
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++) {
            // Take the r value in the transparency map and assign it to the alpha value in the current texture.
            unsigned int pix = LookupPixel(x, y) + ((0xFF - (t->LookupPixel(x,y) & 0xFF)) << 24);
            SetPixel(x, y, pix);
        }
    type = TEXTURE_TYPE_NORMAL;
    UploadGL(false);
}

unsigned int sreTexture::LookupPixel(int x, int y) {
    if (x < 0)
        x = 0;
    if (x >= width)
        x = width - 1;
    if (y < 0)
        y = 0;
    if (y >= height)
        y = height - 1;
    if (bytes_per_pixel == 4)
        return data[y * width + x];
    int offset = (y * width + x) * nu_components;
    if (bytes_per_pixel == 3) {
        unsigned char *datap = (unsigned char *)data;
        return (unsigned int)datap[offset] | ((unsigned int)datap[offset + 1] << 8) |
            ((unsigned int)datap[offset + 2] << 16);
    }
    else if (bytes_per_pixel == 2) {
        // Assume 16-bit depth grayscale texture.
        unsigned short *datap = (unsigned short *)data;
        return (unsigned int)datap[offset];
    }
    else {
        // Assume bytes_per_pixel == 1 (8-bit grayscale or red only).
        unsigned char *datap = (unsigned char *)data;
        return (unsigned int)datap[offset];
    }
}

// SetPixel assumes 32-bit pixels.

void sreTexture::SetPixel(int x, int y, unsigned int value) {
    data[y * width + x] = value;
}


// Simulate GPU texture lookup.

void sreTexture::TextureLookupNearest(float u, float v, Color& c) {
    int x = floorf(u * width);
    int y = floorf(v * height);
    if (x == width)
        x = width - 1;
    if (y == height)
        y = height - 1;
    unsigned int pixel = LookupPixel(x, y);
    int r = pixel & 0xFF;
    int g = (pixel >> 8) & 0xFF;
    int b = (pixel >> 16) & 0xFF;
    c.SetRGB888(r, g, b);
}

sreTexture *sreCreateCheckerboardTexture(int type, int w, int h, int bw, int bh,
Color color0, Color color1) {
    sreTexture *tex = new sreTexture(w, h);
    unsigned int pixel[2];
    pixel[0] = color0.GetRGBX8();
    pixel[1] = color1.GetRGBX8();
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int tile_on = ((x / bw) + (y / bh)) & 1;
            tex->SetPixel(x, y, pixel[tile_on]);
        }
    tex->width = w;
    tex->height = h;
    tex->bytes_per_pixel = 4;
    tex->format = TEXTURE_FORMAT_RAW;
    tex->type = type;
    tex->UploadGL(false);
    RegisterTexture(tex);
    return tex;
}

sreTexture *sreCreateStripesTexture(int type, int w, int h, int bh,
Color color0, Color color1) {
    sreTexture *tex = new sreTexture(w, h);
    unsigned int pixel[2];
    pixel[0] = color0.GetRGBX8();
    pixel[1] = color1.GetRGBX8();
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int tile_on = (y / bh) & 1;
            tex->SetPixel(x, y, pixel[tile_on]);
        }
    tex->width = w;
    tex->height = h;
    tex->bytes_per_pixel = 4;
    tex->format = TEXTURE_FORMAT_RAW;
    tex->type = type;
    tex->UploadGL(false);
    RegisterTexture(tex);
    return tex;
}

// Create a standard texture for fall-back and test purposes.

sreTexture *sreGetStandardTexture() {
    if (standard_texture == NULL)
        standard_texture = sreCreateCheckerboardTexture(TEXTURE_TYPE_LINEAR,
            256, 256, 16, 16, Color(0, 0, 0), Color(1.0f, 1.0f, 1.0f));
    if (standard_texture == NULL) {
        sreFatalError("Unable to upload standard texture (OpenGL problem?), giving up.");
    }
    return standard_texture;
}

sreTexture *sreGetStandardTextureWrapRepeat() {
   if (standard_texture_wrap_repeat == NULL)
        standard_texture_wrap_repeat = sreCreateCheckerboardTexture(TEXTURE_TYPE_NORMAL |
            SRE_TEXTURE_TYPE_FLAG_WRAP_REPEAT,
            256, 256, 16, 16, Color(0, 0, 0), Color(1.0f, 1.0f, 1.0f));
    if (standard_texture_wrap_repeat == NULL) {
        sreFatalError("Unable to upload standard texture (OpenGL problem?), giving up.");
    }
    return standard_texture_wrap_repeat;
}

float sreGetMaxAnisotropyLevel() {
   if (max_anisotropy <= 1.0001)
       return 1.0;
   return max_anisotropy;
}

static int nu_registered_textures = 0;
static int capacity = 0;
static sreTexture **registered_textures;

static void RegisterTexture(sreTexture *tex) {
    if (nu_registered_textures == capacity) {
        int new_capacity;
        if (capacity == 0)
            new_capacity = 256;
        else
            new_capacity = capacity * 4;
        sreTexture **new_registered_textures = new sreTexture *[new_capacity];
        if (capacity > 0) {
            memcpy(new_registered_textures, registered_textures,
                sizeof(sreTexture *) * nu_registered_textures);
            delete registered_textures;
        }
        registered_textures = new_registered_textures;
    }
    registered_textures[nu_registered_textures] = tex;
    nu_registered_textures++;
}

void sreScene::ApplyGlobalTextureParameters(int flags, int filter, float anisotropy) {
    sreMessage(SRE_MESSAGE_INFO,
        "Searching list of %d registered textures to apply new texture parameters.\n",
        nu_registered_textures);
    for (int i = 0; i < nu_registered_textures; i++) {
        switch (registered_textures[i]->type) {
        case TEXTURE_TYPE_NORMAL :
        case TEXTURE_TYPE_SRGB :
        case TEXTURE_TYPE_LINEAR :
            registered_textures[i]->ChangeParameters(flags, filter, anisotropy);
            break;
        }
    }
}

// Apply global texture settings to uncompressed texture (possibly reducing the size
// of the texture).

// The treshold texture area in pixels that triggers reduction in texture size.
#define SRE_TEXTURE_DETAIL_MEDIUM_AREA_THRESHOLD (1024 * 1024)
#define SRE_TEXTURE_DETAIL_LOW_AREA_THRESHOLD (256 * 256)
#define SRE_TEXTURE_DETAIL_VERY_LOW_AREA_THRESHOLD (128 * 128)

void sreTexture::CalculateTargetSize(int& target_width, int& target_height,
int &nu_levels_to_skip, int flags) {
    // Use heuristics to reduce the texture size when the relevant settings
    // are enabled.
    int area = width * height;
    int reduction_shift = 0;
    if (sre_internal_texture_detail_flags & SRE_TEXTURE_DETAIL_MEDIUM) {
        if (area >= SRE_TEXTURE_DETAIL_MEDIUM_AREA_THRESHOLD)
             reduction_shift = 1;
        if (area >= SRE_TEXTURE_DETAIL_MEDIUM_AREA_THRESHOLD * 16)
             reduction_shift = 2;
    }
    if (sre_internal_texture_detail_flags & SRE_TEXTURE_DETAIL_LOW) {
        if (area >= SRE_TEXTURE_DETAIL_LOW_AREA_THRESHOLD)
             reduction_shift = 1;
        if (area >= SRE_TEXTURE_DETAIL_LOW_AREA_THRESHOLD * 16)
             reduction_shift = 2;
    }
    if (sre_internal_texture_detail_flags & SRE_TEXTURE_DETAIL_VERY_LOW) {
        // Reduce 128x128 textures to 64x64.
        if (area >= SRE_TEXTURE_DETAIL_VERY_LOW_AREA_THRESHOLD)
             reduction_shift = 1;
        // Reduce 512x512 textures to 128x128,
        // Reduce 1024x1024 textures to 256x256.
        if (area >= SRE_TEXTURE_DETAIL_VERY_LOW_AREA_THRESHOLD * 16)
             reduction_shift = 2;
        // Reduce 2048x2048 and larger textures to 256x256.
        if (area >= SRE_TEXTURE_DETAIL_VERY_LOW_AREA_THRESHOLD * 256)
             reduction_shift = (int)floor(log2(sqrtf((float)area))) - 8;
    }
    int reduction_factor = 1 << reduction_shift;
    bool force_power_of_two = false;
    bool force_one_mipmap_level = false;
    if (!(sre_internal_texture_detail_flags & SRE_TEXTURE_DETAIL_NPOT)
    || ((flags & SRE_TEXTURE_TYPE_FLAG_WRAP_REPEAT) &&
    !(sre_internal_texture_detail_flags & SRE_TEXTURE_DETAIL_NPOT_WRAP)))
        force_power_of_two = true;
    if (!(sre_internal_texture_detail_flags & SRE_TEXTURE_DETAIL_NPOT_MIPMAPS))
        force_one_mipmap_level = true;
    if (!force_power_of_two) {
        // When NPOT textures are only supported with a single mipmap level, allow
        // taking advantage if there are more than one mipmap levels defined in the source
        // texture to potentially use a lower level one. If the selected reduction is not
        // available, the lowest mipmap level for the texture will be used (which may be the
        // only level if no mipmaps are defined in the source texture.
        target_width = width / reduction_factor;
        target_height = height / reduction_factor;
    }
    else {
        // Force power-of-two.
        target_width = width / reduction_factor;
        target_height = height / reduction_factor;
        target_width = 1 << (int)floorf(log2(target_width));
        target_height = 1 << (int)floorf(log2(target_height));
        // Since the rounding down to a power of two can result in a
        // total reduction factor almost two times greater than targeted,
        // use a one power of two larger size in extreme cases (in this case,
        // the  total reduction factor will be less than targeted).
        if (reduction_factor >= 2 &&(float)width / target_width > reduction_factor * 1.5f) {
            target_width *= 2;
            target_height *= 2;
            reduction_shift--;
        }
    }
    // For textures that were already a power of two, output the number of
    // mipmap levels that may be skipped (e.g. for compressed textures).
    nu_levels_to_skip = reduction_shift;
    // When the texture has to be uploaded to the GPU, check whether the dimensions are
    // within limits.
    if (!(flags & SRE_TEXTURE_TYPE_FLAG_NO_UPLOAD)) {
       float ratio_w = (float)target_width / sre_internal_max_texture_size;
       float ratio_h = (float)target_width / sre_internal_max_texture_size;
       float ratio_max = maxf(ratio_w, ratio_h);
       if (ratio_max > 1.0f) {
           target_width *= 1.0f / ratio_max;
           target_height *= 1.0f / ratio_max;
       }
       if (target_width == width && target_height == height)
           return;
       sreMessage(SRE_MESSAGE_INFO, "Reducing texture size from %dx%d to %dx%d.",
           width, height, target_width, target_height);
    }
}

static void AssignTextureToImage(sreTexture *tex, sreMipmapImage *image) {
    image->pixels = tex->data;
    image->width = tex->width;
    image->height = tex->height;
    image->extended_width = tex->width;
    image->extended_height = tex->height;
    image->alpha_bits = 0;          // 0 for no alpha, 1 if alpha is limited to 0 and 0xFF, 8 otherwise.
    image->nu_components = 3;       // Indicates the number of components.
    image->bits_per_component = 8;  // 8 or 16.
    image->is_signed = 0;           // 1 if the components are signed, 0 if unsigned.
    image->srgb = 0;                // Whether the image is stored in sRGB format.
    image->is_half_float = 0;       // The image pixels are combinations of half-floats. The pixel size is 64-bit.
}

static void AssignImageToTexture(sreMipmapImage *image, sreTexture *tex) {
    tex->data = image->pixels;
    tex->width = image->width;
    tex->height = image->height;
    tex->bytes_per_pixel = 4;
    if (image->alpha_bits == 8)
        tex->format = TEXTURE_FORMAT_RAW_RGBA8;
    else
        tex->format = TEXTURE_FORMAT_RAW_RGBA8;
    tex->type = TEXTURE_TYPE_NORMAL;
}

// Generate extra mipmap levels from Texture. Each new level will be allocated as a
// texture and stored in textures (which must be allocated as an array of pointers).
// The number of levels generated is indicated by the nu_levels arguments, zero
// indicates going down to the smallest possible mipmap level. The number of levels
// generated is returned in nu_levels.

void sreTexture::GenerateMipmapLevels(int starting_level, int& nu_levels,
sreTexture **textures) {
    for (int i = 0;;) {
        sreTexture *source_tex;
        if (i == 0)
            source_tex = this;
        else
            source_tex = textures[i - 1];
        textures[i] = new sreTexture;
        sreTexture *dest_tex = textures[i];
        sreMipmapImage source_image, dest_image;
        AssignTextureToImage(source_tex, &source_image);
        if (i == 0)
            generate_mipmap_level_from_original(&source_image, starting_level, &dest_image);
        else
            generate_mipmap_level_from_previous_level(&source_image, &dest_image);
        AssignImageToTexture(&dest_image, dest_tex);
        i++;
        if (nu_levels == 0) {
            if (dest_tex->width <= 1 || dest_tex->height <= 1) {
                nu_levels = i;
                break;
            }
        }
        else if (i >= nu_levels)
            break;
    }
    return;
}

// Apply texture detail settings to an uncompressed texture.

void sreTexture::ApplyTextureDetailSettings(int flags) {
    if (type == TEXTURE_TYPE_NORMAL_MAP)
        return;
    int target_width, target_height, levels_to_skip;
    CalculateTargetSize(target_width, target_height, levels_to_skip, flags);
    if (target_width == width && target_height == height)
        return;
    // Scale down when necessary.
    int count_target = CountPowersOfTwo(target_width, target_height);
    int count_source = CountPowersOfTwo(width, height);
    if (count_source < 2 && count_target == 2) {
        sreMessage(SRE_MESSAGE_INFO,
            "Texture size reduction from NPOT to POT not yet implemented.");
        return;
    }
    // In all remaining cases, 1 << levels_to_skip should be equivalent to the dividing factor
    // for the largest mipmap level to be generated.
    // Convert to 32-bit pixels when required.
    if (bytes_per_pixel == 3)
        ConvertFrom24BitsTo32Bits();
    sreTexture *textures[32];
    int nu_levels = 1;
    GenerateMipmapLevels(levels_to_skip, nu_levels, textures);
    // Copy the generated texture to the current texture.
    delete [] data;
    int saved_type = type;
    *this = *textures[0];
    type = saved_type;
}

// Create a text string texture.

sreTexture *sreCreateTextTexture(const char *str, sreFont *font) {
    int n = strlen(str);
    int char_width_in_pixels = font->tex->width / font->chars_horizontal;
    int w = n * char_width_in_pixels;
    sreTexture *tex = new sreTexture;
    tex->width = w;
    tex->height = font->char_height * font->tex->height;
    tex->nu_components = 1;
    tex->bit_depth = 8;
    tex->data = new unsigned int[(tex->width * tex->height + 3) / 4];
    tex->bytes_per_pixel = 1;
    tex->format = TEXTURE_FORMAT_RAW_R8;
    tex->type = TEXTURE_TYPE_TRANSPARENT;
    for (int i = 0; i < n; i++)
        for (int y = 0; y < tex->height; y++)
            for (int x = 0; x < char_width_in_pixels; x++) {
                int fonty = str[i] / font->chars_horizontal;
                int fontx = str[i] % font->chars_horizontal;
                unsigned char pix;
                // Assume any black font pixel (zero) is the background.
                if (font->tex->LookupPixel(fontx * char_width_in_pixels + x,
                fonty * tex->height + y) > 0)
                    // Map each foreground pixel to intensity of 1.0.
                    // The color is regulated by the emission color when a
                    // billboard object is used (which is multiplied by the
                    // texture (emission map) pixel).
                    pix = 0xFF;
                else
                    pix = 0;
                *((unsigned char *)tex->data + y * w + i * char_width_in_pixels + x) = pix;
            }
    tex->UploadGL(false);
    RegisterTexture(tex);
    return tex; 
}

