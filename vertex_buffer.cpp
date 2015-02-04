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
#define _USE_MATH_DEFINES
#include <math.h>
#include <malloc.h>
#include <float.h>
#ifdef OPENGL_ES2
#include <GLES2/gl2.h>
#endif
#ifdef OPENGL
#include <GL/glew.h>
#include <GL/gl.h>
#endif

#include "win32_compat.h"
#include "sre.h"
#include "sre_internal.h"
#include "shader.h"

const int sre_internal_attribute_size[SRE_NU_VERTEX_ATTRIBUTES] = {
    16,    // Vertex position (4D float)
    8,     // Texcoords (2D float)
    12,    // Normal (3D float)
    16,    // Tangent (4D float)
#ifdef COMPRESS_COLOR_ATTRIBUTE
    4,     // Compressed color (float)
#else
    12     // Color (3D float)
#endif
};

static GLenum GetAttributeUsage(int attribute_index, int dynamic_flags) {
    if (dynamic_flags & (1 << attribute_index))
        return GL_DYNAMIC_DRAW;
    else
        return GL_STATIC_DRAW;
}

// Interleaved vertex attribute buffer support.
// For every possible configuration of attributes (mask), precalculate the
// offsets of the attribute buffers. Be really pedantic and use char
// for storage.

static bool interleaved_offset_table_initialized = false;

char sre_internal_interleaved_offset_table[(1 << SRE_NU_VERTEX_ATTRIBUTES)][SRE_NU_VERTEX_ATTRIBUTES + 1];

static void sreGenerateInterleavedOffsetTable() {
    for (int i = 0; i < (1 << SRE_NU_VERTEX_ATTRIBUTES); i++) {
        int offset = 0;
        for (int j = 0; j < SRE_NU_VERTEX_ATTRIBUTES; j++)
            if (i & (1 << j)) {
                sre_internal_interleaved_offset_table[i][j + 1] = offset;
                offset += sre_internal_attribute_size[j];
            }
        // Store the stride.
        sre_internal_interleaved_offset_table[i][0] = offset;
    }
    interleaved_offset_table_initialized = true;
}

// For every possible configuration of attributes (mask), precalculate a list of attribute indices.
// The indices are stored efficiently with three bits for every attribute index. This allows
// up to eight different attributes.

static bool attribute_list_table_initialized = false;

unsigned int sre_internal_attribute_list_table[1 << SRE_NU_VERTEX_ATTRIBUTES];

static void sreGenerateAttributeListTable() {
    for (int i = 0; i < (1 << SRE_NU_VERTEX_ATTRIBUTES); i++) {
        unsigned int value = 0;
        int shift = 0;
        for (int j = 0; j < SRE_NU_VERTEX_ATTRIBUTES; j++)
            if (i & (1 << j)) {
                value |= j << shift;
                shift += 3;
            }
        sre_internal_attribute_list_table[i] = value;
    }
    attribute_list_table_initialized = true;
}

// Create non-interleaved vertex buffers. Apart from full initial GL upload of models,
// this function can also be used to selectively create new vertex buffers for one of the
// vertex attributes with the attribute_mask argument.
//
// If the vertex position attribute is set in the attribute mask, the position vertex
// attribute buffer will be buffered from the positions argument which must be an array
// of 4D vertex positions. When the shadow argument is true, it is assumed that positions
// contains extra extruded vertices.
//
// The dynamic_flags argument indicates whether a buffer should be configured as GL_DYNAMIC_DRAW
// (dynamic updates) instead of GL_STATIC_DRAW. It uses the same mask values as attribute_mask.

void sreLODModel::NewVertexBuffers(int attribute_mask, int dynamic_flags, Vector4D *positions,
bool shadow) {
    void *attribute_data[SRE_NU_VERTEX_ATTRIBUTES];
    attribute_data[SRE_ATTRIBUTE_POSITION] = positions;
    attribute_data[SRE_ATTRIBUTE_TEXCOORDS] = &texcoords[0];
    attribute_data[SRE_ATTRIBUTE_NORMAL] = &vertex_normal[0];
    attribute_data[SRE_ATTRIBUTE_TANGENT] = &vertex_tangent[0];
#ifdef COMPRESS_COLOR_ATTRIBUTE
    float *compressed_colors;
    if (attribute_mask & SRE_COLOR_MASK) {
        compressed_colors = new float[nu_vertices];
        for (int i = 0; i < nu_vertices; i++)
            compressed_colors[i] = colors[i].GetCompressed();
        attribute_data[SRE_ATTRIBUTE_COLOR] = &compressed_colors[0];
    }
#else
    attribute_data[SRE_ATTRIBUTE_COLOR] = &colors[0];
#endif
    for (int i = 0; i < SRE_NU_VERTEX_ATTRIBUTES; i++)
        if (attribute_mask & (1 << i)) {
            glGenBuffers(1, &GL_attribute_buffer[i]);
            glBindBuffer(GL_ARRAY_BUFFER, GL_attribute_buffer[i]);
            int buffer_size = nu_vertices * sre_internal_attribute_size[i];
            // Special case for vertex positions: when shadow volumes are supported,
            // the second half of the buffer contains extruded positions.
            if (i == SRE_ATTRIBUTE_POSITION && shadow)
                buffer_size *= 2;
            glBufferData(GL_ARRAY_BUFFER, buffer_size, attribute_data[i],
                GetAttributeUsage(i, dynamic_flags));
//            if (GetAttributeUsage(i, dynamic_flags) == GL_DYNAMIC_DRAW)
//                sreMessage(SRE_MESSAGE_INFO, "Dynamic draw buffer requested for model %d, attribute %d.",
//                    id, i);
            if (glGetError() != GL_NO_ERROR)
                sreFatalError("Error executing glBufferData.");
       }
#ifdef COMPRESS_COLOR_ATTRIBUTE
    if (attribute_mask & SRE_COLOR_MASK)
        delete [] compressed_colors;
#endif
}

// Create one new interleaved vertex buffer. The usage is always GL_STATIC_DRAW.

void sreLODModel::NewVertexBufferInterleaved(int attribute_mask,
Vector4D *positions, bool shadow) {
    if (!interleaved_offset_table_initialized)
        sreGenerateInterleavedOffsetTable();
    void *attribute_data[SRE_NU_VERTEX_ATTRIBUTES];
    attribute_data[SRE_ATTRIBUTE_POSITION] = positions;
    attribute_data[SRE_ATTRIBUTE_TEXCOORDS] = &texcoords[0];
    attribute_data[SRE_ATTRIBUTE_NORMAL] = &vertex_normal[0];
    attribute_data[SRE_ATTRIBUTE_TANGENT] = &vertex_tangent[0];
#ifdef COMPRESS_COLOR_ATTRIBUTE
    float *compressed_colors;
    if (attribute_mask & SRE_COLOR_MASK) {
        compressed_colors = new float[nu_vertices];
        for (int i = 0; i < nu_vertices; i++)
            compressed_colors[i] = colors[i].GetCompressed();
        attribute_data[SRE_ATTRIBUTE_COLOR] = &compressed_colors[0];
    }
#else
    attribute_data[SRE_ATTRIBUTE_COLOR] = &colors[0];
#endif
    // Interleaving position data with shadow volumes enabled is not yet supported
    // and wouldn't really make sense (the buffer would have unused space), but
    // be ready for it.
    int total_nu_vertices = nu_vertices;
    if ((attribute_mask & SRE_POSITION_MASK) && shadow)
        total_nu_vertices *= 2;
    // Create a buffer with properly formatted interleaved attribute data corresponding to
    // the attribute mask.
    int buffer_size = total_nu_vertices * SRE_GET_INTERLEAVED_STRIDE(attribute_mask);
    char *buffer = new char[buffer_size];
    if ((attribute_mask & SRE_POSITION_MASK) && shadow)
        // Clear because there will be some gaps that are unused. 
        memset(buffer, 0, buffer_size);
    // Set the interleaved buffer data one attribute at a time (contiguous writing might be
    // better, but not really important for this code path).
    for (int i = 0; i < SRE_NU_VERTEX_ATTRIBUTES; i++)
        if (attribute_mask & (1 << i)) {
            char *attribute_data_pointer = (char *)attribute_data[i];
            char *buffer_pointer = buffer +
                SRE_GET_INTERLEAVED_ATTRIBUTE_OFFSET(attribute_mask, i);
            int n = nu_vertices;
            if (shadow && i == SRE_ATTRIBUTE_POSITION)
                n = total_nu_vertices;
            for (;n > 0; n--) {
                // Using memcpy for small sizes is not optimal, but this is not a
                // critical code path.
                memcpy(buffer_pointer, attribute_data_pointer, sre_internal_attribute_size[i]);
                attribute_data_pointer += sre_internal_attribute_size[i];
                buffer_pointer += SRE_GET_INTERLEAVED_STRIDE(attribute_mask);
            }
        }
#ifdef COMPRESS_COLOR_ATTRIBUTE
    if (attribute_mask & SRE_COLOR_MASK)
        delete [] compressed_colors;
#endif
    SRE_GLUINT GL_interleaved_buffer;
    glGenBuffers(1, &GL_interleaved_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, GL_interleaved_buffer);
    glBufferData(GL_ARRAY_BUFFER, buffer_size, buffer, GL_STATIC_DRAW);
    if (glGetError() != GL_NO_ERROR)
        sreFatalError("Error executing glBufferData.");
    // Set the model OpenGL buffer IDs for the attributes, all referring to the
    // same interleaved vertex buffer.
    for (int i = 0; i < SRE_NU_VERTEX_ATTRIBUTES; i++)
        if (attribute_mask & (1 << i))
            GL_attribute_buffer[i] = GL_interleaved_buffer;
}

static bool OnlyOneAttributeSet(int attribute_mask) {
    int count = 0;
    for (int i = 0; i < SRE_NU_VERTEX_ATTRIBUTES; i++)
        if (attribute_mask & (1 << i)) {
            count++;
            if (count > 1)
                return false;
        }
    // Count can't be zero, see below.
    return true;
}

// Upload vertex attribute buffers to the GPU. Must be to be called once per model at start-up.
// (currently done by sreScene::PrepareForRendering()).
//
// A wide variety of vertex attribute sharing methods between models is supported; a model
// can optionally be configured to share some of the vertex attributes it uses with another
// model (share the GPU buffer).
//
// Additionally, interleaving vertex attribute data is supported. Apart form all vertex
// attributes being stored in one interleaved buffer, it is possible to interleave some
// attributes and not others. At least for the non-interleaved attributes, it also possible
// to share them with another model.
//
// In order to share attributes with another model, the sreLODModel must already have
// initialized the vertex attribute buffer for the shared attributes in
// sreLODObject::GL_attribute_buffer[]. The current sreLODObject::flags determines the
// attributes used by to the model. The caller should ensure that the combination of the
// already initalized (shared) attributes plus the attributes in attribute_mask (which
// will be newly allocated) is equal to sreLODObject::flags.

void sreLODModel::UploadToGPU(int attribute_mask, int dynamic_flags) {
    // Check that all attributes defined in attribute_mask are present in the model
    // (enabled in sreBaseModel::flags).
    if ((attribute_mask & flags) != attribute_mask)
        sreFatalError("Error (sreLODModel::UploadToGPU): Not all requested attributes are present the base model.");
    if (attribute_mask == 0)
        sreFatalError("Error (sreLODModel::UploadToGPU): attribute_mask = 0 (unexpected).");

    // This is not the best place to initialize this table (which is used when drawing objects).
    if (!attribute_list_table_initialized)
        sreGenerateAttributeListTable();

    bool shadow =
        (sre_internal_rendering_flags & SRE_RENDERING_FLAG_SHADOW_VOLUME_SUPPORT)
        && !(flags & SRE_LOD_MODEL_NO_SHADOW_VOLUME_SUPPORT)
        && (flags & SRE_LOD_MODEL_IS_SHADOW_VOLUME_MODEL);

    if (flags & SRE_LOD_MODEL_BILLBOARD) {
        // Special case for billboards; little has to be uploaded yet.
        glGenBuffers(1, &GL_attribute_buffer[SRE_ATTRIBUTE_POSITION]);
        if (flags & SRE_LOD_MODEL_LIGHT_HALO)
            glGenBuffers(1, &GL_attribute_buffer[SRE_ATTRIBUTE_NORMAL]);
        if (attribute_mask & SRE_TEXCOORDS_MASK) {
            // Any texture coordinates (for use with an emission map) have to be uploaded.
            glGenBuffers(1, &GL_attribute_buffer[SRE_ATTRIBUTE_TEXCOORDS]);
            glBufferData(GL_ARRAY_BUFFER, nu_vertices * sizeof(float) * 2, texcoords,
               GL_STATIC_DRAW);
        }
        // Set the non-interleaved attribute info (just the lower 8 bits are used).
        // The interleaved information is set to zero.
        attribute_info.attribute_masks = attribute_mask;
        if (nu_triangles == 0)
            // For single billboards, no triangles are allocated (they always consist
            // of a two triangle fan with the order of the vertex data).
            return;
        goto copy_indices;
    }

    int total_nu_vertices;
    sreLODModelShadowVolume *model_shadow_volume;
    if (shadow) {
        model_shadow_volume = (sreLODModelShadowVolume *)this;
        total_nu_vertices = nu_vertices * 2; // Times two for extruded vertices.
    }
    else
        total_nu_vertices = nu_vertices;

    Vector4D *positions_4D;
    if (attribute_mask & SRE_POSITION_MASK) {
        // Create 4D array for vertex position buffer from aligned 3D positions in
	// sreBaseModel geometry.
        positions_4D = new Vector4D[total_nu_vertices];
        // Copy vertex positions, adding a w component of 1.0 for the shaders.
        for (int i = 0; i < nu_vertices; i++)
            positions_4D[i] = Vector4D(vertex[i], 1.0f);

        // Create vertices extruded to infinity for shadow volumes.
        if (shadow) {
            for (int i = 0; i < nu_vertices; i++)
                // w = 0 for extruded vertices.
                positions_4D[i + nu_vertices] = Vector4D(vertex[i], 0.0f);
            model_shadow_volume->vertex_index_shadow_offset = nu_vertices;
        }
    }

    // Interleaving vertex attributes may increase GPU cache/memory performance, especially
    // on low-end GPUs.
    // Currently it isn't really optimal when extruded vertex positions for shadow volumes
    // are used, because the second half of the buffer will contain gaps for the non-position
    // attributes.

    // The current behaviour, when SRE_INTERLEAVED_BUFFERS_ENABLED is set, is to simply
    // combine all the model's attributes in one interleaved buffer. However, support is
    // in place for any combination of non-interleaved and up to three interleaved vertex
    // buffers.

    // When extruded shadow volume vertices are required, or dynamic_flags is set, no
    // interleaved buffers are created.
    if (sre_internal_interleaved_vertex_buffers_mode == SRE_INTERLEAVED_BUFFERS_ENABLED
    && dynamic_flags == 0 && !shadow && !OnlyOneAttributeSet(attribute_mask)) {
        // Interleave attribute data.
        NewVertexBufferInterleaved(attribute_mask, positions_4D, shadow);
        // Set the interleaved attribute info. Interleaved slot 0 is used, located
        // at bits 8-15. The non-interleaved information is set to zero.
        attribute_info.attribute_masks = attribute_mask << 8;
        goto finish;
    }

    // The attribute arrays for vertex normals, texcoords, tangents and colors in the
    // model data structure are already properly formatted to be forwarded to the GPU.

    NewVertexBuffers(attribute_mask, dynamic_flags, positions_4D, shadow);
    // Set the non-interleaved attribute info (just the lower 8 bits are used).
    // The interleaved information is set to zero.
    attribute_info.attribute_masks = attribute_mask;

finish :
    if (attribute_mask & SRE_POSITION_MASK)
        // 4D positions no longer required.
        delete [] positions_4D;

    sreMessage(SRE_MESSAGE_LOG,
        "sreLODModel::UploadToGPU: Uploading model %d, attribute_mask 0x%02X.",
        id, attribute_mask);

    // If the model is in any way instanced (at least one attribute shared), then we can
    // be sure that the triangle vertex indices are already present on the GPU and will
    // also be shared.
    if ((attribute_mask ^ flags) & attribute_mask) {
        if (shadow && (attribute_mask & SRE_POSITION_MASK))
            goto calculate_edges;
        // Finished.
        return;
    }

copy_indices:
    // Copy triangle vertex indices.
    unsigned short *triangle_vertex_indices;
    // Decide whether to use short indices (range 0 - 65535);
    // when PRIMITIVE_RESTART is allowed for drawing shadow volumes, the highest index
    // is reserved.
    int max_short_index;
    max_short_index = 65535;
#ifndef NO_PRIMITIVE_RESTART
    if (shadow && GLEW_NV_primitive_restart)
        max_short_index = 65534;
#endif
    if (total_nu_vertices > max_short_index) {
        // Keep new/delete happy by using unsigned short.
        triangle_vertex_indices = new unsigned short[nu_triangles * 3 * 2];
        unsigned int *indices = (unsigned int *)triangle_vertex_indices;
        for (int i = 0; i < nu_triangles; i++)
            for (int j = 0; j < 3; j++)
                indices[i * 3 + j] = triangle[i].vertex_index[j];
        GL_indexsize = 4;
    }
    else {
        triangle_vertex_indices = new unsigned short[nu_triangles * 3 * 2];
        unsigned short *indices16 = triangle_vertex_indices;
        for (int i = 0; i < nu_triangles; i++)
            for (int j = 0; j < 3; j++)
                indices16[i * 3 + j] = triangle[i].vertex_index[j];
        GL_indexsize = 2;
        sreMessage(SRE_MESSAGE_LOG,
            "Less or equal to %d vertices in object (including extruded shadow vertices), "
            "using 16-bit indices.", max_short_index + 1);
    }
    // Upload triangle vertex indices.
    glGenBuffers(1, &GL_element_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_element_buffer);
    sreCheckGLError("OpenGL error before element array buffer creation.\n");
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, nu_triangles * 3 * GL_indexsize,
        triangle_vertex_indices, GL_STATIC_DRAW);
    if (glGetError() != GL_NO_ERROR)
        sreFatalError("OpenGL error occurred during element array buffer creation.");
    delete [] triangle_vertex_indices;

    if (!shadow)
        return; // Finished.

calculate_edges :
    // Create edge array for shadow silhouette determination (shadow volumes).
    if (model_shadow_volume->nu_edges == 0)
        model_shadow_volume->CalculateEdges();
    else
        sreMessage(SRE_MESSAGE_WARNING,
            "Warning: sreLODModel::UploadToGPU: edges already calculated "
            "(shouldn't happen).");
}

void sreLODModel::DeleteFromGPU() {
    // Check the uninterleaved and up to three interleaved attribute masks.
    unsigned int mask = attribute_info.attribute_masks;
    for (int i = 0; i < 4; i++) {
         for (int j = 0; j < SRE_NU_VERTEX_ATTRIBUTES; j++)
             if ((mask & instance_flags) & (1 << j))
                 glDeleteBuffers(1, &GL_attribute_buffer[i]);
         mask >>= 8;
    }
    glDeleteBuffers(1, &GL_element_buffer);
}

// Billboarding (dynamic vertex buffers). Note that the vertex attribute for the
// billboard related shaders only has three components (not homogeneous with an
// added w = 1.0f), because shadow volumes do not apply.

void GL3SetBillboard(sreObject *so) {
    Point3D P = so->sphere.center;
    Vector3D right_vector = Cross(sre_internal_camera_vector, sre_internal_up_vector);
    Vector3D X = 0.5 * so->billboard_width * right_vector;
    Vector3D Y = 0.5 * so->billboard_height * sre_internal_up_vector;
    sreLODModel *m = so->model->lod_model[0];
    // A single billboard is set up as a triangle fan consisting of two triangles.
    m->vertex[0] = P + X + Y;
    m->vertex[1] = P - X + Y;
    m->vertex[2] = P - X - Y;
    m->vertex[3] = P + X - Y;
    // For light halos, also store the center in the normal attribute.
    if (so->flags & SRE_OBJECT_LIGHT_HALO)
        for (int i = 0; i < 4; i++)
            m->vertex_normal[i] = P;
    glBindBuffer(GL_ARRAY_BUFFER, m->GL_attribute_buffer[SRE_ATTRIBUTE_POSITION]);
    glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(float) * 4, m->vertex, GL_DYNAMIC_DRAW);
    if (so->flags & SRE_OBJECT_LIGHT_HALO) {
        glBindBuffer(GL_ARRAY_BUFFER, m->GL_attribute_buffer[SRE_ATTRIBUTE_NORMAL]);
        glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(float) * 3, m->vertex_normal, GL_DYNAMIC_DRAW);
    }
}

void GL3SetParticleSystem(sreObject *so) {
    Vector3D right_vector = Cross(sre_internal_camera_vector, sre_internal_up_vector);
    Vector3D X = 0.5 * so->billboard_width * right_vector;
    Vector3D Y = 0.5 * so->billboard_height * sre_internal_up_vector;
    sreLODModel *m = so->model->lod_model[0];
    // Set LOD model billboard vertex positions and, for halos, centers (stored as normal
    // attribute and duplicated for every vertex of a billboard).
    // Billboards are configured as triangles (six vertices define two triangles for each
    // billboard), but are indexed unlike single billboards.
    for (int i = 0; i < so->nu_particles; i++) {
        Point3D P = so->position + so->particles[i];
        m->vertex[i * 4] = P + X + Y;
        m->vertex[i * 4 + 1] = P - X + Y;
        m->vertex[i * 4 + 2] = P - X - Y;
        m->vertex[i * 4 + 3] = P + X - Y;
         if (so->flags & SRE_OBJECT_LIGHT_HALO)
            for (int j = 0; j < 4; j++)
                m->vertex_normal[i * 4 + j] = P;
    }
    m->nu_vertices = so->nu_particles * 4;
    m->nu_triangles = so->nu_particles * 2;
    // Upload vertex attribute data.
    glBindBuffer(GL_ARRAY_BUFFER, m->GL_attribute_buffer[SRE_ATTRIBUTE_POSITION]);
    glBufferData(GL_ARRAY_BUFFER, so->nu_particles * 4 * sizeof(float) * 4,
        m->position, GL_DYNAMIC_DRAW);
    
    if (so->flags & SRE_OBJECT_LIGHT_HALO) {
        glBindBuffer(GL_ARRAY_BUFFER, m->GL_attribute_buffer[SRE_ATTRIBUTE_NORMAL]);
        glBufferData(GL_ARRAY_BUFFER, so->nu_particles * 4 * sizeof(float) * 3,
            m->vertex_normal, GL_DYNAMIC_DRAW);
    }
}
