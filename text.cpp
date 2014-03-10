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
#ifdef OPENGL_ES2
#include <GLES2/gl2.h>
#endif
#ifdef OPENGL
#include <GL/glew.h>
#include <GL/gl.h>
#endif

#include "sre.h"
#include "sre_internal.h"
#include "shader.h"

// Font texture handling.

// Create a new font. Any 256-character texture laid out in a regular spaced character
// format should work, but a character width that is a power of two is recommended to
// avoid expensive integer divide operation when drawing text.

sreFont::sreFont(const char *texture_name, int chars_x, int chars_y) {
    tex = new sreTexture(texture_name, TEXTURE_TYPE_NORMAL |
        SRE_TEXTURE_TYPE_FLAG_KEEP_DATA);
    chars_horizontal = chars_x;
    shift = 0;
    for (int i = 0; i < 16; i++)
        if (chars_x == (1 << i)) {
            // Font width in characters is a power of two.
            shift = i;
            break;
        }
    chars_vertical = chars_y;
    char_width = 1.0f / (float)chars_x;
    char_height = 1.0f / (float)chars_y;
    tex->ChangeParameters(SRE_TEXTURE_FLAG_SET_FILTER,
        SRE_TEXTURE_FILTER_LINEAR, 1.0);
}

void sreFont::SetFiltering(int filtering) const {
    tex->ChangeParameters(SRE_TEXTURE_FLAG_SET_FILTER,
        filtering, 1.0);
}

sreFont *sreCreateSystemMemoryFont(const char *filename, int chars_x, int chars_y) {
    sreFont *font = new sreFont;
    font->tex = new sreTexture;
    font->tex->type = TEXTURE_TYPE_NORMAL | SRE_TEXTURE_TYPE_FLAG_KEEP_DATA |
        SRE_TEXTURE_TYPE_FLAG_NO_UPLOAD;
    font->tex->LoadPNG(filename, true);
    font->chars_horizontal = chars_x;
    font->shift = 0;
    for (int i = 0; i < 16; i++)
        if (chars_x == (1 << i)) {
            // Font width in characters is a power of two.
            font->shift = i;
            break;
        }
    font->chars_vertical = chars_y;
    font->char_width = 1.0f / (float)chars_x;
    font->char_height = 1.0f / (float)chars_y;
    return font;
}

#define MAX_TEXT_WIDTH 256  // In characters.

#if 0

// This older text implementation using an older shader with
// vertex buffer construction for each displayed text has been
// disabled in favour of a new implementation.

static sreFont *standard_font = NULL;
static sreFont *current_font;
// Extra horizontal space between characters or vertical between lines.
static float horizontal_spacing, vertical_spacing;
static Color text_color;
static int text_flags;
static bool text_active;
static Point2D *text_vertex_array;
static Point2D *text_UV_array;
static GLuint GL_text_position_buffer;
static GLuint GL_text_texcoords_buffer;

void sreInitializeTextEngine() {
    glGenBuffers(1, &GL_text_position_buffer);
    glGenBuffers(1, &GL_text_texcoords_buffer);
    current_font = NULL;
    text_vertex_array = new Point2D[MAX_TEXT_WIDTH * 6];
    text_UV_array = new Point2D[MAX_TEXT_WIDTH * 6];
    horizontal_spacing = 0;
    vertical_spacing = 0;
    text_color = Color(1.0, 1.0, 1.0);
    text_flags = 0;
    text_active = false;
}

// Change the current font. NULL resets the standard texture font, which is returned for
// reference.
//
// Theoretically it may be possible to use a 1D texture when text is always drawn in
// the expected horizontal orientation. With an enhanced shader, it may also be possible
// to draw a text line with a quad instead of a quad for each character.

sreFont *sreSetFont(sreFont *font) {
    if (font == NULL) {
        if (standard_font == NULL)
            standard_font = new sreFont("TextureFont", 16, 16);
        current_font = standard_font;
        return current_font;
    }
    current_font = font;
    return current_font;
}

static void ApplyBlendSettings(int flags) {
    if (flags & SRE_TEXT_FLAG_OPAQUE) {
        glDisable(GL_BLEND);
    }
    else {
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
    }
}

// Change text parameters. Can also be called within sreBeginText/sreEndText.

void sreSetTextParameters(float h_spacing, float v_spacing, Color color, int flags) {
    horizontal_spacing = h_spacing;
    vertical_spacing = v_spacing;
    if (text_active) {
        // Apply settings immediately when we are within sreBeginText/sreEndText.
        if (color != text_color)
            GL3InitializeTextShader(&color);
        if ((text_flags ^ flags) & SRE_TEXT_FLAG_OPAQUE) {
            // When the blending changing within sBeginText/sre/sreEndText, immediately
            // apply the setting.
           ApplyBlendSettings(flags);
        }
    }
    text_color = color;
    text_flags = flags;
}

void sreBeginText() {
    // If there no texture font was set, use the standard texture; loading is deferred
    // until now.
    if (current_font == NULL)
        sreSetFont(NULL);
    // In case demand loading is enabled, really postpone shader load until the first
    // text draw request (the regular start-up logo/FPS etc will force loading of the
    // shader.
    misc_shader[SRE_MISC_SHADER_TEXT1].Validate();
    glUseProgram(misc_shader[SRE_MISC_SHADER_TEXT1].program);
    GL3InitializeTextShader(&text_color);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, current_font->tex->opengl_id);
    ApplyBlendSettings(text_flags);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    // Maybe glDisable(GL_DEPTH_MASK)?
    text_active = true;
}

void sreEndText() {
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    text_active = false;
//    if (glGetError() != GL_NO_ERROR) {
//        printf("OpenGL error after DrawText\n");
//    }
}

// The text function automatically handles newlines encountered within the text string
// by starting a new line below the previous one.

void sreDrawText(const char *text, float x, float y, float char_width, float char_height) {
    float h_step = char_width + horizontal_spacing;
    float v_step = char_height + vertical_spacing;
    int i = 0;  // The index into the string.
again :
    int j = 0;  // The number of characters * 6 processed in one line of text.
    float cx = x;
    for (;; i++) {
        if (j == MAX_TEXT_WIDTH * 6)  // Line too long.
            break;
        unsigned int character = text[i];
        if (character == '\0' || character == '\n')
            break;
        Point2D top_left = Point2D(cx, y);
        Point2D top_right = Point2D(cx + char_width, y);
        Point2D bottom_left = Point2D(cx, y + char_height);
        Point2D bottom_right = Point2D(cx + char_width, y + char_height);
        text_vertex_array[j] = top_left;
        text_vertex_array[j + 1] = bottom_left;
        text_vertex_array[j + 2] = top_right;
        text_vertex_array[j + 3] = bottom_right;
        text_vertex_array[j + 4] = top_right;
        text_vertex_array[j + 5] = bottom_left;
        int font_column, font_row;
        if (current_font->shift > 0) {
            font_column = character & (current_font->chars_horizontal - 1);
            font_row = character >> current_font->shift;
        }
        else {
            font_column = character % current_font->chars_horizontal;
            font_row = character / current_font->chars_horizontal;
        }
        float uv_x, uv_y;
        uv_x = (float)(font_column) * current_font->char_width;
        uv_y = (float)(font_row) * current_font->char_height;
        Point2D UV_top_left = Point2D(uv_x, uv_y);
        Point2D UV_top_right = Point2D(uv_x + current_font->char_width, uv_y);
        Point2D UV_bottom_left = Point2D(uv_x, uv_y +  + current_font->char_height);
        Point2D UV_bottom_right = Point2D(uv_x + current_font->char_width, uv_y
            + current_font->char_height);
        text_UV_array[j] = UV_top_left;
        text_UV_array[j + 1] = UV_bottom_left;
        text_UV_array[j + 2] = UV_top_right;
        text_UV_array[j + 3] = UV_bottom_right;
        text_UV_array[j + 4] = UV_top_right;
        text_UV_array[j + 5] = UV_bottom_left;
        j += 6;
        cx += h_step;
    }
    if (j == 0) {
        // Empty line.
        if (text[i] == '\n') {
            y -= v_step;
            i++;
            goto again;
        }
        // Only remaining case is '\0'.
        return;
    }
    glBindBuffer(GL_ARRAY_BUFFER, GL_text_position_buffer);
    glBufferData(GL_ARRAY_BUFFER, 2 * sizeof(float) * j, &text_vertex_array[0], GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        2,
        GL_FLOAT,
        GL_FALSE, 
        0,
        (void *)0
        );
    glBindBuffer(GL_ARRAY_BUFFER, GL_text_texcoords_buffer);
    glBufferData(GL_ARRAY_BUFFER, 2 * sizeof(float) * j, &text_UV_array[0], GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        2,
        GL_FLOAT,
        GL_FALSE,
        0,
        (void *)0
        );
    glDrawArrays(GL_TRIANGLES, 0, j);
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);

    // If the previous line was too long, look for a newline.
    if (j == MAX_TEXT_WIDTH * 6) {
        for (;; i++) {
            if (text[i] == '\0')
                return;
            if (text[i] == '\n') {
                // Next line.
                y += v_step;
                i++;
                goto again;
            }
        }
    }
    else if (text[i] == '\n') {
        y += v_step;
        i++;
        goto again;
    }
}

// Draw a horizontally centered single line text within the horizontal bounds [x, x + w].
// If it doesn't fit, the font size is reduced.

void sreDrawTextCentered(const char *text, float x, float w, float y, float char_width,
float char_height) {
    int n = strlen(text);
    float text_w = char_width * n;
    if (text_w > w) {
        char_width *= w / text_w;
    }
    else
        x += (w - text_w) * 0.5;
    sreDrawText(text, x, y, char_width, char_height);
}

#endif


// Direct GL blending mode setting, useful for image and text drawing
// functions.

void sreSetImageBlendingMode(int mode) {
    if (mode == SRE_IMAGE_BLEND_OPAQUE) {
        glDisable(GL_BLEND);
    }
    else {
        /// Additive.
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
    }
}

// The following are sreImageShaderInfo member functions that are shared between image and text
// shader implementations.

// Simple static position vertex attribute buffer. Shared between image
// and text shaders. Two triangles form a quad covering the whole texture/area.
//
// To improve interpolation quality, we can subdivide the area and increase the
// number of triangles. It is transparent to the shader.

static const GLfloat image_position_array_1x1[12] = {
    0.0f, 0.0f,  // Top-left.
    0.0f, 1.0f,  // Bottom-left.
    1.0f, 0.0f,  // Top-right.
    1.0f, 1.0f,  // Bottom-right.
    1.0f, 0.0f,  // Top-right.
    0.0f, 1.0f   // Bottom-left.
};

// Image position vertex buffer information has the following fields:
// - Number of horizontal subdivision (w)
// - Number of vertical subdivisions (h)
// - Number of vertices (should be equal to w * h * 6)
// - Reserved

static const int image_position_array_info[SRE_NU_IMAGE_POSITION_BUFFERS][4] = {
    { 1,  1, 6,           0 },   // 0: 1x1: Standard
    { 4,  4, 4 * 4 * 6,   0 },   // 1: 4x4: Increase interpolation quality for images.
    { 16, 1, 16 * 1 * 6,  0 }    // 2: 16x1: Suitable for the text shader.
};

static GLfloat *GeneratePositionArray(int i) {
    int w = image_position_array_info[i][0];
    int h = image_position_array_info[i][1];
    int nu_vertices = image_position_array_info[i][2];
    GLfloat *array = new GLfloat[nu_vertices * 2];
    // To keep shared vertices identical, first compute all possible
    // position x and y coordinates in the grid, and then use them
    // when creating vertex positions.
    float *pos_x = new float[w + 1];
    float *pos_y = new float[h + 1];
    for (int x = 0; x <= w; x++)
        pos_x[x] = (float)((double)x / (double)w);
    for (int y = 0; y <= h; y++)
        pos_y[y] = (float)((double)y / (double)h);
    int count = 0;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            // Use the image_position_array_1x1 vertices as a reference
            // (use the same order).
            for (int j = 0; j < 12; j += 2) {
                if (image_position_array_1x1[j] > 0.5)
                    array[count] = pos_x[x + 1];
                else
                    array[count] = pos_x[x];
                if (image_position_array_1x1[j + 1] > 0.5)
                    array[count + 1] = pos_y[y + 1];
                else
                    array[count + 1] = pos_y[y];
                count += 2;
            }
    delete [] pos_x;
    delete [] pos_y;
    return array;
}

static const GLfloat *image_position_array[SRE_NU_IMAGE_POSITION_BUFFERS];

static int sre_image_position_buffers_initialized = 0;
static SRE_GLUINT GL_image_position_buffer[3];

void sreImageShaderInfo::ValidateImagePositionBuffers(int requested_buffer_flags) const {
    for (int i = 0; i < SRE_NU_IMAGE_POSITION_BUFFERS; i++)
        // Is this position buffer requested?
        if (requested_buffer_flags & (1 << i))
            // Did we already create it?
            if ((sre_image_position_buffers_initialized & (1 << i)) == 0) {
                // Create the position attributes.
                if (i == SRE_IMAGE_POSITION_BUFFER_1X1)
                    image_position_array[0] = &image_position_array_1x1[0];
                else
                    image_position_array[i] = GeneratePositionArray(i);
                glGenBuffers(1, &GL_image_position_buffer[i]);
                glBindBuffer(GL_ARRAY_BUFFER, GL_image_position_buffer[i]);
                glBufferData(GL_ARRAY_BUFFER, 2 * sizeof(float) *
                    image_position_array_info[i][2],
                    image_position_array[i], GL_STATIC_DRAW);
                sre_image_position_buffers_initialized |= (1 << i);
                if (sre_internal_debug_message_level >= 2)
                    printf("Generated %dx%d (%d triangle) position vertex buffer "
                        "for image/text shaders.\n",
                        image_position_array_info[i][0], image_position_array_info[i][1],
                        image_position_array_info[i][2] / 3);
            }
}

static Vector4D default_colors[2] = {
    Vector4D(1.0f, 1.0f, 1.0f, 1.0f),
    Vector4D(0, 0, 0, 0)
};

void sreImageShaderInfo::Initialize(int buffer_flags) {
    ValidateImagePositionBuffers(buffer_flags);
    // Initialize any field in image_shader_info that has not yet
    // been initialized with default values.
    if ((update_mask & SRE_IMAGE_SET_COLORS) == 0) {
        mult_color = default_colors[0];
        add_color = default_colors[1];
        update_mask |= SRE_IMAGE_SET_COLORS;
    }
    if ((update_mask & SRE_IMAGE_SET_TRANSFORM) == 0) {
        uv_transform.SetIdentity();
        update_mask |= SRE_IMAGE_SET_TRANSFORM;
    }
    if ((update_mask & SRE_IMAGE_SET_TEXTURE) == 0) {
        // No texture, this shouldn't happen
        sreTexture *tex = sreGetStandardTexture();
        opengl_id = tex->opengl_id;
        source_flags = 0;
        update_mask |= SRE_IMAGE_SET_TEXTURE;
    }
    // The texture array index isn't needed (if a texture array is configured
    // the update_mask would already contain the update bit).
}

void sreImageShaderInfo::SetSource(int set_mask, SRE_GLUINT _opengl_id,
int _array_index) {
    if (set_mask & SRE_IMAGE_SET_TEXTURE) {
        opengl_id = _opengl_id;
        // Make sure any previous source-related update mask bit are cleared.
        update_mask &= ~(SRE_IMAGE_SET_TEXTURE_ARRAY_INDEX
            | SRE_IMAGE_SET_ONE_COMPONENT_SOURCE);
        update_mask |= SRE_IMAGE_SET_TEXTURE;
        source_flags = 0;
        // Texture source configuration.
        if (set_mask & SRE_IMAGE_SET_TEXTURE_ARRAY_INDEX)
            source_flags |= SRE_IMAGE_SOURCE_FLAG_TEXTURE_ARRAY;
        if (set_mask & SRE_IMAGE_SET_ONE_COMPONENT_SOURCE)
            source_flags |= SRE_IMAGE_SOURCE_FLAG_ONE_COMPONENT_SOURCE;
    }
    if (set_mask & SRE_IMAGE_SET_TEXTURE_ARRAY_INDEX) {
        array_index = _array_index;
        update_mask |= SRE_IMAGE_SET_TEXTURE_ARRAY_INDEX;
    }
}

// Select a vertex position buffer that has a suitable number of subdivisions
// given the area size. At the moment this assume that 1x1, 16x1 and 4x4
// buffers are always available (which is true for the text shader).
// The image shader currently always uses the 1x1 buffer.

static inline int SelectPositionBuffer(float w, float h) {
    // Just return the single quad buffer for small areas, and use the
    // 16x1 buffer for anything that looks like text (wide).
    // Otherwise, use the 4x4 buffer.
    if (w < 0.1f && h < 0.1f)
        return SRE_IMAGE_POSITION_BUFFER_1X1;
    if (w >= h * 4.0f)
        return SRE_IMAGE_POSITION_BUFFER_16X1;
    return SRE_IMAGE_POSITION_BUFFER_4X4;
};

// Finish drawing a 2D image. The texture or texture array has to be bound to
// texture unit 0 before calling this function. The relevant shader (for textures,
// texture arrays, or text) must be activated and initialized with parameters such
// as screen position, size and color.
// The position vertex buffers used neatly subdivide a 1.0 x 1.0 area. Enhanced
// texture precision can be gained by using a finer-grained buffer.

static void sreFinishDrawing2DTexture(int buffer_index) {
    // Note: Blending settings are not touched, but can be changed at any time.
    // It is assumed the rendering engine has set the appropriate flags to disable
    // back-face culling and depth buffer operation.
    glBindBuffer(GL_ARRAY_BUFFER, GL_image_position_buffer[buffer_index]);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        2,
        GL_FLOAT,
        GL_FALSE, 
        0,
        (void *)0
        );
    glDrawArrays(GL_TRIANGLES, 0, image_position_array_info[buffer_index][2]);
    glDisableVertexAttribArray(0);
}


// Image shader.

static bool sre_image_initialized;
static sreImageShaderInfo image_shader_info;

void sreInitializeImageEngine() {
    sre_image_initialized = false;
    // Although we really want to defer initialization, we do need to
    // clear the update mask.
    image_shader_info.update_mask = 0;

    sreInitializeShaders(SRE_SHADER_MASK_IMAGE);
}

void sreSetImageParameters(int set_mask, const Vector4D *colors, const Matrix3D *m) {
    if (set_mask & SRE_IMAGE_SET_COLORS) {
        if (colors == NULL) {
            image_shader_info.mult_color = default_colors[0];
            image_shader_info.add_color = default_colors[1];
        }
        else {
            image_shader_info.mult_color = colors[0];
            image_shader_info.add_color = colors[1];
            image_shader_info.update_mask |= SRE_IMAGE_SET_COLORS;
        }
    }
    if (set_mask & SRE_IMAGE_SET_TRANSFORM) {
        if (m == NULL)
            image_shader_info.uv_transform.SetIdentity();
        else
            image_shader_info.uv_transform = *m;
        image_shader_info.update_mask |= SRE_IMAGE_SET_TRANSFORM;
    }
}

void sreSetImageSource(int set_mask, SRE_GLUINT opengl_id,
int array_index) {
    image_shader_info.SetSource(set_mask, opengl_id, array_index);
}

void sreDrawImage(float x, float y, float w, float h) {
    if (!sre_image_initialized) {
        image_shader_info.Initialize(
            SRE_IMAGE_POSITION_BUFFER_FLAG_1X1 |
            SRE_IMAGE_POSITION_BUFFER_FLAG_16X1 |
            SRE_IMAGE_POSITION_BUFFER_FLAG_4X4);
        sre_image_initialized = true;
    }
    CHECK_GL_ERROR("Error before GL3InitializeImageShader()\n");
    Vector4D rect = Vector4D(x, y, w, h);
    GL3InitializeImageShader(image_shader_info.update_mask | SRE_IMAGE_SET_RECTANGLE,
        &image_shader_info, &rect);
    image_shader_info.update_mask = 0;
    CHECK_GL_ERROR("Error after GL3InitializeImageShader()\n");
    sreFinishDrawing2DTexture(SRE_IMAGE_POSITION_BUFFER_1X1);
}

// Text shader API. Efficient text shader that shares some of the structures with the
// image shader.

static bool sre_text_initialized;
static sreTextShaderInfo text_shader_info;
static sreFont *standard_font_32x8;
static sreFont *current_font_32x8;

void sreInitializeTextEngine() {
    sre_text_initialized = false;
    standard_font_32x8 = NULL;
    current_font_32x8 = NULL;
    text_shader_info.update_mask = 0;

    sreInitializeShaders(SRE_SHADER_MASK_TEXT);
}

void sreSetTextParameters(int set_mask, const Vector4D *colors, const Vector2D *font_size) {
    if (set_mask & SRE_IMAGE_SET_COLORS) {
        if (colors == NULL) {
            text_shader_info.mult_color = default_colors[0];
            text_shader_info.add_color = default_colors[1];
        }
        else {
            text_shader_info.mult_color = colors[0];
            text_shader_info.add_color = colors[1];
            text_shader_info.update_mask |= SRE_IMAGE_SET_COLORS;
        }
    }
    if (set_mask & SRE_TEXT_SET_FONT_SIZE) {
        text_shader_info.screen_size_in_chars = Vector2D(1.0f / font_size->x, 1.0f / font_size->y);
        text_shader_info.update_mask |= SRE_TEXT_SET_SCREEN_SIZE_IN_CHARS;
    }
}

void sreSetTextSource(int set_mask, SRE_GLUINT opengl_id, int array_index) {
    text_shader_info.SetSource(set_mask, opengl_id, array_index);
}

sreFont *sreSetFont(sreFont *font) {
    if (font == NULL) {
        if (standard_font_32x8 == NULL)
            standard_font_32x8 = new sreFont("Lat2-TerminusBold32x16", 32, 8);
        current_font_32x8 = standard_font_32x8;
        text_shader_info.font_format = SRE_FONT_FORMAT_32X8;
    }
    else
        current_font_32x8 = font;

    sreSetTextSource(SRE_IMAGE_SET_TEXTURE, current_font_32x8->tex->opengl_id, 0);
    return current_font_32x8;
}

sreFont *sreGetStandardFont() {
    if (standard_font_32x8 == NULL)
       standard_font_32x8 = new sreFont("Lat2-TerminusBold32x16", 32, 8);
    return standard_font_32x8;
}

// Draw text with current font size with string length specified.

void sreDrawTextN(const char *string, int n, float x, float y) {
    if (n == 0)
        return;
    if (!sre_text_initialized) {
        if (current_font_32x8 == NULL)
            sreSetFont(NULL); // Set default font.
        text_shader_info.Initialize(
            SRE_IMAGE_POSITION_BUFFER_FLAG_1X1 |
            SRE_IMAGE_POSITION_BUFFER_FLAG_4X4 |
            SRE_IMAGE_POSITION_BUFFER_FLAG_16X1);
        sre_text_initialized = true;
    }
    CHECK_GL_ERROR("Error before GL3InitializeTextShader()\n");
    // Note: Would be better to get font size without division.
    float h = 1.0f / text_shader_info.screen_size_in_chars.y;
    for (;;) {
        int text_request_length = n;
        if (text_request_length > SRE_TEXT_MAX_REQUEST_LENGTH)
            text_request_length = SRE_TEXT_MAX_REQUEST_LENGTH;
        float w = (float)text_request_length / text_shader_info.screen_size_in_chars.x;
        Vector4D rect = Vector4D(x, y, w, h);
        GL3InitializeTextShader(text_shader_info.update_mask | SRE_IMAGE_SET_RECTANGLE |
            SRE_TEXT_SET_STRING, &text_shader_info, &rect, string, text_request_length);
        text_shader_info.update_mask = 0;
        CHECK_GL_ERROR("Error after GL3InitializeTextShader()\n");
        int buffer_index = SelectPositionBuffer(w, h);
        sreFinishDrawing2DTexture(buffer_index);
//        printf("%d characters drawn.\n", text_request_length);
        n -= text_request_length;
        if (n == 0)
            break;
        string += text_request_length;
        x += w;
    }
}

// Draw text with the current font size.
void sreDrawText(const char *string, float x, float y) {
    int n = strlen(string);
    sreDrawTextN(string, n, x, y);
}

// Draw text horizontally centered within [x, x + w]. If it doesn't fit,
// the font size is temporarily adjusted so that it fits.

void sreDrawTextCentered(const char *text, float x, float y, float w) {
    int n = strlen(text);
    float char_width = 1.0f / text_shader_info.screen_size_in_chars.x;
    float char_height = 1.0f / text_shader_info.screen_size_in_chars.y;
    float text_w = char_width * n;
    if (text_w > w) {
        float original_char_width = char_width;
        char_width *= w / text_w;
        Vector2D font_size = Vector2D(char_width, char_height);
        sreSetTextParameters(SRE_TEXT_SET_FONT_SIZE, NULL, &font_size);
        sreDrawTextN(text, n, x, y);
        font_size.Set(original_char_width, char_height);
        sreSetTextParameters(SRE_TEXT_SET_FONT_SIZE, NULL, &font_size);
    }
    else {
        sreDrawTextN(text, n, x + (w - text_w) * 0.5f, y);
    }
}
