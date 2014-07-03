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

// Optimized shadow volumes with caching, geometric and scissors optimizations.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#ifdef __GNUC__
#include <fenv.h>
#endif
#ifdef OPENGL_ES2
#include <GLES2/gl2.h>
#endif
#ifdef OPENGL
#include <GL/glew.h>
#include <GL/gl.h>
#endif

#include "sre.h"
#include "sre_internal.h"
#include "sre_bounds.h"
#include "sre_simd.h"

// Cache for objects (point source lights and spotlights).
// The total number of entries is four times this number (four per object).
#define SRE_SHADOW_VOLUME_OBJECT_CACHE_SIZE 1024
// Cache for models (directional lights and beam lights).
// The total number of entries is four times this number (four per model).
#define SRE_SHADOW_VOLUME_MODEL_CACHE_SIZE 256

enum {
    SRE_FACE_FLAG_LIGHT_FACING = 1,
    SRE_FACE_FLAG_PERPENDICULAR_TO_LIGHT = 2
};

// VBO edges implementation

// Class to hold shadow volume building information.

// Number of bits to use for face flags storage. A value of eight uses a byte
// for each triangle and simplifies the code at expense of greater memory requirements.
#define SRE_FACE_FLAG_BITS 8
#define SRE_FACE_FLAG_MASK ((1 << SRE_FACE_FLAG_BITS) - 1)

class EdgeArray {
public :
    sreLODModelShadowVolume *m;
    sreModel *full_model;
    // The silhouette is defined by edge indices (when bit 31 is cleared) into
    // model's edge array. When bit 31 is set, the edge should be reversed.
    unsigned int *edge_index;
    int nu_edges;
    int max_edges;
    // The face type (light facing or not) of every face (triangle in the model).
    // Four flags fields are packed into each byte.
    unsigned char *face_type;
    int max_face_types;

    EdgeArray();
    void CheckEdgeCapacity(int nu_edges_required);
    inline void AppendEdge(int model_edge_index) {
        edge_index[nu_edges] = model_edge_index;
        nu_edges++;    
    }
    inline void AppendEdgeReversed(int model_edge_index) {
        edge_index[nu_edges] = model_edge_index | 0x80000000;
        nu_edges++;    
    }
    inline void GetVertices(unsigned int i, unsigned int& v0, unsigned int &v1) {
        unsigned int index = edge_index[i] & 0x7FFFFFFF;
        unsigned int mask1 = (int)((edge_index[i] & 0x80000000)) >> 31;
        unsigned int mask0 = ~mask1;
        v0 = (m->edge[index].vertex_index[0] & mask0) +
            (m->edge[index].vertex_index[1] & mask1);
        v1 = (m->edge[index].vertex_index[0] & mask1) +
            (m->edge[index].vertex_index[1] & mask0);
    }
    inline void GetFirstVertex(unsigned int i, unsigned int& v0) {
        unsigned int index = edge_index[i] & 0x7FFFFFFF;
        unsigned int mask1 = (int)((edge_index[i] & 0x80000000)) >> 31;
        unsigned int mask0 = ~mask1;
        v0 = (m->edge[index].vertex_index[0] & mask0) +
            (m->edge[index].vertex_index[1] & mask1);
    }
    inline void GetSecondVertex(unsigned int i, unsigned int& v1) {
        unsigned int index = edge_index[i] & 0x7FFFFFFF;
        unsigned int mask1 = (int)((edge_index[i] & 0x80000000)) >> 31;
        unsigned int mask0 = ~mask1;
        v1 = (m->edge[index].vertex_index[0] & mask1) +
            (m->edge[index].vertex_index[1] & mask0);
    }
    void CheckFaceTypeCapacity(int nu_faces_required);
    // Pack four face types into each byte.
    inline void SetFaceType(int i, unsigned int flags) {
#if SRE_FACE_FLAG_BITS == 8
        face_type[i] = flags;
#else
        int j = i / (8 / SRE_FACE_FLAG_BITS);
        unsigned char value = face_type[j];
        int shift = (i & ((8 / SRE_FACE_FLAG_BITS) - 1)) * SRE_FACE_FLAG_BITS;
        unsigned char mask = SRE_FACE_FLAG_MASK << shift;
        face_type[j] = (value & (~mask)) | (flags << shift);
#endif
    }
    inline unsigned int GetFaceType(int i) {
        int j = i / (8 / SRE_FACE_FLAG_BITS);
        int shift = (i & ((8 / SRE_FACE_FLAG_BITS) - 1)) * SRE_FACE_FLAG_BITS;
        return (face_type[j] >> shift) & SRE_FACE_FLAG_MASK;
    }
    // Set afce types for four triangles.
    inline void SetFourFaceTypes(int i, unsigned int byte_flags) {
#if SRE_FACE_FLAG_BITS == 8
        *(unsigned int *)(&face_type[i]) = byte_flags;
#else
        unsigned int packed_flags =
            (byte_flags & 0x1) | ((byte_flags & 0x100) >> (8 - SRE_FACE_FLAG_BITS)) |
            ((byte_flags & 0x10000) >> (16 - SRE_FACE_FLAG_BITS * 2)) |
            ((byte_flags & 0x1000000) >> (24 - SRE_FACE_FLAG_BITS * 3));
        int j = i / (8 / SRE_FACE_FLAG_BITS);
#if SRE_FACE_FLAG_BITS == 4
        *(unsigned short *)(&face_type[j]) = byte_flags;
#elif SRE_FACE_FLAG_BITS == 2
        face_type[j] = packed_flags;
#else
        unsigned char shift = (i & 4);
        face_type[j] = (face_type[j] & (0xF << (4 - shift))) | (packed_flags << shift);
#endif
#endif
    }
    // Return four face types packed into a byte.
    inline unsigned int GetFourFaceTypes(int i) {
        return GetFaceType(i) | (GetFaceType(i + 1) << 8) | (GetFaceType(i + 2) << 16) |
            (GetFaceType(i + 3) << 24);
    }
    inline bool IsLightFacing(int triangle_index) {
        return (GetFaceType(triangle_index) & SRE_FACE_FLAG_LIGHT_FACING) != 0;
    }
};


EdgeArray::EdgeArray() {
    edge_index = new unsigned int[SRE_DEFAULT_MAX_SILHOUETTE_EDGES]; 
    max_edges = SRE_DEFAULT_MAX_SILHOUETTE_EDGES;
    // Face types do not need to be stored after silhouette determination; only maintain
    // one global buffer.
    face_type = NULL;
    max_face_types = 0;
}

void EdgeArray::CheckEdgeCapacity(int nu_edges_required) {
    if (nu_edges_required > max_edges) {
        delete [] edge_index;
        // Add a little extra capacity to avoid constant reallocation in the unlikely
        // theoretical case of a small number of triangles being added to the largest
        // model continuously.
        edge_index = new unsigned int[nu_edges_required + 1024];
        max_edges = nu_edges_required + 1024;
    }
}

void EdgeArray::CheckFaceTypeCapacity(int nu_faces_required) {
    if (nu_faces_required > max_face_types) {
        if (face_type != NULL)
            delete [] face_type;
        // Add a little extra capacity to avoid constant reallocation in the unlikely
        // theoretical case of a small number of triangles being added to the largest
        // model continuously.
        int new_size = (nu_faces_required + 1024 + (8 / SRE_FACE_FLAG_BITS - 1)) /
            (8 / SRE_FACE_FLAG_BITS);
        face_type = new unsigned char[new_size];
        max_face_types = new_size;
    }
}

static EdgeArray *silhouette_edges = NULL;

static void CalculateSilhouetteEdges(const Vector4D& lightpos, EdgeArray *ea) {
    sreLODModelShadowVolume *m = ea->m;

    // Dynamically reallocate the face type buffer when required.
    ea->CheckFaceTypeCapacity(m->nu_triangles);

    // Dynamically enlarge the edge array when required.
    ea->CheckEdgeCapacity(m->nu_edges);

    ea->nu_edges = 0;

    // Determine which triangles are facing the light.
    // Note that lightpos is in model space.
    // In the VBO edges implementation, it is acceptable to do this in the model's
    // original data structure instead of the VBO data.
    if ((m->flags & (SRE_LOD_MODEL_NOT_CLOSED | SRE_LOD_MODEL_OPEN_SIDE_HIDDEN_FROM_LIGHT)) ==
    SRE_LOD_MODEL_NOT_CLOSED)
         goto model_not_closed;

// Configuration for SIMD light facing triangle calculation.
// TRIANGLES_AT_A_TIME must be 4.
#define TRIANGLES_AT_A_TIME 4
// Whether to use full optimized SIMD, as opposed to using SIMD just for dot products.
#define USE_INLINE_SIMD_FOR_LIGHT_VECTOR_CALCULATION
// Whether to use simd_load instructions to load entire vectors into SIMD registers,
// and then transposing vectors to obtain the appropriate set-up for calculation, as
// opposed to using simd_set to set vector components.
// Not using load instruction seems to be slightly faster in this case.
// #define USE_LOAD_INSTRUCTION

    {
    int i = 0;
#ifdef USE_SIMD
    // Process TRIANGLES_AT_AT_TIME triangles at a time using SIMD.
    const __simd128_float m_zeros = simd128_set_zero_float();
#ifdef USE_INLINE_SIMD_FOR_LIGHT_VECTOR_CALCULATION
    const __simd128_float m_lightpos_x = simd128_set_same_float(lightpos.x);
    const __simd128_float m_lightpos_y = simd128_set_same_float(lightpos.y);
    const __simd128_float m_lightpos_z = simd128_set_same_float(lightpos.z);
    const __simd128_float m_lightpos_w = simd128_set_same_float(lightpos.w);
#endif
    for (; i + TRIANGLES_AT_A_TIME - 1 < m->nu_triangles; i += TRIANGLES_AT_A_TIME) {
        for (int j = 0; j < TRIANGLES_AT_A_TIME / 4; j++) {
            int k = i + j * 4;
            // Calculate dot products of the light vector lv and triangle normal tn.
#ifdef USE_INLINE_SIMD_FOR_LIGHT_VECTOR_CALCULATION
#ifdef USE_LOAD_INSTRUCTION
            __simd128_float m_vertex0_0 = simd128_load(
                &m->vertex[m->triangle[k + 0].vertex_index[0]]);
            __simd128_float m_vertex0_1 = simd128_load(
                &m->vertex[m->triangle[k + 1].vertex_index[0]]);
            __simd128_float m_vertex0_2 = simd128_load(
                &m->vertex[m->triangle[k + 2].vertex_index[0]]);
            __simd128_float m_vertex0_3 = simd128_load(
                &m->vertex[m->triangle[k + 3].vertex_index[0]]);
            __simd128_float m_vertex0_x, m_vertex0_y, m_vertex0_z;
            simd128_transpose4to3_float(m_vertex0_0, m_vertex0_1, m_vertex0_2,
                m_vertex0_3, m_vertex0_x, m_vertex0_y, m_vertex0_z);
#else
            Vector3D vertex0[4], tn[4];
            for (int l = 0; l < 4; l++) { 
                 vertex0[l] = m->vertex[m->triangle[k + l].vertex_index[0]];
                 tn[l] = m->triangle[k + l].normal;
            }
            __simd128_float m_vertex0_x = simd128_set_float(vertex0[0].x, vertex0[1].x,
                 vertex0[2].x, vertex0[3].x);
            __simd128_float m_vertex0_y = simd128_set_float(vertex0[0].y, vertex0[1].y,
                 vertex0[2].y, vertex0[3].y);
            __simd128_float m_vertex0_z = simd128_set_float(vertex0[0].z, vertex0[1].z,
                 vertex0[2].z, vertex0[3].z);
#endif
            __simd128_float m_lv_x = simd128_sub_float(m_lightpos_x,
                 simd128_mul_float(m_lightpos_w, m_vertex0_x));
            __simd128_float m_lv_y = simd128_sub_float(m_lightpos_y,
                 simd128_mul_float(m_lightpos_w, m_vertex0_y));
            __simd128_float m_lv_z = simd128_sub_float(m_lightpos_z,
                 simd128_mul_float(m_lightpos_w, m_vertex0_z));
#ifdef USE_LOAD_INSTRUCTION
            __simd128_float m_tn0 = simd128_load(&m->triangle[k + 0].normal);
            __simd128_float m_tn1 = simd128_load(&m->triangle[k + 1].normal);
            __simd128_float m_tn2 = simd128_load(&m->triangle[k + 2].normal);
            __simd128_float m_tn3 = simd128_load(&m->triangle[k + 3].normal);
            __simd128_float m_tn_x, m_tn_y, m_tn_z;
            simd128_transpose4to3_float(m_tn0, m_tn1, m_tn2,
                m_tn3, m_tn_x, m_tn_y, m_tn_z);
#else
            __simd128_float m_tn_x = simd128_set_float(tn[0].x, tn[1].x, tn[2].x, tn[3].x);
            __simd128_float m_tn_y = simd128_set_float(tn[0].y, tn[1].y, tn[2].y, tn[3].y);
            __simd128_float m_tn_z = simd128_set_float(tn[0].z, tn[1].z, tn[2].z, tn[3].z);
#endif
            __simd128_float m_dot_x = simd128_mul_float(m_lv_x, m_tn_x);
            __simd128_float m_dot_y = simd128_mul_float(m_lv_y, m_tn_y);
            __simd128_float m_dot_z = simd128_mul_float(m_lv_z, m_tn_z);
            // mul0 should contain only the x components of the dot products, etc.
            __simd128_float m_result = simd128_add_float(
                simd128_add_float(m_dot_x, m_dot_y), m_dot_z);
#else
            Vector3D lv[4], tn[4];
            for (int l = 0; l < 4; l++) {
                lv[l] = lightpos.GetPoint3D() - lightpos.w *
                    m->vertex[m->triangle[k + l].vertex_index[0]];
                tn[l] = m->triangle[k + l].normal;
            }
            __simd128_float m_result;
            SIMDCalculateFourDotProductsNoStore(&lv[0], &tn[0], m_result);
#endif
            // Produce 32-bit masks (0xFFFFFFFF or 0x00000000) for each comparison result.
            __simd128_int m_comp = simd128_cmpge_float(m_result, m_zeros);
            unsigned int face_flags = simd128_convert_masks_int32_int1(m_comp);
            unsigned int byte_flags = (face_flags & 0x1) + ((face_flags & 0x2) << 7)
                + ((face_flags & 0x4) << 14) + ((face_flags & 0x8) << 21);
            // Set four flags at a time.
            ea->SetFourFaceTypes(k, byte_flags);
//            printf("face_type[0, 1, 2, 3] = 0x%08X\n", ea->GetFourFaceTypes(k));
        }
    }
#endif
    for (;i < m->nu_triangles; i++) {
        Vector3D light_vector = lightpos.GetPoint3D() -
            lightpos.w * m->vertex[m->triangle[i].vertex_index[0]];
        if (Dot(light_vector, m->triangle[i].normal) < 0.0f)
            ea->SetFaceType(i, 0);
        else
            ea->SetFaceType(i, SRE_FACE_FLAG_LIGHT_FACING);
    }
    // Check the orientation of the faces with the respect to the light to determine the
    // silhouette edges.
    i = 0;
    // Processing of multiple edges a time is supported when the face flags are stored
    // as a single byte.
#if SRE_FACE_FLAG_BITS == 8
#define EDGES_AT_A_TIME 8
    for (; i + EDGES_AT_A_TIME - 1 < m->nu_edges; i += EDGES_AT_A_TIME) {
        // Process 4, 8 or 16 edges a time.
        unsigned int face_type_bits_tri0 =
            (unsigned int)ea->face_type[m->edge[i + 0].triangle_index[0]] +
            ((unsigned int)ea->face_type[m->edge[i + 1].triangle_index[0]] << 1) +
            ((unsigned int)ea->face_type[m->edge[i + 2].triangle_index[0]] << 2) +
            ((unsigned int)ea->face_type[m->edge[i + 3].triangle_index[0]] << 3);
        unsigned int face_type_bits_tri1 =
            (unsigned int)ea->face_type[m->edge[i + 0].triangle_index[1]] +
            ((unsigned int)ea->face_type[m->edge[i + 1].triangle_index[1]] << 1) +
            ((unsigned int)ea->face_type[m->edge[i + 2].triangle_index[1]] << 2) +
            ((unsigned int)ea->face_type[m->edge[i + 3].triangle_index[1]] << 3);
#if EDGES_AT_A_TIME >= 8
        face_type_bits_tri0 +=
            ((unsigned int)ea->face_type[m->edge[i + 4].triangle_index[0]] << 4) +
            ((unsigned int)ea->face_type[m->edge[i + 5].triangle_index[0]] << 5) +
            ((unsigned int)ea->face_type[m->edge[i + 6].triangle_index[0]] << 6) +
            ((unsigned int)ea->face_type[m->edge[i + 7].triangle_index[0]] << 7);
        face_type_bits_tri1 +=
            ((unsigned int)ea->face_type[m->edge[i + 4].triangle_index[1]] << 4) +
            ((unsigned int)ea->face_type[m->edge[i + 5].triangle_index[1]] << 5) +
            ((unsigned int)ea->face_type[m->edge[i + 6].triangle_index[1]] << 6) +
            ((unsigned int)ea->face_type[m->edge[i + 7].triangle_index[1]] << 7);
#endif
#if EDGES_AT_A_TIME == 16
        face_type_bits_tri0 +=
            ((unsigned int)ea->face_type[m->edge[i + 8].triangle_index[0]] << 8) +
            ((unsigned int)ea->face_type[m->edge[i + 9].triangle_index[0]] << 9) +
            ((unsigned int)ea->face_type[m->edge[i + 10].triangle_index[0]] << 10) +
            ((unsigned int)ea->face_type[m->edge[i + 11].triangle_index[0]] << 11);
        face_type_bits_tri0 +=
            ((unsigned int)ea->face_type[m->edge[i + 12].triangle_index[0]] << 12) +
            ((unsigned int)ea->face_type[m->edge[i + 13].triangle_index[0]] << 13) +
            ((unsigned int)ea->face_type[m->edge[i + 14].triangle_index[0]] << 14) +
            ((unsigned int)ea->face_type[m->edge[i + 15].triangle_index[0]] << 15);
        face_type_bits_tri1 +=
            ((unsigned int)ea->face_type[m->edge[i + 8].triangle_index[1]] << 8) +
            ((unsigned int)ea->face_type[m->edge[i + 9].triangle_index[1]] << 9) +
            ((unsigned int)ea->face_type[m->edge[i + 10].triangle_index[1]] << 10) +
            ((unsigned int)ea->face_type[m->edge[i + 11].triangle_index[1]] << 11);
        face_type_bits_tri1 +=
            ((unsigned int)ea->face_type[m->edge[i + 12].triangle_index[1]] << 12) +
            ((unsigned int)ea->face_type[m->edge[i + 13].triangle_index[1]] << 13) +
            ((unsigned int)ea->face_type[m->edge[i + 14].triangle_index[1]] << 14) +
            ((unsigned int)ea->face_type[m->edge[i + 15].triangle_index[1]] << 15);
#endif
        unsigned int cond_int = face_type_bits_tri0 ^ face_type_bits_tri1;
        // Process multiple checks at a time, so that a set of four/sixteen edges can be
        // immediately dismissed when none of them are part of the silhouette.
        for (int j = 0; j < EDGES_AT_A_TIME; j++) {
            if (cond_int == 0)
                break;
            // Set the "reverse edge" bit when triangle 0 faces the light.
            unsigned int reversed_bit = face_type_bits_tri0 << 31;
            if (cond_int & 0x1) {
                ea->AppendEdge((i + j) | reversed_bit);
            }
            face_type_bits_tri0 >>= 1;
            cond_int >>= 1;
        }
    }
#endif // SRE_FACE_FLAG_BITS == 8 (processed multiple edges at a time).
    // Process the remaining edges.
    for (; i < m->nu_edges; i++) {
        ModelEdge *e = &m->edge[i];
        // Check the orientation of the faces with the respect to the light to determine the
        // silhouette edge.
        unsigned char face_type0 = ea->GetFaceType(e->triangle_index[0]);
        unsigned char face_type1 = ea->GetFaceType(e->triangle_index[1]);
        if (face_type0 != face_type1) {
            // Convention is that e0 to e1 is counterclockwise in face 0 and clockwise in
            // face 1.
            // Set the "reverse edge" bit when triangle 0 faces the light.
            unsigned int reversed_bit = (unsigned int)face_type0 << 31;
            ea->AppendEdge(i | reversed_bit);
        }
    }
    return;
    }

model_not_closed :
    for (int i = 0; i < m->nu_triangles; i++) {
        Vector3D light_vector = lightpos.GetPoint3D() - lightpos.w * m->vertex[m->triangle[i].vertex_index[0]];
        float dot = Dot(light_vector, m->triangle[i].normal);
        int face_flags;
        if (dot < 0)
            face_flags = 0;
        else
            face_flags = SRE_FACE_FLAG_LIGHT_FACING;
#if SRE_FACE_FLAG_BITS > 1
        if (dot > - 0.01f && dot < 0.01f)
            face_flags |= SRE_FACE_FLAG_PERPENDICULAR_TO_LIGHT;
#endif
        ea->SetFaceType(i, face_flags);
    }
    for (int i = 0; i < m->nu_edges; i++) {
        ModelEdge *e = &m->edge[i];
        if (e->triangle_index[1] == -1) {
            // The edge has only one triangle. 
            bool swapped = false;
            if (!ea->IsLightFacing(e->triangle_index[0]))
                swapped = true;
            if (e->vertex_index[1] < e->vertex_index[0])
                swapped = !swapped;
            int vertex_index0, vertex_index1;
            if (swapped) {
                vertex_index0 = e->vertex_index[1];
                vertex_index1 = e->vertex_index[0];
            }
            else {
                vertex_index0 = e->vertex_index[0];
                vertex_index1 = e->vertex_index[1];
            }
            // Have to make sure normal of side triangle will be pointed outward from the shadow volume.
            Vector3D light_direction = lightpos.w * m->vertex[m->triangle[e->triangle_index[0]]
                .vertex_index[0]] - lightpos.GetPoint3D();
            if (lightpos.w > 0)
                light_direction.Normalize();
            Vector3D E = m->vertex[vertex_index1] - m->vertex[vertex_index0];
            Vector3D N = Cross(E, light_direction);
            N.Normalize();
            // Calculate the plane perpendicular to the light direction going through edge vertex 0.
            Vector4D L = Vector4D(light_direction, - Dot(light_direction, m->vertex[vertex_index0]));
            // Move the model's center along the light direction to the plane L.
            Point3D center = ea->full_model->sphere.center - light_direction
                * Dot(L, ea->full_model->sphere.center);
            // Calculate the plane of the side triangle going through edge vertex 0.
            Vector4D K = Vector4D(N, - Dot(N, m->vertex[vertex_index0]));
            // Make sure the normal is pointed outward by taking the distance to the projected center.
            if ((Dot(K, center) < 0) ^ swapped)
                ea->AppendEdge(i);
            else
                ea->AppendEdgeReversed(i);
            continue;
        }
        // Check the orientation of the faces with the respect to the light to determine the
        // silhouette edge.
        unsigned char face_type0 = ea->GetFaceType(e->triangle_index[0]);
        unsigned char face_type1 = ea->GetFaceType(e->triangle_index[1]);
#if 0
        if ((face_type0 & PERPENDICULAR_TO_LIGHT) || (face_type1 & PERPENDICULAR_TO_LIGHT)) {
            printf("At least one face perpendicular to light (edge %d).\n", i);
            if ((face_type0 & 1) != (face_type1 & 1)) {
                if ((face_type0 & 1) == NON_LIGHT_FACING)
                    ea->AppendEdge(e);
                else
                    ea->AppendEdgeReversed(e);
                continue;
            }
            printf("Further test necessary.\n");
            bool swapped = false;
            if ((face_type0 & 1) == NON_LIGHT_FACING)
                swapped = true;
            int vertex_index0, vertex_index1;
            if (swapped) {
                vertex_index0 = e->vertex_index[1];
                vertex_index1 = e->vertex_index[0];
            }
            else {
                vertex_index0 = e->vertex_index[0];
                vertex_index1 = e->vertex_index[1];
            }
            // Have to make sure normal of side triangle will be pointed outward from the shadow volume.
            Vector3D light_direction = lightpos.w * m->vertex[m->triangle[e->triangle_index[0]].vertex_index[0]] -
                lightpos.GetPoint3D();
            if (lightpos.w > 0)
                light_direction.Normalize();
            Vector3D E = m->vertex[vertex_index1] - m->vertex[vertex_index0];
            Vector3D N = Cross(E, light_direction);
            N.Normalize();
            // Calculate the plane perpendicular to the light direction going through edge vertex 0.
            Vector4D L = Vector4D(light_direction, - Dot(light_direction, m->vertex[vertex_index0]));
            // Move the model's center along the light direction to the plane L.
            Point3D center = ea->full_model->sphere.center - light_direction * Dot(L, ea->full_model->sphere.center);
            // Calculate the plane of the side triangle going through edge vertex 0.
            Vector4D K = Vector4D(N, - Dot(N, m->vertex[vertex_index0]));
            // Make sure the normal is pointed outward by taking the distance to the projected center.
            if ((Dot(K, center) < 0) ^ swapped)
                ea->AppendEdge(i);
            else
                ea->AppendEdgeReversed(i);
            continue;
        }
#endif
        if ((face_type0 & SRE_FACE_FLAG_LIGHT_FACING)
        != (face_type1 & SRE_FACE_FLAG_LIGHT_FACING)) {
            // Convention is that e0 to e1 is counterclockwise in face 0 and clockwise in
            // face 1.
            if (!(face_type0 & SRE_FACE_FLAG_LIGHT_FACING))
                ea->AppendEdge(i);
            else
                ea->AppendEdgeReversed(i);
        }
    }
    return;

//    printf("Found %d edges in silhouette\n", ea->nu_edges);
}

#define SHORT_ELEMENT_BUFFER
// #define PING_PONG_BUFFERS

#define EmitVertexShort(v) \
    ((unsigned short *)shadow_volume_vertex)[nu_shadow_volume_vertices] = v; \
    nu_shadow_volume_vertices++;

#define EmitVertexInt(v) \
    ((unsigned int *)shadow_volume_vertex)[nu_shadow_volume_vertices] = v; \
    nu_shadow_volume_vertices++;

#ifdef PING_PONG_BUFFERS
static GLuint element_buffer0_id, element_buffer1_id;
static int current_element_buffer;
#endif
static GLuint last_vertexbuffer_id;

static void *shadow_volume_vertex = NULL;
static int max_shadow_volume_vertices;
static int nu_shadow_volume_vertices;

static GLuint element_buffer_id = 0xFFFFFFFF;

// Array buffer flags for shadow volumes.

enum {
    SRE_SHADOW_VOLUME_ARRAY_BUFFER_FLAG_SHORT_INDEX = 1,
    SRE_SHADOW_VOLUME_ARRAY_BUFFER_FLAG_TRIANGLE_STRIP = 2,
    SRE_SHADOW_VOLUME_ARRAY_BUFFER_FLAG_TRIANGLE_FAN = 4
};

static void AddSides(EdgeArray *ea, const sreLight& light, int array_buffer_flags) {
    // Add the sides of the shadow volume based on the silhouette. For light cap vertices
    // projected to the dark cap, a w component of 0 is used.
    if (!(array_buffer_flags & SRE_SHADOW_VOLUME_ARRAY_BUFFER_FLAG_SHORT_INDEX))
        goto add_sides_int;
    if (light.type & (SRE_LIGHT_DIRECTIONAL | SRE_LIGHT_BEAM)) {
        // Directional light. The sides converge to a single point -L extruded to infinity.
        // Or a beam light, in which case the sides converge to the beam light direction
        // extruded to infinity. The extruded vertex index used doesn't matter, as long as
        // the w component is 0.
        for (int i = 0; i < ea->nu_edges; i++) {
            unsigned int v0, v1;
            ea->GetVertices(i, v0, v1);
            EmitVertexShort(v0);
            EmitVertexShort(v1);
            EmitVertexShort(ea->m->vertex_index_shadow_offset);
        }
        return;
    }
    // Point light or spot light, the sides are extruded to infinity.
    // Each silhouette edge vertex is extruded to infinity to help
    // construct the sides of the shadow volume.
    for (int i = 0; i < ea->nu_edges; i++) {
        unsigned int v0, v1;
        ea->GetVertices(i, v0, v1);
        unsigned int extr_v0 = v0 + ea->m->vertex_index_shadow_offset;
        unsigned int extr_v1 = v1 + ea->m->vertex_index_shadow_offset;
        // Generate first triangle.
        EmitVertexShort(v0);
        EmitVertexShort(v1);
        EmitVertexShort(extr_v1);
        // Generate second triangle.
        EmitVertexShort(v0);
        EmitVertexShort(extr_v1);
        EmitVertexShort(extr_v0);
    }
    return;

add_sides_int :
    if (light.type & (SRE_LIGHT_DIRECTIONAL | SRE_LIGHT_BEAM)) {
        // Directional light. The sides converge to a single point -L.
        // Or a beam light, in which case the sides converge to the beam light direction.
        for (int i = 0; i < ea->nu_edges; i++) {
            unsigned int v0, v1;
            ea->GetVertices(i, v0, v1);
            EmitVertexInt(v0);
            EmitVertexInt(v1);
            EmitVertexInt(ea->m->vertex_index_shadow_offset);
        }
        return;
    }
    // Point light or spot light, the sides are extruded to infinity.
    for (int i = 0; i < ea->nu_edges; i++) {
        unsigned int v0, v1;
        ea->GetVertices(i, v0, v1);
        unsigned int extr_v0 = v0 + ea->m->vertex_index_shadow_offset;
        unsigned int extr_v1 = v1 + ea->m->vertex_index_shadow_offset;
        // Generate first triangle.
        EmitVertexInt(v0);
        EmitVertexInt(v1);
        EmitVertexInt(extr_v1);
        // Generate second triangle.
        EmitVertexInt(v0);
        EmitVertexInt(extr_v1);
        EmitVertexInt(extr_v0);
    }
    return;
}

#ifndef NO_PRIMITIVE_RESTART

// Add sides using triangle strips consisting of two triangles followed by a primitive restart for
// each side "quad". This saves just one index value per pair of side triangles; the savings are not
// great (it would be difficult to generate larger triangle strips form silhouette data).

static void AddSidesTriangleStrip(EdgeArray *ea, const sreLight& light,
int array_buffer_flags) {
    // Add the sides of the shadow volume based on the silhouette. For light cap vertices
    // projected to the dark cap, a w component of 0 is used.
    if (!(array_buffer_flags & SRE_SHADOW_VOLUME_ARRAY_BUFFER_FLAG_SHORT_INDEX))
        goto add_sides_triangle_strip_int;
    if (light.type & (SRE_LIGHT_DIRECTIONAL | SRE_LIGHT_BEAM)) {
        // Directional light. The sides converge to a single point -L extruded to infinity.
        // Or a beam light, in which case the sides converge to the beam light direction
        // extruded to infinity. The extruded vertex index used doesn't matter, as long as
        // the w component is 0.
        // Construct a triangle strip.
        for (int i = 0; i < ea->nu_edges; i++) {
            unsigned int v0, v1;
            ea->GetVertices(i, v0, v1);
            EmitVertexShort(v0);
            EmitVertexShort(v1);
            EmitVertexShort(ea->m->vertex_index_shadow_offset);
        }
        return;
    }
    // The light is guaranteed to be a point light or spot light; the sides are extruded to infinity,
    // allowing the use of a small triangle strip for each pair of side triangles.
    // Each silhouette edge vertex is extruded to infinity to help
    // construct the sides of the shadow volume.
    for (int i = 0; i < ea->nu_edges; i++) {
        unsigned int v0, v1;
        ea->GetVertices(i, v0, v1);
        unsigned int extr_v0 = v0 + ea->m->vertex_index_shadow_offset;
        unsigned int extr_v1 = v1 + ea->m->vertex_index_shadow_offset;
        // Generate triangle strip consisting of the triangles:
        // (v1, extr_v1, v0) and (v0, extr_v1, extr_v0).
        EmitVertexShort(v1);
        EmitVertexShort(extr_v1);
        EmitVertexShort(v0);
        EmitVertexShort(extr_v0);
        EmitVertexShort(0xFFFF); // Primitive restart.
    }
    return;

add_sides_triangle_strip_int :
    // Point light or spot light, the sides are extruded to infinity.
    for (int i = 0; i < ea->nu_edges; i++) {
        unsigned int v0, v1;
        ea->GetVertices(i, v0, v1);
        unsigned int extr_v0 = v0 + ea->m->vertex_index_shadow_offset;
        unsigned int extr_v1 = v1 + ea->m->vertex_index_shadow_offset;
        // Generate triangle strip consisting of the triangles:
        // (v1, extr_v1, v0) and (v0, extr_v1, extr_v0).
        EmitVertexInt(v1);
        EmitVertexInt(extr_v1);
        EmitVertexInt(v0);
        EmitVertexInt(extr_v0);
        EmitVertexInt(0xFFFFFFFF); // Primitive restart.
    }
    return;
}

#endif

// For directional and beam lights, a triangle fan can be used when only the sides need to be drawn.
// There is no dependency on the PRIMITIVE_RESTART feature.
// Because the silhouette edges are in no particular order, the triangle fan must be constructed
// using an algorithm takes a little time. This is only works for closed models without holes. The
// resulting triangle fan will be relatively cache-coherent, and should be fast to draw.
// Returns true when succesful, false otherwise.

static bool AddSidesTriangleFan(EdgeArray *ea, const sreLight& light,
int array_buffer_flags) {
    // Add the sides of the shadow volume based on the silhouette. For light cap vertices
    // projected to the dark cap, a w component of 0 is used.
    // The light is guaranteed to be a directional light or beam light. In case of a directional
    // light, the sides converge to a single point -L extruded to infinity.
    // In case of a beam light the sides converge to the beam light direction
    // extruded to infinity. The extruded vertex index used doesn't matter, as long as
    // the w component is 0.

    // Perform a sanity check.
    if (ea->nu_edges <= 1)
        return false;

    // We are looking to link the whole edge array together, forming a polygon representing the
    // silhouette, which can then be output as a triangle fan based at the extruded point.
    // The model must be closed and may not contain holes, because there is no complete
    // silhouette with ordered edges in that case. Irregular models are detected and the
    // function returns false; a standard shadow volume (consisting of triangles) can be
    // constructed instead.
    int *edge_starting_at_vertex = (int *)alloca(sizeof(int) * ea->m->nu_vertices);
    for (int i = 0; i < ea->m->nu_vertices; i++)
        edge_starting_at_vertex[i] = - 1;
    for (int i = 0; i < ea->nu_edges; i++) {
        unsigned int v0;
        ea->GetFirstVertex(i, v0);
        edge_starting_at_vertex[v0] = i;
    }

    unsigned int starting_vertex;
    unsigned int v0, v1;

    if (!(array_buffer_flags & SRE_SHADOW_VOLUME_ARRAY_BUFFER_FLAG_SHORT_INDEX))
        goto add_sides_triangle_fan_int;

    // Construct a triangle fan around the extruded point.
    EmitVertexShort(ea->m->vertex_index_shadow_offset);
    ea->GetVertices(0, v0, v1);
    starting_vertex = v0;
    for (;;) { 
        EmitVertexShort(v0);
        v0 = v1;
        if (v0 == starting_vertex)
            break;
        int e = edge_starting_at_vertex[v0];
        if (e < 0) {
            // Error. Cannot construct triangle fan.
            nu_shadow_volume_vertices = 0;
            return false;
        }
        ea->GetSecondVertex(e, v1);
    }
    EmitVertexShort(starting_vertex); // Close the volume (around the silhouette).
    return true;

add_sides_triangle_fan_int :
    EmitVertexInt(ea->m->vertex_index_shadow_offset);
    ea->GetVertices(0, v0, v1);
    starting_vertex = v0;
    for (;;) {
        EmitVertexInt(v0);
        v0 = v1;
        if (v0 == starting_vertex)
            break;
        int e = edge_starting_at_vertex[v0];
        if (e < 0) {
            // Error.
            nu_shadow_volume_vertices = 0;
            return false;
        }
        ea->GetSecondVertex(e, v1);
    }
    EmitVertexShort(starting_vertex); // Close the volume (around the silhouette).
    return true;
}


static void AddLightCap(EdgeArray *ea, int array_buffer_flags) {
    sreLODModelShadowVolume *m = ea->m;
    // A light cap may be required for point or spot lights for depth-fail rendering.
    // The light cap consists of all model triangles that face the light.
    if (m->flags & SRE_LOD_MODEL_NOT_CLOSED)
        goto add_light_cap_not_closed;
    if (!(array_buffer_flags & SRE_SHADOW_VOLUME_ARRAY_BUFFER_FLAG_SHORT_INDEX))
        goto add_light_cap_int;
    for (int i = 0; i < m->nu_triangles; i++)
        if (ea->IsLightFacing(i)) {
            EmitVertexShort(m->triangle[i].vertex_index[0]);
            EmitVertexShort(m->triangle[i].vertex_index[1]);
            EmitVertexShort(m->triangle[i].vertex_index[2]);
        }
    return;

add_light_cap_int :
    for (int i = 0; i < m->nu_triangles; i++)
        if (ea->IsLightFacing(i)) {
            EmitVertexInt(m->triangle[i].vertex_index[0]);
            EmitVertexInt(m->triangle[i].vertex_index[1]);
            EmitVertexInt(m->triangle[i].vertex_index[2]);
        }
    return;

add_light_cap_not_closed :
    if (!(array_buffer_flags & SRE_SHADOW_VOLUME_ARRAY_BUFFER_FLAG_SHORT_INDEX))
        goto add_light_cap_not_closed_int;
    for (int i = 0; i < m->nu_triangles; i++)
        if (ea->IsLightFacing(i)) {
            EmitVertexShort(m->triangle[i].vertex_index[0]);
            EmitVertexShort(m->triangle[i].vertex_index[1]);
            EmitVertexShort(m->triangle[i].vertex_index[2]);
        }
#if 0
        else {
            EmitVertexShort(m->triangle[i].vertex_index[2]);
            EmitVertexShort(m->triangle[i].vertex_index[1]);
            EmitVertexShort(m->triangle[i].vertex_index[0]);
        }
#endif
    return;

add_light_cap_not_closed_int :
    for (int i = 0; i < m->nu_triangles; i++)
        if (ea->IsLightFacing(i)) {
            EmitVertexInt(m->triangle[i].vertex_index[0]);
            EmitVertexInt(m->triangle[i].vertex_index[1]);
            EmitVertexInt(m->triangle[i].vertex_index[2]);
        }
#if 0
        else {
            EmitVertexInt(m->triangle[i].vertex_index[2]);
            EmitVertexInt(m->triangle[i].vertex_index[1]);
            EmitVertexInt(m->triangle[i].vertex_index[0]);
        }
#endif
    return;
}

static void AddDarkCap(EdgeArray *ea, int array_buffer_flags) {
    sreLODModelShadowVolume *m = ea->m;
    // A dark cap may be required for point or spot lights for depth-fail rendering.
    // Since vertices are extruded to infinity, we can use the same vertices
    // as the light cap, but extruded to infinity (w == 0), and their order reversed
    // within the triangles.
    if (m->flags & SRE_LOD_MODEL_NOT_CLOSED)
        goto add_dark_cap_not_closed;
    if (!(array_buffer_flags & SRE_SHADOW_VOLUME_ARRAY_BUFFER_FLAG_SHORT_INDEX))
        goto add_dark_cap_int;
    for (int i = 0; i < m->nu_triangles; i++)
        if (!ea->IsLightFacing(i)) {
            EmitVertexShort(m->triangle[i].vertex_index[2] + m->vertex_index_shadow_offset);
            EmitVertexShort(m->triangle[i].vertex_index[1] + m->vertex_index_shadow_offset);
            EmitVertexShort(m->triangle[i].vertex_index[0] + m->vertex_index_shadow_offset);
        }
    return;

add_dark_cap_int :
    for (int i = 0; i < m->nu_triangles; i++)
        if (!ea->IsLightFacing(i)) {
            EmitVertexInt(m->triangle[i].vertex_index[2] + m->vertex_index_shadow_offset);
            EmitVertexInt(m->triangle[i].vertex_index[1] + m->vertex_index_shadow_offset);
            EmitVertexInt(m->triangle[i].vertex_index[0] + m->vertex_index_shadow_offset);
        }
    return;

add_dark_cap_not_closed :
    if (!(array_buffer_flags & SRE_SHADOW_VOLUME_ARRAY_BUFFER_FLAG_SHORT_INDEX))
        goto add_dark_cap_not_closed_int;
    for (int i = 0; i < m->nu_triangles; i++)
        if (!ea->IsLightFacing(i)) {
            EmitVertexShort(m->triangle[i].vertex_index[2] + m->vertex_index_shadow_offset);
            EmitVertexShort(m->triangle[i].vertex_index[1] + m->vertex_index_shadow_offset);
            EmitVertexShort(m->triangle[i].vertex_index[0] + m->vertex_index_shadow_offset);
        }
#if 0
        else {
            EmitVertexShort(m->triangle[i].vertex_index[0] + m->vertex_index_shadow_offset);
            EmitVertexShort(m->triangle[i].vertex_index[1] + m->vertex_index_shadow_offset);
            EmitVertexShort(m->triangle[i].vertex_index[2] + m->vertex_index_shadow_offset);
        }
#endif
    return;

add_dark_cap_not_closed_int :
    for (int i = 0; i < m->nu_triangles; i++)
        if (!ea->IsLightFacing(i)) {
            EmitVertexInt(m->triangle[i].vertex_index[2] + m->vertex_index_shadow_offset);
            EmitVertexInt(m->triangle[i].vertex_index[1] + m->vertex_index_shadow_offset);
            EmitVertexInt(m->triangle[i].vertex_index[0] + m->vertex_index_shadow_offset);
        }
#if 0
        else {
            EmitVertexInt(m->triangle[i].vertex_index[0] + m->vertex_index_shadow_offset);
            EmitVertexInt(m->triangle[i].vertex_index[1] + m->vertex_index_shadow_offset);
            EmitVertexInt(m->triangle[i].vertex_index[2] + m->vertex_index_shadow_offset);
        }
#endif
    return;
}

// Drawing a shadow volume,

enum {
  TYPE_DEPTH_PASS = 1,
  TYPE_DEPTH_FAIL = 2,
  TYPE_SKIP_SIDES = 4,
  TYPE_SKIP_LIGHTCAP = 8,
  TYPE_SKIP_DARKCAP = 16,
};

// After the shader has been initialized, draw the shadow volume. Used by both types of cache
// after a hit.

static void FinishDrawingShadowVolume(int type, sreLODModelShadowVolume *model, GLuint opengl_id,
int array_buffer_flags, int nu_vertices) {
    if (type & TYPE_DEPTH_PASS) {
        glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_INCR_WRAP);
        glStencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_DECR_WRAP);
    }
    else {
        glStencilOpSeparate(GL_BACK, GL_KEEP, GL_INCR_WRAP, GL_KEEP);
        glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_DECR_WRAP, GL_KEEP);
    }

    // Enable the vertex buffer of the model  (if it wasn't already set up).
    if (model->GL_attribute_buffer[SRE_ATTRIBUTE_POSITION] != last_vertexbuffer_id) {
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, model->GL_attribute_buffer[SRE_ATTRIBUTE_POSITION]);
        glVertexAttribPointer(
           0,                  // attribute 0 (positions)
           4,                  // size
           GL_FLOAT,           // type
           GL_FALSE,           // normalized?
           0,                  // stride
           (void *)0	   // array buffer offset in bytes
        );
        last_vertexbuffer_id = model->GL_attribute_buffer[SRE_ATTRIBUTE_POSITION];
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, opengl_id);

#ifndef NO_PRIMITIVE_RESTART
    if (array_buffer_flags & SRE_SHADOW_VOLUME_ARRAY_BUFFER_FLAG_TRIANGLE_STRIP) {
        // Triangle strip implies availability of primitive restart.
        // Since the unsigned short token is enabled normally, we have to change it when
        // using 32-bit indices.
        if (model->GL_indexsize == 4) {
            glPrimitiveRestartIndexNV(0xFFFFFFFF);
            glDrawElements(GL_TRIANGLE_STRIP, nu_vertices, GL_UNSIGNED_INT, (void *)0);
            // Restore the expected state.
            glPrimitiveRestartIndexNV(0xFFFF);
        }
        else
            glDrawElements(GL_TRIANGLE_STRIP, nu_vertices, GL_UNSIGNED_SHORT, (void *)0);
        return;
    }
#endif

    GLenum mode;
    if (array_buffer_flags & SRE_SHADOW_VOLUME_ARRAY_BUFFER_FLAG_TRIANGLE_FAN)
        mode = GL_TRIANGLE_FAN;
    else
        mode = GL_TRIANGLES;
    if (model->GL_indexsize == 2)
        glDrawElements(mode, nu_vertices, GL_UNSIGNED_SHORT, (void *)0);
    else
        glDrawElements(mode, nu_vertices, GL_UNSIGNED_INT, (void *)0);
}

static void DrawShadowVolumeGL(sreLODModelShadowVolume *m, int array_buffer_flags) {
    // Enable the vertex buffer of the model.
    if (m->GL_attribute_buffer[SRE_ATTRIBUTE_POSITION] != last_vertexbuffer_id) {
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, m->GL_attribute_buffer[SRE_ATTRIBUTE_POSITION]);
        glVertexAttribPointer(
           0,                  // attribute 0 (positions)
           4,                  // size
           GL_FLOAT,           // type
           GL_FALSE,           // normalized?
           0,                  // stride
           (void *)0	   // array buffer offset in bytes
        );

        last_vertexbuffer_id = m->GL_attribute_buffer[SRE_ATTRIBUTE_POSITION];
    }
#ifdef PING_PONG_BUFFERS
    // Enable the elements (index) buffer.
    if (current_element_buffer == 0) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_buffer1_id);
        current_element_buffer = 1;
    }
    else {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_buffer0_id);
        current_element_buffer = 0;
    }
#else
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_buffer_id);
#endif

    // Upload the element data.
    if (m->GL_indexsize == 2)
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, nu_shadow_volume_vertices * sizeof(short int),
            shadow_volume_vertex, GL_DYNAMIC_DRAW);
    else
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, nu_shadow_volume_vertices * sizeof(int),
            shadow_volume_vertex, GL_DYNAMIC_DRAW);

    // Draw the element array.
#ifndef NO_PRIMITIVE_RESTART
    if (array_buffer_flags & SRE_SHADOW_VOLUME_ARRAY_BUFFER_FLAG_TRIANGLE_STRIP) {
        // Triangle strip implies availability of primitive restart.
        // Since the unsigned short token is enabled normally, we have to change it when
        // using 32-bit indices.
        if (m->GL_indexsize == 4) {
            glPrimitiveRestartIndexNV(0xFFFFFFFF);
            glDrawElements(GL_TRIANGLE_STRIP, nu_shadow_volume_vertices, GL_UNSIGNED_INT, (void *)0);
            // Restore the expected state.
            glPrimitiveRestartIndexNV(0xFFFF);
        }
        else
            glDrawElements(GL_TRIANGLE_STRIP, nu_shadow_volume_vertices, GL_UNSIGNED_SHORT, (void *)0);
        return;
    }
#endif

    GLenum mode;
    if (array_buffer_flags & SRE_SHADOW_VOLUME_ARRAY_BUFFER_FLAG_TRIANGLE_FAN)
        mode = GL_TRIANGLE_FAN;
    else
        mode = GL_TRIANGLES;
    if (m->GL_indexsize == 2)
        glDrawElements(mode, nu_shadow_volume_vertices, GL_UNSIGNED_SHORT, (void *)0);
    else
        glDrawElements(mode, nu_shadow_volume_vertices, GL_UNSIGNED_INT, (void *)0);
}

// Object shadow volumes cache for point lights/spot lights. Direct-mapped with
// scene object id with the light id mixed in with a multiplication factor.

class ShadowVolumeObjectCacheEntry {
public :
    int so_id; // -1 if empty.
    sreLODModelShadowVolume *model;
    Vector4D lightpos;
    GLuint opengl_id;
    int nu_vertices;
    char type;
    char array_buffer_flags;
    int timestamp;
};

// There are SRE_SHADOW_VOLUME_OBJECT_CACHE_SIZE cache lines of four cache entries each.
// Scene object i is mapped to entries at ((i + light_index * 77) %
// SRE_SHADOW_VOLUME_OBJECT_CACHE_SIZE) * 4.

class ShadowVolumeObjectCache {
public :
    ShadowVolumeObjectCacheEntry entry[SRE_SHADOW_VOLUME_OBJECT_CACHE_SIZE * 4];
    int total_vertex_count;

    ShadowVolumeObjectCache();
    ShadowVolumeObjectCacheEntry *Lookup(int so_id, sreLODModelShadowVolume *model,
        const Vector4D& lightpos_model, int type);
    bool Add(int so_id, sreLODModelShadowVolume *model, const Vector4D& lightpos_model,
        GLuint opengl_id, int nu_vertices, int type, int array_buffer_flags);
    void PrintStats();
    void Clear();
};

static ShadowVolumeObjectCache object_cache;

ShadowVolumeObjectCache::ShadowVolumeObjectCache() {
   for (int i = 0; i < SRE_SHADOW_VOLUME_OBJECT_CACHE_SIZE * 4; i++)
       entry[i].so_id = - 1;
   total_vertex_count = 0;
}

static inline int CacheIndex(int so_id) {
    return ((so_id + sre_internal_current_light_index * 77) &
        (SRE_SHADOW_VOLUME_OBJECT_CACHE_SIZE - 1)) * 4;
}

ShadowVolumeObjectCacheEntry *ShadowVolumeObjectCache::Lookup(int so_id,
sreLODModelShadowVolume *model, const Vector4D& lightpos, int type) {
    int start_i = CacheIndex(so_id);
    for (int i = start_i; i < start_i + 4; i++)
        if (entry[i].so_id == so_id) {
            if (entry[i].model == model && entry[i].lightpos == lightpos && entry[i].type == type)
                return &entry[i];
        }
    return NULL;
}

bool ShadowVolumeObjectCache::Add(int so_id, sreLODModelShadowVolume *model,
const Vector4D& lightpos, GLuint opengl_id, int nu_vertices, int type, int array_buffer_flags) {
    // Check whether there is an empty space at the cache position.
    int min_timestamp = INT_MAX;
    int j;
    int start_i = CacheIndex(so_id);
    for (int i = start_i; i < start_i + 4; i++) {
        if (entry[i].so_id == -1) {
            // There is an empty space, add the entry
            j = i;
            goto set_entry;
        }
        if (entry[i].timestamp < min_timestamp) {
            min_timestamp = entry[i].timestamp;
            j = i;
        }
    }
    // Replace the least recently used entry.
    glDeleteBuffers(1, &entry[j].opengl_id);
    total_vertex_count -= entry[j].nu_vertices;
set_entry :
    entry[j].so_id = so_id;
    entry[j].model = model;
    entry[j].lightpos = lightpos;
    entry[j].opengl_id = opengl_id;
    entry[j].nu_vertices = nu_vertices;
    entry[j].type = type;
    entry[j].array_buffer_flags = array_buffer_flags;
    entry[j].timestamp = sre_internal_current_frame;
    total_vertex_count += nu_vertices;
    return true;
}

static int object_cache_hits = 0;
static int object_cache_hits_depthfail = 0;
static int object_cache_misses = 0;

void ShadowVolumeObjectCache::PrintStats() {
    int count = 0;
    int depth_fail = 0;
    for (int i = 0; i < SRE_SHADOW_VOLUME_OBJECT_CACHE_SIZE * 4; i++) {
        if (entry[i].so_id != -1) {
            count++;
            if (entry[i].type & TYPE_DEPTH_FAIL)
                depth_fail++;
        }
    }
    sreMessage(SRE_MESSAGE_INFO,
        "Shadow volume cache stats (frame = %d): Use = %3.2f%%, Hit-rate = %3.2f%%",
        sre_internal_current_frame, (float)count * 100 / (SRE_SHADOW_VOLUME_OBJECT_CACHE_SIZE * 4),
        (float)object_cache_hits * 100 / (object_cache_misses + object_cache_hits));
    sreMessage(SRE_MESSAGE_INFO,
        "Depth fail (of entries) = %3.2f%%, of hits = %3.2f%%", (float)depth_fail * 100 / count,
        (float)object_cache_hits_depthfail * 100 / object_cache_hits);
}

void ShadowVolumeObjectCache::Clear() {
   for (int i = 0; i < SRE_SHADOW_VOLUME_OBJECT_CACHE_SIZE * 4; i++)
        if (entry[i].so_id != -1) {
           glDeleteBuffers(1, &entry[i].opengl_id);
           entry[i].so_id = - 1;
        }
   total_vertex_count = 0;
}

// Check whether the scene object shadow volume is in the cache, if so draw it.

bool ShadowVolumeObjectCacheHit(sreObject *so, sreLODModelShadowVolume *model,
const Vector4D& lightpos_model, int type) {
    ShadowVolumeObjectCacheEntry *entry = object_cache.Lookup(so->id, model, lightpos_model, type);
    if (entry == NULL) {
        object_cache_misses++;
        return false;
    }
    object_cache_hits++;
    if (entry->type & TYPE_DEPTH_FAIL)
        object_cache_hits_depthfail++;

    entry->timestamp = sre_internal_current_frame; // Update LRU stat.

    // We found a match, draw from the cache.

    // Early exit is there are no vertices.
    if (entry->nu_vertices == 0)
        return true;

    GL3InitializeShadowVolumeShader(*so, lightpos_model);
    FinishDrawingShadowVolume(entry->type, model, entry->opengl_id, entry->array_buffer_flags,
        entry->nu_vertices);

    return true;
}

// Model shadow volume cache for directional lights/beam lights. Direct-mapped with
// model object id. Because there are only possible entries per object, in the rare
// case of multiple directional (or beam) lights affecting the model, the cache
// might become ineffective.

class ShadowVolumeModelCacheEntry {
public :
    sreLODModelShadowVolume *model; // NULL if empty.
    Vector4D lightpos;
    GLuint opengl_id;
    int nu_vertices;
    char type;
    char array_buffer_flags;
    int timestamp;
};

// There are SRE_SHADOW_VOLUME_MODEL_CACHE_SIZE cache lines of four cache entries each.
// Model object i is mapped to entries (i % (SRE_SHADOW_VOLUME_MODEL_CACHE_SIZE -1)) * 4
// to (i % (SRE_SHADOW_VOLUME_MODEL_CACHE_SIZE - 1)) * 4 + 3.

class ShadowVolumeModelCache {
public :
    ShadowVolumeModelCacheEntry entry[SRE_SHADOW_VOLUME_MODEL_CACHE_SIZE * 4];
    int total_vertex_count;

    ShadowVolumeModelCache();
    ShadowVolumeModelCacheEntry *Lookup(sreLODModelShadowVolume *model,
        const Vector4D& lightpos_model, int type);
    bool Add(sreLODModelShadowVolume *model, const Vector4D& lightpos_model,
        GLuint opengl_id, int nu_vertices, int type, int array_buffer_flags);
    void PrintStats();
    void Clear();
};

static ShadowVolumeModelCache model_cache;

ShadowVolumeModelCache::ShadowVolumeModelCache() {
   for (int i = 0; i < SRE_SHADOW_VOLUME_MODEL_CACHE_SIZE * 4; i++)
       entry[i].model = NULL;
   total_vertex_count = 0;
}

ShadowVolumeModelCacheEntry *ShadowVolumeModelCache::Lookup(sreLODModelShadowVolume *model,
const Vector4D& lightpos, int type) {
    int map_id = model->id & (SRE_SHADOW_VOLUME_MODEL_CACHE_SIZE - 1);
    for (int i = map_id * 4; i < map_id * 4 + 4; i++)
        if (entry[i].model == model) {
            if (entry[i].lightpos == lightpos && entry[i].type == type)
                return &entry[i];
        }
    return NULL;
}

bool ShadowVolumeModelCache::Add(sreLODModelShadowVolume *model, const Vector4D& lightpos,
GLuint opengl_id, int nu_vertices, int type, int array_buffer_flags) {
    // Check whether there is already an entry with this object id.
    int min_timestamp = INT_MAX;
    int j;
    int map_id = model->id & (SRE_SHADOW_VOLUME_MODEL_CACHE_SIZE - 1);
    for (int i = map_id * 4; i < map_id * 4 + 4; i++) {
        if (entry[i].model == NULL) {
            // There is an empty space, add the entry
            j = i;
            goto set_entry;
        }
        if (entry[i].timestamp < min_timestamp) {
            min_timestamp = entry[i].timestamp;
            j = i;
        }
    }
    // Replace the least recently used entry.
    glDeleteBuffers(1, &entry[j].opengl_id);
    total_vertex_count -= entry[j].nu_vertices;
set_entry :
    entry[j].model = model;
    entry[j].lightpos = lightpos;
    entry[j].opengl_id = opengl_id;
    entry[j].nu_vertices = nu_vertices;
    entry[j].type = type;
    entry[j].array_buffer_flags = array_buffer_flags;
    entry[j].timestamp = sre_internal_current_frame;
    total_vertex_count += entry[j].nu_vertices;
    return true;
}

static int model_cache_hits = 0;
static int model_cache_hits_depthfail = 0;
static int model_cache_misses = 0;

void ShadowVolumeModelCache::PrintStats() {
    int count = 0;
    int depth_fail = 0;
    for (int i = 0; i < SRE_SHADOW_VOLUME_MODEL_CACHE_SIZE * 4; i++) {
        if (entry[i].model != NULL) {
            count++;
            if (entry[i].type & TYPE_DEPTH_FAIL)
                depth_fail++;
        }
    }
    sreMessage(SRE_MESSAGE_INFO,
        "Shadow volume model_cache stats (frame = %d): Use = %3.2f%%, Hit-rate = %3.2f%%",
        sre_internal_current_frame, (float)count * 100 / (SRE_SHADOW_VOLUME_MODEL_CACHE_SIZE  * 4),
        (float)model_cache_hits * 100 / (model_cache_misses + model_cache_hits));
    sreMessage(SRE_MESSAGE_INFO,
        "Depth fail (of entries) = %3.2f%%, of hits = %3.2f%%", (float)depth_fail * 100 / count,
        (float)model_cache_hits_depthfail * 100 / model_cache_hits);
    model_cache_hits = 0;
    model_cache_misses = 0;
    model_cache_hits_depthfail = 0;
}

void ShadowVolumeModelCache::Clear() {
   for (int i = 0; i < SRE_SHADOW_VOLUME_MODEL_CACHE_SIZE * 4; i++)
        if (entry[i].model != NULL) {
           glDeleteBuffers(1, &entry[i].opengl_id);
           entry[i].model = NULL;
        }
   total_vertex_count = 0;
}

// Check whether the model object shadow volume is in the cache, if so draw it.

bool ShadowVolumeModelCacheHit(sreObject *so, sreLODModelShadowVolume *model,
const Vector4D& lightpos_model, int type) {
    ShadowVolumeModelCacheEntry *entry = model_cache.Lookup(model, lightpos_model, type);
    if (entry == NULL) {
        model_cache_misses++;
        return false;
    }
    model_cache_hits++;
    if (entry->type & TYPE_DEPTH_FAIL)
        model_cache_hits_depthfail++;

    entry->timestamp = sre_internal_current_frame; // Update LRU stat.

    // We found a match, draw from the cache.

    // Early exit is there are no vertices.
    if (entry->nu_vertices == 0)
        return true;

    GL3InitializeShadowVolumeShader(*so, lightpos_model);
    FinishDrawingShadowVolume(entry->type, model, entry->opengl_id, entry->array_buffer_flags,
        entry->nu_vertices);
    return true;
}


// Draw an object's shadow volume for the given light. The object has already been
// determined to be a shadow caster in terms of being in the light volume and in
// the shadow caster volume. However, it is still possible that the actual shadow
// volume does not intersect the frustum. When geometry scissors are active, the geometrical
// shadow volume may already have been calculated (sv_in is not NULL), otherwise it will be
// calculated when required. When the relevant rendering flag set, geometrical shadow volume
// will be tested against the view frustum when the object itself is outside the frustum.
// Another rendering flag defines whether the dark cap visibility test with depth-fail rendering
// is enabled.
//
// Any GPU scissors settings have been applied.

static void DrawShadowVolume(sreObject *so, sreLight *light, sreFrustum &frustum, sreShadowVolume *sv_in) {
        // Determine whether depth-pass or depth-fail rendering must be used.
        // If the shadow volume visibility test is enabled, also test whether the geometrical
        // shadow volume intersects with the view frustum.
        int type;
        sreShadowVolume *sv = sv_in;
        // Determine whether the object is visible in the current frame. The check required depends
        // on which type of octree (static or dynamic objects) the object was stored in. This should
        // be defined by the SRE_OBJECT_DYNAMIC_POSITION flag.
        bool object_is_visible = frustum.ObjectIsVisibleInCurrentFrame(
            *so, sre_internal_current_frame);
        // If shadow volume visibility test is enabled, check whether the geometrical shadow
        // volume is completely outside the frustum, in which case it can be skipped entirely.
        // If the test is disabled, just assume the shadow volume intersects the frustum.
        // Note: The calculated shadow volumes bound the actual geometrical shadow volume, not
        // the shadow volumes extruded to infinity that are used on the GPU.
        if (!object_is_visible &&
        (sre_internal_rendering_flags & SRE_RENDERING_FLAG_SHADOW_VOLUME_VISIBILITY_TEST)) {
            // Use any static precalculated shadow volume when available (sv != NULL), otherwise
            // calculate a temporary shadow volume. Generally when precalculated shadow volumes
            // are enabled, calculating a temporary shadow volume will only be required if the
            // combination of light and object produces changing a shadow volume (shape or
            // position changes).
            //
            // This includes cases such as a dynamic (moving or rotating) object with any kind
            // light, any type of object with a dynamic (moving) point/spot/beam light, or any
            // kind of object with a directional, spot or beam light that changes direction.
            //
            if (sv == NULL)
                so->CalculateTemporaryShadowVolume(*light, &sv);
            // If the shadow volume is completely outside the frustum, it can be skipped.
            if (frustum.ShadowVolumeIsOutsideFrustum(*sv)) {
//                printf("Shadow volume is outside frustum.\n");
                return;
            }
        }
        // If the object does not intersect the near-clip volume, depth pass rendering can be used.
        if (!(sre_internal_rendering_flags & SRE_RENDERING_FLAG_FORCE_DEPTH_FAIL)
        && !frustum.ObjectIntersectsNearClipVolume(*so))
            // Depth-pass rendering. Always only renders the sides of the shadow volume.
            type = TYPE_DEPTH_PASS;
        else {
            // Depth-fail rendering. Potentially, light cap, sides and dark cap may
            // need to be rendered.
            type = TYPE_DEPTH_FAIL;
            // The light cap can be skipped is the object itself is not visible.
            if (!object_is_visible)
                type |= TYPE_SKIP_LIGHTCAP;
            // Note: If the shadow volume was determined to be completely outside the frustum,
            // it has already been skipped. Otherwise, at least the sides of shadow volume
            // will always be visible. 
            // For directional lights or beam lights no dark cap is needed.
            if (light->type & (SRE_LIGHT_DIRECTIONAL | SRE_LIGHT_BEAM))
                 type |= TYPE_SKIP_DARKCAP;
            else if (sre_internal_rendering_flags & SRE_RENDERING_FLAG_SHADOW_VOLUME_DARKCAP_VISIBILITY_TEST) {
                // Otherwise, when enabled do a geometrical test to see whether the dark cap
                // is outside the frustum.
                // If the object is not visible, the shadow volume was already calculated;
                // otherwise calculate it.
                if (object_is_visible && sv == NULL)
                    so->CalculateTemporaryShadowVolume(*light, &sv);
                // If the dark cap is outside the frustum, it can be skipped.
                if (frustum.DarkCapIsOutsideFrustum(*sv))
                    type |= TYPE_SKIP_DARKCAP;
            }
            // At least the sides will need to be drawn.
        }
        sre_internal_shadow_volume_count++;

        // Calculate the light position in model space.
        Vector4D lightpos_model;
        if (light->type & SRE_LIGHT_BEAM)
            // Special case for beam lights: the "light position" is set to the negative beam
            // light direction, with a w component of 0. This uses the same mechanism as
            // directional lights so that the extruded dark cap vertices will all be equal to the
            // beam light direction extruded to infinity.
            lightpos_model = so->inverted_model_matrix * Vector4D(- light->spotlight.GetVector3D(), 0);
        else
            // Otherwise, simply use the light vector (which is the light position for point
            // and spot lights, and the negative light direction with a w component of 0 for
            // directional lights).
            lightpos_model = so->inverted_model_matrix * light->vector;
        // Determine the LOD model.
        sreLODModelShadowVolume *m = (sreLODModelShadowVolume *)sreCalculateLODModel(*so);

        // Keep track of the cache use for this object, 0 is no cache, 1 is object
        // cache, 2 is model cache.
        int cache_used = 0;

        // Check the shadow volume cache, and if it's a hit draw the shadow volume from the cache.
        if (!(sre_internal_rendering_flags & SRE_RENDERING_FLAG_SHADOW_CACHE_ENABLED))
            goto skip_cache;

        // If the light is changing every frame in such a way that shadow volumes will be affected,
        // skip the cache.
        if (light->ShadowVolumeIsChangingEveryFrame(sre_internal_current_frame)) {
            object_cache_misses++;
            goto skip_cache;
        }

        if (light->type & (SRE_LIGHT_POINT_SOURCE | SRE_LIGHT_SPOT)) {
            // Any continuous change of position, rotation or scaling of the object
            // will affect the shadow volume for point/spot lights.
            if (so->IsChangingEveryFrame(sre_internal_current_frame)) {
                object_cache_misses++;
                goto skip_cache;
            }
            cache_used = 1;
        }
        else {
            // For directional and beam lights, only rotation or scaling will
            // affect the shadow volume (just position change doesn't change the
            // shadow volume).
            if (so->IsChangingTransformationEveryFrame(sre_internal_current_frame)) {
                model_cache_misses++;
                goto skip_cache;
           }
           // Set the cache to the model cache, except when the SRE_OBJECT_USE_OBJECT_SHADOW_CACHE
           // flag is set. This flag should generally be set when multiple objects
           // with different fixed rotation/scaling of the same model are used.
           cache_used = 2 - ((so->flags & SRE_OBJECT_USE_OBJECT_SHADOW_CACHE) != 0);
        }

        // The CacheHit function will draw the shadow volume and return true in case of
        // a case hit.
        if (cache_used == 1) {
            // Use the object shadow volume cache (primarily for point source and spot
            // lights, but may be used for other lights if SRE_OBJECT_USE_OBJECT_SHADOW_CACHE
            // flag was set).
            if (ShadowVolumeObjectCacheHit(so, m, lightpos_model, type))
                return;
        }
        else {
            // Directional light or beam light, use the model cache.
            if (ShadowVolumeModelCacheHit(so, m, lightpos_model, type))
                return;
        }

skip_cache :
        // Have to calculate a new shadow volume vertex buffer and upload
        // it to the GPU.
        sre_internal_silhouette_count++;
        // Generate a new buffer if required.
        if (element_buffer_id == 0xFFFFFFFF)
            glGenBuffers(1, &element_buffer_id);

        // Calculate silhouette edges.
        silhouette_edges->m = m;
        silhouette_edges->full_model = so->model;
        CalculateSilhouetteEdges(lightpos_model, silhouette_edges);

        // The number of edges in the silhouette limits the worst-case total amount of vertices
        // in the shadow volume.
        //
        // With depth-pass rendering (sides only), the maximum number of vertices is equal
        // to the number of silhouette edges * 6 for point and spot lights (two triangles
        // required for each edge), while for directional and beam lights it is
        // equal to the number of silhouette edges * 3 (one triangle required for each edge).
        //
        // With depth-fail rendering, front cap, sides and dark cap may need to be included.
        // For point or spot lights, the maximum number of vertices in the shadow volume is
        // the number of silhouette edges * 6 (sides) + the number of model triangles * 3
        // (the light cap and dark cap combined total not more than the total number of
        // triangles in the object).
        //
        // For directional lights and beam lights, the maximum number of vertices is also
        // number of silhouette edges * 3 (sides) + the number of model triangles * 3
        // (only a light cap is required, but it can potentially have as many triangles
        // as the whole object if the triangle detail of the object is concentrated on
        // the light-facing side).
        int max_vertices;
        if (type & TYPE_DEPTH_PASS) {
            if (light->type & (SRE_LIGHT_SPOT | SRE_LIGHT_POINT_SOURCE))
                max_vertices = silhouette_edges->nu_edges * 6;
            else
                max_vertices = silhouette_edges->nu_edges * 3;
        }
        else {
            if (!(type & TYPE_SKIP_SIDES)) {
                if (light->type & (SRE_LIGHT_SPOT | SRE_LIGHT_POINT_SOURCE))
                    max_vertices = silhouette_edges->nu_edges * 6;
                else
                    max_vertices = silhouette_edges->nu_edges * 3;
            }
            // Add the vertices for the front and/or dark cap. If any of these
            // is present, the maximum amount of combined vertices is never
            // greater than the total number of triangles in the model * 3.
            if ((type & (TYPE_SKIP_DARKCAP | TYPE_SKIP_LIGHTCAP)) !=
            (TYPE_SKIP_DARKCAP | TYPE_SKIP_LIGHTCAP))
                max_vertices += m->nu_triangles * 3;
        }
        // Dynamically enlarge the shadow volume vertex buffer when needed.
        if (max_vertices > max_shadow_volume_vertices) {
            delete [] (int *)shadow_volume_vertex;
            // Add a little extra capacity to avoid constant reallocation in the unlikely
            // theoretical case of a small number of triangles being added to the largest
            // model continuously.
            shadow_volume_vertex = new int[max_vertices + 1024];
            max_shadow_volume_vertices = max_vertices + 1024;
        }

        int array_buffer_flags;
        if (m->GL_indexsize == 2)
            array_buffer_flags = SRE_SHADOW_VOLUME_ARRAY_BUFFER_FLAG_SHORT_INDEX;
        else
            array_buffer_flags = 0;
        GL3InitializeShadowVolumeShader(*so, lightpos_model);
        nu_shadow_volume_vertices = 0;
        if (type & TYPE_DEPTH_PASS) {
            // Depth-pass rendering.
            if ((sre_internal_rendering_flags & SRE_RENDERING_FLAG_USE_TRIANGLE_FANS_FOR_SHADOW_VOLUMES)
            && cache_used != 0 && (light->type & (SRE_LIGHT_DIRECTIONAL | SRE_LIGHT_BEAM))
            && !(m->flags & (SRE_LOD_MODEL_NOT_CLOSED | SRE_LOD_MODEL_CONTAINS_HOLES))) {
                // For closed models without holes with a directional light or beam light, we
                // can create a triangle fan representing the shadow volume.
                // Because constructing a triangle fan is more processor/memory intensive than
                // a regular shadow volume, only try when the shadow volume will be cached
                // subsequently.
                array_buffer_flags |= SRE_SHADOW_VOLUME_ARRAY_BUFFER_FLAG_TRIANGLE_FAN;
                bool r = AddSidesTriangleFan(silhouette_edges, *light, array_buffer_flags);
                if (r) {
                    array_buffer_flags |= SRE_SHADOW_VOLUME_ARRAY_BUFFER_FLAG_TRIANGLE_FAN;
#if 0
                    printf("Triangle fan shadow volume constructed (%d vertices).\n", nu_shadow_volume_vertices);
#endif
                }
                else {
                    // Triangle fan construction not succesful; use a regular shadow volume
                    // consisting of triangles.
#ifdef DEBUG_RENDER_LOG
                    if (sre_internal_debug_message_level >= 1)
                        printf("Triangle fan shadow volume construction failed for model %d.\n", m->id);
#endif
                    AddSides(silhouette_edges, *light, array_buffer_flags);
                }
            }
            else
#ifndef NO_PRIMITIVE_RESTART
            if ((sre_internal_rendering_flags & SRE_RENDERING_FLAG_USE_TRIANGLE_STRIPS_FOR_SHADOW_VOLUMES)
            && (light->type & (SRE_LIGHT_POINT_SOURCE | SRE_LIGHT_SPOT))) {
                // When we just need the sides for a point or spot light, we can use triangle strips with
                // primitive restart for all of the shadow volume, resulting in a small saving of GPU space.
                array_buffer_flags |= SRE_SHADOW_VOLUME_ARRAY_BUFFER_FLAG_TRIANGLE_STRIP;
                AddSidesTriangleStrip(silhouette_edges, *light, array_buffer_flags);
            }
            else
#endif
                AddSides(silhouette_edges, *light, array_buffer_flags);
            if (nu_shadow_volume_vertices > 0) {
                glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_INCR_WRAP);
                glStencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_DECR_WRAP);
                DrawShadowVolumeGL(m, array_buffer_flags);
            }
        }
        else {
            // Depth-fail rendering.
            if ((type & (TYPE_SKIP_SIDES | TYPE_SKIP_DARKCAP | TYPE_SKIP_LIGHTCAP)) ==
            (TYPE_SKIP_DARKCAP | TYPE_SKIP_LIGHTCAP)) {
                // Just sides required.
                if ((sre_internal_rendering_flags & SRE_RENDERING_FLAG_USE_TRIANGLE_FANS_FOR_SHADOW_VOLUMES)
                && cache_used != 0 && (light->type & (SRE_LIGHT_DIRECTIONAL | SRE_LIGHT_BEAM))
                && !(m->flags & (SRE_LOD_MODEL_NOT_CLOSED | SRE_LOD_MODEL_CONTAINS_HOLES))) {
                    // For closed models with a directional light or beam light, we can create a
                    // triangle fan representing the shadow volume (sides only).
                    bool r = AddSidesTriangleFan(silhouette_edges, *light, array_buffer_flags);
                    if (r)
                        array_buffer_flags |= SRE_SHADOW_VOLUME_ARRAY_BUFFER_FLAG_TRIANGLE_FAN;
                    else {
                        // Triangle fan construction not succesful; use a regular shadow volume
                        // consisting of triangles.
#ifdef DEBUG_RENDER_LOG
                        if (sre_internal_debug_message_level >= 1)
                            printf("Triangle fan shadow volume construction failed for model %d.\n", m->id);
#endif
                        AddSides(silhouette_edges, *light, array_buffer_flags);
                    }
                }
                else
#ifndef NO_PRIMITIVE_RESTART
                if ((sre_internal_rendering_flags & SRE_RENDERING_FLAG_USE_TRIANGLE_STRIPS_FOR_SHADOW_VOLUMES)
                && (light->type & (SRE_LIGHT_SPOT | SRE_LIGHT_POINT_SOURCE))) {
                    // When we just need the sides for a point or spot light, we can use triangle strips with
                    // primitive restart for all of the shadow volume.
                    array_buffer_flags |= SRE_SHADOW_VOLUME_ARRAY_BUFFER_FLAG_TRIANGLE_STRIP;
                    AddSidesTriangleStrip(silhouette_edges, *light, array_buffer_flags);
                }
                else
#endif
                    AddSides(silhouette_edges, *light, array_buffer_flags);
            }
            else {
                 // At least a lightcap or darkcap is needed.
                if (!(type & TYPE_SKIP_SIDES))
                    AddSides(silhouette_edges, *light, array_buffer_flags);
                if (!(type & TYPE_SKIP_DARKCAP))
                    AddDarkCap(silhouette_edges, array_buffer_flags);
                if (!(type & TYPE_SKIP_LIGHTCAP))
                    AddLightCap(silhouette_edges, array_buffer_flags);
            }
            if (nu_shadow_volume_vertices > 0) {
                glStencilOpSeparate(GL_BACK, GL_KEEP, GL_INCR_WRAP, GL_KEEP);
                glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_DECR_WRAP, GL_KEEP);
                DrawShadowVolumeGL(m, array_buffer_flags);
            }
        }

        // Add to the cache when applicable.
        // Note: In the unlikely case that the number of shadow volumes vertices is zero,
        // (which probably shouldn't happen), still add to the cache to minimize overhead.
        if (cache_used == 1) {
            if (object_cache.Add(so->id, m, lightpos_model, element_buffer_id,
            nu_shadow_volume_vertices, type, array_buffer_flags))
                // If added to the object cache, mark the current buffer as invalid.
                element_buffer_id = 0xFFFFFFFF;
        }
        else if (cache_used == 2) {
            if (model_cache.Add(m, lightpos_model, element_buffer_id, nu_shadow_volume_vertices, type,
            array_buffer_flags))
                // If added to the model cache, mark the current buffer as invalid.
                element_buffer_id = 0xFFFFFFFF;
        }
}

static void SetGLScissors(const sreScissors& scissors) {
#ifndef __GNUC__
    int left = floorf((scissors.left + 1.0f) * 0.5f * sre_internal_window_width);
    int right = ceilf((scissors.right + 1.0f) * 0.5f * sre_internal_window_width);
    int bottom = floorf((scissors.bottom + 1.0f) * 0.5f * sre_internal_window_height);
    int top = ceilf((scissors.top + 1.0f) * 0.5f * sre_internal_window_height);
#else
    fesetround(FE_DOWNWARD);
    int left = lrint((scissors.left + 1.0f) * 0.5f * sre_internal_window_width);
    int bottom = lrint((scissors.bottom + 1.0f) * 0.5f * sre_internal_window_height);
    fesetround(FE_UPWARD);
    int right = lrint((scissors.right + 1.0f) * 0.5f * sre_internal_window_width);
    int top = lrint((scissors.top + 1.0f) * 0.5f * sre_internal_window_height);
    fesetround(FE_TONEAREST);
#endif
//    printf("glScissor(%d, %d, %d, %d)\n", left, bottom, right - left, top - bottom);
    glScissor(left, bottom, right - left, top - bottom);
}

static int octree_count, octree_count2, octree_count3;
static bool custom_scissors_set;
static bool custom_depth_bounds_set;

// Render a shadow volume for a shadow-casting object with per-object geometry
// scissors enabled. A specific scissors region for the object's shadow volume is
// calculated and applied if it is smaller than the pre-existing light scissors region.

static void RenderShadowVolumeGeometryScissors(sreObject *so, sreLight *light, sreFrustum& frustum) {
            sreShadowVolume *sv;
            bool viewport_adjusted = false;
            bool depth_bounds_adjusted = false;
            // Calculate the shadow volume, or get a precalculated one.
            so->CalculateTemporaryShadowVolume(*light, &sv);
            if (sv->type == SRE_BOUNDING_VOLUME_EMPTY)
                return;
            sreScissors *scissors;
            sreScissors shadow_volume_scissors;
            if (sv->type == SRE_BOUNDING_VOLUME_EVERYWHERE)
                // No shadow volume could be calculated.
                scissors = &frustum.scissors;
            else {
                // A shadow volume was calculated.
                // Calculate shadow volume scissors.
                bool region_is_not_empty = so->CalculateShadowVolumeScissors(*light, frustum, *sv,
                    shadow_volume_scissors);
                if (!region_is_not_empty)
                    return;
                // If the light scissors region is smaller than the geometry scissors calculated
                // for the shadow volume, adjust the shadow volume scissors.
                shadow_volume_scissors.left = maxf(shadow_volume_scissors.left,
                    frustum.scissors.left);
                shadow_volume_scissors.right = minf(shadow_volume_scissors.right,
                    frustum.scissors.right);
                shadow_volume_scissors.bottom = maxf(shadow_volume_scissors.bottom,
                    frustum.scissors.bottom);
                shadow_volume_scissors.top = minf(shadow_volume_scissors.top,
                    frustum.scissors.top);
                viewport_adjusted = !shadow_volume_scissors.ScissorsRegionIsEqual(frustum.scissors);
#ifdef DEBUG_SCISSORS
                if (viewport_adjusted && so->id == 0 && light->id == 1) {
                    printf("Light scissors (%f, %f), (%f, %f) ", frustum.scissors.left, frustum.scissors.right,
                        frustum.scissors.bottom, frustum.scissors.top);
                    printf(" adjusted to (%f, %f), (%f, %f) for object %d shadow volume\n",
                        shadow_volume_scissors.left, shadow_volume_scissors.right,
                        shadow_volume_scissors.bottom, shadow_volume_scissors.top, so->id);
                }
#endif
#ifndef NO_DEPTH_BOUNDS
                shadow_volume_scissors.near = maxf(shadow_volume_scissors.near,
                    frustum.scissors.near);
                shadow_volume_scissors.far = minf(shadow_volume_scissors.far,
                    frustum.scissors.far);
                depth_bounds_adjusted = !shadow_volume_scissors.DepthBoundsAreEqual(frustum.scissors);
#ifdef DEBUG_SCISSORS
                if (depth_bounds_adjusted)
                    printf("Depth bounds adjusted to (%f, %f) for object %d\n",
                        shadow_volume_scissors.near, shadow_volume_scissors.far, so->id);
#endif
#endif
                 scissors = &shadow_volume_scissors;
            }

            // Update scissors and depth bounds when required.
            if (viewport_adjusted || custom_scissors_set) {
                SetGLScissors(*scissors);
                custom_scissors_set = viewport_adjusted;
            }
#ifndef NO_DEPTH_BOUNDS
            if (GLEW_EXT_depth_bounds_test && (depth_bounds_adjusted || custom_depth_bounds_set)) {
                glDepthBoundsEXT(scissors->near, scissors->far);
                custom_depth_bounds_set = depth_bounds_adjusted;
            }
#endif
            DrawShadowVolume(so, light, frustum, sv);
}

static void RenderShadowVolume(sreObject *so, sreLight *light, sreFrustum& frustum) {
    DrawShadowVolume(so, light, frustum, NULL);
}

// Flags indicating whether an octree is completely inside the light volume
// or shadow caster volume.
enum {
    OCTREE_IS_INSIDE_LIGHT_VOLUME = 1,
    OCTREE_IS_INSIDE_SHADOW_CASTER_VOLUME = 2,
    OCTREE_IS_INSIDE_BOTH = 3,
    OCTREE_HAS_NO_BOUNDS = 4
};

// Scene octree traversal, rendering required shadow volumes for shadow casting objects as
// we encounter them. This renders shadow volumes for all objects in the octree. We keep track
// of how the octree node intersects with the light volume, so that octrees that
// are outside the light volume are skipped entirely, and for octrees that are completely
// inside the light volume we don't have to check whether the object intersects with the
// light volume.
//
// Note: the root node must have defined bounds, or the OCTREE_HAS_NO_BOUNDS flag must be
// set in intersection_flags.
//
// This function is currently not used, because we prefer to precompile a list of all
// shadow casters before rendering the shadow volumes.

#if 0

static void RenderShadowVolumesForFastOctree(const sreFastOctree& fast_oct, int array_index,
sreScene *scene, sreLight *light, sreFrustum &frustum, int intersection_flags) {
    octree_count++;

        // Update whether the intersection of the light volume and
        // the shadow caster volume intersect with the octree.
        int node_index = fast_oct.array[array_index];
        // At this point intersection_flags represents the properties of the parent octree.
        // If the parent octree was inside the light volume or shadow caster volume, the
        // current node will also be.
        if ((intersection_flags & (OCTREE_IS_INSIDE_SHADOW_CASTER_VOLUME
        | OCTREE_HAS_NO_BOUNDS)) == 0) {
            // If the octree is not completely inside the shadow caster volume, check whether
            // the octree intersects shadow caster volume.
            BoundsCheckResult r = QueryIntersection(fast_oct.node_bounds[node_index],
                frustum.shadow_caster_volume);
            if (r == SRE_COMPLETELY_OUTSIDE) {
                // If the octree is completely outside the shadow caster volume,
                // we can discard it completely.
                octree_count2++;
                return;
            }
            if (r == SRE_COMPLETELY_INSIDE) {
                // Set the flag indicating the octree is completely inside both light volume and
                // shadow caster volume (this will also be true for all subnodes that descend from
                // this node).
                intersection_flags |= OCTREE_IS_INSIDE_SHADOW_CASTER_VOLUME;
            }
        }
        if ((intersection_flags & (OCTREE_IS_INSIDE_LIGHT_VOLUME
        | OCTREE_HAS_NO_BOUNDS)) == 0) {
            // Parent octree is not completely inside the light volume.
            // Check whether the current octree node is inside the light volume; if it completely
            // outside, discard the octree, if it is completely inside, update the flags.
            BoundsCheckResult r =  QueryIntersection(fast_oct.node_bounds[node_index], *light);
            if (r == SRE_COMPLETELY_OUTSIDE) {
                octree_count2++;
                return;
            }
            if (r == SRE_COMPLETELY_INSIDE)
                intersection_flags |= OCTREE_IS_INSIDE_LIGHT_VOLUME;
        }

    bool use_geometry_scissors;
    // Geometry scissors generally do not make sense for a directional light's
    // shadow volumes, because they are not bounded by a light volume, so the
    // extruded vertices normally need to clipped only by screen boundaries,
    // not a specific scissors. However, for local lights defining a scissors
    // region based on the geometrical shadow volume (which is bounded by the
    // light volume) is useful because the GPU draws shadow volumes extruded
    // to infinity; the shadow volume-specific scissors region is often smaller
    // than the default scissors region defined for the whole light volume.
    if ((sre_internal_scissors & SRE_SCISSORS_GEOMETRY_MASK) &&
    !(light->type & SRE_LIGHT_DIRECTIONAL))
        use_geometry_scissors = true;
    else
        use_geometry_scissors = false;

    // Render all objects in this node.
    int nu_octants = fast_oct.GetNumberOfOctants(array_index + 1);
    int nu_entities = fast_oct.array[array_index + 2];
    array_index += 3;
    for (int i = 0; i < nu_entities; i++) {
        sreSceneEntityType type;
        int index;
        fast_oct.GetEntity(array_index + i, type, index);
        if (type != SRE_ENTITY_OBJECT)
            continue;
        sreObject *so = scene->object[index];
        if (!so->exists)
            continue;
        // Exclude objects that do not cast shadows.
        if (!(so->flags & SRE_OBJECT_CAST_SHADOWS) ||
        !(so->model->model_flags & SRE_MODEL_SHADOW_VOLUMES_CONFIGURED))
            continue;
        // If the object is attached to the current light, don't cast shadows for this object.
        if (so->attached_light == sre_internal_current_light_index)
            continue;
        octree_count3++;
        if (!(intersection_flags & OCTREE_IS_INSIDE_LIGHT_VOLUME))
            // Check whether the object intersects the light volume.
            if (!Intersects(*so, *light))
                continue;
        if (!(intersection_flags & OCTREE_IS_INSIDE_SHADOW_CASTER_VOLUME))
            // Check whether the object can cast shadows into the frustum.
            if (!Intersects(*so, frustum.shadow_caster_volume))
                continue;
        if (use_geometry_scissors)
            RenderShadowVolumeGeometryScissors(so, light, frustum);
        else
            RenderShadowVolume(so, light, frustum);
    }
    // Render every non-empty subnode.
    array_index += nu_entities;
    for (int i = 0; i < nu_octants; i++)
        RenderShadowVolumesForsreFastOctree(fast_oct, fast_oct.array[array_index + i], scene, light, frustum,
            intersection_flags);
}

#endif

// This function is duplicated in shadowmap.cpp.

static void CheckShadowCasterCapacity(sreScene *scene) {
    if (scene->nu_shadow_caster_objects == scene->max_shadow_caster_objects) {
        // Dynamically increase the shadow caster object array when needed.
        int *new_shadow_caster_object = new int[scene->max_shadow_caster_objects * 2];
        memcpy(new_shadow_caster_object, scene->shadow_caster_object,
            sizeof(int) * scene->max_shadow_caster_objects);
        delete [] scene->shadow_caster_object;
        scene->shadow_caster_object = new_shadow_caster_object;
        scene->max_shadow_caster_objects *= 2;
    }
}

// Determine shadow casters for just the root node of an octree, used for dynamic object octrees
// which contain only the root node. We perform a light volume intersection test and
// shadow caster volume intersection test for every potential shadow casting object. Any
// shadow casters found are stored; the shadow volumes will be rendered later on together
// with those from the list of the static object shadow casters for the light.
//
// Just for directional lights, OCTREE_IS_INSIDE_LIGHT_VOLUME can be set in intersection_flags
// (directional lights are everywhere even if the octree bounds are not defined).
// In the current implementation, for directional lights no shadow caster array is
// predetermined but rather shadow volumes are rendered immediately when they are encountered
// during octree traversal. Potentially, directional light shadow casters lists for static
// objects could be reused from the previous frame if the frustum hasn't changed (similar to
// the way visible object lists are reused in this scenario).

static void DetermineShadowCastersFromFastOctreeRootNode(const sreFastOctree& fast_oct,
sreScene *scene, sreLight *light, sreFrustum &frustum, int intersection_flags) {
    octree_count++;
    // Render all objects in this node.
    int nu_entities = fast_oct.array[2];
    for (int i = 0; i < nu_entities; i++) {
        sreSceneEntityType type;
        int index;
        fast_oct.GetEntity(3 + i, type, index);
        if (type != SRE_ENTITY_OBJECT)
            continue;
        sreObject *so = scene->object[index];
        if (!so->exists)
            continue;
        // Exclude objects that do not cast shadows.
        if (!(so->flags & SRE_OBJECT_CAST_SHADOWS) ||
        !(so->model->model_flags & SRE_MODEL_SHADOW_VOLUMES_CONFIGURED))
            continue;
        // If the object is attached to the current light, don't cast shadows for this object.
        if (so->attached_light == sre_internal_current_light_index)
            continue;
        octree_count3++;
        // Check whether the object intersects with the light volume.
        if (!(intersection_flags & OCTREE_IS_INSIDE_LIGHT_VOLUME))
            if (!Intersects(*so, *light))
                continue;
        // Check whether the object can cast shadows into the frustum.
        if (!Intersects(*so, frustum.shadow_caster_volume)) {
//            printf("Object %d is outside shadow caster volume.\n", so->id);
            continue;
        }

        CheckShadowCasterCapacity(scene);
        scene->shadow_caster_object[scene->nu_shadow_caster_objects] = index;
        scene->nu_shadow_caster_objects++;
    }
}

// Determine shadow casters for a whole octree, usually the static object octree.
// We perform a light volume intersection test and shadow caster volume intersection test
// for every potential shadow casting object. Any shadow casters found are stored; the
// shadow volumes will be rendered later on together with those determined the dynamic
// object octree.

static void DetermineShadowCastersFromFastOctree(const sreFastOctree& fast_oct, int array_index,
sreScene *scene, sreLight *light, sreFrustum &frustum, int intersection_flags) {
        // Update whether the intersection of the light volume and
        // the shadow caster volume intersect with the octree.
        int node_index = fast_oct.array[array_index];
        // At this point intersection_flags represents the properties of the parent octree.
        // If the parent octree was inside the light volume or shadow caster volume, the
        // current node will also be.
        if ((intersection_flags & (OCTREE_IS_INSIDE_SHADOW_CASTER_VOLUME
        | OCTREE_HAS_NO_BOUNDS)) == 0) {
            // If the octree is not completely inside the shadow caster volume, check whether
            // the octree intersects shadow caster volume.
            BoundsCheckResult r = QueryIntersection(fast_oct.node_bounds[node_index],
                frustum.shadow_caster_volume);
            if (r == SRE_COMPLETELY_OUTSIDE) {
                // If the octree is completely outside the shadow caster volume,
                // we can discard it completely.
                octree_count2++;
                return;
            }
            if (r == SRE_COMPLETELY_INSIDE) {
                // Set the flag indicating the octree is completely inside both light volume and
                // shadow caster volume (this will also be true for all subnodes that descend from
                // this node).
                intersection_flags |= OCTREE_IS_INSIDE_SHADOW_CASTER_VOLUME;
            }
        }
        if ((intersection_flags & (OCTREE_IS_INSIDE_LIGHT_VOLUME
        | OCTREE_HAS_NO_BOUNDS)) == 0) {
            // Parent octree is not completely inside the light volume.
            // Check whether the current octree node is inside the light volume; if it completely
            // outside, discard the octree, if it is completely inside, update the flags.
            BoundsCheckResult r =  QueryIntersection(fast_oct.node_bounds[node_index], *light);
            if (r == SRE_COMPLETELY_OUTSIDE) {
                octree_count2++;
                return;
            }
            if (r == SRE_COMPLETELY_INSIDE)
                intersection_flags |= OCTREE_IS_INSIDE_LIGHT_VOLUME;
        }

    // Add all shadow-casting objects in this node.
    int nu_octants = fast_oct.GetNumberOfOctants(array_index + 1);
    int nu_entities = fast_oct.array[array_index + 2];
    array_index += 3;
    for (int i = 0; i < nu_entities; i++) {
        sreSceneEntityType type;
        int index;
        fast_oct.GetEntity(array_index + i, type, index);
        if (type != SRE_ENTITY_OBJECT)
            continue;
        sreObject *so = scene->object[index];
        if (!so->exists)
            continue;
        // Exclude objects that do not cast shadows.
        if (!(so->flags & SRE_OBJECT_CAST_SHADOWS) ||
        !(so->model->model_flags & SRE_MODEL_SHADOW_VOLUMES_CONFIGURED))
            continue;
        // If the object is attached to the current light, don't cast shadows for this object.
        if (so->attached_light == sre_internal_current_light_index)
            continue;
        octree_count3++;
        if (!(intersection_flags & OCTREE_IS_INSIDE_LIGHT_VOLUME))
            // Check whether the object intersects the light volume.
            if (!Intersects(*so, *light))
                continue;
        if (!(intersection_flags & OCTREE_IS_INSIDE_SHADOW_CASTER_VOLUME))
            // Check whether the object can cast shadows into the frustum.
            if (!Intersects(*so, frustum.shadow_caster_volume)) {
//                printf("Object %d is outside shadow caster volume.\n", so->id);
                continue;
            }
        // Add the object.
        CheckShadowCasterCapacity(scene);
        scene->shadow_caster_object[scene->nu_shadow_caster_objects] = index;
        scene->nu_shadow_caster_objects++;
    }
    // Traverse every non-empty subnode.
    array_index += nu_entities;
    for (int i = 0; i < nu_octants; i++)
        DetermineShadowCastersFromFastOctree(fast_oct, fast_oct.array[array_index + i], scene,
            light, frustum, intersection_flags);
}

// Determine shadow casters from a list of predetermined static shadow-casting objects for
// the light stored in the light's data structure. Since the object might be outside
// the shadow caster volume that is associated with the current frustum, we have to check
// whether the object intersects with it. Additionally, when the light has
// SRE_LIGHT_WORST_CASE_BOUNDS_SPHERE is set, the light has a variable light volume (that is
// bounded by a sphere) and we have to check whether the object really intersects the smaller,
// actual (current) light volume. The lights that match are stored in the scene's shadow
// caster array.

static void DetermineShadowCastersFromLightStaticCasterArray(const sreFastOctree& fast_oct,
sreScene *scene, sreLight *light, sreFrustum &frustum) {
    for (int i = 0; i < light->nu_shadow_caster_objects; i++) {
        int j = light->shadow_caster_object[i];
        sreObject *so = scene->object[j];
        if (!so->exists)
            continue;
        octree_count3++;
        if (light->type & SRE_LIGHT_WORST_CASE_BOUNDS_SPHERE)
            if (!Intersects(*so, *light))
                continue;
        // Check whether the object can cast shadows into the frustum.
        if (!Intersects(*so, frustum.shadow_caster_volume))
            continue;
        CheckShadowCasterCapacity(scene);
        scene->shadow_caster_object[scene->nu_shadow_caster_objects] = j;
        scene->nu_shadow_caster_objects++;
    }
}

// Render shadow volumes from the compiled list of shadow-casting objects.
// They have already been predetermined to be shadow casters and intersect the
// shadow caster volume for the current frustum.

static void RenderShadowVolumesFromCompiledCasterArray(sreScene *scene,
sreLight *light, sreFrustum &frustum) {
    bool use_geometry_scissors;
    if ((sre_internal_scissors & SRE_SCISSORS_GEOMETRY_MASK) &&
    !(light->type & SRE_LIGHT_DIRECTIONAL))
        use_geometry_scissors = true;
    else
        use_geometry_scissors = false;
    if (use_geometry_scissors)
       for (int i = 0; i < scene->nu_shadow_caster_objects; i++) {
            sreObject *so = scene->object[scene->shadow_caster_object[i]];
            RenderShadowVolumeGeometryScissors(so, light, frustum);
       }
    else
       for (int i = 0; i < scene->nu_shadow_caster_objects; i++) {
            sreObject *so = scene->object[scene->shadow_caster_object[i]];
            RenderShadowVolume(so, light, frustum);
       }
}

// Render all shadow volumes for a light.

void sreRenderShadowVolumes(sreScene *scene, sreLight *light, sreFrustum &frustum) {
    // Calculate the shadow caster volume that encloses the light source and the view volume.
    frustum.CalculateShadowCasterVolume(scene->light[sre_internal_current_light_index]->vector, 5);

    // Compile a list of all shadow casters, and store them in the scene's shadow caster array.
    scene->nu_shadow_caster_objects = 0;
    if (sre_internal_light_object_lists_enabled &&
    (light->type & SRE_LIGHT_STATIC_SHADOW_CASTER_LIST)) {
        // When static object lists for lights are enabled, there will be list precalculated
        // at initialization time of likely shadow casting static objects for every local
        // light that has a light volume that can by bounded in any reasonable way (for
        // variable lights, it may be relatively large, but will be usable if not extremely
        // large).
        // From that list, we can add the objects that can cast shadows into the current
        // frustum. For variable lights with only worst-case light volume bounds, the static
        // list might include objects outside the light volume so we have to check that too.
        DetermineShadowCastersFromLightStaticCasterArray(scene->fast_octree_dynamic,
            scene, light, frustum);
        // Add the dynamic object shadow casters from the dynamic objects octree.
        DetermineShadowCastersFromFastOctreeRootNode(scene->fast_octree_dynamic,
            scene, light, frustum, 0);
    }
    else {
        // When there is no static objects list for the light, we have to walk both
        // the static and dynamic object octrees.
        // Add the static object shadow casters from the static objects octree.
        DetermineShadowCastersFromFastOctree(scene->fast_octree_static, 0,
            scene, light, frustum, 0);
        // Add the dynamic object shadow casters from the dynamic objects octree.
        DetermineShadowCastersFromFastOctreeRootNode(scene->fast_octree_dynamic,
            scene, light, frustum, 0);
    }

    // Predetermining shadow casting objects has the advantage that we can exit
    // early when they are none such objects, avoid overhead like stencil buffer
    // clearing, and we can disable the stencil test entirely for the light.
    if (scene->nu_shadow_caster_objects == 0) {
        glDisable(GL_STENCIL_TEST);
        return;
    }

    // One-pass two-sided stencil rendering.
#ifdef PING_PONG_BUFFERS
    glGenBuffers(1, &element_buffer0_id);
    glGenBuffers(1, &element_buffer1_id);
    current_element_buffer = 0;
#endif
    last_vertexbuffer_id = 0xFFFFFFFF;
    // Create global silhouette edges data structure.
    if (silhouette_edges == NULL) {
        silhouette_edges = new EdgeArray;
    }
    if (shadow_volume_vertex == NULL) {
        shadow_volume_vertex = new int[SRE_DEFAULT_MAX_SHADOW_VOLUME_VERTICES];
        max_shadow_volume_vertices = SRE_DEFAULT_MAX_SHADOW_VOLUME_VERTICES;
    }
    octree_count = octree_count2 = octree_count3 = 0;

    custom_scissors_set = false;
    custom_depth_bounds_set = false;

    // Clear the stencil buffer, taking advantage of the scissor region.
    glClear(GL_STENCIL_BUFFER_BIT);

    // Calculate near clip volume from the light source to the viewport.
    frustum.CalculateNearClipVolume(scene->light[sre_internal_current_light_index]->vector);

    // Draw the shadow volumes for this light.
    // Render into the stencil buffer.

    // Enable stencil updates. The stencil test should already be enabled.
    glStencilFunc(GL_ALWAYS, 0x00, ~0);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE); // Disable color writing.
//    glDepthMask(GL_FALSE); // Disable writing into the depth buffer (this is already set at a higher level).

    glDisable(GL_CULL_FACE);

#ifdef SHADOW_COLOR_DEBUG
    // Visualize shadow volumes for debugging
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE); // DEBUG
    glDisable(GL_STENCIL_TEST);
    glDepthMask(GL_TRUE);
#endif

    // There are two possible modes of operation: either we precompiled the list of guaranteed
    // static and dynamic object shadow casters, or we have to walk the octrees to determine
    // the shadow casters and render the shadow volumes as we find them. At the moment, the first
    // method is always used, especially because it allows us to exit early for any kind of light
    // when there are no shadow casters.
    if (true)
        // Render shadows from the compiled list of shadow casters.
        RenderShadowVolumesFromCompiledCasterArray(scene, light, frustum);
#if 0
    else {
        // Render static, then dynamic objects from their respective octrees.
        // For directional lights, the octree is always completely inside the light volume.
        int flags;
        if (light->type & SRE_LIGHT_DIRECTIONAL)
            flags = OCTREE_IS_INSIDE_LIGHT_VOLUME;
        else
            flags = 0;
        RenderShadowVolumesForFastOctree(scene->fast_octree_static, 0, scene, light,
            frustum, flags);
        RenderShadowVolumesForFastOctree(scene->fast_octree_dynamic, 0, scene, light,
            frustum, flags | OCTREE_HAS_NO_BOUNDS);
    }
#endif

#ifdef SHADOW_COLOR_DEBUG
    glEnable(GL_STENCIL_TEST);
    glDepthMask(GL_FALSE);
#endif

    // Restore GL settings.
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glEnable(GL_CULL_FACE);

    // Configure the stencil test for the additive lighting pass.
    glStencilFunc(GL_EQUAL, 0x00, ~0);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP); // Using GL_REPLACE might help clearing for next frame.

//    printf("Light %d: %d of %d octree nodes rejected in RenderShadowVolumesWithOctree\n", sre_internal_current_light_index,
//        octree_count2, octree_count);
//    printf("Light %d: %d scene objects iterated.\n", sre_internal_current_light_index, octree_count3);
//    printf("%d shadow volumes drawn, %d silhouettes calculated.\n", sre_internal_shadow_volume_count, sre_internal_silhouette_count);
//    delete [] shadow_volume_vertex;
//    delete silhouette_edges;
    glDisableVertexAttribArray(0);
#ifdef PING_PONG_BUFFERS
    glDeleteBuffers(1, &element_buffer0_id);
    glDeleteBuffers(1, &element_buffer1_id);
#endif
}

void sreReportShadowCacheStats() {
    if ((sre_internal_rendering_flags & SRE_RENDERING_FLAG_SHADOW_CACHE_ENABLED)
    && sre_internal_current_frame % 50 == 0) {
        object_cache.PrintStats();
        model_cache.PrintStats();
    }
}

void sreResetShadowCacheStats() {
    // Reset stats before frame.
    sre_internal_shadow_volume_count = 0;
    sre_internal_silhouette_count = 0;
    model_cache_hits = 0;
    model_cache_misses = 0;
    model_cache_hits_depthfail = 0;
    object_cache_hits = 0;
    object_cache_misses = 0;
    object_cache_hits_depthfail = 0;
}

void sreSetShadowCacheStatsInfo(sreShadowRenderingInfo *info) {
    int object_count = 0;
    int object_depth_fail = 0;
    for (int i = 0; i < SRE_SHADOW_VOLUME_OBJECT_CACHE_SIZE * 4; i++) {
        if (object_cache.entry[i].so_id != -1)
            object_count++;
        if (object_cache.entry[i].type & TYPE_DEPTH_FAIL)
            object_depth_fail++;
    }
    int model_count = 0;
    int model_depth_fail = 0;
    for (int i = 0; i < SRE_SHADOW_VOLUME_MODEL_CACHE_SIZE * 4; i++) {
        if (model_cache.entry[i].model != NULL)
            model_count++;
        if (model_cache.entry[i].type & TYPE_DEPTH_FAIL)
            model_depth_fail++;
    }
    info->object_cache_total_entries = SRE_SHADOW_VOLUME_OBJECT_CACHE_SIZE * 4;
    info->object_cache_entries_used = object_count;
    info->object_cache_total_vertex_count = object_cache.total_vertex_count;
    info->object_cache_hits = object_cache_hits;
    info->object_cache_misses = object_cache_misses;
    info->object_cache_entries_depthfail = object_depth_fail;
    info->object_cache_hits_depthfail = object_cache_hits_depthfail;
    info->model_cache_total_entries = SRE_SHADOW_VOLUME_MODEL_CACHE_SIZE * 4;
    info->model_cache_entries_used = model_count;
    info->model_cache_total_vertex_count = model_cache.total_vertex_count;
    info->model_cache_hits = model_cache_hits;
    info->model_cache_misses = model_cache_misses;
    info->model_cache_entries_depthfail = model_depth_fail;
    info->model_cache_hits_depthfail = model_cache_hits_depthfail;
}

void sreClearShadowCache() {
    object_cache.Clear();
    model_cache.Clear();
}
