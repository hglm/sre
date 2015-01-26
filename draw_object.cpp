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

// Drawing objects (OpenGL).

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

// Calculate the level of detail model to use.

sreLODModel *sreCalculateLODModel(const sreObject& so) {
    sreModel *m = so.model;
    if (so.lod_flags & SRE_LOD_FIXED) {
        return m->lod_model[so.min_lod_level];
    }
    if (so.max_lod_level > 0) {
        float w = Dot(sre_internal_view_projection_matrix.GetRow(3), so.sphere.center);
        if (w <= 0.0001f)
            return m->lod_model[so.min_lod_level];
        float size = fabsf(so.sphere.radius * 2.0f / w);
        sreLODModel *new_m = m->lod_model[so.min_lod_level];
        // Compound the object's threshold scaling with that of the model.
        float threshold_scaling = so.lod_threshold_scaling * m->lod_threshold_scaling;
        if (so.min_lod_level + 1 <= so.max_lod_level
        && size < SRE_LOD_LEVEL_1_THRESHOLD * threshold_scaling) {
            new_m = m->lod_model[1 + so.min_lod_level];
            if (so.min_lod_level + 2 <= so.max_lod_level
            && size < SRE_LOD_LEVEL_2_THRESHOLD * threshold_scaling) {
                new_m = m->lod_model[2 + so.min_lod_level];
                if (so.min_lod_level + 3 <= so.max_lod_level
                && size < SRE_LOD_LEVEL_3_THRESHOLD * threshold_scaling) {
                    new_m = m->lod_model[3 + so.min_lod_level];
                }
            }
        }
        return new_m;
    }
    return m->lod_model[0];
}


// Vertex shader attribute layout:
//
// layout (location = 0) in vec4 position_in;
// layout (location = 1) in vec2 texcoord_in;
// layout (location = 2) in vec3 normal_in;
// layout (location = 3) in vec4 tangent_in;
// layout (location = 4) in vec3 color_in;

// Set up OpenGL shader vertex attribute pointers.
// We use a table of efficiently encoded table of attribute lists
// (sre_internal_attribute_list_table) to iterate the used attributes
// based on mask values.

void sreLODModel::SetupAttributesNonInterleaved(sreObjectAttributeInfo *info) const {
    unsigned int list = sre_internal_attribute_list_table[info->attribute_masks & 0xFF];
    // The number of attributes in the list is guaranteed to be >= 1.
    do {
        int attribute = list & 7;
        glEnableVertexAttribArray(attribute);
        glBindBuffer(GL_ARRAY_BUFFER, GL_attribute_buffer[attribute]);
        glVertexAttribPointer(
            attribute,
            sre_internal_attribute_size[attribute] >> 2, // Number of floats per vertex.
            GL_FLOAT,           // Data type.
            GL_FALSE,           // Should the GPU normalize integer values?
            0,                  // Stride  (0 is contiguous).
            (void *)0	        // Buffer offset in bytes.
            );
        list >>= 3;
    } while (list != 0);
}

// Set up interleaved attributes. Multiple sets (up to three) of interleaved
// attributes are supported.

void sreLODModel::SetupAttributesInterleaved(sreObjectAttributeInfo *info) const {
    unsigned int mask = info->attribute_masks >> 8;
    unsigned int model_mask = info->model_attribute_masks >> 8;
    // The number of non-empty slots is guaranteed to be >= 1.
    do {
        // Get the stride and offsets of the vertex attribute buffer based on
        // the model's attribute mask info.
        int m = model_mask & 0xFF;
        int stride = SRE_GET_INTERLEAVED_STRIDE(m);
        char *offsets = SRE_GET_INTERLEAVED_OFFSET_LIST(m);
        // Get the list of attributes that are needed for the object from
        // this vertex attribute buffer.
        unsigned int list = sre_internal_attribute_list_table[mask & 0xFF];
        // The number of attributes in the list is guaranteed to be >= 1.
        int i = 0;
        do {
            int attribute = list & 7;
            glEnableVertexAttribArray(attribute);
            glBindBuffer(GL_ARRAY_BUFFER, GL_attribute_buffer[attribute]);
            glVertexAttribPointer(
                attribute,
                sre_internal_attribute_size[attribute] >> 2, // Number of floats per vertex.
                GL_FLOAT,           // Data type.
                GL_FALSE,           // Should the GPU normalize integer values?
                stride,             // Stride  (0 is contiguous).
                (void *)offsets[i]  // Buffer offset in bytes.
                );
            i++;
           list >>= 3;
        } while (list != 0);
        model_mask >>= 8;
        mask >>= 8;
    } while (mask != 0);
}

static void GL3SetGLFlags(sreObject *so) {
    if (so->render_flags & SRE_OBJECT_INFINITE_DISTANCE) {
        glDepthMask(GL_FALSE);
    }
    if (so->render_flags & SRE_OBJECT_NO_BACKFACE_CULLING)
        glDisable(GL_CULL_FACE);
    if (so->render_flags & SRE_OBJECT_TRANSPARENT_EMISSION_MAP) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
#ifndef OPENGL_ES2
    if (so->render_flags & SRE_OBJECT_TRANSPARENT_TEXTURE) {
        // Disable writing to the color and depth buffers for alpha values <= 0.1.
        glAlphaFunc(GL_GREATER, 0.1f);
        glEnable(GL_ALPHA_TEST);
    }
#endif
}

static void GL3ResetGLFlags(sreObject *so) {
    if (so->render_flags & SRE_OBJECT_INFINITE_DISTANCE) {
        glDepthMask(GL_TRUE);
    }
    if (so->render_flags & SRE_OBJECT_NO_BACKFACE_CULLING)
        glEnable(GL_CULL_FACE);
    if (so->render_flags & SRE_OBJECT_TRANSPARENT_EMISSION_MAP) {
        glDisable(GL_BLEND);
    }
#ifndef OPENGL_ES2
    if (so->render_flags & SRE_OBJECT_TRANSPARENT_TEXTURE) {
        glDisable(GL_ALPHA_TEST);
    }
#endif
}

void sreDrawObjectLightHalo(sreObject *so) {
    // Initialize the shader.
    sreInitializeObjectShaderLightHalo(*so);
    sreLODModel *m = so->model->lod_model[0];
    // Disable writing into the depth buffer (when a large object is drawn
    // afterwards that is partly behind the transparent halo, it should be
    // visible through the halo).
    glDepthMask(GL_FALSE);
    // Enable a particular kind of blending.
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
   // The normal buffer is used for the centers of the halo billboards.
    glEnableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, m->GL_attribute_buffer[SRE_ATTRIBUTE_NORMAL]);
    glVertexAttribPointer(
        2,
        3,
        GL_FLOAT,
        GL_FALSE,
        0,
        (void *)0
        );
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, m->GL_attribute_buffer[SRE_ATTRIBUTE_POSITION]);
    glVertexAttribPointer(
        0,
        4,
        GL_FLOAT,
        GL_FALSE,
        0,
        (void *)0
    );
    if (so->flags & SRE_OBJECT_PARTICLE_SYSTEM) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->GL_element_buffer);
        glDrawElements(GL_TRIANGLES, m->nu_triangles * 3, GL_UNSIGNED_SHORT, (void *)(0));
    }
    else
        // Draw a triangle fan consisting of two triangles from the still bound vertex position buffer.
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(2);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

// Draw billboard object (single or particle system).

void sreDrawObjectBillboard(sreObject *so) {
    // Initialize the shader.
    sreInitializeObjectShaderBillboard(*so);
    sreLODModel *m = so->model->lod_model[0];
    if (so->render_flags & SRE_OBJECT_INFINITE_DISTANCE) {
        glDepthMask(GL_FALSE);
    }
    if (so->render_flags & SRE_OBJECT_TRANSPARENT_EMISSION_MAP) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, m->GL_attribute_buffer[SRE_ATTRIBUTE_POSITION]);
    glVertexAttribPointer(
        0,
        4,
        GL_FLOAT,
        GL_FALSE,
        0,
        (void *)0
    );
    if (so->flags & SRE_OBJECT_PARTICLE_SYSTEM) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->GL_element_buffer);
        glDrawElements(GL_TRIANGLES, m->nu_triangles * 3, GL_UNSIGNED_SHORT, (void *)(0));
    }
    else
        // Draw a triangle fan consisting of two triangles from the still bound vertex position buffer.
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glDisableVertexAttribArray(0);
    if (so->render_flags & SRE_OBJECT_INFINITE_DISTANCE) {
        glDepthMask(GL_TRUE);
    }
    if (so->render_flags & SRE_OBJECT_TRANSPARENT_EMISSION_MAP)
        glDisable(GL_BLEND);
}

void sreObjectAttributeInfo::Set(unsigned int object_attribute_mask,
const sreAttributeInfo& model_attribute_info) {
    // Set the non-interleaved (regular) attribute information.
    unsigned int model_attribute_bits = model_attribute_info.attribute_masks;
    // Set bits 0-7 of attribute_masks (non-interleaved mask; the interleaved slots
    // are cleared).
    attribute_masks = object_attribute_mask & (model_attribute_bits & 0xFF);

    // Return when there are no interleaved attributes.
    if (attribute_masks == object_attribute_mask)
        return;

    int remaining_object_attribute_mask = object_attribute_mask ^ attribute_masks;

    // Set the interleaved attribute info for up to three slots.
    // Reset the auxilliary object slot information holding model buffer masks.
    model_attribute_masks = 0;
    int object_slot = 0;
    // Iterate model interleaved buffer slots.
    for (int slot = 0;; slot++) {
        unsigned int model_slot_mask = (model_attribute_bits >> (8 + slot * 8)) & 0xFF;
        if (model_slot_mask == 0)
            // We have processed all of the model's interleaved attribute buffer slots.
            // This shouldn't happen since all the object's needed attributes should
            // already have been processed, but check anyway.
            return;
        // Check whether any of the object's attributes is included in this model slot.
        int attributes_present = remaining_object_attribute_mask & model_slot_mask;
        if (attributes_present != 0) {
            // If so, add a new slot in the object's interleaved attribute information.
            attribute_masks |= attributes_present << (8 + object_slot * 8);
            // Also store the model's full attribute mask for this model slot in the
            // object slot's auxilliary attribute information.
            model_attribute_masks |= model_slot_mask << (8 + object_slot * 8);
            // Clear the object attribute bits that we just handled.
            remaining_object_attribute_mask ^= attributes_present;
            if (remaining_object_attribute_mask == 0)
                // All the needed object attributes have been covered.
                return;
            object_slot++;
        }
    }
}

// This function issue the actual draw commands. The attribute information in info must be
// initialized.

static void sreFinishDrawingObject(sreObject *so, sreLODModel *m,
sreObjectAttributeInfo *info) {
    if (info->attribute_masks & 0xFF)
        m->SetupAttributesNonInterleaved(info);
    if (info->attribute_masks & 0xFFFFFF00)
        m->SetupAttributesInterleaved(info);

#ifdef DEBUG_RENDER_LOG
    if (sre_internal_debug_message_level >= SRE_MESSAGE_VERBOSE_LOG) {
        sreMessage(SRE_MESSAGE_VERBOSE_LOG,
            "sreDrawObject: Drawing elements, %d triangles (%d vertices), %d meshes.\n",
            m->nu_triangles, m->nu_triangles * 3, m->nu_meshes);
        fflush(stdout);
    }
#endif

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->GL_element_buffer);
    // Multiple meshes are supported, which can have different textures.
    // Multiple meshes are currently only generated by assimp-imported objects.
    for (int i = 0; i < m->nu_meshes; i++) {
        int vertex_offset = 0;
        int nu_vertices = m->nu_triangles * 3;
        if (m->nu_meshes > 1) {
            vertex_offset = m->mesh[i].starting_vertex;
            nu_vertices = m->mesh[i].nu_vertices;
//            printf("mesh = %d; start = %d, number = %d\n", i, vertex_offset, nu_vertices);
            if (nu_vertices == 0) // Skip empty meshes
                continue;
            // For multi-mesh object with different textures per mesh, the no textures
            // has been bound yet. Bind the textures for the mesh.
            sreInitializeShaderWithMesh(so, &m->mesh[i]);
        }
        if (m->GL_indexsize == 2)
            glDrawElements(GL_TRIANGLES, nu_vertices, GL_UNSIGNED_SHORT, (void *)(vertex_offset * 2));
        else
            glDrawElements(GL_TRIANGLES, nu_vertices, GL_UNSIGNED_INT, (void *)(vertex_offset * 4));
    } // end for each each mesh

    // Disable non-interleaved attribute buffers.
    unsigned int mask = info->attribute_masks & 0xFF;
    if (mask != 0) {
        unsigned int list = sre_internal_attribute_list_table[mask];
        do {
            glDisableVertexAttribArray(list & 7);
            list >>= 3;
        } while (list != 0);
    }
    // Disable interleaved attribute buffers.
    mask = (info->attribute_masks >> 8);
    while (mask != 0) {
        unsigned int list = sre_internal_attribute_list_table[mask & 0xFF];
        do {
            glDisableVertexAttribArray(list & 7);
            list >>= 3;
        } while (list != 0);
        mask >>= 8;
    }

    GL3ResetGLFlags(so);
}

#ifdef DEBUG_RENDER_LOG

static void PrintAttributeList(sreObjectAttributeInfo *info, bool interleaved) {
    int slot = 0;
    int last_slot = 0;
    if (interleaved) {
        slot = 1;
        last_slot = 3;
    }
    for (;slot <= last_slot; slot++) {
        int mask = (info->attribute_masks >> (slot * 8)) & 0xFF;
        if (mask == 0) {
            if (slot < 2)
                sreMessageNoNewline(SRE_MESSAGE_LOG, "none");
            return;
        }
        for (int i = 0; i < SRE_NU_VERTEX_ATTRIBUTES; i++)
            if (mask & (1 << i))
                sreMessageNoNewline(SRE_MESSAGE_LOG, "%c", '0' + i);
    }
}

static void PrintShaderInfo(const sreObject *so, sreLODModel *m, sreObjectAttributeInfo *info,
const char *pass,  int light_type_slot) {
    if (sre_internal_debug_message_level >= SRE_MESSAGE_LOG) {
        sreMessageNoNewline(SRE_MESSAGE_LOG,
            "object %d, model %d, %s shader %d (light type %d), "
            "non-interleaved attributes: ",
            so->id, m->id,  pass, so->current_shader[light_type_slot], light_type_slot);
        PrintAttributeList(info, false);
        sreMessageNoNewline(SRE_MESSAGE_LOG, ", interleaved: ");
        PrintAttributeList(info, true);
        sreMessage(SRE_MESSAGE_LOG, ", object flags (filtered) 0x%08X, "
            "model has %d triangles (%d vertices), %d meshes.\n",
            so->render_flags, m->nu_triangles, m->nu_triangles * 3, m->nu_meshes);
    }
}

static void PrintNewShaderInfo(const sreObject *so, sreLODModel *m, sreObjectAttributeInfo *info,
const char *pass, int light_type_slot) {
    if (sre_internal_debug_message_level >= SRE_MESSAGE_LOG) {
        sreMessage(SRE_MESSAGE_LOG, "New shader selected: ");
        PrintShaderInfo(so, m, info, pass, light_type_slot);
    }
}

static void PrintDrawObjectInfo(const sreObject *so, sreLODModel *m, sreObjectAttributeInfo *info,
const char *pass, int light_type_slot) {
    if (sre_internal_debug_message_level >= SRE_MESSAGE_VERBOSE_LOG) {
        sreMessage(SRE_MESSAGE_VERBOSE_LOG, "sreDrawObject: ");
        PrintShaderInfo(so, m, info, pass, light_type_slot);
    }
}

#endif

static inline void SetRenderFlags(sreObject *so) {
    so->render_flags = so->flags & sre_internal_object_flags_mask;
}

// The final pass of both single-pass and multi-pass rendering. At the moment, reserved for
// the following objects:
//
// - Objects with the SRE_OBJECT_EMISSION_ONLY flag set. They are not influenced by lights. These
//   objects may additionally have the SRE_OBJECT_BILLBOARD, SRE_OBJECT_LIGHT_HALO or
//   SRE_OBJECT_PARTICLE_SYSTEM flags set, which indicates they consist of one or multiple billboards.
//   They may be transparent such as halos or transparent emission maps.

void sreDrawObjectFinalPass(sreObject *so) {
    // Explicitly apply the render settings object flags mask.
    SetRenderFlags(so);
    if (so->render_flags & SRE_OBJECT_LIGHT_HALO) {
        // Single light halos and particle systems with light halos are handled seperately.
        sreDrawObjectLightHalo(so);
        return;
    }
    if (so->render_flags & (SRE_OBJECT_BILLBOARD | SRE_OBJECT_PARTICLE_SYSTEM)) {
        // Billboards and (billboard) particle systems are also handled seperately.
        sreDrawObjectBillboard(so);
        return;
    }

    // The only remaining cases is objects with the EMISSION_ONLY flag, with optional
    // use of an emission texture map (with optional alpha transparency) instead of a
    // single color, or optionally adding the (multi-color) diffuse reflection color
    // to the emission color (EMISSION_ADD_DIFFUSE_REFLECTION_COLOR).
    // Check that emission only hasn't been masked out due to global rendering settings;
    // in that case, simply skip the object.
    if (!(so->render_flags & SRE_OBJECT_EMISSION_ONLY))
        return;
    bool select_new_shader = sreInitializeObjectShaderEmissionOnly(*so);

    if (so->render_flags & SRE_OBJECT_INFINITE_DISTANCE) {
        // Disable writing into depth buffer.
        glDepthMask(GL_FALSE);
    }
    if (so->render_flags & SRE_OBJECT_NO_BACKFACE_CULLING)
        glDisable(GL_CULL_FACE);
    if (so->render_flags & SRE_OBJECT_TRANSPARENT_EMISSION_MAP) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    // Level-of-detail handling.
    sreLODModel *m = sreCalculateLODModel(*so);

    // We still use general vertex attribute setup functions, because it is possible that
    // position and texcoords attributed are interleaved.
    if (select_new_shader) {
        int attribute_mask = SRE_POSITION_MASK;
        attribute_mask += ((so->render_flags & SRE_OBJECT_USE_EMISSION_MAP) != 0)
            << SRE_ATTRIBUTE_TEXCOORDS;
        attribute_mask += ((so->render_flags & SRE_OBJECT_MULTI_COLOR) != 0)
            << SRE_ATTRIBUTE_COLOR;

        so->attribute_info.Set(attribute_mask, m->attribute_info);
#ifdef DEBUG_RENDER_LOG
        PrintNewShaderInfo(so, m, &so->attribute_info, "final pass (emission only)",
            SRE_SHADER_LIGHT_TYPE_ALL);
#endif
    }

#ifdef DEBUG_RENDER_LOG
    PrintDrawObjectInfo(so, m, &so->attribute_info, "final pass (emission only)",
        SRE_SHADER_LIGHT_TYPE_ALL);
#endif
    sreFinishDrawingObject(so, m, &so->attribute_info);
}

void sreDrawObjectSinglePass(sreObject *so) {
    bool new_shader_selected = sreInitializeObjectShaderSinglePass(*so);

    GL3SetGLFlags(so);

    // Level-of-detail handling.
    sreLODModel *m = sreCalculateLODModel(*so);

    // Use the stored attribute list if possible, only determine the attributes when a new
    // shader is selected.
    if (new_shader_selected) {
        // Determine the required attributes based on the scene object's properties.
        // Normally this should only be required rarely (most of the time the object's
        // stored attribute information will be utilized).
        // Position is required.
        // Normals should also be present, since objects with EMISSION_ONLY are always handled
        // in the final pass.
        int attribute_mask = SRE_POSITION_MASK | SRE_NORMAL_MASK; 
        int flags = so->render_flags;
        // Avoiding if statements may allow more efficient (non-branching) code on some CPUs.
        // The boolean expression evaluates to zero or or one.
        attribute_mask += ((flags & (SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_USE_EMISSION_MAP |
            SRE_OBJECT_USE_NORMAL_MAP | SRE_OBJECT_USE_SPECULARITY_MAP)) != 0)
            << SRE_ATTRIBUTE_TEXCOORDS;
        // When the anisotropic variant of the micro-facet shading model is enabled,
        // tangents are required (although micro-facet is not yet implemented for single-pass).
        attribute_mask += ((flags & SRE_OBJECT_USE_NORMAL_MAP) ||
            (sre_internal_reflection_model == SRE_REFLECTION_MODEL_MICROFACET && so->anisotropic))
            << SRE_ATTRIBUTE_TANGENT;
        attribute_mask += ((flags & SRE_OBJECT_MULTI_COLOR) != 0) << SRE_ATTRIBUTE_COLOR;
        // To be safe, mask with the attribute mask of the shader
        attribute_mask &= single_pass_shader[so->current_shader[SRE_SHADER_LIGHT_TYPE_ALL]].attribute_mask;

        so->attribute_info.Set(attribute_mask, m->attribute_info);
#ifdef DEBUG_RENDER_LOG
        PrintNewShaderInfo(so, m, &so->attribute_info, "single pass", SRE_SHADER_LIGHT_TYPE_ALL);
#endif
    }

#ifdef DEBUG_RENDER_LOG
    PrintDrawObjectInfo(so, m, &so->attribute_info, "single pass", SRE_SHADER_LIGHT_TYPE_ALL);
#endif
    sreFinishDrawingObject(so, m, &so->attribute_info);
}

// The ambient pass allows a few simplifications. The shader is always the same
// (multi-pass SHADER1), and certain attributes (such as normals and tangents) aren't used,
// as well as normal maps and specularity maps.

void sreDrawObjectAmbientPass(sreObject *so) {
    bool new_shader_selected = sreInitializeObjectShaderAmbientPass(*so);

    GL3SetGLFlags(so);

    // Level-of-detail handling.
    sreLODModel *m = sreCalculateLODModel(*so);

    // Use the stored attribute list if possible, only determine the attributes when a new
    // shader is selected.
    if (new_shader_selected) {
        // Determine the required attributes based on the scene object's properties.
        // Normally this should only be required rarely (most of the time the object's
        // stored attribute information will be utilized).
        int attribute_mask = SRE_POSITION_MASK; // Should normally be required.
        // Avoiding if statements may allow more efficient (non-branching) code on some CPUs.
        // The boolean expression evaluates to zero or or one.
        int flags = so->render_flags;
        attribute_mask += ((flags & (SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_USE_EMISSION_MAP)) != 0)
            << SRE_ATTRIBUTE_TEXCOORDS;
        attribute_mask += ((flags & SRE_OBJECT_MULTI_COLOR) != 0) << SRE_ATTRIBUTE_COLOR;

        so->attribute_info_ambient_pass.Set(attribute_mask, m->attribute_info);
#ifdef DEBUG_RENDER_LOG
        PrintNewShaderInfo(so, m, &so->attribute_info_ambient_pass, "ambient pass",
            SRE_SHADER_LIGHT_TYPE_AMBIENT);
#endif
    }

#ifdef DEBUG_RENDER_LOG
    PrintDrawObjectInfo(so, m, &so->attribute_info_ambient_pass, "multi-pass ambient",
        SRE_SHADER_LIGHT_TYPE_AMBIENT);
#endif
    sreFinishDrawingObject(so, m, &so->attribute_info_ambient_pass);
}

void sreDrawObjectMultiPassLightingPass(sreObject *so, bool shadow_map_required) {
    bool new_shader_selected;
    sreObjectAttributeInfo *info;
#ifndef NO_SHADOW_MAP
    // Note: when shadow mapping or cube shadow mapping is not supported, shadow_map_required
    // will be false.
    if (shadow_map_required) {
        // Note: For the same object and light type, a different vertex attribute
        // configuration may be required for the shadow map-enabled shader compared
        // to the regular one. We therefore have a seperate attribute info when a shadow
        // map is required for drawing the object.
        new_shader_selected = sreInitializeObjectShaderMultiPassShadowMapLightingPass(*so);
        info = &so->attribute_info_shadow_map;
    }
    else
#endif
    {
        new_shader_selected = sreInitializeObjectShaderMultiPassLightingPass(*so);
        info = &so->attribute_info;
    }

    GL3SetGLFlags(so);

    // Level-of-detail handling.
    sreLODModel *m = sreCalculateLODModel(*so);

    // Use the stored attribute list if possible, only determine the attributes when a new
    // shader is selected.
    // Note: Because the attribute mask is the same for every light type, ideally we should avoid
    // recalculating the attributes more than once per frame.
    if (new_shader_selected) {
        // Determine the required attributes based on the scene object's properties.
        // Normally this should only be required rarely (most of the time the object's
        // stored attribute information will be utilized).
        // Position is required.
        // Normals should also be present, since objects with EMISSION_ONLY are always handled
        // in the final pass.
        int attribute_mask = SRE_POSITION_MASK | SRE_NORMAL_MASK; 
        // Avoiding if statements may allow more efficient (non-branching) code on some CPUs.
        // The boolean expression evaluates to zero or or one.
        int flags = so->render_flags;
        attribute_mask += ((flags & (SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_USE_EMISSION_MAP |
            SRE_OBJECT_USE_NORMAL_MAP | SRE_OBJECT_USE_SPECULARITY_MAP)) != 0)
            << SRE_ATTRIBUTE_TEXCOORDS;
        // When the anisotropic variant of the micro-facet shading model is enabled,
        // tangents are required.
        attribute_mask += ((flags & SRE_OBJECT_USE_NORMAL_MAP) || so->anisotropic)
            << SRE_ATTRIBUTE_TANGENT;
        attribute_mask += ((flags & SRE_OBJECT_MULTI_COLOR) != 0) << SRE_ATTRIBUTE_COLOR;

        // To be safe, mask with the attribute mask of the shader.
        int shader_attribute_mask;
        if (shadow_map_required)
             shader_attribute_mask = multi_pass_shader[so->current_shader_shadow_map[
                 sre_internal_current_light->shader_light_type]].attribute_mask;
        else
             shader_attribute_mask = multi_pass_shader[so->current_shader[
                 sre_internal_current_light->shader_light_type]].attribute_mask;
        attribute_mask &= shader_attribute_mask;

        // Set up the attribute information for the object, based on the required
        // attribute mask and the attribute info for the model.
        info->Set(attribute_mask, m->attribute_info);
#ifdef DEBUG_RENDER_LOG
        PrintNewShaderInfo(so, m, info, shadow_map_required ?
            "shadow map multi-pass lighting" : "multi-pass lighting",
            sre_internal_current_light->shader_light_type);
#endif
    }

#ifdef DEBUG_RENDER_LOG
    PrintDrawObjectInfo(so, m, info, shadow_map_required ?
        "shadow map multi-pass lighting" : "multi-pass lighting",
        sre_internal_current_light->shader_light_type);
#endif
    sreFinishDrawingObject(so, m, info);
}


