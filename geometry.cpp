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
#include <malloc.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <float.h>

#include "win32_compat.h"
#include "sre.h"
#include "sre_internal.h"
#include "sre_bounds.h"

#include <dstMemory.h>

// Higher level model class constuctor.

sreModel::sreModel() {
    // Reset the number of LOD levels.
    nu_lod_levels = 0;
    lod_model[0] = NULL;
    lod_threshold_scaling = 1.0f;
    nu_polygons = 0;
    is_static = false;
    referenced = false;
    model_flags = 0;
    bv_special.type = SRE_BOUNDING_VOLUME_UNDEFINED;
}

sreModel::~sreModel() {
    for (int j = 0; j < nu_lod_levels; j++) {
        sreLODModel *lm = lod_model[j];
        if (lm->flags & SRE_LOD_MODEL_UPLOADED)
            lm->DeleteFromGPU();
        lm->flags &= ~SRE_LOD_MODEL_UPLOADED;
        if (lm->flags & SRE_LOD_MODEL_IS_FLUID_MODEL)
            delete (sreLODModelFluid *)lm;
        else if (lm->flags & SRE_LOD_MODEL_IS_SHADOW_VOLUME_MODEL)
            delete (sreLODModelShadowVolume *)lm;
        else
            delete lm;
    }
    // Note: The deconstructor for bv_special will be called automatically.
    if (bounds_flags & SRE_BOUNDS_SPECIAL_SRE_COLLISION_SHAPE)
        delete special_collision_shape;
}

// Model instancing. A new sreModel structure is created, but the
// fields are copied from the source model.

sreModel *sreModel::CreateNewInstance() const {
    sreModel *m = new sreModel;
    *m = (*this);
    m->referenced = false;
    // Instance the LOD levels as well.
    for (int i = 0; i < nu_lod_levels; i++) {
        m->lod_model[i] = lod_model[i]->CreateCopy();
        m->lod_model[i]->referenced = false;
        // Set instance flags to indicate that all attributes are
        // shared from parent LOD model.
        m->lod_model[i]->instance_flags = 0;
    }
    return m;
}

// Polygon data types; only used with higher level sreModel class
// for preprocessing purposes.

sreModelPolygon::sreModelPolygon() {
    nu_vertices = 0;
}

void sreModelPolygon::InitializeWithSize(int n) {
    nu_vertices = n;
    vertex_index = new int[n];
}

void sreModelPolygon::AddVertex(int j) {
    int *new_vertex_index = new int[nu_vertices + 1];
    for (int i = 0; i < nu_vertices; i++)
        new_vertex_index[i] = vertex_index[i];
    if (nu_vertices > 0)
        delete vertex_index;
    vertex_index = new_vertex_index;
    vertex_index[nu_vertices] = j;
    nu_vertices++;
}

// LOD model constructors.

sreLODModel::sreLODModel() {
    nu_meshes = 1;
    referenced = false;
    instance_flags = SRE_ALL_ATTRIBUTES_MASK;
    flags = 0;
    // Note: when a new sreLODModel is created, normally
    // the sreBaseModel constructor and this constructor
    // are called in succession.
}

sreLODModelShadowVolume::sreLODModelShadowVolume() {
    nu_edges = 0;
    flags = SRE_LOD_MODEL_IS_SHADOW_VOLUME_MODEL;
    // Note: when a new sreLODModelShadowVolume is created,
    // normally the sreBaseModel, sreLODModel and this constructor
    // are called in succession.
}

sreLODModelShadowVolume::~sreLODModelShadowVolume() {
    DestroyEdges();
}

// Constructor helper functions.

sreLODModel *sreLODModel::AllocateNewOfSameType() const {
    if (flags & SRE_LOD_MODEL_IS_SHADOW_VOLUME_MODEL)
        return new sreLODModelShadowVolume;
    else
        return new sreLODModel;
}

sreLODModel *sreLODModel::CreateCopy() const {
    sreLODModel *m = AllocateNewOfSameType();
    if (flags & SRE_LOD_MODEL_IS_SHADOW_VOLUME_MODEL)
        *(sreLODModelShadowVolume *)m = *(sreLODModelShadowVolume *)this;
    else
        *m = *this;
    return m;
}

// Library functions for sreLODModel creation. 

sreLODModel *sreNewLODModel() {
    if (sre_internal_rendering_flags & SRE_RENDERING_FLAG_SHADOW_VOLUME_SUPPORT)
        return new sreLODModelShadowVolume;
    else
        return new sreLODModel;
}

sreLODModel *sreNewLODModelNoShadowVolume() {
    return new sreLODModel;
}

// Base model constructor.

sreBaseModel::sreBaseModel() {
    nu_vertices = 0;
    nu_triangles = 0;
    sorting_dimension = - 1; // Not sorted.
    flags = 0;
}

// Base model geometry component constructor.

sreBaseModel::sreBaseModel(int nu_vertices, int nu_triangles, int flags) {
    sreBaseModel();
    position = dstNewAligned <Point3DPadded>((size_t)nu_vertices, 16);
    triangle = new sreModelTriangle[nu_triangles];
    if (flags & SRE_NORMAL_MASK)
        vertex_normal = new Vector3D[nu_vertices];
    if (flags & SRE_TEXCOORDS_MASK)
        texcoords = new Point2D[nu_vertices];
    if (flags & SRE_TANGENT_MASK)
        vertex_tangent = new Vector4D[nu_vertices];
    if (flags & SRE_COLOR_MASK)
        colors = new Color[nu_vertices];
}

void sreBaseModel::SetPositions(Point3D *_positions) {
    // Allocate to 16-byte boundary for SIMD.
    // This functions requires special handling for the case when the class
    // position array pointer is already initialized and passed as _positions.
    if (((uintptr_t)_positions & 0xF) == 0 && sizeof(Point3D) == sizeof(Point3DPadded)) {
        position = (Point3DPadded *)_positions;
        return;
    }
    // Buffer has to be realigned.
    Point3DPadded *new_position = dstNewAligned <Point3DPadded>((size_t)nu_vertices, 16);
    if (sizeof(Point3D) == sizeof(Point3DPadded))
        dstMemcpyAlignedLarge(new_position, (Point3DPadded *)_positions,
		nu_vertices * sizeof(Point3DPadded));
    else {
        for (int i = 0; i < nu_vertices; i++)
            new_position[i] = _positions[i];
    }
    free(_positions);
    position = new_position;
}

void sreBaseModel::SetPositions(Point3DPadded *_positions) {
    // Allocate to 16-byte boundary for SIMD.
    // This functions requires special handling for the case when the class
    // position array pointer is already initialized and passed as _positions.
    if (((uintptr_t)_positions & 0xF) == 0) {
        position = _positions;
        return;
    }
    // Buffer has to be realigned.
    Point3DPadded *new_position = dstNewAligned <Point3DPadded>((size_t)nu_vertices, 16);
    dstMemcpyAlignedLarge(new_position, _positions, nu_vertices * sizeof(Point3DPadded));
    free(_positions);
    position = new_position;
}

void sreBaseModel::SetTexcoords(Point2D *_texcoords) {
    texcoords = _texcoords;
}

void sreBaseModel::SetColors(Color *_colors) {
    colors = _colors;
}

void sreBaseModel::SetTangents(Vector4D *_tangents) {
    vertex_tangent = _tangents;
}

// Base model destructor.

sreBaseModel::~sreBaseModel() {
    if (flags & instance_flags & SRE_POSITION_MASK)
        free(position);
    if (flags & instance_flags & SRE_TEXCOORDS_MASK)
        delete [] texcoords;
    if (flags & instance_flags & SRE_COLOR_MASK)
        delete [] colors;
    if (flags & instance_flags & SRE_NORMAL_MASK)
        delete [] vertex_normal;
    if (flags & instance_flags & SRE_TANGENT_MASK)
        delete [] vertex_tangent;
    if (nu_triangles > 0)
        delete [] triangle;
}

// Set/clear specific flags of all LOD models in a sreModel.

void sreModel::SetLODModelFlags(int flag_mask) {
    for (int i = 0; i < nu_lod_levels; i++)
        lod_model[i]->flags |= flag_mask;
}

void sreModel::ClearLODModelFlags(int flag_mask) {
    for (int i = 0; i < nu_lod_levels; i++)
        lod_model[i]->flags &= ~flag_mask;
}

// Remap vertices using the index mapping provided (from new index to
// previous index).
// When n is not equal to nu_vertices (the mapping changes the number of
// vertices), the number of vertices will be reduced to n and _vertex_mapping2
// provides an additional mapping from previous index to new index.

void sreBaseModel::RemapVertices(int *vertex_mapping, int n, int *_vertex_mapping2) {
    // We have the mapping from new indices to the original index.
    // Now determine the vertex mapping from the original index to the new index.
    int *vertex_mapping2 ;
    if (n != nu_vertices)
        // When the number of vertices changes, the second mapping from previous index
        // to new index is provided as an argument.
        vertex_mapping2 = _vertex_mapping2;
    else {
        // Otherwise, calculate vertex mapping from the original index to the new index,
        // but do it lazily by only creating the vertex mapping when it is detected
        // that vertex_mapping is not a simple one-to-one mapping.
        int bits = 0;
        for (int i = 0; i < nu_vertices; i++)
            bits |= vertex_mapping[i] - i;
        if (bits == 0)
            // The mapping is an identical one-to-one mapping. There's nothing to do.
            return;
        vertex_mapping2 = new int[nu_vertices];
        for (int i = 0; i < nu_vertices; i++)
            vertex_mapping2[vertex_mapping[i]] = i;
    }
    // Remap the triangle vertices using the second mapping.
    for (int i = 0; i < nu_triangles; i++)
        for (int j = 0; j < 3; j++)
            triangle[i].vertex_index[j] = vertex_mapping2[triangle[i].vertex_index[j]];
    // Free the second mapping if we created it locally.
    if (n == nu_vertices)
        delete [] vertex_mapping2;
    else
        // The number of vertices has changed.
        nu_vertices = n;
    Point3DPadded *new_vertex = dstNewAligned <Point3DPadded>((size_t)nu_vertices, 16);
    Point2D *new_texcoords;
    if (flags & SRE_TEXCOORDS_MASK)
        new_texcoords = new Point2D[nu_vertices];
    Color *new_colors;
    if (flags & SRE_COLOR_MASK)
        new_colors = new Color[nu_vertices];
    Vector3D *new_normals;
    if (flags & SRE_NORMAL_MASK)
        new_normals = new Vector3D[nu_vertices];
    Vector4D *new_tangents;
    if (flags & SRE_TANGENT_MASK)
        new_tangents = new Vector4D[nu_vertices];
    for (int i = 0 ; i < nu_vertices; i++) {
        new_vertex[i] = vertex[vertex_mapping[i]];
        if (flags & SRE_TEXCOORDS_MASK)
            new_texcoords[i] = texcoords[vertex_mapping[i]];
        if (flags & SRE_COLOR_MASK)
            new_colors[i] = colors[vertex_mapping[i]];
        if (flags & SRE_NORMAL_MASK)
            new_normals[i] = vertex_normal[vertex_mapping[i]];
        if (flags & SRE_TANGENT_MASK)
            new_tangents[i] = vertex_tangent[vertex_mapping[i]];
    }
    delete [] vertex;
    vertex = new_vertex;
    if (flags & SRE_TEXCOORDS_MASK) {
        delete [] texcoords;
        texcoords = new_texcoords;
    }
    if (flags & SRE_COLOR_MASK) {
        delete [] colors;
        colors = new_colors;
    }
    if (flags & SRE_NORMAL_MASK) {
        delete [] vertex_normal;
        vertex_normal = new_normals;
    }
    if (flags & SRE_TANGENT_MASK) {
        delete [] vertex_tangent;
        vertex_tangent = new_tangents;
    }
}

// Sorting.

static int qsort_sorting_dimension;
static sreBaseModel *qsort_object;

static int CompareVertices(const void *e1, const void *e2) {
    int i1 = *(int *)e1;
    int i2 = *(int *)e2;
    if (qsort_object->vertex[i1][qsort_sorting_dimension] <
    qsort_object->vertex[i2][qsort_sorting_dimension])
        return - 1;
    if (qsort_object->vertex[i1][qsort_sorting_dimension] >
    qsort_object->vertex[i2][qsort_sorting_dimension])
        return 1;
    return 0;
}

// Sort vertices on the given coordinate dimension. Sorted models greatly increase the speed
// of operations such as merging identical vertices, calculating vertex normals, etc.

void sreBaseModel::SortVertices(int dimension) {
    // Vertex mapping from new index to original index.
    int *vertex_mapping = new int[nu_vertices];
    for (int i = 0; i < nu_vertices; i++)
        vertex_mapping[i] = i;
    qsort_sorting_dimension = dimension;
    qsort_object = this;
    qsort(vertex_mapping, nu_vertices, sizeof(int), CompareVertices);
    RemapVertices(vertex_mapping, nu_vertices, NULL);
    delete [] vertex_mapping;
    sorting_dimension = dimension; // Indicate on which dimension the object has been sorted.
}

// Find the optimal sorting dimension (the one with the least number of vertices
// with identical sorting coordinate) and sort the vertices.

void sreBaseModel::SortVerticesOptimalDimension() {
    // Vertex mapping from new index to original index, for each
    // sorting dimension.
    int *vertex_mapping[3];
    int nu_shared_coordinates[3];
    for (int dim = 0; dim < 3; dim++) {
        vertex_mapping[dim] = new int[nu_vertices];
        for (int i = 0; i < nu_vertices; i++)
            vertex_mapping[dim][i] = i;
        qsort_sorting_dimension = dim;
        qsort_object = this;
        qsort(vertex_mapping[dim], nu_vertices, sizeof(int), CompareVertices);
        // Determine the number number of vertices that share exactly the same
        // sorting coordinate with the previous vertex in the array.
        nu_shared_coordinates[dim] = 0;
        for (int i = 0; i < nu_vertices - 1; i++)
            if (vertex[vertex_mapping[dim][i]] == vertex[vertex_mapping[dim][i + 1]])
                nu_shared_coordinates[dim]++;
    }
    int best_dim = 0;
    if (nu_shared_coordinates[1] < nu_shared_coordinates[0])
        best_dim = 1;
    if (nu_shared_coordinates[2] < nu_shared_coordinates[best_dim])
        best_dim = 2;
    // If the vertices were already sorted on the optimal sorting dimension,
    // keep the model unchanged.
    if (best_dim != sorting_dimension) {
        // Remap the vertices.
        RemapVertices(vertex_mapping[best_dim], nu_vertices, NULL);
        sorting_dimension = best_dim; // Indicate on which dimension the object has been sorted.
    }
    for (int dim = 0; dim > 3; dim++)
        delete [] vertex_mapping[dim];
}

// Merge vertices with almost identical vertices and texcoords, colors and normals if applicable.
// Any previously existing sorting order will be preserved.
// When the model vertices is not sorted, the optimal sorting dimension is determined and the
// vertices are sorted. For large models, this greatly increases processing speed.
//
// If the save_indices argument is not NULL, the vertex index mapping from new index to
// original index is stored in the array pointed to by saved_indices, which must be
// preallocated with enough space to hold the vertex indices (which is never more
// than the original number of vertices). This is used during edge calculation.

void sreBaseModel::MergeIdenticalVertices(int *saved_indices) {
    if (sorting_dimension == - 1)
        SortVerticesOptimalDimension();
    // Index mapping from new index to original index.
    int *vertex_mapping;
    if (saved_indices != NULL)
        vertex_mapping = saved_indices;
    else
        vertex_mapping = new int[nu_vertices];
    // Vertex mapping from original index to new index.
    int *vertex_mapping2 = new int[nu_vertices];
    int n = 0;  // Number of vertices assigned.
    for (int i = 0; i < nu_vertices; i++) {
        // Try to find a similar vertex among those we already assigned.
        int k;
        bool found_similar = false;
        for (k = n - 1; k >= 0; k--) {
            // If the vertices are sorted, the assigned vertices are also sorted, and we can stop
            // when we reach a distance of EPSILON_DEFAULT (defined in sreVectorMath.h for the
            // AlmostEqual functions) in the sorted direction, moving in negative order
            // direction.
            if (sorting_dimension != - 1) {
                if (vertex[vertex_mapping[k]][sorting_dimension] < 
                vertex[i][sorting_dimension] - EPSILON_DEFAULT)
                    break;
            }
            if (!AlmostEqual(vertex[i], vertex[vertex_mapping[k]]))
                continue;
            if ((flags & SRE_TEXCOORDS_MASK) &&
            !AlmostEqual(texcoords[i], texcoords[vertex_mapping[k]]))
                continue;
            if ((flags & SRE_COLOR_MASK) &&
            !AlmostEqual(colors[i], colors[vertex_mapping[k]]))
                continue;
            if ((flags & SRE_NORMAL_MASK) &&
            !AlmostEqual(vertex_normal[i], vertex_normal[vertex_mapping[k]]))
                continue;
            // The vertices are similar.
            found_similar = true;
            break;
        }
        if (found_similar) {
            // We found a similar vertex among those we already processed. Remove
            // vertex i and replace any references to it by updating the mapping
            // from original index to new index to point to the similar vertex k.
            vertex_mapping2[i] = k;
            // Note: Since we are just removing vertices, the sorting order is unaffected.
        }
        else {
            // No similar vertex was found; copy the vertex and update the mappings.
            vertex_mapping[n] = i;
            vertex_mapping2[i] = n;
            n++;
        }
    }
    int original_nu_vertices = nu_vertices;
    // Remap vertices if we removed any.
    if (n != nu_vertices)
        RemapVertices(vertex_mapping, n, vertex_mapping2);
    if (saved_indices == NULL)
        delete [] vertex_mapping;
    delete [] vertex_mapping2;
    if (nu_vertices != original_nu_vertices && sre_internal_debug_message_level >= 2)
        printf("MergeIdenticalVertices: vertices reduced from %d to %d.\n",
            original_nu_vertices, nu_vertices);
}

void sreBaseModel::MergeIdenticalVertices() {
    MergeIdenticalVertices(NULL);
}

// Remove vertices not used in any triangle.

void sreBaseModel::RemoveUnusedVertices(int *saved_indices) {
    bool *vertex_used = new bool[nu_vertices];
    for (int i = 0; i < nu_vertices; i++)
        vertex_used[i] = false;
    int count = 0;
    for (int i = 0; i < nu_triangles; i++)
        for (int j = 0; j < 3; j++)
            if (!vertex_used[triangle[i].vertex_index[j]]) {
                count++;
                vertex_used[triangle[i].vertex_index[j]] = true;
            }
    if (count == nu_vertices && saved_indices == NULL) {
        // No unused vertices were found.
        delete [] vertex_used;
        return;
    }
    //  Mapping from new index to original index.
    int *vertex_mapping;
    if (saved_indices != NULL)
        vertex_mapping = saved_indices;
    else
        vertex_mapping = new int[nu_vertices];
    // Vertex mapping from original index to new index.
    int *vertex_mapping2 = new int[nu_vertices];
    int n = 0;  // Number of vertices assigned.
    for (int i = 0; i < nu_vertices; i++) {
        if (vertex_used[i]) {
            // Copy the vertex.
            vertex_mapping[n] = i;
            vertex_mapping2[i] = n;
            n++;
        }
    }
    delete [] vertex_used;
    int original_nu_vertices = nu_vertices;
    RemapVertices(vertex_mapping, n, vertex_mapping2);
    if (saved_indices == NULL)
        delete [] vertex_mapping;
    delete [] vertex_mapping2;
    if (sre_internal_debug_message_level >= 2)
        printf("RemoveUnusedVertices: vertices reduced from %d to %d.\n",
            original_nu_vertices, nu_vertices);
}

void sreBaseModel::RemoveUnusedVertices() {
    RemoveUnusedVertices(NULL);
}

// Weld vertices with almost equal position coordinates so that they have exactly
// the same coordinates. No vertices are removed, and no triangle vertex indices
// are changed.
//
// This is different from MergeIdenticalVertices, which actually removes similar
// vertices but only when all attibutes used (including texcoords, normals etc.)
// are the same.
//
// When the vertices are not sorted, this function sorts them on the optimal
// sorting dimension; also, the function preserves the sorting order (either
// pre-existing or the new optimal sorting order) upon exit.

void sreBaseModel::WeldVertices() {
    // When the vertices are not sorted, this an O(n^2) algorithm which is very
    // slow for large models. Force sorting on optimal dimension.
    if (sorting_dimension == - 1)
        SortVerticesOptimalDimension();
    int count = 0;
    bool need_resort = false;
    for (int i = 0; i < nu_vertices; i++) {
        // Try to find a similar vertex among those we already checked.
        bool found_similar = false;
        int k;
        for (k = i - 1; k >= 0; k--) {
            // If the vertices are sorted, the checked vertices are also sorted, and we can stop
            // when we reach a distance of EPSILON_DEFAULT (defined in sreVectorMath.h for the
            // AlmostEqual functions) in the sorted direction, moving in negative order
            // direction.

            if (sorting_dimension != - 1)
                if (vertex[k][sorting_dimension] < vertex[i][sorting_dimension] - EPSILON_DEFAULT)
                    break;
            if (AlmostEqual(vertex[i], vertex[k])) {
                // The vertices are similar.
                found_similar = true;
                break;
            }
        }
        if (found_similar) {
            // We found a similar vertex. Use Point3D class comparison function
            // to check that the position is not already exactly the same.
            if (vertex[i] != vertex[k]) {
                // Make the vertices identical.
                vertex[i] = vertex[k];
                count++;
                 // It is possible that this operation invalidates the sorting order
                // when there are vertices in between index k and i that have a
                // sorting coordinate that is greater than the vertex at index k.
                if (k < i - 1 &&
                vertex[i - 1][sorting_dimension] > vertex[k][sorting_dimension])
                    need_resort = true;
           }
        }
    }
    // Re-sort the vertices if required.
    if (need_resort)
        SortVertices(sorting_dimension);
    if (sre_internal_debug_message_level >= 1)
        printf("WeldVertices: %d vertices welded for model.\n", count);
}

// Create a copy of the model with the same basic geometry (vertex positions
// and triangles). The clone argument must be an already allocated
// sreBaseModel. The flags field of the clone is set to indicate that
// only positions are present.

void sreBaseModel::CloneGeometry(sreBaseModel *clone) {
     clone->nu_vertices = nu_vertices;
     clone->vertex = new Point3DPadded[nu_vertices];
     for (int i = 0; i < nu_vertices; i++)
         clone->vertex[i] = vertex[i];
     clone->nu_triangles = nu_triangles;
     clone->triangle = new sreModelTriangle[nu_triangles];
     for (int i = 0; i < nu_triangles; i++)
         clone->triangle[i] = triangle[i];
     clone->sorting_dimension = sorting_dimension;
     clone->flags = SRE_POSITION_MASK;
}

// Create a copy of a model, including normals, texcoords, colors and tangents
// when present. The clone argument must be an already allocated sreBaseModel
// (or sreLODModel). The flags field of the clone is set to reflect the
// attributes that are present.

void sreBaseModel::Clone(sreBaseModel *clone) {
    CloneGeometry(clone);
    // Make sure the position array is properly aligned.
    clone->SetPositions(clone->position);
    clone->flags = flags;
    if (flags & SRE_NORMAL_MASK)
        clone->vertex_normal = new Vector3D[nu_vertices];
    if (flags & SRE_TEXCOORDS_MASK)
        clone->texcoords = new Point2D[nu_vertices];
    if (flags & SRE_COLOR_MASK)
        clone->colors = new Color[nu_vertices];
    if (flags & SRE_TANGENT_MASK)
        clone->vertex_tangent = new Vector4D[nu_vertices];    
    for (int i = 0 ; i < nu_vertices; i++) {
        if (flags & SRE_TEXCOORDS_MASK)
            clone->texcoords[i] = texcoords[i];
        if (flags & SRE_COLOR_MASK)
            clone->colors[i] = colors[i];
        if (flags & SRE_NORMAL_MASK)
            clone->vertex_normal[i] = vertex_normal[i];
        if (flags & SRE_TANGENT_MASK)
            clone->vertex_tangent[i] = vertex_tangent[i];
    }
}

// Calculate the normal of every triangle in the model.

void sreBaseModel::CalculateTriangleNormals() {
    // Calculate triangle normals
     for (int i = 0; i < nu_triangles; i++) {
        triangle[i].normal = CalculateNormal(vertex[triangle[i].vertex_index[0]],
            vertex[triangle[i].vertex_index[1]], vertex[triangle[i].vertex_index[2]]);
    }
}

// Calculate vertex normals by averaging the triangle normals of triangles that
// include vertex. Apart from triangles that contain the same vertex index, the normal
// of triangles that contain a vertex with a different index but exactly the same
// position is also taken into account. Generally, this function applies to curved
// models rather than blocky models.
//
// The vertex index range for which to calculate vertex normals is supplied as arguments
// (usually it would be nu_vertices vertices starting at index 0).
//
// When the vertices are not sorted, this function sorts them on the optimal
// sorting dimension, but only when the given vertex index range covers the whole model.
// So for a large model segment (not the whole model), this function is very slow when
// when the vertices were not previously sorted.

void sreBaseModel::CalculateNormals(int start_index, int nu_vertices_in_segment, bool verbose) {
    CalculateTriangleNormals();
    // When the vertices are not sorted, this is an O(n^2) algorithm which is very
    // slow for large models. Force sorting on optimal dimension, but ony when the given
    // vertex index range covers the whole model.
    if (sorting_dimension == - 1 && nu_vertices_in_segment == nu_vertices)
        SortVerticesOptimalDimension();
    // Calculate vertex normals
    if (verbose) {
        printf(", vertices ");
        fflush(stdout);
    }
    // Set vertex normals to zero.
    for (int i = start_index; i < start_index + nu_vertices_in_segment; i++)
        vertex_normal[i].Set(0, 0, 0);
    // Determine every triangle that includes a specific vertex, and for each
    // of these triangles add the triangle normal to the vertex normal. Also include
    // triangles that do not contain the specific vertex index, but do contain a vertex
    // with exactly the same position.
    for (int j = 0; j < nu_triangles; j++) {
        // It would help to know the triangle index range for the segment.
        if (triangle[j].vertex_index[0] >= start_index &&
        triangle[j].vertex_index[0] < start_index + nu_vertices_in_segment)
            vertex_normal[triangle[j].vertex_index[0]] += triangle[j].normal;
        if (triangle[j].vertex_index[1] >= start_index &&
        triangle[j].vertex_index[1] < start_index + nu_vertices_in_segment)
            vertex_normal[triangle[j].vertex_index[1]] += triangle[j].normal;
        if (triangle[j].vertex_index[2] >= start_index &&
        triangle[j].vertex_index[2] < start_index + nu_vertices_in_segment)
            vertex_normal[triangle[j].vertex_index[2]] += triangle[j].normal;
        // Also look for other vertices with exactly the same position as one of the
        // triangle vertices.
        if (sorting_dimension != - 1) {
            // Take advantage when vertices are sorted. When this is the case,
            // we only need to look for other vertices with exactly the same sorted
            // coordinate, and check whether the other coordinates are the same.
            // We only need to look just below and just above the specific vertex
            // index in the ordered vertex array.
            for (int k = 0; k < 3; k++) {
                // Look just above.
                for (int i = triangle[j].vertex_index[k] + 1;
                i <= start_index + nu_vertices_in_segment; i++) {
                    if (vertex[i][sorting_dimension] !=
                    vertex[triangle[j].vertex_index[k]][sorting_dimension])
                        break;
                    if (vertex[i] == vertex[triangle[j].vertex_index[k]])
                        vertex_normal[i] += triangle[j].normal;
                }
                // Look just below.
                for (int i = triangle[j].vertex_index[k] - 1; i >= start_index; i--) {
                    if (vertex[i][sorting_dimension] !=
                    vertex[triangle[j].vertex_index[k]][sorting_dimension])
                        break;
                    if (vertex[i] == vertex[triangle[j].vertex_index[k]])
                        vertex_normal[i] += triangle[j].normal;
                }
            }
        }
        else
            // Have to traverse the whole set of vertices when they are unsorted.
            // For large models, this can be very slow (algorithm takes
            // O(nu_vertices * nu_triangles) time). We have to skip the vertices
            // for which we already added the triangle's normal.
            for (int i = start_index; i < start_index + nu_vertices_in_segment; i++) {
                if (i != triangle[j].vertex_index[0]
                && vertex[i] == vertex[triangle[j].vertex_index[0]])
                    vertex_normal[i] += triangle[j].normal;
                if (i != triangle[j].vertex_index[1]
                && vertex[i] == vertex[triangle[j].vertex_index[1]])
                    vertex_normal[i] += triangle[j].normal;
                if (i != triangle[j].vertex_index[2]
                && vertex[i] == vertex[triangle[j].vertex_index[2]])
                    vertex_normal[i] += triangle[j].normal;
            }
    }
    for (int i = start_index; i < start_index + nu_vertices_in_segment; i++)
        vertex_normal[i].Normalize();
#if 0
    for (int i = start_index; i < start_index + nu_vertices_in_segment; i++) {
        Vector3D sum;
        sum.Set(0, 0, 0);
        for (int j = 0; j < nu_triangles; j++) {
            if (triangle[j].vertex_index[0] == i ||
            triangle[j].vertex_index[1] == i ||
            triangle[j].vertex_index[2] == i ||
            // Also check the actual position of the vertex. For cases of multiple occurences of the
            // same vertex with different texcoords, this avoids visible discordances in the shading.
            vertex[triangle[j].vertex_index[0]] == vertex[i] ||
            vertex[triangle[j].vertex_index[1]] == vertex[i] ||
            vertex[triangle[j].vertex_index[2]] == vertex[i]) {
                sum += triangle[j].normal;
            }
        }
        sum.Normalize();
        vertex_normal[i] = sum;
        if (verbose && i % (nu_vertices / 10) == 0 && i != 0) {
            printf("%d0%% ", i * 10 / nu_vertices + 1);
            fflush(stdout);
        }
    }
#endif
    if (verbose)
        printf("\n");
    flags |= SRE_NORMAL_MASK;
}

// Calculate vertex normals for the whole model, no progress output.

void sreBaseModel::CalculateNormals() {
    CalculateNormals(0, nu_vertices, false);
}

// Calculate vertex normals for a model that should not be smoothly shaded (generally,
// a blocky model instead of a curved model).
// It calculates the vertex normals by averaging the triangle normals of triangles
// that include the vertex by index.

void sreBaseModel::CalculateNormalsNotSmooth(int start_index, int nu_vertices_in_segment) {
    CalculateTriangleNormals();
    // Calculate vertex normals.
    // Set vertex normals to zero.
    for (int i = start_index; i < start_index + nu_vertices_in_segment; i++)
        vertex_normal[i].Set(0, 0, 0);
    // Determine every triangle that includes a specific vertex, and for each
    // of these triangles add the triangle normal to the vertex normal.
    for (int j = 0; j < nu_triangles; j++) {
        // It would help to know the triangle index range for the segment.
        if (triangle[j].vertex_index[0] >= start_index &&
        triangle[j].vertex_index[0] < start_index + nu_vertices_in_segment)
            vertex_normal[triangle[j].vertex_index[0]] += triangle[j].normal;
        if (triangle[j].vertex_index[1] >= start_index &&
        triangle[j].vertex_index[1] < start_index + nu_vertices_in_segment)
            vertex_normal[triangle[j].vertex_index[1]] += triangle[j].normal;
        if (triangle[j].vertex_index[2] >= start_index &&
        triangle[j].vertex_index[2] < start_index + nu_vertices_in_segment)
            vertex_normal[triangle[j].vertex_index[2]] += triangle[j].normal;
    }
    for (int i = start_index; i < start_index + nu_vertices_in_segment; i++)
        vertex_normal[i].Normalize();
    flags |= SRE_NORMAL_MASK;
}

void sreBaseModel::CalculateNormalsNotSmooth() {
    CalculateNormalsNotSmooth(0, nu_vertices);
}

void sreBaseModel::CalculateVertexTangentVectors() {
    Vector3D *tan1 = new Vector3D[nu_vertices];
    Vector3D *tan2 = new Vector3D[nu_vertices];
    for (int i = 0; i < nu_vertices; i++) {
        tan1[i].Set(0, 0, 0);
        tan2[i].Set(0, 0, 0);
    }
    for (int a = 0; a < nu_triangles; a++) {
        int i1 = triangle[a].vertex_index[0];
        int i2 = triangle[a].vertex_index[1];
        int i3 = triangle[a].vertex_index[2];

        const Vector3D& v1 = vertex[i1];
        const Vector3D& v2 = vertex[i2];
        const Vector3D& v3 = vertex[i3];

        float x1 = v2.x - v1.x;
        float x2 = v3.x - v1.x;
        float y1 = v2.y - v1.y;
        float y2 = v3.y - v1.y;
        float z1 = v2.z - v1.z;
        float z2 = v3.z - v1.z;

        const Point2D& w1 = texcoords[i1];
        const Point2D& w2 = texcoords[i2];
        const Point2D& w3 = texcoords[i3];

        float s1 = w2.x - w1.x;
        float s2 = w3.x - w1.x;
        float t1 = w2.y - w1.y;
        float t2 = w3.y - w1.y;

        float r = (float)1.0f / (s1 * t2 - s2 * t1);
        Vector3D sdir;
        sdir.Set((t2 * x1 - t1 * x2) * r, (t2 * y1 - t1 * y2) * r,
                (t2 * z1 - t1 * z2) * r);
        Vector3D tdir;
        tdir.Set((s1 * x2 - s2 * x1) * r, (s1 * y2 - s2 * y1) * r,
                (s1 * z2 - s2 * z1) * r);

        tan1[i1] += sdir;
        tan1[i2] += sdir;
        tan1[i3] += sdir;

        tan2[i1] += tdir;
        tan2[i2] += tdir;
        tan2[i3] += tdir;
    }

    for (int a = 0; a < nu_vertices; a++) {
        const Vector3D& n = vertex_normal[a];
        const Vector3D& t = tan1[a];
        
        // Gram-Schmidt orthogonalize
        vertex_tangent[a] = (t - n * Dot(n, t)).Normalize();
        
        // Calculate handedness
        vertex_tangent[a].w = (Dot(Cross(n, t), tan2[a]) < 0.0F) ? -1.0F : 1.0F;
    }

    delete [] tan1;
    delete [] tan2;
}


void sreBaseModel::CalculateTangentVectors() {
    vertex_tangent = new Vector4D[nu_vertices];
    CalculateVertexTangentVectors();
    flags |= SRE_TANGENT_MASK;
}


// Remove empty triangles (triangles with two or three vertices with exactly the same
// position, so that the triangle is either a point or a line (zero area)).
// Note that the actual vertex positions are checked; if a triangle contains multiple
// vertices with the same position but different indices, it will also be removed.

void sreBaseModel::RemoveEmptyTriangles() {
    // First count the number of empty triangles.
    int count = 0;
    for (int i = 0; i < nu_triangles; i++) {
        int vindex0 = triangle[i].vertex_index[0];
        int vindex1 = triangle[i].vertex_index[1];
        int vindex2 = triangle[i].vertex_index[2];
        if (vindex0 == - 1 || (vertex[vindex0] == vertex[vindex1] || vertex[vindex0] == vertex[vindex2]
        || vertex[vindex1] == vertex[vindex2]))
            count++;
    }
    if (count == 0)
        return;
    // Remove the empty triangles.
    sreModelTriangle *new_triangle = new sreModelTriangle[nu_triangles - count];
    int j = 0;
    for (int i = 0; i < nu_triangles; i++) {
        int vindex0 = triangle[i].vertex_index[0];
        int vindex1 = triangle[i].vertex_index[1];
        int vindex2 = triangle[i].vertex_index[2];
        if (vindex0 == - 1 || (vertex[vindex0] == vertex[vindex1] || vertex[vindex0] == vertex[vindex2]
        || vertex[vindex1] == vertex[vindex2]))
            continue;
        new_triangle[j] = triangle[i];
        j++;
    }
    delete [] triangle;
    nu_triangles = j;
    triangle = new_triangle;
    if (sre_internal_debug_message_level >= 2)
        printf("Removed %d empty triangles from a total of %d\n", count, nu_triangles);
}

// From:
// Lengyel, Eric. “Building an Edge List for an Arbitrary Mesh”. Terathon Software 3D Graphics
// Library, 2005. http://www.terathon.com/code/edges.html

static int BuildEdges(int vertexCount, int triangleCount,
                const sreModelTriangle *triangleArray, ModelEdge *edgeArray)
{
    int maxEdgeCount = triangleCount * 3;
    unsigned int *firstEdge = new unsigned int[vertexCount + maxEdgeCount];
    unsigned int *nextEdge = firstEdge + vertexCount;
    
    for (int a = 0; a < vertexCount; a++) firstEdge[a] = 0xFFFFFFFF;
    
    // First pass over all triangles. This finds all the edges satisfying the
    // condition that the first vertex index is less than the second vertex index
    // when the direction from the first vertex to the second vertex represents
    // a counterclockwise winding around the triangle to which the edge belongs.
    // For each edge found, the edge index is stored in a linked list of edges
    // belonging to the lower-numbered vertex index i. This allows us to quickly
    // find an edge in the second pass whose higher-numbered vertex index is i.
    
    int edgeCount = 0;
    const sreModelTriangle *triangle = triangleArray;
    for (int a = 0; a < triangleCount; a++)
    {
        int i1 = triangle->vertex_index[2];
        for (int b = 0; b < 3; b++)
        {
            int i2 = triangle->vertex_index[b];
            if (i1 < i2)
            {
                ModelEdge *edge = &edgeArray[edgeCount];
                
                edge->vertex_index[0] = i1;
                edge->vertex_index[1] = i2;
                edge->triangle_index[0] = a;
                edge->triangle_index[1] = - 1;
                
                unsigned int edgeIndex = firstEdge[i1];
                if (edgeIndex == 0xFFFFFFFF)
                {
                    firstEdge[i1] = edgeCount;
                }
                else
                {
                    for (;;)
                    {
                        unsigned int index = nextEdge[edgeIndex];
                        if (index == 0xFFFFFFFF)
                        {
                            nextEdge[edgeIndex] = edgeCount;
                            break;
                        }
                        
                        edgeIndex = index;
                    }
                }
                
                nextEdge[edgeCount] = 0xFFFFFFFF;
                edgeCount++;
            }
            
            i1 = i2;
        }
        
        triangle++;
    }
    
    // Second pass over all triangles. This finds all the edges satisfying the
    // condition that the first vertex index is greater than the second vertex index
    // when the direction from the first vertex to the second vertex represents
    // a counterclockwise winding around the triangle to which the edge belongs.
    // For each of these edges, the same edge should have already been found in
    // the first pass for a different triangle. So we search the list of edges
    // for the higher-numbered vertex index for the matching edge and fill in the
    // second triangle index. The maximum number of comparisons in this search for
    // any vertex is the number of edges having that vertex as an endpoint.
    
    triangle = triangleArray;
    for (int a = 0; a < triangleCount; a++)
    {
        int i1 = triangle->vertex_index[2];
        for (int b = 0; b < 3; b++)
        {
            int i2 = triangle->vertex_index[b];
            if (i1 > i2)
            {
                bool success = false;
                for (unsigned int edgeIndex = firstEdge[i2]; edgeIndex != 0xFFFFFFFF;
                        edgeIndex = nextEdge[edgeIndex])
                {
                    ModelEdge *edge = &edgeArray[edgeIndex];
                    if ((edge->vertex_index[1] == i1) &&
                            (edge->triangle_index[1] == - 1))
                    {
                        edge->triangle_index[1] = a;
                        success = true;
                        break;
                    }
                }
                if (!success) {
                    // The edge has only one triangle, and the winding is clockwise.
                    edgeArray[edgeCount].vertex_index[0] = i2;
                    edgeArray[edgeCount].vertex_index[1] = i1;
                    edgeArray[edgeCount].triangle_index[0] = a;
                    edgeArray[edgeCount].triangle_index[1] = - 1;
                    edgeCount++;
                }
            }
            
            i1 = i2;
        }
        
        triangle++;
    }
    
    delete[] firstEdge;
    return (edgeCount);
}

// The epsilon for comparing normal vectors using a dot product should be quite
// small because small angles have a dot product extremely close to 1.0.

#define EPSILON_DOT_PRODUCT_ONE 0.0000001f

void sreLODModelShadowVolume::RemoveUnnecessaryEdges() {
    int new_nu_edges = 0;
    // Pass one: Determine the number of edges that will be left.
    for (int i = 0; i < nu_edges; i++) {
        // Skip edges for which the two adjacent triangles are facing in virtually
        // the same direction.
        if (AlmostEqual(Dot(triangle[edge[i].triangle_index[0]].normal,
        triangle[edge[i].triangle_index[1]].normal), 1.0f, EPSILON_DOT_PRODUCT_ONE))
            // The normals of the triangles are almost identical, so skip
            // the edge.
            continue;
        new_nu_edges++;
    }
    // Pass two: Allocate and assign the new edge array.
    ModelEdge *new_edge = new ModelEdge[new_nu_edges];
    int j = 0;
    for (int i = 0; i < nu_edges; i++) {
        if (AlmostEqual(Dot(triangle[edge[i].triangle_index[0]].normal,
        triangle[edge[i].triangle_index[1]].normal), 1.0f, EPSILON_DOT_PRODUCT_ONE))
            // The normals of the triangles are almost identical, so skip
            // the edge.
            continue;
        new_edge[j] = edge[i];
        j++;
    }
    delete [] edge;
    edge = new_edge;
    nu_edges = new_nu_edges;
}

// #define REDUCE_TRIANGLES_WHEN_CALCULATING_EDGES

void sreLODModelShadowVolume::CalculateEdges() {
    sreBaseModel *clone = new sreBaseModel;
    // Create a clone with just vertex position and triangle information.
    CloneGeometry(clone);

    if (sre_internal_debug_message_level >= 2)
        // Prepend output for MergeIdenticalVertices().
        printf("(geometry only) ");
    // Optimize the geometry-only model by merging all similar vertices, and keep
    // track of the vertex index mapping to the original model vertices.
    int *saved_indices = new int[nu_vertices];
    clone->MergeIdenticalVertices(saved_indices);

    // We want to reduce the complexity of the geometry when doing so does not
    // change the shape. However, we must make sure that are two opposing triangles
    // for each edge.
#ifdef REDUCE_TRIANGLES_WHEN_CALCULATING_EDGES
    // Copy vertex normals for triangle reduction function.
    if (flags & SRE_NORMAL_MASK)
        clone->vertex_normal = new Vector3D[nu_vertices];
    for (int i = 0; i < nu_vertices; i++)
        clone->vertex_normal[i] = vertex_normal[i];
    clone->flags |= SRE_NORMAL_MASK;
    // Remove edges (reduce triangles) when the geometric shape doesn't change at
    // all by removing them.
    int *saved_indices2 = new int[clone->nu_vertices];
    clone->ReduceTriangleCount(
        0,         // Max. surface roughness (no limit).
        0.0001f,   // Cost threshold (only allow changes that virtually preserve the shape).
        false,     // Check vertex normals.
        1.0f,       // Vertex normal threshold (unused).
        saved_indices2 // Vertex mapping from new index to old index.
        );
    // Delete the vertex normals.
    delete [] clone->vertex_normal;
    clone->flags &= ~SRE_NORMAL_MASK;
#endif

    // Calculate edges and store them in the full model.
    edge = new ModelEdge[clone->nu_triangles * 3];
    nu_edges = BuildEdges(clone->nu_vertices, clone->nu_triangles, clone->triangle, edge);
    flags |= SRE_LOD_MODEL_HAS_EDGE_INFORMATION;
    // Remap the edge vertex indices from the reduced geometry edge calculation first
    // to the triangle-reduced version and then to the full model before MergeIdenticalVertices
    // was called.
    for (int i = 0; i < nu_edges; i++) {
#ifdef REDUCE_TRIANGLES_WHEN_CALCULATING_EDGES
        edge[i].vertex_index[0] = saved_indices[saved_indices2[edge[i].vertex_index[0]]];
        edge[i].vertex_index[1] = saved_indices[saved_indices2[edge[i].vertex_index[1]]];
#else
        edge[i].vertex_index[0] = saved_indices[edge[i].vertex_index[0]];
        edge[i].vertex_index[1] = saved_indices[edge[i].vertex_index[1]];
#endif
    }
    // Clean up.
    delete [] saved_indices;
#ifdef REDUCE_TRIANGLES_WHEN_CALCULATING_EDGES
    delete [] saved_indices2;
#endif
    // The deconstructor should automatically free vertex positions and triangles.
    delete clone;
    if (sre_internal_debug_message_level >= 2)
        printf("CalculateEdges (geometry only): found %d edges.\n", nu_edges);
}

void sreLODModelShadowVolume::DestroyEdges() {
    if (nu_edges > 0)
        delete [] edge;
    nu_edges = 0;
    flags &= ~SRE_LOD_MODEL_HAS_EDGE_INFORMATION;
}

class TriangleList {
public :
    int triangle_index;
    int next;
};

static float *endpoint_cost[2];

static int CompareEdges(const void *e1, const void *e2) {
    int i1 = *(int *)e1;
    int i2 = *(int *)e2;
    float min_cost_edge1 = minf(endpoint_cost[0][i1], endpoint_cost[1][i1]);
    float min_cost_edge2 = minf(endpoint_cost[0][i2], endpoint_cost[1][i2]);
    if (min_cost_edge1 < min_cost_edge2)
        return - 1;
    if (min_cost_edge1 > min_cost_edge2)
        return 1;
    return 0;
}

//#define EDGE_COLLAPSE_DEBUG

// EPSILON_DEFAULT is defined in sreVectorMath.h as 0.0001f.
#define EPSILON EPSILON_DEFAULT
// This epsilon value applies to avoidance of generation a triangles that are too thin or invalid.
#define EPSILON3 0.005

// When saved_indices is not NULL the vertex mapping from new index to old index is stored there.

void sreBaseModel::ReduceTriangleCount(float max_surface_roughness, float cost_threshold,
bool check_vertex_normals, float vertex_normal_threshold, int *saved_indices) {
    ModelEdge *edge = new ModelEdge[nu_triangles * 3];
    bool *is_boundary_vertex = new bool[nu_vertices];
    TriangleList *triangle_list = new TriangleList[nu_triangles * 6];
    int *triangle_list_head = new int[nu_vertices];
    int *triangle_list_tail = new int[nu_vertices];
    endpoint_cost[0] = new float[nu_triangles * 3];
    endpoint_cost[1] = new float[nu_triangles * 3];
    bool *triangle_changed = new bool[nu_triangles];
    int *edge_order = new int[nu_triangles * 3];
    int original_nu_triangles = nu_triangles;
    int pass = 0;
again :
    int nu_edges = BuildEdges(nu_vertices, nu_triangles, triangle, edge);
//    printf("ReduceTriangleCount: found %d edges.\n", nu_edges);
    // Calculate whether every edge leading away from each vertex is shared by two triangles.
    for (int i = 0; i < nu_vertices; i++)
        is_boundary_vertex[i] = false;
    int boundary_edge_count = 0;
    for (int i = 0; i < nu_edges; i++)
        if (edge[i].triangle_index[1] == - 1) {
            is_boundary_vertex[edge[i].vertex_index[0]] = true;
            is_boundary_vertex[edge[i].vertex_index[1]] = true;
            boundary_edge_count++;
        }
#if 0
    for (int i = 0; i < nu_triangles; i++) {
        bool boundary_triangle = false;
        for (int j = 0; j < 3; j++)
            if (is_boundary_vertex[triangle[i].vertex_index[j]])
                boundary_triangle = true;
        if (boundary_triangle) {
            for (int j = 0; j < 3; j++)
                texcoords[triangle[i].vertex_index[j]] = Point2D(0, 1.0);
            if (triangle[i].normal == Vector3D(1, 0, 0))
                printf("Triangle %d with normal (1, 0, 0) has a boundary vertex.\n", i);
        }
        else
            if (triangle[i].normal == Vector3D(1, 0, 0))
                printf("Triangle %d with normal (1, 0, 0) does not have a boundary vertex.\n", i);
    }
#endif
#if 0
//    int boundary_vertex_count = 0;
//    for (int i = 0; i < nu_vertices; i++)
//        if (is_boundary_vertex[i]) {
//            boundary_vertex_count++;
//            vertex[i].z += 10.0;
//        }
//    printf("%d boundary edges, %d boundary vertices.\n", boundary_edge_count, boundary_vertex_count);
    delete [] is_boundary_vertex;
    DestroyEdges();
    return;
#endif
    // Calculate a list of triangles that each vertex is part of.
    for (int i = 0; i < nu_vertices; i++)
        triangle_list_head[i] = - 1;
    int count = 0;
    for (int i = 0; i < nu_triangles; i++)
        for (int j = 0; j < 3; j++) {
            if (count >= original_nu_triangles * 6) {
                printf("ReduceTriangleCount: triangle list array overflow.\n");
                exit(1);
            }
            // Add the triangle to the list for the vertex.
            int vertex_index = triangle[i].vertex_index[j];
            if (triangle_list_head[vertex_index] == - 1) {
                triangle_list_head[vertex_index] = count;
            }
            else {
                triangle_list[triangle_list_tail[vertex_index]].next = count;
            }
            triangle_list_tail[vertex_index] = count;
            triangle_list[count].triangle_index = i;
            triangle_list[count].next = - 1;
            count++;
        }
//    printf("ReduceTriangleCount: %d entries in triangle list.\n", count);
    // Calculate edge endpoint collapse costs.
    int potential_reductions = 0;
    for (int i = 0; i < nu_edges; i++) {
        for (int j = 0; j < 2; j++) {
            int V1_vertex_index = edge[i].vertex_index[j];
            // Check whether any of the edges leading away from V1 are not shared by two triangles. In that case,
            // V1 should not be eliminated.
            if (is_boundary_vertex[V1_vertex_index]) {
                endpoint_cost[j][i] = POSITIVE_INFINITY_FLOAT;
                continue;
            }
            int V2_vertex_index = edge[i].vertex_index[j ^ 1];
            if (check_vertex_normals) {
                // If vertex normal checking is enabled, don't allow the edge collapse if there is significant difference
                // in vertex normals between V1 and V2.
                if (Dot(vertex_normal[V1_vertex_index], vertex_normal[V2_vertex_index]) < vertex_normal_threshold) {
                    // The edge should not be collapsed.
                    endpoint_cost[j][i] = POSITIVE_INFINITY_FLOAT;
                    endpoint_cost[j ^ 1][i] = POSITIVE_INFINITY_FLOAT;
                    break;
                }
            }
            Vector3D N = vertex_normal[V1_vertex_index];
            Vector3D E = vertex[V2_vertex_index] - vertex[V1_vertex_index];
            Vector3D D = Cross(N, E);
            D.Normalize();
            // For each of the two triangles sharing the edge that connects V1 and V2, examine the vertex V3 that
            // does not lie on the edge.
            float side[2];
            for (int k = 0; k < 2; k++) {
                if (edge[i].triangle_index[k] == - 1) {
                    printf("ReduceTriangleCount: Edge with unexpected less than two triangles.\n");
                    exit(1);
                }
                int V3_vertex_index;
                for (int l = 0; l < 3; l++)
                    if (triangle[edge[i].triangle_index[k]].vertex_index[l] != V1_vertex_index &&
                    triangle[edge[i].triangle_index[k]].vertex_index[l] != V2_vertex_index) {
                        V3_vertex_index = triangle[edge[i].triangle_index[k]].vertex_index[l];
                        break;
                    }
                side[k] = Dot(D, vertex[V3_vertex_index] - vertex[V1_vertex_index]);
            }
            Vector3D Tpos, Tneg;
            if (side[0] >= 0 && side[1] < 0) {
                Tpos = triangle[edge[i].triangle_index[0]].normal;
                Tneg = triangle[edge[i].triangle_index[1]].normal;
            }
            else
            if (side[1] >= 0 && side[0] < 0) {
                Tpos = triangle[edge[i].triangle_index[1]].normal;
                Tneg = triangle[edge[i].triangle_index[0]].normal;
            }
            else {
                // The edge should not be collapsed.
                endpoint_cost[j][i] = POSITIVE_INFINITY_FLOAT;
                endpoint_cost[j ^ 1][i] = POSITIVE_INFINITY_FLOAT;
                break;
            }
            // Check the other triangles using the vertex V1.
            float d = POSITIVE_INFINITY_FLOAT;
            int triangle_list_index = triangle_list_head[V1_vertex_index];
            if (triangle_list_index == - 1) {
                printf("ReduceTriangleCount: Vertex with unexpected empty triangle list.\n");
                exit(1);
            }
            bool invalid_triangle = false;
            for (; triangle_list_index != - 1; triangle_list_index = triangle_list[triangle_list_index].next) {
                int triangle_index = triangle_list[triangle_list_index].triangle_index;
//                printf("Triangle list index = %d, triangle list next = %d, triangle index = %d.\n",
//                    triangle_list_index, triangle_list[triangle_list_index].next, triangle_index);
                int A_vertex_index, B_vertex_index;
                if (triangle[triangle_index].vertex_index[0] != V1_vertex_index) {
                    A_vertex_index = triangle[triangle_index].vertex_index[0];
                    if (triangle[triangle_index].vertex_index[1] != V1_vertex_index)
                        B_vertex_index = triangle[triangle_index].vertex_index[1];
                    else
                        B_vertex_index = triangle[triangle_index].vertex_index[2];
                }
                else {
                    A_vertex_index = triangle[triangle_index].vertex_index[1];
                    B_vertex_index = triangle[triangle_index].vertex_index[2];
                }
                // Neither A nor B should be equal to V2.
                if (A_vertex_index == V2_vertex_index || B_vertex_index == V2_vertex_index)
                    continue;
                if (check_vertex_normals) {
                    // If vertex normal checking is enabled, don't allow the edge collapse if there is significant difference
                    // in vertex normals between A or B and V1.
                    if (Dot(vertex_normal[V1_vertex_index], vertex_normal[A_vertex_index]) < vertex_normal_threshold ||
                    Dot(vertex_normal[V1_vertex_index], vertex_normal[B_vertex_index]) < vertex_normal_threshold) {
                        invalid_triangle = true;
                        break;
                    }
                }
                Vector3D V1_A = vertex[A_vertex_index] - vertex[V1_vertex_index];
                // Check whether moving V1 to V2 in the triangle (V1, A, B) results in the edge (V2, A) moving to the
                // other side of the edge (V2, B) as compared to original orientation of the edges (V1, A) and (V2, B).
                // This would result in an invalid triangle.
                // Calculate the normalized vector perpendicular to both the triangle normal and the edge (A, B).
                Vector3D F = Cross(triangle[triangle_index].normal, vertex[B_vertex_index] - vertex[A_vertex_index]);
                F.Normalize();
                // Calculate the distance of V1 to the plane through the edge (A, B)
                float dist_V1 = Dot(F, V1_A);
                float dist_V2 = Dot(F, vertex[A_vertex_index] - vertex[V2_vertex_index]);
                // Require that dist_V1 > EPSILON and dist_V2 > EPSILON, or dist_V1 < - EPSILON and dist_V2 < - EPSILON,
                // meaning that V2 does not cross the edge (A, B) compared to be the position of V1, and that we have
                // a triangle that is not too thin.
                if ((dist_V1 <= EPSILON3 && dist_V2 >= - EPSILON3) || (dist_V1 >= - EPSILON3 && dist_V2 <= EPSILON3)) {
                    invalid_triangle = true;
                    break;
                }
                Vector3D V1_B = vertex[B_vertex_index] - vertex[V1_vertex_index];
                float a = Dot(D, V1_A);
                float b = Dot(D, V1_B);
                if (a > EPSILON || b > EPSILON)
                    d = minf(d, Dot(triangle[triangle_index].normal, Tpos));
                if (a < - EPSILON || b < - EPSILON)
                    d = minf(d, Dot(triangle[triangle_index].normal, Tneg));
            }
            if (invalid_triangle || d < max_surface_roughness || d == POSITIVE_INFINITY_FLOAT) {
                endpoint_cost[j][i] = POSITIVE_INFINITY_FLOAT;
                continue;
            }
            endpoint_cost[j][i] = (1.0 - d) * Magnitude(E);
            if (endpoint_cost[j][i] <= cost_threshold)
                potential_reductions++;
        }
    }
//    printf("ReduceTriangleCount: %d edge endpoints considered for collapse.\n", potential_reductions);
    for (int i = 0; i < nu_edges; i++)
        edge_order[i] = i;
    qsort(edge_order, nu_edges, sizeof(int), CompareEdges);
    for (int i = 0; i < nu_triangles; i++)
        triangle_changed[i] = false;
    int eliminated_count = 0;
    int edge_index = 0;
    for (; edge_index < nu_edges; edge_index++) {
        float min_cost = minf(endpoint_cost[0][edge_order[edge_index]], endpoint_cost[1][edge_order[edge_index]]);
        if (min_cost > cost_threshold)
            break;
        int V1_vertex_index, V2_vertex_index;
        if (endpoint_cost[0][edge_order[edge_index]] < endpoint_cost[1][edge_order[edge_index]]) {
            V1_vertex_index = edge[edge_order[edge_index]].vertex_index[0];
            V2_vertex_index = edge[edge_order[edge_index]].vertex_index[1];
        }
        else {
            V1_vertex_index = edge[edge_order[edge_index]].vertex_index[1];
            V2_vertex_index = edge[edge_order[edge_index]].vertex_index[0];
        }
        if (is_boundary_vertex[V1_vertex_index]) {
            printf("Shouldn't happen.\n");
            exit(1);
        }
        // Check that any of the two endpoints has not already been eliminated or had its triangle list changed.
        if (triangle_list_head[V1_vertex_index] == - 1 || triangle_list_head[V2_vertex_index] == - 1)
            continue;
        // Check that any triangle containing endpoint V1 has not been changed already; if so, the costs are not valid
        // anymore and have to be recalculated, handle it in a later pass. Also skip the edge if any triangle does not
        // exist anymore.
        int triangle_list_index = triangle_list_head[V1_vertex_index];
        bool invalid_cost = false;
        for (; triangle_list_index != - 1; triangle_list_index = triangle_list[triangle_list_index].next) {
            int triangle_index = triangle_list[triangle_list_index].triangle_index;
            if (triangle_changed[triangle_index] || triangle[triangle_index].vertex_index[0] == - 1) {
                invalid_cost = true;
                break;
            }
        }
        if (invalid_cost)
            continue;
#if 0
        // Check V2 too.
        triangle_list_index = triangle_list_head[V2_vertex_index];
        invalid_cost = false;
        for (; triangle_list_index != - 1; triangle_list_index = triangle_list[triangle_list_index].next) {
            int triangle_index = triangle_list[triangle_list_index].triangle_index;
            if (triangle_changed[triangle_index] || triangle[triangle_index].vertex_index[0] == - 1) {
                invalid_cost = true;
                break;
            }
        }
        if (invalid_cost)
            continue;
#endif
        // Mark the triangles that share the edge as invalid, and move the vertex to the other endpoint in other triangles
        // that include V1.
        triangle_list_index = triangle_list_head[V1_vertex_index];
        int triangles_eliminated_count = 0;
        int edges_stretched_count = 0;
        for (; triangle_list_index != - 1; triangle_list_index = triangle_list[triangle_list_index].next) {
            int triangle_index = triangle_list[triangle_list_index].triangle_index;
            int A_vertex_index, B_vertex_index;
            if (triangle[triangle_index].vertex_index[0] != V1_vertex_index) {
                A_vertex_index = triangle[triangle_index].vertex_index[0];
                if (triangle[triangle_index].vertex_index[1] != V1_vertex_index) {
                    B_vertex_index = triangle[triangle_index].vertex_index[1];
                    if (triangle[triangle_index].vertex_index[2] != V1_vertex_index) {
                        printf("ReduceTriangleCount: triangle does not contain V1.\n");
                        exit(1);
                    }
                }
                else
                    B_vertex_index = triangle[triangle_index].vertex_index[2];
            }
            else {
                A_vertex_index = triangle[triangle_index].vertex_index[1];
                B_vertex_index = triangle[triangle_index].vertex_index[2];
            }
#ifdef EDGE_COLLAPSE_DEBUG
            // Visualize the triangles involved in the edge collapse.
            for (int j = 0; j < 3; j++)
                if (is_boundary_vertex[triangle[triangle_index].vertex_index[j]])
                    texcoords[triangle[triangle_index].vertex_index[j]] = Point2D(0, 1.0);
#else
            if (triangle_index < 0 || triangle_index >= nu_triangles) {
                printf("Triangle index out of bounds.\n");
                exit(1);
            }
#if 0
            bool has_boundary_vertex = false;
            for (int j = 0; j < 3; j++)
                if (is_boundary_vertex[triangle[triangle_index].vertex_index[j]]) {
                    has_boundary_vertex = true;
                    break;
                }
            if (has_boundary_vertex)
                continue;
            if (triangle[triangle_index].normal == Vector3D(1, 0, 0))
                printf("Triangle %d with normal (1, 0, 0) does not have a boundary vertex.\n", triangle_index);
#endif
            if (A_vertex_index == V2_vertex_index || B_vertex_index == V2_vertex_index) {
                // The triangle includes the edge; it should be eliminated.
                triangle[triangle_index].vertex_index[0] = - 1;
                eliminated_count++;
                triangle_changed[triangle_index] = true;
                triangles_eliminated_count++;
//                printf("Triangle %d with normal (%f, %f, %f) eliminated.\n", triangle_index,
//                    triangle[triangle_index].normal.x, triangle[triangle_index].normal.y, triangle[triangle_index].normal.z);
            }
            else {
                // The triangle includes the endpoint V1 only; replace it with V2.
                int j = 0;
                for (j = 0; j < 3; j++)
                    if (triangle[triangle_index].vertex_index[j] == V1_vertex_index) {
                        triangle[triangle_index].vertex_index[j] = V2_vertex_index;
                        triangle_changed[triangle_index] = true;
                        break;
                    }
                edges_stretched_count++; 
            }
            // Mark the triangle list of vertices A and B as invalid.
            triangle_list_head[A_vertex_index] = - 1;
            triangle_list_head[B_vertex_index] = - 1;
#endif
        }
        // Mark the vertex as invalid.
        triangle_list_head[V1_vertex_index] = - 1;
        // Mark the triangles list of V2 as invalid.
        triangle_list_head[V2_vertex_index] = - 1;
//        printf("Edge between vertices %d and %d eliminated.\n", V1_vertex_index, V2_vertex_index);
//        printf("%d triangles eliminated and %d edges stretched in collapse operation.\n", triangles_eliminated_count,
//            edges_stretched_count);
    }
//    printf("ReduceTriangleCount: %d triangles eliminated (pass %d).\n", eliminated_count, pass);
    RemoveEmptyTriangles(); // Remove triangles marked as invalid.
    RemoveUnusedVertices(saved_indices);
    if (eliminated_count > 0) {
        pass++;
        if (pass < 5000)
            goto again;
    }
    delete [] edge_order;
    delete [] triangle_changed;
    delete [] endpoint_cost[0];
    delete [] endpoint_cost[1];
    delete [] triangle_list_head;
    delete [] triangle_list_tail;
    delete [] triangle_list;
    delete [] is_boundary_vertex;
    delete [] edge;
    printf("ReduceTriangleCount: number of triangles reduced from %d to %d.\n", original_nu_triangles, nu_triangles);
}

void sreBaseModel::ReduceTriangleCount(float max_surface_roughness, float cost_threshold,
bool check_vertex_normals, float vertex_normal_threshold) {
    ReduceTriangleCount(max_surface_roughness, cost_threshold, check_vertex_normals,
        vertex_normal_threshold, NULL);
}

// Calculate the AABB of an object. Used during octree creation.

void sreObject::CalculateAABB() {
    // Calculate the extents of the static object in world space.
    if (flags & SRE_OBJECT_PARTICLE_SYSTEM) {
        // Static particle system.
        AABB.dim_min.Set(POSITIVE_INFINITY_FLOAT, POSITIVE_INFINITY_FLOAT,
            POSITIVE_INFINITY_FLOAT);
        AABB.dim_max.Set(NEGATIVE_INFINITY_FLOAT, NEGATIVE_INFINITY_FLOAT,
            NEGATIVE_INFINITY_FLOAT);
        float billboard_size = maxf(billboard_width, billboard_height);
        for (int i = 0; nu_particles; i++) {
            Point3D pos = position + particles[i];
            sreBoundingVolumeAABB particle_AABB;
            particle_AABB.dim_min = Vector3D(
                pos.x - 0.5f * billboard_size,
                pos.y - 0.5f * billboard_size,
                pos.z - 0.5f * billboard_size);
            particle_AABB.dim_max = Vector3D(
                pos.x + 0.5f * billboard_size,
                pos.y + 0.5f * billboard_size,
                pos.z + 0.5f * billboard_size);
            UpdateAABB(AABB, particle_AABB);
        }
        return;
    }
    if (flags & (SRE_OBJECT_LIGHT_HALO | SRE_OBJECT_BILLBOARD)) {
         float billboard_size = maxf(billboard_width, billboard_height);
         AABB.dim_min = sphere.center - 0.5f * Vector3D(billboard_size, billboard_size, billboard_size);
         AABB.dim_max = sphere.center + 0.5f * Vector3D(billboard_size, billboard_size, billboard_size);
         return;
    } 
    AABB.dim_min.Set(POSITIVE_INFINITY_FLOAT, POSITIVE_INFINITY_FLOAT,
        POSITIVE_INFINITY_FLOAT);
    AABB.dim_max.Set(NEGATIVE_INFINITY_FLOAT, NEGATIVE_INFINITY_FLOAT,
        NEGATIVE_INFINITY_FLOAT);
    sreLODModel *lm = model->lod_model[0];
    for (int i = 0; i < lm->nu_vertices; i++) {
        // Transform the model vertex into world space.
        Point3D P = (model_matrix * lm->vertex[i]).GetPoint3D();
        UpdateAABB(AABB, P);
    }
}

// Scene-level model handling.

void sreScene::RegisterModel(sreModel *m) {
    m->id = models.Size();
    models.Add(m);
    // Set the id of the LOD models.
    for (int i = 0; i < m->nu_lod_levels; i++) {
        sreLODModel *lm = m->lod_model[i];
        // Set the LOD model's id in such a way that it identifies the parent model
        // as well as which LOD level it is.
        lm->id = m->id * 10 + i;
        // This function provides an opportunity to sanitize some values.
        if (!(lm->flags & SRE_NORMAL_MASK) && (lm->instance_flags & SRE_NORMAL_MASK))
            lm->instance_flags &= ~SRE_NORMAL_MASK;
        if (!(lm->flags & SRE_TEXCOORDS_MASK) && (lm->instance_flags & SRE_TEXCOORDS_MASK))
            lm->instance_flags &= ~SRE_TEXCOORDS_MASK;
        if (!(lm->flags & SRE_TANGENT_MASK) && (lm->instance_flags & SRE_TANGENT_MASK))
            lm->instance_flags &= ~SRE_TANGENT_MASK;
        if (!(lm->flags & SRE_COLOR_MASK) && (lm->instance_flags & SRE_COLOR_MASK))
            lm->instance_flags &= ~SRE_COLOR_MASK;
    }
    if (sre_internal_debug_message_level >= 2)
        printf("Registering model %d, bounds_flags = 0x%04X, %d LOD models\n", m->id,
            m->bounds_flags, m->nu_lod_levels);
}

// Gather statistical information about the scene and do a consistency check, marking
// models that are unreferenced (never actually used). Normally models should already
// be marked as unreferenced before this function is called, but with preprocessing
// enabled there might be additional unreferenced models.

void sreScene::RemoveUnreferencedModels() {
    char *model_used = new char[models.Size()];
    memset(model_used, 0, models.Size());
    // First pass: iterate all LOD models and set their ID to - 2.
    // Also check that the index and ID of full models are the same.
    for (int i = 0; i < models.Size(); i++) {
        if (models.Get(i) == NULL)
            continue;
        if (models.Get(i)->id != i) {
            printf("Warning: model index %d does not match model id of %d.\n",
                i, models.Get(i)->id);
        }
        for (int j = 0; j < models.Get(i)->nu_lod_levels; j++)
            models.Get(i)->lod_model[j]->id = - 2;
    }
    // Second pass: iterate all objects and mark every model and LOD model used;
    // set the ID of every LOD model encountered (some of which may be shared
    // between different models) to - 1.
    int scene_triangle_count = 0;
    for (int i = 0; i < nu_objects; i++) {
        sreObject *so = object[i];
        sreModel *m = so->model;
        if (m == NULL)
            continue;
        model_used[m->id] = 1;
        // XXX Should actually take lod_level and lod_flags from scene object into account
        // rather than assuming all LOD levels will be used.
        for (int j = 0; j < m->nu_lod_levels; j++) {
            m->lod_model[j]->id = - 1;
        }
        // Use the first LOD level (worst case) triangle count for statistics.
        scene_triangle_count += m->lod_model[0]->nu_triangles;
    }
    // Third pass: iterate all models and LOD models, check consistency of and fix model
    // references, assign unique IDs to LOD models, and check consistency/fix LOD model
    // references.
    int gpu_triangle_count = 0;
    int lod_model_count = 0;
    for (int i = 0; i < models.Size(); i++) {
        if (models.Get(i) == NULL)
            continue;
        if (models.Get(i)->referenced && model_used[i] == 0) {
            printf("Model id %d is marked is referenced but not actually used -- marking as unreferenced.\n", i);
            models.Get(i)->referenced = false;
        }
        else {
            if (!models.Get(i)->referenced && model_used[i] != 0) {
                printf("Warning: Model id %d is not marked as referenced but is actually used -- "
                    "marking as referenced.\n", i);
                models.Get(i)->referenced = true;
            }
        }
        for (int j = 0; j < models.Get(i)->nu_lod_levels; j++) {
            sreLODModel *lm = models.Get(i)->lod_model[j];
            // When we encounter a LOD model that is actually used for the first time,
            // assign a unique ID.
            if (lm->id == - 1) {
                lm->id = lod_model_count;
                if (!lm->referenced) {
                     printf("Warning: LOD model id %d is not marked as referenced but is actually used "
                         "-- marking as referenced.\n", lm->id);
                     lm->referenced = true;
                }
                // Add to the total GPU triangle count.
                gpu_triangle_count += lm->nu_triangles;
                lod_model_count++;
            }
            else if (lm->id == - 2 && lm->referenced) {
                // The LOD model is not actually used, but marked as referenced.
                printf("A LOD model for model %d is marked is referenced but not actually used "
                    "-- marking as unreferenced.\n", i);
                lm->referenced = false;
            }
        }
    }
    delete [] model_used;
    // Set number of LOD models actually used in scene structure.
    nu_lod_models = lod_model_count;
    // As a result, all LOD models that are actually used should now have an ID >= 0, and
    // unused LOD models will have an ID of - 2. The referenced flag also reflects this.
    printf("Scene statistics: %d objects, worst case %d triangles, %d models, "
        "%d LOD models actually used, %d triangles uploaded to GPU.\n",
         nu_objects, scene_triangle_count, models.Size(),
         nu_lod_models, gpu_triangle_count);
}

void sreScene::MarkAllModelsReferenced() const {
    for (int i = 0; i < models.Size(); i++) {
        if (models.Get(i) == NULL)
            continue;
        models.Get(i)->referenced = true;
        for (int j = 0; j < models.Get(i)->nu_lod_levels; j++)
            models.Get(i)->lod_model[j]->referenced = true;
    }
}

// RemoveUnreferencedModels() or MarkAllModelsReferences() must be called before
// uploading models.

void sreScene::UploadModels() const {
    // Iterate all models.
    for (int i = 0; i < models.Size(); i++) {
        if (models.Get(i) == NULL)
            continue;
        bool shadow_volumes_configured = true;
        for (int j = 0; j < models.Get(i)->nu_lod_levels; j++) {
            sreLODModel *m = models.Get(i)->lod_model[j];
            if (m->referenced) {
                int dynamic_flags = 0;
                if (m->flags & SRE_LOD_MODEL_VERTEX_BUFFER_DYNAMIC) {
                    // For the deprecated SRE_LOD_MODEL_VERTEX_BUFFER_DYNAMIC flag,
                    // set all attributes related to position to dynamic.
                    dynamic_flags = SRE_POSITION_MASK;
                    if (m->flags & SRE_NORMAL_MASK)
                        dynamic_flags |= SRE_NORMAL_MASK;
                    if (m->flags & SRE_TANGENT_MASK)
                        dynamic_flags |= SRE_TANGENT_MASK;
                }
                // Upload every used LOD model that we have not already uploaded.
                if (!(m->flags & SRE_LOD_MODEL_UPLOADED)) {
                    m->UploadToGPU(m->instance_flags, dynamic_flags);
                    m->flags |= SRE_LOD_MODEL_UPLOADED;
                }
                // Check whether all the LOD models for the model support
                // shadow volumes. (There might be a need for a more precise
                // flag that also guarantees the presence of extruded vertices).
                if ((m->flags & SRE_LOD_MODEL_NO_SHADOW_VOLUME_SUPPORT)
                || !(m->flags & SRE_LOD_MODEL_HAS_EDGE_INFORMATION))
                    shadow_volumes_configured = false;
            }
        }
        // Mark the model as supporting shadow volumes if all LOD models
        // support shadow volumes.
        if (shadow_volumes_configured)
            models.Get(i)->model_flags |= SRE_MODEL_SHADOW_VOLUMES_CONFIGURED;
    }
}

void sreScene::ClearModels() {
    for (int i = 0; i < models.Size(); i++) {
        if (models.Get(i) == NULL)
            continue;
        // Delete sreModel.
        // The deconstructor will trigger deletion of the LOD levels too,
        // and corresponding bufferson the GPU are also deleted.
        delete models.Get(i);
    }
    models.MakeEmpty();
}

