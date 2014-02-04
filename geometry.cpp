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

// Higher level model class constuctor.

sreModel::sreModel() {
    // Reset the number of LOD levels.
    nu_lod_levels = 0;
    lod_model[0] = NULL;
    nu_polygons = 0;
    is_static = false;
    referenced = false;
    flags = 0;
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
    }
    return m;
}

// Polygon data types; only used with higher level sreModel class
// for preprocessing purposes.

ModelPolygon::ModelPolygon() {
    nu_vertices = 0;
}

void ModelPolygon::InitializeWithSize(int n) {
    nu_vertices = n;
    vertex_index = new int[n];
}

void ModelPolygon::AddVertex(int j) {
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
    if (sre_internal_shadow_volumes_disabled)
        return new sreLODModel;
    else
        return new sreLODModelShadowVolume;
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

// Base model geometry component constructors.

sreBaseModel::sreBaseModel(int nu_vertices, int nu_triangles, int flags) {
    sreBaseModel();
    vertex = new Point3D[nu_vertices];
    triangle = new ModelTriangle[nu_triangles];
    if (flags & SRE_NORMAL_MASK)
        vertex_normal = new Vector3D[nu_vertices];
    if (flags & SRE_TEXCOORDS_MASK)
        texcoords = new Point2D[nu_vertices];
    if (flags & SRE_TANGENT_MASK)
        vertex_tangent = new Vector4D[nu_vertices];
    if (flags & SRE_COLOR_MASK)
        colors = new Color[nu_vertices];
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
    Point3D *new_vertex = new Point3D[nu_vertices];
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

void sreBaseModel::RemoveUnusedVertices() {
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
    if (count == nu_vertices) {
        // No unused vertices were found.
        delete [] vertex_used;
        return;
    }
    //  Mapping from new index to original index.
    int *vertex_mapping = new int[nu_vertices];
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
    delete [] vertex_mapping;
    delete [] vertex_mapping2;
    if (sre_internal_debug_message_level >= 2)
        printf("RemoveUnusedVertices: vertices reduced from %d to %d.\n",
            original_nu_vertices, nu_vertices);
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
     clone->vertex = new Point3D[nu_vertices];
     for (int i = 0; i < nu_vertices; i++)
         clone->vertex[i] = vertex[i];
     clone->nu_triangles = nu_triangles;
     clone->triangle = new ModelTriangle[nu_triangles];
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
    ModelTriangle *new_triangle = new ModelTriangle[nu_triangles - count];
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
                const ModelTriangle *triangleArray, ModelEdge *edgeArray)
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
    const ModelTriangle *triangle = triangleArray;
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

void sreLODModelShadowVolume::CalculateEdges() {
    sreBaseModel *clone = new sreBaseModel;
    // Create a clone with just vertex position and triangle information.
    CloneGeometry(clone);
    if (sre_internal_debug_message_level >= 2)
        printf("(geometry only) ");
    // Optimize the geometry-only mode by merging all similar vertices, and keep
    // track of the vertex index mapping to the original model vertices.
    int *saved_indices = new int[nu_vertices];
    clone->MergeIdenticalVertices(saved_indices);
    // Calculate edges and store them in the full model.
    edge = new ModelEdge[clone->nu_triangles * 3];
    nu_edges = BuildEdges(clone->nu_vertices, clone->nu_triangles, clone->triangle, edge);
    if (sre_internal_debug_message_level >= 2)
        printf("CalculateEdges (geometry only): found %d edges.\n", nu_edges);
    flags |= SRE_LOD_MODEL_HAS_EDGE_INFORMATION;
    // Remapping the edge vertex indices from the geometry only edge calculation
    // to the ones of the full model.
    for (int i = 0; i < nu_edges; i++) {
        edge[i].vertex_index[0] = saved_indices[edge[i].vertex_index[0]];
        edge[i].vertex_index[1] = saved_indices[edge[i].vertex_index[1]];
    }
    // Clean up.
    delete [] saved_indices;
    delete [] clone->vertex;
    delete [] clone->triangle;
    delete clone;
}

void sreLODModelShadowVolume::DestroyEdges() {
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

void sreBaseModel::ReduceTriangleCount(float max_surface_roughness, float cost_threshold,
bool check_vertex_normals, float vertex_normal_threshold) {
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
    RemoveUnusedVertices();
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
    if (flags & (SRE_OBJECT_LIGHT_HALO | SRE_LOD_MODEL_IS_BILLBOARD)) {
         float billboard_size = maxf(billboard_width, billboard_height);
         AABB.dim_min = sphere.center - 0.5 * Vector3D(billboard_size, billboard_size, billboard_size);
         AABB.dim_max = sphere.center + 0.5 * Vector3D(billboard_size, billboard_size, billboard_size);
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

// Preprocessing stage functions. Preprocessing adjusts some static objects to reduce the risk of
// rendering artifacts (especially for shadow volumes), creating a new seperate model for them.
// Preprocessing can only handle the objects with just one LOD level.

// Convert an instantation of a sceneobject to static scenery polygons with absolute coordinates.

sreModel *SceneObject::ConvertToStaticScenery() const {
    sreModel *m = model;
    sreModel *new_m = new sreModel;
    sreLODModel *lm = model->lod_model[0];
    sreLODModel *new_lm = lm->AllocateNewOfSameType();
    // Set just one LOD levels. Extra LOD levels are incompatible with T-Junction elimination.
    new_m->lod_model[0] = new_lm;
    new_m->nu_lod_levels = 1;

    // Copy LOD model information.
    new_lm->sorting_dimension = lm->sorting_dimension;
    new_lm->nu_vertices = lm->nu_vertices;
    new_lm->flags = lm->flags;
    new_lm->vertex = new Point3D[lm->nu_vertices];
    new_lm->vertex_normal = new Vector3D[lm->nu_vertices];
    if (lm->flags & SRE_TEXCOORDS_MASK)
        new_lm->texcoords = new Point2D[lm->nu_vertices];
    if (lm->flags & SRE_COLOR_MASK)
        new_lm->colors = new Color[lm->nu_vertices];
    if (lm->flags & SRE_TANGENT_MASK)
        new_lm->vertex_tangent = new Vector4D[lm->nu_vertices];
    // The number of triangles is set to zero. Only polygons are initialized (with the triangles
    // of the source object). When the modified static object is actually used, the triangle
    // information will be properly set.
    new_lm->nu_triangles = 0;
    for (int i = 0; i < lm->nu_vertices; i++) {
        new_lm->vertex[i] = (model_matrix * lm->vertex[i]).GetPoint3D();
        new_lm->vertex_normal[i] = rotation_matrix * lm->vertex_normal[i];
        if (lm->flags & SRE_TANGENT_MASK)
            new_lm->vertex_tangent[i] = Vector4D(rotation_matrix * lm->vertex_tangent[i].GetVector3D(),
                lm->vertex_tangent[i].w);
        if (lm->flags & SRE_TEXCOORDS_MASK)
            new_lm->texcoords[i] = lm->texcoords[i];
        if (lm->flags & SRE_COLOR_MASK)
            new_lm->colors[i] = lm->colors[i];
    }

    // Initialize the polygon data for the sreModel.
    new_m->nu_polygons = lm->nu_triangles;
    new_m->polygon = new ModelPolygon[new_m->nu_polygons];
    for (int i = 0; i < new_m->nu_polygons; i++)
        new_m->polygon[i].InitializeWithSize(3);
    for (int i = 0; i < lm->nu_triangles; i++) {
        new_m->polygon[i].normal = rotation_matrix * lm->triangle[i].normal;
        for (int j = 0; j < 3; j++)
            new_m->polygon[i].vertex_index[j] = lm->triangle[i].vertex_index[j];
    }
    new_m->is_static = true;

    // Copy remaining fields in the sreModel.
    new_m->bounds_flags = m->bounds_flags;
    if (flags & (SRE_OBJECT_LIGHT_HALO | SRE_LOD_MODEL_IS_BILLBOARD | SRE_OBJECT_PARTICLE_SYSTEM)) {
        new_m->sphere.center = sphere.center;
        new_m->sphere.radius = sphere.radius;
    }
    else {
        new_m->PCA[0].vector = (rotation_matrix * m->PCA[0].vector);
        new_m->PCA[1].vector = (rotation_matrix * m->PCA[1].vector);
        new_m->PCA[2].vector = (rotation_matrix * m->PCA[2].vector);
        new_m->PCA[0].size = m->PCA[0].size * scaling;
        new_m->PCA[1].size = m->PCA[1].size * scaling;
        new_m->PCA[2].size = m->PCA[2].size * scaling;
        new_m->sphere.center = (model_matrix * m->sphere.center).GetPoint3D();
        new_m->sphere.radius = m->sphere.radius * scaling;
        new_m->box_center = (model_matrix * m->box_center).GetPoint3D();
        new_m->CalculateAABB();
        if (m->bounds_flags & SRE_BOUNDS_PREFER_SPECIAL) {
            new_m->bv_special.type = m->bv_special.type;
            if (m->bv_special.type == SRE_BOUNDING_VOLUME_ELLIPSOID) {
                new_m->bv_special.ellipsoid = new sreBoundingVolumeEllipsoid;
                new_m->bv_special.ellipsoid->center = (model_matrix * m->bv_special.ellipsoid->center).GetPoint3D();
                new_m->bv_special.ellipsoid->PCA[0].vector = (rotation_matrix * m->bv_special.ellipsoid->PCA[0].vector) * scaling;
                new_m->bv_special.ellipsoid->PCA[1].vector = (rotation_matrix * m->bv_special.ellipsoid->PCA[1].vector) * scaling;
                new_m->bv_special.ellipsoid->PCA[2].vector = (rotation_matrix * m->bv_special.ellipsoid->PCA[2].vector) * scaling;
            }
            else
            if (m->bv_special.type ==  SRE_BOUNDING_VOLUME_CYLINDER) {
                new_m->bv_special.cylinder = new sreBoundingVolumeCylinder;
                new_m->bv_special.cylinder->center = (model_matrix * m->bv_special.cylinder->center).GetPoint3D();
                new_m->bv_special.cylinder->radius = m->bv_special.cylinder->radius * scaling;
                new_m->bv_special.cylinder->length = m->bv_special.cylinder->length * scaling;
                new_m->bv_special.cylinder->axis = rotation_matrix * m->bv_special.cylinder->axis;
            }
        }
    }
    new_m->collision_shape_static = m->collision_shape_static;
    new_m->collision_shape_dynamic = m->collision_shape_dynamic;
    new_m->fluid = m->fluid;
    return new_m;
}

// Check whether two objects converted to static scenery intersect, with a small margin.

// For bounding volume tests, define a larger epsilon.
#define EPSILON2 0.001

static bool ModelBoundsIntersectWithMargin(const sreModel& m1, const sreModel& m2) {
    // First do a sphere check.
    sreBoundingVolumeSphere sphere1, sphere2;
    sphere1.center = m1.sphere.center;
    sphere1.radius = m1.sphere.radius + EPSILON2;
    sphere2.center = m2.sphere.center;
    sphere2.radius = m2.sphere.radius + EPSILON2;
    if (!Intersects(sphere1, sphere2))
        // The two spheres do not intersect.
        return false;
    // Try an AABB check.
    if (m1.AABB.dim_min.x - EPSILON2 > m2.AABB.dim_max.x || m1.AABB.dim_max.x + EPSILON2 < m2.AABB.dim_min.x ||
    m1.AABB.dim_min.y - EPSILON2 > m2.AABB.dim_max.y || m1.AABB.dim_max.y + EPSILON2 < m2.AABB.dim_min.y ||
    m1.AABB.dim_min.z - EPSILON2 > m2.AABB.dim_max.z || m1.AABB.dim_max.z + EPSILON2 < m2.AABB.dim_min.z) {
        return false;
    }
    return true;
}

static bool PointIntersectsWithModelBoundsWithMargin(const Point3D& P, const sreModel& m) {
    // Do a sphere check.
    sreBoundingVolumeSphere sphere;
    sphere.center = m.sphere.center;
    sphere.radius = m.sphere.radius + EPSILON2;
    if (!Intersects(P, sphere))
        return false;
    // Try an AABB check.
    if (P.x - EPSILON2 > m.AABB.dim_max.x || P.x + EPSILON2 < m.AABB.dim_min.x ||
    P.y - EPSILON2 > m.AABB.dim_max.y || P.y + EPSILON2 < m.AABB.dim_min.y ||
    P.z - EPSILON2 > m.AABB.dim_max.z || P.z + EPSILON2 < m.AABB.dim_min.z) {
        return false;
    }
    sreBoundingVolumeBox box;
    box.center = m.box_center;
    // Create a box that slightly larger.
    float size[3];
    size[0] = m.PCA[0].size + EPSILON2;
    size[1] = m.PCA[1].size + EPSILON2;
    size[2] = m.PCA[2].size + EPSILON2;
    // Note: sreBoundingVolumeBox uses scaled PCA components.
    box.PCA[0].scale_factor = 1.0f / size[0];
    box.PCA[1].scale_factor = 1.0f / size[1];
    box.PCA[2].scale_factor = 1.0f / size[2];
    box.PCA[0].vector = m.PCA[0].vector * size[0];
    box.PCA[1].vector = m.PCA[1].vector * size[1];
    box.PCA[2].vector = m.PCA[2].vector * size[2];
    box.CalculatePlanes();
    if (!Intersects(P, box))
        return false;
    return true;
}

static bool EdgeIntersectsWithModelBoundsWithMargin(const Point3D& P1, const Point3D& P2,
const sreModel& m) {
    // Try an AABB check.
    if ((P1.x - EPSILON2 > m.AABB.dim_max.x && P1.x - EPSILON2 > m.AABB.dim_max.x) ||
    (P1.x + EPSILON2 < m.AABB.dim_min.x && P2.x + EPSILON2 < m.AABB.dim_min.x) || 
    (P1.y - EPSILON2 > m.AABB.dim_max.y && P2.y - EPSILON2 > m.AABB.dim_max.y) ||
    (P1.y + EPSILON2 < m.AABB.dim_min.y && P2.y + EPSILON2 < m.AABB.dim_min.y) ||
    (P1.z - EPSILON2 > m.AABB.dim_max.z && P2.z - EPSILON2 > m.AABB.dim_max.z) ||
    (P1.z + EPSILON2 < m.AABB.dim_min.z && P2.z + EPSILON2 < m.AABB.dim_min.z))
        return false;
#if 0
#endif
    return true;
}

// Weld models m1 and m2, correcting vertices in m1.
// Returns true if one or more vertices in m1 where changed (welded).

static bool WeldModels(sreModel& m1, const sreModel& m2) {
    int count = 0;
    int shared_count = 0;
    sreLODModel *lm1 = m1.lod_model[0];
    sreLODModel *lm2 = m2.lod_model[0];
    for (int i = 0; i < lm1->nu_vertices; i++) {
        // First check that the vertex of object 1 is, with a small negative margin, outside the bounding volume of
        // object 2.
        if (!PointIntersectsWithModelBoundsWithMargin(lm1->vertex[i], m2))
            continue;
        // Try to find a similar vertex.
        int k;
        for (k = 0; k < lm2->nu_vertices; k++) {
            if (SquaredMag(lm1->vertex[i] - lm2->vertex[k]) >= EPSILON * EPSILON)
                continue;
            // The vertices are similar.
            break;
        }
        if (k < lm2->nu_vertices) {
            // We found a similar vertex. If they are not exactly same, make them have the same position.
            if (lm1->vertex[i] != lm2->vertex[k]) {
                lm1->vertex[i] = lm2->vertex[k];
                count++;
            }
            else
                shared_count++;
        }
    }
//    if (shared_count > 0 || count > 0)
//        printf("WeldSceneModels: %d shared vertices found, %d vertices welded.\n", shared_count, count);
    if (count > 0) {
        // The welding process may have corrupted the sorting order object m1->
        if (lm1->sorting_dimension != - 1)
            lm1->SortVertices(lm1->sorting_dimension);
        return true;
    }
    else
        return false;
}

// Insert a new polygon vertex at polygon index i.

void sreModel::InsertPolygonVertex(int p, int i, const Point3D& Q, float t) {
    int vindex1;
    if (i > 0)
        vindex1 = polygon[p].vertex_index[i - 1];
    else
        vindex1 = polygon[p].vertex_index[polygon[p].nu_vertices - 1];
    int vindex2 = polygon[p].vertex_index[i];
    // Insert a new vertex into the polygon.
    int *new_vertex_index = new int[polygon[p].nu_vertices + 1];
    for (int j = 0; j < i; j++)
        new_vertex_index[j] = polygon[p].vertex_index[j];
    sreLODModel *lm = lod_model[0];
    new_vertex_index[i] = lm->nu_vertices;
    for (int j = i; j < polygon[p].nu_vertices; j++)
        new_vertex_index[j + 1] = polygon[p].vertex_index[j];
    delete [] polygon[p].vertex_index;
    polygon[p].vertex_index = new_vertex_index;
    polygon[p].nu_vertices++;
    // Append a new vertex to the vertex array.
    Point3D *new_vertex = new Point3D[lm->nu_vertices + 1];
    memcpy(new_vertex, lm->vertex, sizeof(Point3D) * lm->nu_vertices);
    new_vertex[lm->nu_vertices] = Q;
    delete [] lm->vertex;
    lm->vertex = new_vertex;
    // Append a new vertex normal.
    Vector3D *new_vertex_normal = new Vector3D[lm->nu_vertices + 1];
    memcpy(new_vertex_normal, lm->vertex_normal, sizeof(Vector3D) * lm->nu_vertices);
    // Interpolate the vertex normal between P1 and P2.
    new_vertex_normal[lm->nu_vertices] =
        (lm->vertex_normal[vindex1] * (1.0 - t) + lm->vertex_normal[vindex2] * t).Normalize();
    delete [] lm->vertex_normal;
    lm->vertex_normal = new_vertex_normal;
    if (lm->flags & SRE_TANGENT_MASK) {
        // Append a new vertex tangent.
        Vector4D *new_vertex_tangent = new Vector4D[lm->nu_vertices + 1];
        memcpy(new_vertex_tangent, lm->vertex_tangent, sizeof(Vector4D) * lm->nu_vertices);
        // Interpolate the vertex tangent between P1 and P2.
        Vector3D tangent = (lm->vertex_tangent[vindex1].GetVector3D() * (1.0 - t) +
            lm->vertex_tangent[vindex2].GetVector3D() * t).Normalize();
        // Preserve the w component from P1.
        new_vertex_tangent[lm->nu_vertices] = Vector4D(tangent, lm->vertex_tangent[vindex1].w); 
        delete [] lm->vertex_tangent;
        lm->vertex_tangent = new_vertex_tangent;
    }
    if (flags & SRE_TEXCOORDS_MASK) {
        // Append a new texcoord.
        Point2D *new_texcoords = new Point2D[lm->nu_vertices + 1];
        memcpy(new_texcoords, lm->texcoords, sizeof(Point2D) * lm->nu_vertices);
        // Interpolate the texcoords between P1 and P2.
        new_texcoords[lm->nu_vertices] = lm->texcoords[vindex1] * (1.0 - t) + lm->texcoords[vindex2] * t;
        delete [] lm->texcoords;
        lm->texcoords = new_texcoords;
    }
    if (flags & SRE_COLOR_MASK) {
        // Append a new multi-color color.
        Color *new_colors = new Color[lm->nu_vertices + 1];
        memcpy(new_colors, lm->colors, sizeof(Color) * lm->nu_vertices);
        // Interpolate the color between P1 and P2.
        new_colors[lm->nu_vertices] = lm->colors[vindex1] * (1.0 - t) + lm->colors[vindex2] * t;
        delete [] lm->colors;
        lm->colors = new_colors;
    }
    lm->nu_vertices++;
}

class VertexInsertion {
public :
    sreModel *m;
    int polygon_index;
    int vertex_index;
    Point3D vertex;
    float t;
};

class VertexInsertionArray {
public :
    int max_size;
    int size;
    VertexInsertion *vertex_insertion;

    VertexInsertionArray() {
        max_size = 1024;
        size = 0;
        vertex_insertion = new VertexInsertion[max_size];
    }
    ~VertexInsertionArray() {
        delete [] vertex_insertion;
    }
    void AddInsertion(sreModel *m, int polygon_index, int vertex_index, Point3D vertex, float t) {
        if (size == max_size) {
             max_size *= 2;
             VertexInsertion *new_vertex_insertion = new VertexInsertion[max_size];
             memcpy(new_vertex_insertion, vertex_insertion, size * sizeof(VertexInsertion));
             delete [] vertex_insertion;
             vertex_insertion = new_vertex_insertion;
        }
        vertex_insertion[size].m = m;
        vertex_insertion[size].polygon_index = polygon_index;
        vertex_insertion[size].vertex_index = vertex_index;
        vertex_insertion[size].vertex = vertex;
        vertex_insertion[size].t = t;
        size++;
    }
};

static VertexInsertionArray *vertex_insertion_array;

bool sreScene::EliminateTJunctionsForModels(sreModel& m1, const sreModel& m2) const {
    if (sre_internal_debug_message_level >= 2)
        printf("Eliminating T-junctions for objects %d and %d.\n", m1.id, m2.id);
    sreLODModel *lm1 = m1.lod_model[0];
    sreLODModel *lm2 = m2.lod_model[0];
    // First determine whether any vertex of m2 lies within distance EPSILON of any vertex in m1.
    bool *close_to_vertex = new bool[lm1->nu_vertices];
    for (int i = 0; i < lm1->nu_vertices; i++) {
        close_to_vertex[i] = false;
        // Check whether the vertex of object 1 is, with a small margin, outside the bounding volume of object 2.
        if (!PointIntersectsWithModelBoundsWithMargin(lm1->vertex[i], m2))
            continue;
        // Try to find a similar vertex.
        int starting_index = 0;
        int ending_index = lm2->nu_vertices - 1;
        if (lm2->sorting_dimension != - 1) {
            // If object 2 is sorted on a coordinate, we can optimize the searching process.
            float coordinate = lm2->vertex[i][lm2->sorting_dimension];
            // A binary search would be a lot better.
            int i = 0;
            for (; i < lm2->nu_vertices; i++)
                if (lm2->vertex[i][lm2->sorting_dimension] >= coordinate - EPSILON)
                    break;
            starting_index = i;
            for (; i < lm2->nu_vertices; i++)
                if (lm2->vertex[i][lm2->sorting_dimension] > coordinate + EPSILON)
                    break;
            ending_index = i - 1;
        }
        for (int k = starting_index; k <= ending_index; k++) {
            if (SquaredMag(lm1->vertex[i] - lm2->vertex[k]) >= EPSILON * EPSILON)
                continue;
            // The vertices are close.
            close_to_vertex[i] = true;
        }
    }
    bool changed = false;
    int count = 0;
    // For every edge in m1.
    for (int j = 0; j < m1.nu_polygons; j++)
        for (int k = 0; k < m1.polygon[j].nu_vertices; k++) {
            Point3D P1 = lm1->vertex[m1.polygon[j].vertex_index[k]];
            int next_vertex;
            if (k == m1.polygon[j].nu_vertices - 1)
                next_vertex = 0;
            else
                next_vertex = k + 1;
            Point3D P2 = lm1->vertex[m1.polygon[j].vertex_index[next_vertex]];
            Vector3D S = P2 - P1;
            // Check whether the edge is, with margin, outside the bounding volume of object 2.
            if (!EdgeIntersectsWithModelBoundsWithMargin(P1, P2, m2))
                continue;
            // For every vertex in m2.
            int starting_index = 0;
            int ending_index = lm2->nu_vertices - 1;
            if (lm2->sorting_dimension != - 1) {
                // If object 2 is sorted on a coordinate, we can optimize the searching process.
                float edge_min_coordinate = minf(P1[lm2->sorting_dimension], P2[lm2->sorting_dimension]);
                float edge_max_coordinate = maxf(P1[lm2->sorting_dimension], P2[lm2->sorting_dimension]);
                // A binary search would be a lot better.
                int i = 0;
                for (i = 0; i < lm2->nu_vertices; i++)
                    if (lm2->vertex[i][lm2->sorting_dimension] >= edge_min_coordinate - EPSILON)
                        break;
                starting_index = i;
                for (; i < lm2->nu_vertices; i++)
                    if (lm2->vertex[i][lm2->sorting_dimension] > edge_max_coordinate + EPSILON)
                        break;
                ending_index = i - 1;
            }
            for (int i = starting_index; i <= ending_index; i++) {
                // Skip vertices of m2 that are close to any vertices in m1.
                if (close_to_vertex[i])
                    continue;
                Vector3D R = lm2->vertex[i] - P1;
                float term = Dot(R, S);
                float d_squared = Dot(R, R) - term * term / Dot(S, S);
                if (d_squared < EPSILON * EPSILON) {
                    // The vertex in m2 lies close to the line defined by the edge of m1.
                    float edge_length = Magnitude(S);
                    float t = Dot(R, S) / edge_length;
                    if (t < EPSILON || t > edge_length - EPSILON)
                        // The vertex in m2 does not lie on the edge.
                        continue;
                    // We have found a T-junction, insert a new vertex in the polygon between P1 and P2.
                    vertex_insertion_array->AddInsertion(&m1, j, next_vertex, lm2->vertex[i], t / edge_length);
                    changed = true;
                    count++;
                }
            }
    }
    delete [] close_to_vertex;
    if (sre_internal_debug_message_level >= 2)
        printf("%d new vertices to be inserted.\n", count);
    return changed;
}

void sreScene::DetermineStaticIntersectingObjects(const FastOctree& fast_oct, int array_index,
int model_index, const sreBoundingVolumeAABB& AABB, int *static_object_belonging_to_sceneobject,
int &nu_intersecting_objects, int *intersecting_object) const {
    int node_index = fast_oct.array[array_index];
    if (!Intersects(AABB, fast_oct.node_bounds[node_index].AABB))
        return;
    int nu_octants = fast_oct.array[array_index + 1] & 0xFF;
    int nu_entities = fast_oct.array[array_index + 2];
    array_index += 3;
    for (int i = 0; i < nu_entities; i++) {
        SceneEntityType type;
        int index;
        fast_oct.GetEntity(array_index + i, type, index);
        if (type == SRE_ENTITY_OBJECT && static_object_belonging_to_sceneobject[index] != - 1) {
            if (ModelBoundsIntersectWithMargin(*model[model_index],
            *model[static_object_belonging_to_sceneobject[index]])) {
                intersecting_object[nu_intersecting_objects] =
                   static_object_belonging_to_sceneobject[index];
                nu_intersecting_objects++;
            }
        }
    }
    array_index += nu_entities;
    for (int i = 0; i < nu_octants; i++)
        DetermineStaticIntersectingObjects(fast_oct, fast_oct.array[array_index + i], model_index,
            AABB, static_object_belonging_to_sceneobject, nu_intersecting_objects, intersecting_object);
}

static int CompareVertexInsertions(const void *e1, const void *e2) {
    VertexInsertion *vi1 = (VertexInsertion *)e1;
    VertexInsertion *vi2 = (VertexInsertion *)e2;
    if (vi1->polygon_index < vi2->polygon_index)
        return - 1;
    if (vi1->polygon_index > vi2->polygon_index)
        return 1;
    // The polygon index is the same.
    if (vi1->vertex_index < vi2->vertex_index)
        return - 1;
    if (vi1->vertex_index > vi2->vertex_index)
        return 1;
    // The insertion position is the same.
    if (vi1->t < vi2->t)
        return - 1;
    if (vi1->t > vi2->t)
        return 1;
    // The insertions are the same.
    return 0;
}

void sreScene::EliminateTJunctions() {
    // Convert static objects to absolute coordinates.
    int count = 0;
    int *sceneobject_belonging_to_object = new int[nu_objects + nu_models];
    int *static_object_belonging_to_sceneobject = new int[nu_objects];
    for (int i = 0; i < nu_objects; i++) {
        if (sceneobject[i]->model->is_static)
            printf("Unexpected scene object found with model already marked static"
                "before conversion to absolute coordinates (model id = %d).\n",
                 sceneobject[i]->model->id);
        if (!(sceneobject[i]->flags & (SRE_OBJECT_DYNAMIC_POSITION | SRE_OBJECT_INFINITE_DISTANCE |
        SRE_LOD_MODEL_IS_BILLBOARD | SRE_OBJECT_LIGHT_HALO))) {
            sreModel *m = sceneobject[i]->ConvertToStaticScenery();
            RegisterModel(m);
            sceneobject_belonging_to_object[m->id] = i;
            static_object_belonging_to_sceneobject[i] = m->id;
            count++;
//            printf("New static model with id %d created for scene object %d.\n", m->id, i);
        }
        else
            static_object_belonging_to_sceneobject[i] = - 1;
    }
    printf("%d objects considered for being weldable static scenery objects.\n", count);
    // For every close pair of objects, weld them and remove T-Junctions.
    int pair_count = 0;
    int weld_count = 0;
    int t_junction_count = 0;
    bool *model_changed = new bool[nu_models];
    int *intersecting_object = new int[nu_models];
    vertex_insertion_array = new VertexInsertionArray;
    for (int i = 0; i < nu_models; i++) {
        // Converted scenery is maked with the is_static flag.
        if (model[i]->is_static) {
            model_changed[i] = false;
            sreBoundingVolumeAABB AABB;
            AABB.dim_min = model[i]->AABB.dim_min - Vector3D(EPSILON2, EPSILON2, EPSILON2);
            AABB.dim_max = model[i]->AABB.dim_max + Vector3D(EPSILON2, EPSILON2, EPSILON2);
            int nu_intersecting_objects = 0;
            // All static objects are conveniently grouped in the static entities octree.
            DetermineStaticIntersectingObjects(fast_octree_static, 0, i, AABB,
                static_object_belonging_to_sceneobject, nu_intersecting_objects, intersecting_object);
            for (int k = 0; k < nu_intersecting_objects; k++) {
                int j = intersecting_object[k];
                if (j != i && model[j]->is_static) {
                    pair_count++;
                    bool r = WeldModels(*model[i], *model[j]);
                    if (r) {
                        model_changed[i] = true;
                        weld_count++;
                    }
                    r = EliminateTJunctionsForModels(*model[i], *model[j]);
                    if (r) {
                       model_changed[i] = true;
                       t_junction_count++;
                    }
                }
            }
        }
    }
    delete [] intersecting_object;
    // Insert the queued polygon vertices.
    // All insertions for a single object are guaranteed to be grouped together.
    for (int i = 0; i < vertex_insertion_array->size;) {
        int starting_index = i;
        int ending_index = i;
        sreModel *m = vertex_insertion_array->vertex_insertion[i].m;
        // Check how many insertions for this object there are.
        for (int j = i + 1; j < vertex_insertion_array->size; j++) {
            if (vertex_insertion_array->vertex_insertion[j].m != m) {
                ending_index = j - 1;
                break;
            }
            if (j == vertex_insertion_array->size - 1)
                ending_index = j;
        }
        if (sre_internal_debug_message_level >= 1)
            printf("%d vertex insertions for object %d.\n", ending_index - starting_index + 1, m->id);
        // Sort the insertions on polygon index, then on insertion position, then on t.
        if (ending_index - starting_index > 0)
            qsort(&vertex_insertion_array->vertex_insertion[starting_index],
                ending_index - starting_index + 1,
                sizeof(VertexInsertion), CompareVertexInsertions);
        for (int j = starting_index; j <= ending_index; j++) {
            int polygon_index = vertex_insertion_array->vertex_insertion[j].polygon_index;
            int count = 0; // Keep track how many vertices have already been inserted into the same polygon before
                           // the current position.
            // For each insertion with the same polygon index.
            for (;; j++) {
                int vertex_index = vertex_insertion_array->vertex_insertion[j].vertex_index;
                int vertex_index_starting_index = j;
                // For each insertion with the same vertex index.
                float t0 = 0;
                for (;; j++) {
                    // If the insert vertex is identical to the previously inserted vertex, skip it.
                    if (j > vertex_index_starting_index)
                        if (vertex_insertion_array->vertex_insertion[j].t ==
                        vertex_insertion_array->vertex_insertion[j - 1].t) {
//                             printf("Skipping identical vertex.\n");
                             goto next_t;
                        }
//                    printf("Inserting vertex into polygon %d at position %d (originally %d), t = %f, (polygon size = %d).\n",
//                        polygon_index, vertex_index + count, vertex_index, vertex_insertion_array->vertex_insertion[j].t,
//                        m->polygon[vertex_insertion_array->vertex_insertion[j].polygon_index].nu_vertices);
                    m->InsertPolygonVertex(polygon_index, vertex_index + count,
                        vertex_insertion_array->vertex_insertion[j].vertex,
                        (vertex_insertion_array->vertex_insertion[j].t - t0) / (1.0 - t0));
                    t0 = vertex_insertion_array->vertex_insertion[j].t;
                    count++;
next_t :
                    if (j == ending_index)
                        break;
                    if (vertex_insertion_array->vertex_insertion[j + 1].vertex_index != vertex_index)
                        break;
                }
                if (j == ending_index)
                    break;
                if (vertex_insertion_array->vertex_insertion[j + 1].polygon_index != polygon_index)
                    break;
            }
        }
        i = ending_index + 1;
    }
    delete vertex_insertion_array; // With destructor.
    int changed_count = 0;
    for (int i = 0; i < nu_models; i++) {
        if (model[i]->is_static) {
            if (model_changed[i]) {
                 // The duplicated static object will be used.
                 // Mark the model as referenced.
                 model[i]->referenced = true;
                 // Mark the LOD model as referenced.
                 model[i]->lod_model[0]->referenced = true;
                 // Update fields in scene object to reflect the fact that coordinates are absolute.
                 int j = sceneobject_belonging_to_object[i];
                 sceneobject[j]->model = model[i];
                 sceneobject[j]->model_matrix.SetIdentity();
                 // Save the original rotation matrix.
                 sceneobject[j]->original_rotation_matrix = new Matrix3D;
                 *sceneobject[j]->original_rotation_matrix = sceneobject[j]->rotation_matrix;
                 sceneobject[j]->rotation_matrix.SetIdentity();
                 sceneobject[j]->inverted_model_matrix.SetIdentity();
                 sceneobject[j]->position = Point3D(0, 0, 0);
                 sceneobject[j]->scaling = 1.0;
                 changed_count++;
             }
             else {
                 // Not necessary to duplicate this object, so go back to using the original instantiation.
                 // Free the unused static object.
                 // Delete the single LOD level.
                 sreLODModel *lm = model[i]->lod_model[0];
                 delete [] lm->vertex;
                 delete [] lm->vertex_normal;
                 if (lm->flags & SRE_TANGENT_MASK)
                     delete [] lm->vertex_tangent;
                 delete lm;
                 // Delete the sreModel.
                 for (int j = 0; j < model[i]->nu_polygons; j++)
                     delete model[i]->polygon[j].vertex_index;
                 delete model[i]->polygon;
                 if (model[i]->bounds_flags & SRE_BOUNDS_PREFER_SPECIAL) {
                     if (model[i]->bv_special.type == SRE_BOUNDING_VOLUME_ELLIPSOID)
                         delete model[i]->bv_special.ellipsoid;
                     else
                     if (model[i]->bv_special.type ==  SRE_BOUNDING_VOLUME_CYLINDER)
                         delete model[i]->bv_special.cylinder;
                 }
                 delete model[i];
                 model[i] = NULL;	// Mark as invalid.
             }
         }
    }
    delete [] model_changed;
    delete [] sceneobject_belonging_to_object;
    delete [] static_object_belonging_to_sceneobject;
    printf("%d close object pairs checked for weldable vertices.\n", pair_count);
    printf("%d objects welded or adjusted and duplicated.\n", changed_count);
    if (changed_count > 0)
        printf("%d object pairs welded and T-junctions removed in %d pairs.\n", weld_count, t_junction_count);
}

typedef struct {
    int index[3];
} Triangle;

//============================================================================
//
// Listing 9.2
//
// Mathematics for 3D Game Programming and Computer Graphics, 3rd ed.
// By Eric Lengyel
//
// The code in this file may be freely used in any software. It is provided
// as-is, with no warranty of any kind.
//
//============================================================================

const float epsilon = 0.001F;

static int GetNextActive(int x, int vertexCount, const bool *active)
{
	for (;;)
	{
		if (++x == vertexCount) x = 0;
		if (active[x]) return (x);
	}
}

static int GetPrevActive(int x, int vertexCount, const bool *active)
{
	for (;;)
	{
		if (--x == -1) x = vertexCount - 1;
		if (active[x]) return (x);
	}
}

int TriangulatePolygon(int vertexCount, const Point3D *vertex,
		const Vector3D& normal, Triangle *triangle)
{
	bool *active = new bool[vertexCount];
	for (int a = 0; a < vertexCount; a++) active[a] = true;
	
	int triangleCount = 0;
	int start = 0;
	int p1 = 0;
	int p2 = 1;
	int m1 = vertexCount - 1;
	int m2 = vertexCount - 2;
	
	bool lastPositive = false;
	for (;;)
	{
		if (p2 == m2)
		{
			// Only three vertices remain.
			triangle->index[0] = m1;
			triangle->index[1] = p1;
			triangle->index[2] = p2;
			triangleCount++;
			break;
		}
		
		const Point3D& vp1 = vertex[p1];
		const Point3D& vp2 = vertex[p2];
		const Point3D& vm1 = vertex[m1];
		const Point3D& vm2 = vertex[m2];
		bool positive = false;
		bool negative = false;
		
		// Determine whether vp1, vp2, and vm1 form a valid triangle.
		Vector3D n1 = normal % (vm1 - vp2).Normalize();
		if (n1 * (vp1 - vp2) > epsilon)
		{
			positive = true;
			Vector3D n2 = (normal % (vp1 - vm1).Normalize());
			Vector3D n3 = (normal % (vp2 - vp1).Normalize());
			
			for (int a = 0; a < vertexCount; a++)
			{
				// Look for other vertices inside the triangle.
				if ((active[a]) && (a != p1) && (a != p2) && (a != m1))
				{
					const Vector3D& v = vertex[a];
					if ((n1 * (v - vp2).Normalize() > -epsilon)
						&& (n2 * (v - vm1).Normalize() > -epsilon)
						&& (n3 * (v - vp1).Normalize() > -epsilon))
					{
						positive = false;
						break;
					}
				}
			}
		}
		
		// Determine whether vm1, vm2, and vp1 form a valid triangle.
		n1 = normal % (vm2 - vp1).Normalize();
		if (n1 * (vm1 - vp1) > epsilon)
		{
			negative = true;
			Vector3D n2 = (normal % (vm1 - vm2).Normalize());
			Vector3D n3 = (normal % (vp1 - vm1).Normalize());
			
			for (int a = 0; a < vertexCount; a++)
			{
				// Look for other vertices inside the triangle.
				if ((active[a]) && (a != m1) && (a != m2) && (a != p1))
				{
					const Vector3D& v = vertex[a];
					if ((n1 * (v - vp1).Normalize() > -epsilon)
						&& (n2 * (v - vm2).Normalize() > -epsilon)
						&& (n3 * (v - vm1).Normalize() > -epsilon))
					{
						negative = false;
						break;
					}
				}
			}
		}
		
		// If both triangles are valid, choose the one having
		// the larger smallest angle.
		if ((positive) && (negative))
		{
			float pd = (vp2 - vm1).Normalize() * (vm2 - vm1).Normalize();
			float md = (vm2 - vp1).Normalize() * (vp2 - vp1).Normalize();
			if (fabs(pd - md) < epsilon)
			{
				if (lastPositive) positive = false;
				else negative = false;
			}
			else
			{
				if (pd < md) negative = false;
				else positive = false;
			}
		}
		
		if (positive)
		{
			// Output the triangle m1, p1, p2.
			active[p1] = false;
			triangle->index[0] = m1;
			triangle->index[1] = p1;
			triangle->index[2] = p2;
			triangleCount++;
			triangle++;
			
			p1 = GetNextActive(p1, vertexCount, active);
			p2 = GetNextActive(p2, vertexCount, active);
			lastPositive = true;
			start = -1;
		}
		else if (negative)
		{
			// Output the triangle m2, m1, p1.
			active[m1] = false;
			triangle->index[0] = m2;
			triangle->index[1] = m1;
			triangle->index[2] = p1;
			triangleCount++;
			triangle++;
			
			m1 = GetPrevActive(m1, vertexCount, active);
			m2 = GetPrevActive(m2, vertexCount, active);
			lastPositive = false;
			start = -1;
		}
		else
		{
			// Exit if we've gone all the way around the
			// polygon without finding a valid triangle.
			if (start == -1) start = p2;
			else if (p2 == start) break;
			
			// Advance working set of vertices.
			m2 = m1;
			m1 = p1;
			p1 = p2;
			p2 = GetNextActive(p2, vertexCount, active);
		}
	}

	delete[] active;
	return (triangleCount);
}

void sreModel::Triangulate() {
    // Calculate an upper bound for the amount of triangles
    int triangle_count = 0;
    int max_triangle_count_per_polygon = 0;
    for (int i = 0; i < nu_polygons; i++) {
        triangle_count += polygon[i].nu_vertices - 2;
        if (polygon[i].nu_vertices - 2 > max_triangle_count_per_polygon)
            max_triangle_count_per_polygon = polygon[i].nu_vertices - 2;
    }
    // The model will always have just one LOD level.
    sreLODModel *m = lod_model[0];
    m->triangle = new ModelTriangle[triangle_count];
    Triangle *tri = new Triangle[max_triangle_count_per_polygon];
    Point3D *polygon_vertex = new Point3D[max_triangle_count_per_polygon + 2];
    m->nu_triangles = 0;
    for (int i = 0; i < nu_polygons; i++) {
        for (int j = 0; j < polygon[i].nu_vertices; j++)
            polygon_vertex[j] = m->vertex[polygon[i].vertex_index[j]];
        int n = TriangulatePolygon(polygon[i].nu_vertices, polygon_vertex, polygon[i].normal, &tri[0]);
        if (polygon[i].nu_vertices >= 4 && n == 0)
            printf("Polygon of size %d being triangulated for object %d, normal = (%f, %f, %f), returned %d triangles.\n",
                polygon[i].nu_vertices, id, polygon[i].normal.x, polygon[i].normal.y, polygon[i].normal.z, n);
        // Add the triangles in the LOD model that represent the polygon.
        for (int j = 0; j < n; j++) {
            m->triangle[m->nu_triangles].normal = polygon[i].normal;
            m->triangle[m->nu_triangles].vertex_index[0] = polygon[i].vertex_index[tri[j].index[0]];
            m->triangle[m->nu_triangles].vertex_index[1] = polygon[i].vertex_index[tri[j].index[1]];
            m->triangle[m->nu_triangles].vertex_index[2] = polygon[i].vertex_index[tri[j].index[2]];
            m->nu_triangles++;
        }
    }
    delete [] tri;
    // Clean up the polygon structures in the sreModel.
    delete [] polygon_vertex;
    for (int j = 0; j < nu_polygons; j++)
        delete polygon[j].vertex_index;
    delete [] polygon;
}

void sreScene::Triangulate() {
    for (int i = 0; i < nu_models; i++) {
        if (model[i] != NULL) {
            if (model[i]->is_static)
                model[i]->Triangulate();
        }
    }
}

void sreScene::Preprocess() {
    // T-junction elimination, converting static objects to absolute coordinates if necessary.
    EliminateTJunctions();
    Triangulate();
    for (int i = 0; i < nu_models; i++)
        if (model[i] != NULL)
            if (model[i]->is_static) {
                 // A newly created static object always has just one LOD level.
                 sreLODModel *lm = model[i]->lod_model[0];
                 // The model is modified copy with vertices added or adjusted.
                 // The new vertices will have corrupted the sorting order of the vertices.
                 // Sort the vertices.
                 if (lm->sorting_dimension != - 1)
                     lm->SortVertices(lm->sorting_dimension);
            }
}


// Scene-level model handling.

void sreScene::RegisterModel(sreModel *m) {
    if (nu_models >= max_models) {
        if (sre_internal_debug_message_level >= 1)
            printf("Maximum number of models reached -- doubling capacity to %d.\n",
                max_models * 2);
        sreModel **new_models = new sreModel *[max_models * 2];
        memcpy(new_models, model, sizeof(sreModel *) * max_models);
        delete [] model;
        model = new_models;
        max_models *= 2;
    }
    m->id = nu_models;
    model[nu_models] = m;
    nu_models++;
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
    char *model_used = new char[nu_models];
    memset(model_used, 0, nu_models);
    // First pass: iterate all LOD models and set their ID to - 2.
    // Also check that the index and ID of full models are the same.
    for (int i = 0; i < nu_models; i++) {
        if (model[i] == NULL)
            continue;
        if (model[i]->id != i) {
            printf("Warning: model index %d does not match model id of %d.\n",
                i, model[i]->id);
        }
        for (int j = 0; j < model[i]->nu_lod_levels; j++)
            model[i]->lod_model[j]->id = - 2;
    }
    // Second pass: iterate all objects and mark every model and LOD model used;
    // set the ID of every LOD model encountered (some of which may be shared
    // between different models) to - 1.
    int scene_triangle_count = 0;
    for (int i = 0; i < nu_objects; i++) {
        SceneObject *so = sceneobject[i];
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
    for (int i = 0; i < nu_models; i++) {
        if (model[i] == NULL)
            continue;
        if (model[i]->referenced && model_used[i] == 0) {
            printf("Model id %d is marked is referenced but not actually used -- marking as unreferenced.\n", i);
            model[i]->referenced = false;
        }
        else {
            if (!model[i]->referenced && model_used[i] != 0) {
                printf("Warning: Model id %d is not marked as referenced but is actually used -- "
                    "marking as referenced.\n", i);
                model[i]->referenced = true;
            }
        }
        for (int j = 0; j < model[i]->nu_lod_levels; j++) {
            sreLODModel *lm = model[i]->lod_model[j];
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
         nu_objects, scene_triangle_count, nu_models,
         nu_lod_models, gpu_triangle_count);
}

// RemoveUnreferencedModels() must be called before uploading models.

void sreScene::UploadModels() const {
    // Iterate all models.
    for (int i = 0; i < nu_models; i++) {
        if (model[i] == NULL)
            continue;
        bool shadow_volumes_configured = true;
        for (int j = 0; j < model[i]->nu_lod_levels; j++) {
            sreLODModel *m = model[i]->lod_model[j];
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
                    m->InitVertexBuffers(m->instance_flags, dynamic_flags);
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
            model[i]->flags |= SRE_MODEL_SHADOW_VOLUMES_CONFIGURED;
    }
}