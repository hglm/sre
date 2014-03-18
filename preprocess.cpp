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

// Preprocessing stage functions. Preprocessing adjusts some static objects to reduce the risk of
// rendering artifacts (especially for shadow volumes), creating a new seperate model for them.
// Preprocessing can only handle the objects with just one LOD level.

// Convert an instantation of a object to static scenery polygons with absolute coordinates.

sreModel *sreObject::ConvertToStaticScenery() const {
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
    new_m->polygon = new sreModelPolygon[new_m->nu_polygons];
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
    if (flags & (SRE_OBJECT_LIGHT_HALO | SRE_OBJECT_BILLBOARD | SRE_OBJECT_PARTICLE_SYSTEM)) {
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
    return new_m;
}

// EPSILON_DEFAULT is defined in sreVectorMath.h as 0.0001f.
#define EPSILON EPSILON_DEFAULT
// For bounding volume tests, define a larger epsilon.
#define EPSILON2 0.001

// Check whether two objects converted to static scenery intersect, with a small margin.

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
    if (lm->flags & SRE_TEXCOORDS_MASK) {
        // Append a new texcoord.
        Point2D *new_texcoords = new Point2D[lm->nu_vertices + 1];
        memcpy(new_texcoords, lm->texcoords, sizeof(Point2D) * lm->nu_vertices);
        // Interpolate the texcoords between P1 and P2.
        new_texcoords[lm->nu_vertices] = lm->texcoords[vindex1] * (1.0 - t) + lm->texcoords[vindex2] * t;
        delete [] lm->texcoords;
        lm->texcoords = new_texcoords;
    }
    if (lm->flags & SRE_COLOR_MASK) {
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

void sreScene::DetermineStaticIntersectingObjects(const sreFastOctree& fast_oct, int array_index,
int model_index, const sreBoundingVolumeAABB& AABB, int *static_object_belonging_to_object,
int &nu_intersecting_objects, int *intersecting_object) const {
    int node_index = fast_oct.array[array_index];
    if (!Intersects(AABB, fast_oct.node_bounds[node_index].AABB))
        return;
    int nu_octants = fast_oct.array[array_index + 1] & 0xFF;
    int nu_entities = fast_oct.array[array_index + 2];
    array_index += 3;
    for (int i = 0; i < nu_entities; i++) {
        sreSceneEntityType type;
        int index;
        fast_oct.GetEntity(array_index + i, type, index);
        if (type == SRE_ENTITY_OBJECT && static_object_belonging_to_object[index] != - 1) {
            if (ModelBoundsIntersectWithMargin(*model[model_index],
            *model[static_object_belonging_to_object[index]])) {
                intersecting_object[nu_intersecting_objects] =
                   static_object_belonging_to_object[index];
                nu_intersecting_objects++;
            }
        }
    }
    array_index += nu_entities;
    for (int i = 0; i < nu_octants; i++)
        DetermineStaticIntersectingObjects(fast_oct, fast_oct.array[array_index + i], model_index,
            AABB, static_object_belonging_to_object, nu_intersecting_objects, intersecting_object);
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
    int *object_belonging_to_object = new int[nu_objects + nu_models];
    int *static_object_belonging_to_object = new int[nu_objects];
    for (int i = 0; i < nu_objects; i++) {
        if (object[i]->model->is_static)
            printf("Unexpected scene object found with model already marked static"
                "before conversion to absolute coordinates (model id = %d).\n",
                 object[i]->model->id);
        if (!(object[i]->flags & (SRE_OBJECT_DYNAMIC_POSITION | SRE_OBJECT_INFINITE_DISTANCE |
        SRE_OBJECT_BILLBOARD | SRE_OBJECT_LIGHT_HALO | SRE_OBJECT_PARTICLE_SYSTEM |
        SRE_OBJECT_ANIMATED))) {
            sreModel *m = object[i]->ConvertToStaticScenery();
            RegisterModel(m);
            object_belonging_to_object[m->id] = i;
            static_object_belonging_to_object[i] = m->id;
            count++;
//            printf("New static model with id %d created for scene object %d.\n", m->id, i);
        }
        else
            static_object_belonging_to_object[i] = - 1;
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
                static_object_belonging_to_object, nu_intersecting_objects, intersecting_object);
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
                 int j = object_belonging_to_object[i];
                 object[j]->model = model[i];
                 object[j]->model_matrix.SetIdentity();
                 // Save the original rotation matrix.
                 object[j]->original_rotation_matrix = new Matrix3D;
                 *object[j]->original_rotation_matrix = object[j]->rotation_matrix;
                 object[j]->rotation_matrix.SetIdentity();
                 object[j]->inverted_model_matrix.SetIdentity();
                 object[j]->position = Point3D(0, 0, 0);
                 object[j]->scaling = 1.0;
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
    delete [] object_belonging_to_object;
    delete [] static_object_belonging_to_object;
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

static const float epsilon = 0.001F;

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

static int TriangulatePolygon(int vertexCount, const Point3D *vertex,
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
    m->triangle = new sreModelTriangle[triangle_count];
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
