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

#include "sre.h"
#include "sre_internal.h"


static void sreInitializeEllipsoidModel(sreLODModel *m, int LONGITUDE_SEGMENTS,
int LATITUDE_SEGMENTS, float radius_y, float radius_z) {
    int *grid_vertex = (int *)alloca(sizeof(int) * (LATITUDE_SEGMENTS + 1) * (LONGITUDE_SEGMENTS + 1));
    int row_size = LONGITUDE_SEGMENTS + 1;
    float radius = 1;
    int vertex_index = 0;
    m->nu_vertices = (LONGITUDE_SEGMENTS + 1) * (LATITUDE_SEGMENTS + 1);
    Point3DPadded *vertex = dstNewAligned <Point3DPadded>(m->nu_vertices, 16);
    m->texcoords = new Point2D[m->nu_vertices];
    for (int i = 0; i <= LONGITUDE_SEGMENTS; i++) {
        for (int j = - (LATITUDE_SEGMENTS / 2 ); j <= LATITUDE_SEGMENTS / 2; j++) {
            float latitude = (j + LATITUDE_SEGMENTS / 2) * (180 / (float)LATITUDE_SEGMENTS) * M_PI / 180;
            float longitude = i * (360 / (float)LONGITUDE_SEGMENTS) * M_PI / 180;
            if (i == LONGITUDE_SEGMENTS) // Make sure vertex is exactly the same for longitude 360 as for longitude 0
                longitude = 0;
            float x, y, z;
            if (j == LATITUDE_SEGMENTS / 2) {
                x = 0;
                y = 0;
                z = - radius_z;
            }
            else {
                x = radius * sinf(latitude) * cosf(longitude);
                y = radius_y * sinf(latitude) * sinf(longitude);
                z = radius_z * cosf(latitude);
            }
            if (x == -0)
                x = 0;
            if (y == -0)
                y = 0;
            if (z == -0)
                z = 0;
            vertex[vertex_index].Set(x, y, z);
            m->texcoords[vertex_index].Set((float)i / LONGITUDE_SEGMENTS, (float)(j +
                LATITUDE_SEGMENTS / 2) / LATITUDE_SEGMENTS);
            grid_vertex[(j + LATITUDE_SEGMENTS / 2) * row_size + i] = vertex_index;
            vertex_index++;
        }
    }
    m->nu_triangles = LONGITUDE_SEGMENTS * LATITUDE_SEGMENTS * 2;
    m->triangle = new sreModelTriangle[LONGITUDE_SEGMENTS * LATITUDE_SEGMENTS * 2];
    int triangle_index = 0;
    for (int i = 0; i < LONGITUDE_SEGMENTS; i++) {
        for (int j = 0; j < LATITUDE_SEGMENTS; j++) {
            m->triangle[triangle_index].AssignVertices(grid_vertex[j * row_size + i], grid_vertex[(j + 1) * row_size + i],
                grid_vertex[(j + 1) * row_size + i + 1]);
            triangle_index++;
            m->triangle[triangle_index].AssignVertices(grid_vertex[j * row_size + i], grid_vertex[(j + 1) * row_size + i + 1],
                grid_vertex[j * row_size + i + 1]);
            triangle_index++;
        }
    }
    m->SetPositions(vertex);
    m->SetAttributeFlags(SRE_POSITION_MASK | SRE_TEXCOORDS_MASK);
    m->RemoveEmptyTriangles();
    m->SortVertices(2); // Sort on z-coordinate.
    m->MergeIdenticalVertices();
    m->vertex_normal = new Vector3D[m->nu_vertices];
    m->CalculateNormals();
    m->CalculateTangentVectors();
}

sreModel *sreCreateEllipsoidModel(sreScene *scene, float radius_y, float radius_z) {
    sreModel *m = new sreModel;
    m->lod_model[0] = sreNewLODModel();
    sreInitializeEllipsoidModel(m->lod_model[0], 64, 32, radius_y, radius_z);
    m->lod_model[0]->cache_coherency_sorting_hint = 18;
    m->lod_model[1] = sreNewLODModel();
    sreInitializeEllipsoidModel(m->lod_model[1], 32, 16, radius_y, radius_z);
    m->lod_model[1]->cache_coherency_sorting_hint = 23;
    m->lod_model[2] = sreNewLODModel();
    sreInitializeEllipsoidModel(m->lod_model[2], 16, 8, radius_y, radius_z);
    m->lod_model[2]->cache_coherency_sorting_hint = 23;
    m->lod_model[3] = sreNewLODModel();
    sreInitializeEllipsoidModel(m->lod_model[3], 8, 4, radius_y, radius_z);
    m->lod_model[3]->cache_coherency_sorting_hint = 14;
    m->nu_lod_levels = 4;
    m->CalculateBounds();
    if (radius_y == 1.0 && radius_z == 1.0) {
        m->collision_shape_static = SRE_COLLISION_SHAPE_SPHERE;
        m->collision_shape_dynamic = SRE_COLLISION_SHAPE_SPHERE;
    }
    else {
        m->collision_shape_static = SRE_COLLISION_SHAPE_ELLIPSOID;
        m->collision_shape_dynamic = SRE_COLLISION_SHAPE_ELLIPSOID;
    }
    scene->RegisterModel(m);
    return m;
}

sreModel *sreCreateSphereModel(sreScene *scene, float oblateness) {
    sreModel *m = new sreModel;
    m->lod_model[0] = sreNewLODModel();
    sreInitializeEllipsoidModel(m->lod_model[0], 64, 32, 1.0f, 1.0f - oblateness);
    m->lod_model[0]->cache_coherency_sorting_hint = 18;
    m->lod_model[1] = sreNewLODModel();
    sreInitializeEllipsoidModel(m->lod_model[1], 32, 16, 1.0f, 1.0f - oblateness);
    m->lod_model[1]->cache_coherency_sorting_hint = 23;
    m->lod_model[2] = sreNewLODModel();
    sreInitializeEllipsoidModel(m->lod_model[2], 16, 8, 1.0f, 1.0f - oblateness);
    m->lod_model[2]->cache_coherency_sorting_hint = 23;
    m->lod_model[3] = sreNewLODModel();
    sreInitializeEllipsoidModel(m->lod_model[3], 8, 4, 1.0f, 1.0f - oblateness);
    m->lod_model[3]->cache_coherency_sorting_hint = 14;
    m->nu_lod_levels = 4;
    m->CalculateBounds();
    if (oblateness == 0) {
        m->collision_shape_static = SRE_COLLISION_SHAPE_SPHERE;
        m->collision_shape_dynamic = SRE_COLLISION_SHAPE_SPHERE;
    }
    else {
        m->collision_shape_static = SRE_COLLISION_SHAPE_ELLIPSOID;
        m->collision_shape_dynamic = SRE_COLLISION_SHAPE_ELLIPSOID;
    }
    scene->RegisterModel(m);
    return m;
}

sreModel *sreCreateSphereModelSimple(sreScene *scene, float oblateness) {
    sreModel *m = new sreModel;
    m->lod_model[0] = sreNewLODModel();
    sreInitializeEllipsoidModel(m->lod_model[0], 16, 8, 1.0f, 1.0f - oblateness);
    m->nu_lod_levels = 1;
    m->CalculateBounds();
    if (oblateness == 0) {
        m->collision_shape_static = SRE_COLLISION_SHAPE_SPHERE;
        m->collision_shape_dynamic = SRE_COLLISION_SHAPE_SPHERE;
    }
    else {
        m->collision_shape_static = SRE_COLLISION_SHAPE_ELLIPSOID;
        m->collision_shape_dynamic = SRE_COLLISION_SHAPE_ELLIPSOID;
    }
    scene->RegisterModel(m);
    return m;
}

sreModel *sreCreateBillboardModel(sreScene *scene, bool is_halo) {
    sreModel *m = new sreModel;
    sreLODModel *lm = m->lod_model[0] = sreNewLODModelNoShadowVolume();
    m->nu_lod_levels = 1;
    lm->nu_vertices = 4;
    lm->nu_triangles = 0;  // Indicate that no triangle indices are allocated.
    // A single billboard does not have any indices (triangle data).
    // The model is a triangle fan consisting of two triangles.
    Point3DPadded *vertex = dstNewAligned <Point3DPadded>(4, 16);
    lm->SetPositions(vertex);
    lm->SetAttributeFlags(SRE_POSITION_MASK);
    lm->flags |= SRE_LOD_MODEL_NO_SHADOW_VOLUME_SUPPORT |
        SRE_LOD_MODEL_VERTEX_BUFFER_DYNAMIC | SRE_LOD_MODEL_BILLBOARD;
    m->model_flags |= SRE_MODEL_BILLBOARD;
    if (is_halo) {
        lm->flags |= SRE_LOD_MODEL_LIGHT_HALO;
        lm->vertex_normal = new Vector3D[4];
        m->model_flags |= SRE_MODEL_LIGHT_HALO;
    }
    scene->RegisterModel(m);
    return m;
}

sreModel *sreCreateParticleSystemModel(sreScene *scene, int n, bool is_halo) {
    sreModel *m = new sreModel;
    sreLODModel *lm = m->lod_model[0] = sreNewLODModelNoShadowVolume();
    m->nu_lod_levels = 1;
    lm->triangle = new sreModelTriangle[2 * n];
    lm->nu_vertices = 4 * n;
    lm->nu_triangles = 2 * n;
    // Assign the triangles.
    for (int i = 0; i < n; i++) {
       lm->triangle[i * 2].AssignVertices(i * 4, i * 4 + 1, i * 4 + 2);
       lm->triangle[i * 2 + 1].AssignVertices(i * 4 + 2, i * 4 + 3, i * 4);
    }
    Point3DPadded *vertex = dstNewAligned <Point3DPadded>(4 * n, 16);
    lm->SetPositions(vertex);
    // Note: normals buffer used as centers of each billboard.
    lm->SetAttributeFlags(SRE_POSITION_MASK | SRE_NORMAL_MASK);
    lm->flags |= SRE_LOD_MODEL_NO_SHADOW_VOLUME_SUPPORT |
        SRE_LOD_MODEL_VERTEX_BUFFER_DYNAMIC | SRE_LOD_MODEL_BILLBOARD;
    m->model_flags |= SRE_MODEL_BILLBOARD | SRE_MODEL_PARTICLE_SYSTEM;
    if (is_halo) {
        lm->flags |= SRE_LOD_MODEL_LIGHT_HALO;
        lm->vertex_normal = new Vector3D[lm->nu_vertices];
        m->model_flags |= SRE_MODEL_LIGHT_HALO;
    }
    m->bounds_flags = SRE_BOUNDS_PREFER_SPHERE;
    scene->RegisterModel(m);
    return m;
}

sreModel *sreCreateUnitBlockModel(sreScene *scene) {
    sreModel *m = new sreModel;
    sreLODModel *lm = m->lod_model[0] = sreNewLODModel();
    m->nu_lod_levels = 1;
    lm->nu_triangles = 12;
    lm->nu_vertices = 24;
    Point3DPadded *vertex = dstNewAligned <Point3DPadded>(lm->nu_vertices, 16);
    lm->triangle = new sreModelTriangle[lm->nu_triangles];
    lm->texcoords = new Point2D[lm->nu_vertices];
    // We have to define the vertices for each face seperately to allow different normals at the same
    // vertex position.
    // Bottom face.
    vertex[0].Set(0, 0, 0);
    vertex[1].Set(1, 0, 0);
    vertex[2].Set(1, 1, 0);
    vertex[3].Set(0, 1, 0);
    lm->texcoords[0].Set(0, 1);
    lm->texcoords[1].Set(1, 1);
    lm->texcoords[2].Set(1, 0);
    lm->texcoords[3].Set(0, 0);
    lm->triangle[0].AssignVertices(3, 2, 1);
    lm->triangle[1].AssignVertices(3, 1, 0);
    // Top face.
    vertex[4].Set(0, 0, 1);
    vertex[5].Set(1, 0, 1);
    vertex[6].Set(1, 1, 1);
    vertex[7].Set(0, 1, 1);
    lm->texcoords[4].Set(0, 0);
    lm->texcoords[5].Set(1, 0);
    lm->texcoords[6].Set(1, 1);
    lm->texcoords[7].Set(0, 1);
    lm->triangle[2].AssignVertices(4, 5, 6);
    lm->triangle[3].AssignVertices(4, 6, 7);
    // Front face.
    vertex[8].Set(0, 0, 0);
    vertex[9].Set(1, 0, 0);
    vertex[10].Set(1, 0, 1);
    vertex[11].Set(0, 0, 1);
    lm->texcoords[8].Set(0, 0);
    lm->texcoords[9].Set(1, 0);
    lm->texcoords[10].Set(1, 1);
    lm->texcoords[11].Set(0, 1);
    lm->triangle[4].AssignVertices(8, 9, 10);
    lm->triangle[5].AssignVertices(8, 10, 11);
    // Back face.
    vertex[12].Set(0, 1, 0);
    vertex[13].Set(1, 1, 0);
    vertex[14].Set(1, 1, 1);
    vertex[15].Set(0, 1, 1);
    lm->texcoords[12].Set(1, 0);
    lm->texcoords[13].Set(0, 0);
    lm->texcoords[14].Set(0, 1);
    lm->texcoords[15].Set(1, 1);
    lm->triangle[6].AssignVertices(13, 12, 15);
    lm->triangle[7].AssignVertices(13, 15, 14);
    // Left face.
    vertex[16].Set(0, 1, 0);
    vertex[17].Set(0, 0, 0);
    vertex[18].Set(0, 0, 1);
    vertex[19].Set(0, 1, 1);
    lm->texcoords[16].Set(0, 0);
    lm->texcoords[17].Set(1, 0);
    lm->texcoords[18].Set(1, 1);
    lm->texcoords[19].Set(0, 1);
    lm->triangle[8].AssignVertices(16, 17, 18);
    lm->triangle[9].AssignVertices(16, 18, 19);
    // Right face.
    vertex[20].Set(1, 0, 0);
    vertex[21].Set(1, 1, 0);
    vertex[22].Set(1, 1, 1);
    vertex[23].Set(1, 0, 1);
    lm->texcoords[20].Set(0, 0);
    lm->texcoords[21].Set(1, 0);
    lm->texcoords[22].Set(1, 1);
    lm->texcoords[23].Set(0, 1);
    lm->triangle[10].AssignVertices(20, 21, 22);
    lm->triangle[11].AssignVertices(20, 22, 23);
    lm->SetPositions(vertex);
    lm->SetAttributeFlags(SRE_POSITION_MASK | SRE_TEXCOORDS_MASK);
    lm->SortVertices(0); // Sort on x-coordinate.
//    m->MergeIdenticalVertices();
    lm->vertex_normal = new Vector3D[lm->nu_vertices];
    lm->CalculateNormalsNotSmooth();
    lm->CalculateTangentVectors();
    m->CalculateBounds();
    m->collision_shape_static = SRE_COLLISION_SHAPE_BOX; // Should be BOX for efficiency.
    m->collision_shape_dynamic = SRE_COLLISION_SHAPE_BOX;
    scene->RegisterModel(m);
    return m;
}

static void AddRamp(sreBaseModel *m, float x, float y, float xdim, float ydim, float zdim, int type) {
    // We have to define the vertices for each face seperately to allow different normals at the same
    // vertex position.
    // Bottom face.
    m->vertex[m->nu_vertices + 0].Set(x, y, 0);
    m->vertex[m->nu_vertices + 1].Set(x + xdim, y, 0);
    m->vertex[m->nu_vertices + 2].Set(x + xdim, y + ydim, 0);
    m->vertex[m->nu_vertices + 3].Set(x, y + ydim, 0);
    m->texcoords[m->nu_vertices + 0].Set(0, 1);
    m->texcoords[m->nu_vertices + 1].Set(1, 1);
    m->texcoords[m->nu_vertices + 2].Set(1, 0);
    m->texcoords[m->nu_vertices + 3].Set(0, 0);
    m->triangle[m->nu_triangles + 0].AssignVertices(m->nu_vertices + 3, m->nu_vertices + 2, m->nu_vertices + 1);
    m->triangle[m->nu_triangles + 1].AssignVertices(m->nu_vertices + 3, m->nu_vertices + 1, m->nu_vertices + 0);
    // Top face.
    if (type == RAMP_TOWARDS_BACK) {
       m->vertex[m->nu_vertices + 4].Set(x, y, 0);
       m->vertex[m->nu_vertices + 5].Set(x + xdim, y, 0);
       m->vertex[m->nu_vertices + 6].Set(x + xdim, y + ydim, zdim);
       m->vertex[m->nu_vertices + 7].Set(x, y + ydim, zdim);
    }
    if (type == RAMP_TOWARDS_LEFT) {
       m->vertex[m->nu_vertices + 4].Set(x, y, zdim);
       m->vertex[m->nu_vertices + 5].Set(x + xdim, y, 0);
       m->vertex[m->nu_vertices + 6].Set(x + xdim, y + ydim, 0);
       m->vertex[m->nu_vertices + 7].Set(x, y + ydim, zdim);
    }
    if (type == RAMP_TOWARDS_RIGHT) {
       m->vertex[m->nu_vertices + 4].Set(x, y, 0);
       m->vertex[m->nu_vertices + 5].Set(x + xdim, y, zdim);
       m->vertex[m->nu_vertices + 6].Set(x + xdim, y + ydim, zdim);
       m->vertex[m->nu_vertices + 7].Set(x, y + ydim, 0);
    }
    if (type == RAMP_TOWARDS_FRONT) {
       m->vertex[m->nu_vertices + 4].Set(x, y, zdim);
       m->vertex[m->nu_vertices + 5].Set(x + xdim, y, zdim);
       m->vertex[m->nu_vertices + 6].Set(x + xdim, y + ydim, 0);
       m->vertex[m->nu_vertices + 7].Set(x, y + ydim, 0);
    }
    m->texcoords[m->nu_vertices + 4].Set(0, 0);
    m->texcoords[m->nu_vertices + 5].Set(1, 0);
    m->texcoords[m->nu_vertices + 6].Set(1, 1);
    m->texcoords[m->nu_vertices + 7].Set(0, 1);
    m->triangle[m->nu_triangles + 2].AssignVertices(m->nu_vertices + 4, m->nu_vertices + 5, m->nu_vertices + 6);
    m->triangle[m->nu_triangles + 3].AssignVertices(m->nu_vertices + 4, m->nu_vertices + 6, m->nu_vertices + 7);
    m->nu_vertices += 8;
    m->nu_triangles += 4;
    // Front face.
    if (type == RAMP_TOWARDS_FRONT) {
        m->vertex[m->nu_vertices].Set(x, y, 0);
        m->vertex[m->nu_vertices + 1].Set(x + xdim, y, 0);
        m->vertex[m->nu_vertices + 2].Set(x + xdim, y, zdim);
        m->vertex[m->nu_vertices + 3].Set(x, y, zdim);
        m->texcoords[m->nu_vertices].Set(0, 0);
        m->texcoords[m->nu_vertices + 1].Set(1, 0);
        m->texcoords[m->nu_vertices + 2].Set(1, 1);
        m->texcoords[m->nu_vertices + 3].Set(0, 1);
        m->triangle[m->nu_triangles].AssignVertices(m->nu_vertices, m->nu_vertices + 1, m->nu_vertices + 2);
        m->triangle[m->nu_triangles + 1].AssignVertices(m->nu_vertices, m->nu_vertices + 2, m->nu_vertices + 3);
        m->nu_vertices += 4;
        m->nu_triangles += 2;
    }
    if (type == RAMP_TOWARDS_LEFT) {
        m->vertex[m->nu_vertices].Set(x, y, 0);
        m->vertex[m->nu_vertices + 1].Set(x + xdim, y, 0);
        m->vertex[m->nu_vertices + 2].Set(x, y, zdim);
        m->texcoords[m->nu_vertices].Set(0, 0);
        m->texcoords[m->nu_vertices + 1].Set(1, 0);
        m->texcoords[m->nu_vertices + 2].Set(0, 1);
        m->triangle[m->nu_triangles].AssignVertices(m->nu_vertices, m->nu_vertices + 1, m->nu_vertices + 2);
        m->nu_vertices += 3;
        m->nu_triangles ++;
    }
    if (type == RAMP_TOWARDS_RIGHT) {
        m->vertex[m->nu_vertices].Set(x, y, 0);
        m->vertex[m->nu_vertices + 1].Set(x + xdim, y, 0);
        m->vertex[m->nu_vertices + 2].Set(x + xdim, y, zdim);
        m->texcoords[m->nu_vertices].Set(0, 0);
        m->texcoords[m->nu_vertices + 1].Set(1, 0);
        m->texcoords[m->nu_vertices + 2].Set(0, 1);
        m->triangle[m->nu_triangles].AssignVertices(m->nu_vertices, m->nu_vertices + 1, m->nu_vertices + 2);
        m->nu_vertices += 3;
        m->nu_triangles ++;
    }
    // Back face.
    if (type == RAMP_TOWARDS_BACK) {
        m->vertex[m->nu_vertices].Set(x, y + ydim, 0);
        m->vertex[m->nu_vertices + 1].Set(x + xdim, y + ydim, 0);
        m->vertex[m->nu_vertices + 2].Set(x + xdim, y + ydim, zdim);
        m->vertex[m->nu_vertices + 3].Set(x, y + ydim, zdim);
        m->texcoords[m->nu_vertices].Set(1, 0);
        m->texcoords[m->nu_vertices + 1].Set(0, 0);
        m->texcoords[m->nu_vertices + 2].Set(0, 1);
        m->texcoords[m->nu_vertices + 3].Set(1, 1);
        m->triangle[m->nu_triangles].AssignVertices(m->nu_vertices + 1, m->nu_vertices, m->nu_vertices + 3);
        m->triangle[m->nu_triangles + 1].AssignVertices(m->nu_vertices + 1, m->nu_vertices + 3, m->nu_vertices + 2);
        m->nu_vertices += 4;
        m->nu_triangles += 2;
    }
    if (type == RAMP_TOWARDS_LEFT) {
        m->vertex[m->nu_vertices].Set(x, y + ydim, 0);
        m->vertex[m->nu_vertices + 1].Set(x + xdim, y + ydim, 0);
        m->vertex[m->nu_vertices + 2].Set(x, y + ydim, zdim);
        m->texcoords[m->nu_vertices].Set(1, 0);
        m->texcoords[m->nu_vertices + 1].Set(0, 0);
        m->texcoords[m->nu_vertices + 2].Set(0, 1);
        m->triangle[m->nu_triangles].AssignVertices(m->nu_vertices + 2, m->nu_vertices + 1, m->nu_vertices);
        m->nu_vertices += 3;
        m->nu_triangles++;
    }
    if (type == RAMP_TOWARDS_RIGHT) {
        m->vertex[m->nu_vertices].Set(x + xdim, y + ydim, 0);
        m->vertex[m->nu_vertices + 1].Set(x, y + ydim, 0);
        m->vertex[m->nu_vertices + 2].Set(x + xdim, y + ydim, zdim);
        m->texcoords[m->nu_vertices].Set(1, 0);
        m->texcoords[m->nu_vertices + 1].Set(0, 0);
        m->texcoords[m->nu_vertices + 2].Set(0, 1);
        m->triangle[m->nu_triangles].AssignVertices(m->nu_vertices, m->nu_vertices + 1, m->nu_vertices + 2);
        m->nu_vertices += 3;
        m->nu_triangles++;
    }
    // Left face.
    if (type == RAMP_TOWARDS_BACK) {
        m->vertex[m->nu_vertices].Set(x, y + ydim, 0);
        m->vertex[m->nu_vertices + 1].Set(x, y, 0);
        m->vertex[m->nu_vertices + 2].Set(x, y + ydim, zdim);
        m->texcoords[m->nu_vertices] .Set(0, 0);
        m->texcoords[m->nu_vertices + 1].Set(1, 0);
        m->texcoords[m->nu_vertices + 2].Set(1, 1);
        m->triangle[m->nu_triangles].AssignVertices(m->nu_vertices, m->nu_vertices + 1, m->nu_vertices + 2);
        m->nu_vertices += 3;
        m->nu_triangles++;
    }
    if (type == RAMP_TOWARDS_LEFT) {
        m->vertex[m->nu_vertices].Set(x, y + ydim, 0);
        m->vertex[m->nu_vertices + 1].Set(x, y, 0);
        m->vertex[m->nu_vertices + 2].Set(x, y, zdim);
        m->vertex[m->nu_vertices + 3].Set(x, y + ydim, zdim);
        m->texcoords[m->nu_vertices] .Set(0, 0);
        m->texcoords[m->nu_vertices + 1].Set(1, 0);
        m->texcoords[m->nu_vertices + 2].Set(1, 1);
        m->texcoords[m->nu_vertices + 3].Set(0, 1);
        m->triangle[m->nu_triangles].AssignVertices(m->nu_vertices, m->nu_vertices + 1, m->nu_vertices + 2);
        m->triangle[m->nu_triangles + 1].AssignVertices(m->nu_vertices, m->nu_vertices + 2, m->nu_vertices + 3);
        m->nu_vertices += 4;
        m->nu_triangles += 2;
    }
    if (type == RAMP_TOWARDS_FRONT) {
        m->vertex[m->nu_vertices].Set(x, y + ydim, 0);
        m->vertex[m->nu_vertices + 1].Set(x, y, 0);
        m->vertex[m->nu_vertices + 2].Set(x, y, zdim);
        m->texcoords[m->nu_vertices] .Set(0, 0);
        m->texcoords[m->nu_vertices + 1].Set(1, 0);
        m->texcoords[m->nu_vertices + 2].Set(1, 1);
        m->triangle[m->nu_triangles].AssignVertices(m->nu_vertices, m->nu_vertices + 1, m->nu_vertices + 2);
        m->nu_vertices += 3;
        m->nu_triangles++;
    }
    // Right face.
    if (type == RAMP_TOWARDS_BACK) {
        m->vertex[m->nu_vertices].Set(x + xdim, y, 0);
        m->vertex[m->nu_vertices + 1].Set(x + xdim, y + ydim, 0);
        m->vertex[m->nu_vertices + 2].Set(x + xdim, y + ydim, zdim);
        m->texcoords[m->nu_vertices].Set(0, 0);
        m->texcoords[m->nu_vertices + 1].Set(1, 0);
        m->texcoords[m->nu_vertices + 2].Set(1, 1);
        m->triangle[m->nu_triangles].AssignVertices(m->nu_vertices, m->nu_vertices + 1, m->nu_vertices + 2);
        m->nu_vertices += 3;
        m->nu_triangles++;
    }
    if (type == RAMP_TOWARDS_RIGHT) {
        m->vertex[m->nu_vertices].Set(x + xdim, y, 0);
        m->vertex[m->nu_vertices + 1].Set(x + xdim, y + ydim, 0);
        m->vertex[m->nu_vertices + 2].Set(x + xdim, y + ydim, zdim);
        m->vertex[m->nu_vertices + 3].Set(x + xdim, y, zdim);
        m->texcoords[m->nu_vertices] .Set(0, 0);
        m->texcoords[m->nu_vertices + 1].Set(1, 0);
        m->texcoords[m->nu_vertices + 2].Set(1, 1);
        m->texcoords[m->nu_vertices + 3].Set(0, 1);
        m->triangle[m->nu_triangles].AssignVertices(m->nu_vertices, m->nu_vertices + 1, m->nu_vertices + 2);
        m->triangle[m->nu_triangles + 1].AssignVertices(m->nu_vertices, m->nu_vertices + 2, m->nu_vertices + 3);
        m->nu_vertices += 4;
        m->nu_triangles += 2;
    }
    if (type == RAMP_TOWARDS_FRONT) {
        m->vertex[m->nu_vertices].Set(x + xdim, y + ydim, 0);
        m->vertex[m->nu_vertices + 1].Set(x + xdim, y, zdim);
        m->vertex[m->nu_vertices + 2].Set(x + xdim, y, 0);
        m->texcoords[m->nu_vertices].Set(0, 0);
        m->texcoords[m->nu_vertices + 1].Set(1, 0);
        m->texcoords[m->nu_vertices + 2].Set(1, 1);
        m->triangle[m->nu_triangles].AssignVertices(m->nu_vertices, m->nu_vertices + 1, m->nu_vertices + 2);
        m->nu_vertices += 3;
        m->nu_triangles++;
    }

}

sreModel *sreCreateRampModel(sreScene *scene, float xdim, float ydim, float zdim, int type) {
    int max_triangles = 8;
    int max_vertices = 18;
    sreModel *m = new sreModel;
    sreLODModel *lm = m->lod_model[0] = sreNewLODModel();
    m->nu_lod_levels = 1;
    lm->position = dstNewAligned <Point3DPadded>(max_vertices, 16);
    lm->triangle = new sreModelTriangle[max_triangles];
    lm->texcoords = new Point2D[max_vertices];
    lm->nu_vertices = 0;
    lm->nu_triangles = 0;
    AddRamp(lm, 0, 0, xdim, ydim, zdim, type);
    lm->SetPositions(lm->position);
    lm->SetAttributeFlags(SRE_POSITION_MASK | SRE_TEXCOORDS_MASK);
    lm->SortVertices(0); // Sort on x-coordinate.
//    m->MergeIdenticalVertices();
    lm->vertex_normal = new Vector3D[lm->nu_vertices];
    lm->CalculateNormalsNotSmooth();
    lm->CalculateTangentVectors();
    m->CalculateBounds();
    m->collision_shape_static = SRE_COLLISION_SHAPE_STATIC;
    m->collision_shape_dynamic = SRE_COLLISION_SHAPE_CONVEX_HULL;
    scene->RegisterModel(m);
    return m;
}

#define RINGS_LONGITUDE_SEGMENTS 256
#define RINGS_RADIAL_SEGMENTS 16

sreModel *sreCreateRingsModel(sreScene *scene, float min_radius, float max_radius) {
    int grid_vertex[RINGS_RADIAL_SEGMENTS + 1][RINGS_LONGITUDE_SEGMENTS + 1];
    int vertex_index = 0;
    sreModel *model = new sreModel;
    sreLODModel *m = model->lod_model[0] = sreNewLODModel();
    model->nu_lod_levels = 1;
    m->nu_vertices = (RINGS_LONGITUDE_SEGMENTS + 1) * (RINGS_RADIAL_SEGMENTS + 1);
    Point3DPadded *vertex = dstNewAligned <Point3DPadded>(m->nu_vertices, 16);
    m->texcoords = new Point2D[m->nu_vertices];
    for (int i = 0; i <= RINGS_LONGITUDE_SEGMENTS; i++) {
        for (int j = 0; j <= RINGS_RADIAL_SEGMENTS; j++) {
            float radial_distance = min_radius + (max_radius - min_radius) * j / RINGS_RADIAL_SEGMENTS;
            float longitude = i * (360 / (float)RINGS_LONGITUDE_SEGMENTS) * M_PI / 180;
            if (i == RINGS_LONGITUDE_SEGMENTS)
                // Make sure vertex is exactly the same for longitude 360 as for longitude 0
                longitude = 0;
            float x, y, z;
            x = radial_distance * cosf(longitude);
            y = radial_distance * sinf(longitude);
            z = 0;
            if (x == -0)
                x = 0;
            if (y == -0)
                y = 0;
            vertex[vertex_index].Set(x, y, z);
            m->texcoords[vertex_index].Set((float)j / RINGS_RADIAL_SEGMENTS, 0);
            grid_vertex[j][i] = vertex_index;
            vertex_index++;
        }
    }
    m->nu_triangles = RINGS_LONGITUDE_SEGMENTS * RINGS_RADIAL_SEGMENTS * 2;
    m->triangle = new sreModelTriangle[RINGS_LONGITUDE_SEGMENTS * RINGS_RADIAL_SEGMENTS * 2];
    int triangle_index = 0;
    for (int i = 0; i < RINGS_LONGITUDE_SEGMENTS; i++) {
        for (int j = 0; j < RINGS_RADIAL_SEGMENTS; j++) {
            m->triangle[triangle_index].AssignVertices(grid_vertex[j][i], grid_vertex[j + 1][i],
                grid_vertex[j + 1][i + 1]);
            triangle_index++;
            m->triangle[triangle_index].AssignVertices(grid_vertex[j][i], grid_vertex[j + 1][i + 1],
                grid_vertex[j][i + 1]);
           triangle_index++;
        }
    }
    m->SetPositions(vertex);
    m->SetAttributeFlags(SRE_POSITION_MASK | SRE_TEXCOORDS_MASK);
    m->RemoveEmptyTriangles(); // Added
    m->SortVertices(0); // Sort on x-coordinate.
    m->MergeIdenticalVertices();
    m->vertex_normal = new Vector3D[m->nu_vertices];
    m->CalculateNormals();
    model->CalculateBounds();
    model->collision_shape_static = SRE_COLLISION_SHAPE_STATIC;
    model->collision_shape_dynamic = SRE_COLLISION_SHAPE_CONVEX_HULL;
    scene->RegisterModel(model);
    return model;
}

// Create a checkerboard plane with size rectangles of unit_size using the given colors.

sreModel *sreCreateCheckerboardModel(sreScene *scene, int size, float unit_size,
Color color1, Color color2) {
    Point3D *mesh = new Point3D[(size + 1) * (size + 1)];
    for (int y = 0; y <= size; y++)
        for (int x = 0; x <= size; x++)
            mesh[y * (size + 1) + x].Set(x * unit_size, y * unit_size, 0);
    sreModel *model = new sreModel;
    sreLODModel *m = model->lod_model[0] = sreNewLODModel();
    model->nu_lod_levels = 1;
    m->nu_triangles = size * size * 2;
    m->nu_vertices = m->nu_triangles * 3;
    Point3DPadded *vertex = dstNewAligned <Point3DPadded>(m->nu_vertices, 16);
    m->colors = new Color[m->nu_vertices];
    m->triangle = new sreModelTriangle[m->nu_triangles];
    int i = 0;
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            sreModelTriangle *t1 = &m->triangle[(y * size + x) * 2];
            sreModelTriangle *t2 = &m->triangle[(y * size + x) * 2 + 1];
            Color color;
            if (((x + y) & 1) == 0)
                color = color1;
            else
                color = color2;
            for (int j = 0; j < 6; j++)
                 m->colors[i + j] = color;
            vertex[i] = mesh[y * (size + 1) + x];
            vertex[i + 1] = mesh[y * (size + 1) + x + 1];
            vertex[i + 2] = mesh[(y + 1) * (size + 1) + x];
            t1->AssignVertices(i, i + 1, i + 2);
            i += 3;
            vertex[i] = mesh[y * (size + 1) + x + 1];
            vertex[i + 1] = mesh[(y + 1) * (size + 1) + x + 1];
            vertex[i + 2] = mesh[(y + 1) * (size + 1) + x];
            t2->AssignVertices(i, i + 1, i + 2);
            i += 3;
        }
    delete [] mesh;
    m->SetPositions(vertex);
    m->SetAttributeFlags(SRE_POSITION_MASK | SRE_COLOR_MASK);
    m->flags |= SRE_LOD_MODEL_NOT_CLOSED | SRE_LOD_MODEL_NO_SHADOW_VOLUME_SUPPORT |
        SRE_LOD_MODEL_SINGLE_PLANE;
    m->SortVertices(0); // Sort on x-coordinate.
    m->cache_coherency_sorting_hint = 19;
    m->MergeIdenticalVertices();
    m->vertex_normal = new Vector3D[m->nu_vertices];
    m->CalculateNormals();
    model->CalculateBounds();
    model->collision_shape_static = SRE_COLLISION_SHAPE_BOX;
    model->collision_shape_dynamic = SRE_COLLISION_SHAPE_BOX;
    scene->RegisterModel(model);
    return model;
}

static float CalculateTextureX(int l, int m, bool right_or_bottom, int TORUS_LONGITUDE_SEGMENTS_PER_TEXTURE) {
    float x = (float)l / TORUS_LONGITUDE_SEGMENTS_PER_TEXTURE;
    float x_mod = fmod(x, 1.0f);
    if (right_or_bottom && x_mod < 0.0001)
        return 1.0;
    return x_mod;    
}

static float CalculateTextureY(int l, int m, bool right_or_bottom, int TORUS_LATITUDE_SEGMENTS_PER_TEXTURE) {
    float y = (float)m / TORUS_LATITUDE_SEGMENTS_PER_TEXTURE;
    float y_mod = fmod(y, 1.0f);
    if (right_or_bottom && y_mod < 0.0001)
        return 1.0;
    return y_mod;    
}

static void sreInitializeTorusModel(sreBaseModel *m, int TORUS_LONGITUDE_SEGMENTS,
int TORUS_LATITUDE_SEGMENTS, int TORUS_LONGITUDE_SEGMENTS_PER_TEXTURE, int TORUS_LATITUDE_SEGMENTS_PER_TEXTURE) {
    // Calculate the grid of vertices.
    Point3D *grid_vertex = new Point3D[TORUS_LONGITUDE_SEGMENTS * TORUS_LATITUDE_SEGMENTS];
    for (int l = 0; l < TORUS_LONGITUDE_SEGMENTS; l++) {
        float x = TORUS_RADIUS * cosf(l * 2 * M_PI / TORUS_LONGITUDE_SEGMENTS);
        float y = TORUS_RADIUS * sinf(l * 2 * M_PI / TORUS_LONGITUDE_SEGMENTS);
        for (int m = 0; m < TORUS_LATITUDE_SEGMENTS; m++) {
            // Calculate the offset from the centerpoint on the ring
            float x2 = TORUS_RADIUS2 * cosf(m * 2 * M_PI / TORUS_LATITUDE_SEGMENTS);
            float y2 = 0;
            float z2 = TORUS_RADIUS2 * sinf(m * 2 * M_PI / TORUS_LATITUDE_SEGMENTS);
            // Rotate (x2, y2, z2) along z-axis by l * 2 * M_PI / TORUS_LONGITUDE_SEGMENTS degrees
            float x3 = x2 * cosf(l * 2 * M_PI / TORUS_LONGITUDE_SEGMENTS) -
                + y2 * sinf(l * 2 * M_PI / TORUS_LONGITUDE_SEGMENTS);
            float y3 = x2 * sinf(l * 2 * M_PI / TORUS_LONGITUDE_SEGMENTS) +
                y2 * cosf(l * 2 * M_PI / TORUS_LONGITUDE_SEGMENTS);
            grid_vertex[l * TORUS_LATITUDE_SEGMENTS + m].Set(x + x3, y + y3, z2);
        }
    }
    m->nu_vertices = TORUS_LONGITUDE_SEGMENTS * TORUS_LATITUDE_SEGMENTS * 4;
    Point3DPadded *vertex = dstNewAligned <Point3DPadded>(m->nu_vertices, 16);
    m->texcoords = new Point2D[m->nu_vertices];
    m->nu_triangles = TORUS_LONGITUDE_SEGMENTS * TORUS_LATITUDE_SEGMENTS * 2;
    m->triangle = new sreModelTriangle[m->nu_triangles];
    int v = 0;
    int tlongpt = TORUS_LONGITUDE_SEGMENTS_PER_TEXTURE;
    int tlatpt = TORUS_LATITUDE_SEGMENTS_PER_TEXTURE;
    for (int l = 0; l < TORUS_LONGITUDE_SEGMENTS; l++) {
        for (int k = 0; k < TORUS_LATITUDE_SEGMENTS; k++) {
            // Set the vertices of this segment quad
            vertex[v] = grid_vertex[l * TORUS_LATITUDE_SEGMENTS + k];
            vertex[v + 1] = grid_vertex[((l + 1) % TORUS_LONGITUDE_SEGMENTS) * TORUS_LATITUDE_SEGMENTS + k];
            vertex[v + 2] = grid_vertex[((l + 1) % TORUS_LONGITUDE_SEGMENTS) * TORUS_LATITUDE_SEGMENTS +
                (k + 1) % TORUS_LATITUDE_SEGMENTS];
            vertex[v + 3] = grid_vertex[l * TORUS_LATITUDE_SEGMENTS + (k + 1) % TORUS_LATITUDE_SEGMENTS];
            // Set the texcoords, dependent on whether the vertex is left/top or right/bottom within the quad.
            m->texcoords[v].Set(CalculateTextureX(l, k, false, tlongpt), CalculateTextureY(l, k, false, tlatpt));
            m->texcoords[v + 1].Set(CalculateTextureX(l + 1, k, true, tlongpt), CalculateTextureY(l + 1, k, false, tlatpt));
            m->texcoords[v + 2].Set(CalculateTextureX(l + 1, k + 1, true, tlongpt),
                CalculateTextureY(l + 1, k + 1, true, tlatpt));
            m->texcoords[v + 3].Set(CalculateTextureX(l, k + 1, false, tlongpt), CalculateTextureY(l, k + 1, true, tlatpt));
            int i = (l * TORUS_LATITUDE_SEGMENTS + k) * 2;
            m->triangle[i].AssignVertices(v, v + 1, v + 3);
            m->triangle[i + 1].AssignVertices(v + 1, v + 2, v + 3);
            v += 4;
        }
    }
    delete [] grid_vertex;
    m->SetPositions(vertex);
    m->SetAttributeFlags(SRE_POSITION_MASK | SRE_TEXCOORDS_MASK);
    m->flags |= SRE_LOD_MODEL_CONTAINS_HOLES;
    m->SortVertices(0); // Sort on x-coordinate.
    m->MergeIdenticalVertices();
    m->vertex_normal = new Vector3D[m->nu_vertices];
    m->CalculateNormals();
    m->CalculateTangentVectors();
}

sreModel *sreCreateTorusModel(sreScene *scene) {
    sreModel *m = new sreModel;
#if 1
    m->lod_model[0] = sreNewLODModel();
    sreInitializeTorusModel(m->lod_model[0], 64, 32, 64, 21);
    m->lod_model[0]->cache_coherency_sorting_hint = 30;
    m->lod_model[1] = sreNewLODModel();
    sreInitializeTorusModel(m->lod_model[1], 32, 16, 32, 16);
    m->lod_model[1]->cache_coherency_sorting_hint = 6;
    m->lod_model[2] = sreNewLODModel();
    sreInitializeTorusModel(m->lod_model[2], 16, 8, 16, 8);
    m->lod_model[2]->cache_coherency_sorting_hint = 12;
    m->lod_model[3] = sreNewLODModel();
    sreInitializeTorusModel(m->lod_model[3], 8, 4, 8, 4);
    m->lod_model[3]->cache_coherency_sorting_hint = 0;
#else
    m->lod_model[0] = sreNewLODModel();
    sreInitializeTorusModel(m->lod_model[0], 64, 32, 8, 4);
    m->lod_model[1] = sreNewLODModel();
    sreInitializeTorusModel(m->lod_model[1], 32, 16, 4, 2);
    m->lod_model[2] = sreNewLODModel();
    sreInitializeTorusModel(m->lod_model[2], 16, 8, 2, 1);
    m->lod_model[3] = sreNewLODModel();
    sreInitializeTorusModel(m->lod_model[3], 8, 4, 1, 1);
#endif
    m->nu_lod_levels = 4;
    m->CalculateBounds();
    m->collision_shape_static = SRE_COLLISION_SHAPE_STATIC;
    m->collision_shape_dynamic = SRE_COLLISION_SHAPE_CONVEX_HULL;
    scene->RegisterModel(m);
    return m;
}

static void AddBar(sreBaseModel *m, float x, float y, float xdim, float ydim, float zdim, int flags) {
    // We have to define the vertices for each face seperately to allow different normals at the same
    // vertex position.
    // Bottom face.
    if (!(flags & SRE_BLOCK_NO_BOTTOM)) {
        m->vertex[m->nu_vertices + 0].Set(x, y, 0);
        m->vertex[m->nu_vertices + 1].Set(x + xdim, y, 0);
        m->vertex[m->nu_vertices + 2].Set(x + xdim, y + ydim, 0);
        m->vertex[m->nu_vertices + 3].Set(x, y + ydim, 0);
        m->texcoords[m->nu_vertices + 0].Set(0, 1);
        m->texcoords[m->nu_vertices + 1].Set(1, 1);
        m->texcoords[m->nu_vertices + 2].Set(1, 0);
        m->texcoords[m->nu_vertices + 3].Set(0, 0);
        m->triangle[m->nu_triangles + 0].AssignVertices(m->nu_vertices + 3, m->nu_vertices + 2, m->nu_vertices + 1);
        m->triangle[m->nu_triangles + 1].AssignVertices(m->nu_vertices + 3, m->nu_vertices + 1, m->nu_vertices + 0);
        m->nu_vertices += 4;
        m->nu_triangles += 2;
    }
    // Top face.
    if (!(flags & SRE_BLOCK_NO_TOP)) {
        m->vertex[m->nu_vertices + 0].Set(x, y, zdim);
        m->vertex[m->nu_vertices + 1].Set(x + xdim, y, zdim);
        m->vertex[m->nu_vertices + 2].Set(x + xdim, y + ydim, zdim);
        m->vertex[m->nu_vertices + 3].Set(x, y + ydim, zdim);
        m->texcoords[m->nu_vertices + 0].Set(0, 0);
        m->texcoords[m->nu_vertices + 1].Set(1, 0);
        m->texcoords[m->nu_vertices + 2].Set(1, 1);
        m->texcoords[m->nu_vertices + 3].Set(0, 1);
        m->triangle[m->nu_triangles + 0].AssignVertices(m->nu_vertices + 0, m->nu_vertices + 1, m->nu_vertices + 2);
        m->triangle[m->nu_triangles + 1].AssignVertices(m->nu_vertices + 0, m->nu_vertices + 2, m->nu_vertices + 3);
        m->nu_vertices += 4;
        m->nu_triangles += 2;
    }
    // Front face.
    if (!(flags & SRE_BLOCK_NO_FRONT)) {
        m->vertex[m->nu_vertices].Set(x, y, 0);
        m->vertex[m->nu_vertices + 1].Set(x + xdim, y, 0);
        m->vertex[m->nu_vertices + 2].Set(x + xdim, y, zdim);
        m->vertex[m->nu_vertices + 3].Set(x, y, zdim);
        m->texcoords[m->nu_vertices].Set(0, 0);
        m->texcoords[m->nu_vertices + 1].Set(1, 0);
        m->texcoords[m->nu_vertices + 2].Set(1, 1);
        m->texcoords[m->nu_vertices + 3].Set(0, 1);
        m->triangle[m->nu_triangles].AssignVertices(m->nu_vertices, m->nu_vertices + 1, m->nu_vertices + 2);
        m->triangle[m->nu_triangles + 1].AssignVertices(m->nu_vertices, m->nu_vertices + 2, m->nu_vertices + 3);
        m->nu_vertices += 4;
        m->nu_triangles += 2;
    }
    // Back face.
    if (!(flags & SRE_BLOCK_NO_BACK)) {
        m->vertex[m->nu_vertices].Set(x, y + ydim, 0);
        m->vertex[m->nu_vertices + 1].Set(x + xdim, y + ydim, 0);
        m->vertex[m->nu_vertices + 2].Set(x + xdim, y + ydim, zdim);
        m->vertex[m->nu_vertices + 3].Set(x, y + ydim, zdim);
        m->texcoords[m->nu_vertices].Set(1, 0);
        m->texcoords[m->nu_vertices + 1].Set(0, 0);
        m->texcoords[m->nu_vertices + 2].Set(0, 1);
        m->texcoords[m->nu_vertices + 3].Set(1, 1);
        m->triangle[m->nu_triangles].AssignVertices(m->nu_vertices + 1, m->nu_vertices, m->nu_vertices + 3);
        m->triangle[m->nu_triangles + 1].AssignVertices(m->nu_vertices + 1, m->nu_vertices + 3, m->nu_vertices + 2);
        m->nu_vertices += 4;
        m->nu_triangles += 2;
    }
    // Left face.
    if (!(flags & SRE_BLOCK_NO_LEFT)) {
        m->vertex[m->nu_vertices].Set(x, y + ydim, 0);
        m->vertex[m->nu_vertices + 1].Set(x, y, 0);
        m->vertex[m->nu_vertices + 2].Set(x, y, zdim);
        m->vertex[m->nu_vertices + 3].Set(x, y + ydim, zdim);
        m->texcoords[m->nu_vertices] .Set(0, 0);
        m->texcoords[m->nu_vertices + 1].Set(1, 0);
        m->texcoords[m->nu_vertices + 2].Set(1, 1);
        m->texcoords[m->nu_vertices + 3].Set(0, 1);
        m->triangle[m->nu_triangles].AssignVertices(m->nu_vertices, m->nu_vertices + 1, m->nu_vertices + 2);
        m->triangle[m->nu_triangles + 1].AssignVertices(m->nu_vertices, m->nu_vertices + 2, m->nu_vertices + 3);
        m->nu_vertices += 4;
        m->nu_triangles += 2;
    }
    // Right face.
    if (!(flags & SRE_BLOCK_NO_RIGHT)) {
        m->vertex[m->nu_vertices].Set(x + xdim, y, 0);
        m->vertex[m->nu_vertices + 1].Set(x + xdim, y + ydim, 0);
        m->vertex[m->nu_vertices + 2].Set(x + xdim, y + ydim, zdim);
        m->vertex[m->nu_vertices + 3].Set(x + xdim, y, zdim);
        m->texcoords[m->nu_vertices].Set(0, 0);
        m->texcoords[m->nu_vertices + 1].Set(1, 0);
        m->texcoords[m->nu_vertices + 2].Set(1, 1);
        m->texcoords[m->nu_vertices + 3].Set(0, 1);
        m->triangle[m->nu_triangles].AssignVertices(m->nu_vertices, m->nu_vertices + 1, m->nu_vertices + 2);
        m->triangle[m->nu_triangles + 1].AssignVertices(m->nu_vertices, m->nu_vertices + 2, m->nu_vertices + 3);
        m->nu_vertices += 4;
        m->nu_triangles += 2;
    }
}

// #define NU_BARS 20
// #define BORDER_WIDTH 0.2
// #define GAP_WIDTH 0.9
// #define BAR_WIDTH 0.1
// #define THICKNESS 0.2

static void sreInitializeGratingModel(sreBaseModel *m, int NU_HOLES_X, int NU_HOLES_Y, float BORDER_WIDTH,
float GAP_WIDTH, float BAR_WIDTH, float THICKNESS) {
    int max_triangles =
        1 * 8 + (NU_HOLES_X - 1) * 8 + (NU_HOLES_X - 1) * 6 + 2 * 8 +
        1 * 8 + (NU_HOLES_X - 1) * 8 + (NU_HOLES_X - 1) * 6 + 2 * 8 +
        (NU_HOLES_Y - 1) * 8 + (NU_HOLES_Y - 1) * 6 + 1 * 8 +
        (NU_HOLES_Y - 1) * 8 + (NU_HOLES_Y - 1) * 6 + 1 * 8 +
        (NU_HOLES_Y - 1) * ((NU_HOLES_X - 1) * (8 + 4 + 8) + 8) + (NU_HOLES_X - 1) * 8;
    int max_vertices = max_triangles * 2;
    m->position = dstNewAligned <Point3DPadded>(max_vertices, 16);
    m->triangle = new sreModelTriangle[max_triangles];
    m->texcoords = new Point2D[max_vertices];
    m->nu_vertices = 0;
    m->nu_triangles = 0;
    // Define the near side bar.
    AddBar(m, 0.0, 0.0, BORDER_WIDTH, BORDER_WIDTH, THICKNESS, SRE_BLOCK_NO_BACK | SRE_BLOCK_NO_RIGHT);
    float x = BORDER_WIDTH;
    for (int i = 0; i < NU_HOLES_X - 1; i++) {
        AddBar(m, x, 0.0, GAP_WIDTH, BORDER_WIDTH, THICKNESS, SRE_BLOCK_NO_LEFT | SRE_BLOCK_NO_RIGHT);
        AddBar(m, x + GAP_WIDTH, 0.0, BAR_WIDTH, BORDER_WIDTH, THICKNESS, SRE_BLOCK_NO_LEFT | SRE_BLOCK_NO_RIGHT | SRE_BLOCK_NO_BACK);
        x += GAP_WIDTH + BAR_WIDTH;
    }
    AddBar(m, x, 0.0, GAP_WIDTH, BORDER_WIDTH, THICKNESS, SRE_BLOCK_NO_LEFT | SRE_BLOCK_NO_RIGHT);
    AddBar(m, x + GAP_WIDTH, 0.0, BORDER_WIDTH, BORDER_WIDTH, THICKNESS, SRE_BLOCK_NO_LEFT | SRE_BLOCK_NO_BACK);
    // Define the back side bar.
    AddBar(m, 0.0, BORDER_WIDTH + (NU_HOLES_Y - 1) * (GAP_WIDTH + BAR_WIDTH) + GAP_WIDTH, BORDER_WIDTH, BORDER_WIDTH,
        THICKNESS, SRE_BLOCK_NO_FRONT | SRE_BLOCK_NO_RIGHT);
    x = BORDER_WIDTH;
    for (int i = 0; i < NU_HOLES_X - 1; i++) {
        AddBar(m, x, BORDER_WIDTH + (NU_HOLES_Y - 1) * (GAP_WIDTH + BAR_WIDTH) + GAP_WIDTH, GAP_WIDTH, BORDER_WIDTH,
            THICKNESS, SRE_BLOCK_NO_LEFT | SRE_BLOCK_NO_RIGHT);
        AddBar(m, x + GAP_WIDTH, BORDER_WIDTH + (NU_HOLES_Y - 1) * (GAP_WIDTH + BAR_WIDTH) + GAP_WIDTH, BAR_WIDTH,
            BORDER_WIDTH, THICKNESS, SRE_BLOCK_NO_LEFT | SRE_BLOCK_NO_RIGHT | SRE_BLOCK_NO_FRONT);
        x += GAP_WIDTH + BAR_WIDTH;
    }
    AddBar(m, x, BORDER_WIDTH + (NU_HOLES_Y - 1) * (GAP_WIDTH + BAR_WIDTH) + GAP_WIDTH, GAP_WIDTH, BORDER_WIDTH,
        THICKNESS, SRE_BLOCK_NO_LEFT | SRE_BLOCK_NO_RIGHT);
    AddBar(m, x + GAP_WIDTH, BORDER_WIDTH + (NU_HOLES_Y - 1) * (GAP_WIDTH + BAR_WIDTH) + GAP_WIDTH, BORDER_WIDTH,
        BORDER_WIDTH, THICKNESS, SRE_BLOCK_NO_LEFT | SRE_BLOCK_NO_FRONT);
    // Define the left side bar.
    float y = BORDER_WIDTH;
    for (int i = 0; i < NU_HOLES_Y - 1; i++) {
        AddBar(m, 0.0, y, BORDER_WIDTH, GAP_WIDTH, THICKNESS, SRE_BLOCK_NO_FRONT | SRE_BLOCK_NO_BACK);
        AddBar(m, 0.0, y + GAP_WIDTH, BORDER_WIDTH, BAR_WIDTH, THICKNESS, SRE_BLOCK_NO_FRONT | SRE_BLOCK_NO_BACK | SRE_BLOCK_NO_RIGHT);
        y += GAP_WIDTH + BAR_WIDTH;
    }
    AddBar(m, 0.0, y, BORDER_WIDTH, GAP_WIDTH, THICKNESS, SRE_BLOCK_NO_FRONT | SRE_BLOCK_NO_BACK);
    // Define the right side bar.
    y = BORDER_WIDTH;
    for (int i = 0; i < NU_HOLES_Y - 1; i++) {
        AddBar(m, BORDER_WIDTH + (NU_HOLES_X - 1) * (GAP_WIDTH + BAR_WIDTH) + GAP_WIDTH, y, BORDER_WIDTH, GAP_WIDTH,
            THICKNESS, SRE_BLOCK_NO_FRONT | SRE_BLOCK_NO_BACK);
        AddBar(m, BORDER_WIDTH + (NU_HOLES_X - 1) * (GAP_WIDTH + BAR_WIDTH) + GAP_WIDTH, y + GAP_WIDTH, BORDER_WIDTH,
            BAR_WIDTH, THICKNESS, SRE_BLOCK_NO_FRONT | SRE_BLOCK_NO_BACK | SRE_BLOCK_NO_LEFT);
        y += GAP_WIDTH + BAR_WIDTH;
    }
    AddBar(m, BORDER_WIDTH + (NU_HOLES_X - 1) * (GAP_WIDTH + BAR_WIDTH) + GAP_WIDTH, y, BORDER_WIDTH, GAP_WIDTH,
        THICKNESS, SRE_BLOCK_NO_FRONT | SRE_BLOCK_NO_BACK);
    // Define the inside.
    y = BORDER_WIDTH;
    for (int i = 0; i < NU_HOLES_Y - 1; i++) {
        x = BORDER_WIDTH + GAP_WIDTH;
        for (int j = 0; j < NU_HOLES_X - 1; j++) {
            AddBar(m, x, y, BAR_WIDTH, GAP_WIDTH, THICKNESS, SRE_BLOCK_NO_FRONT | SRE_BLOCK_NO_BACK);
            AddBar(m, x, y + GAP_WIDTH, BAR_WIDTH, BAR_WIDTH, THICKNESS, SRE_BLOCK_NO_LEFT | SRE_BLOCK_NO_RIGHT | SRE_BLOCK_NO_FRONT | SRE_BLOCK_NO_BACK);
            AddBar(m, x - GAP_WIDTH, y + GAP_WIDTH, GAP_WIDTH, BAR_WIDTH, THICKNESS, SRE_BLOCK_NO_LEFT | SRE_BLOCK_NO_RIGHT);
            x += GAP_WIDTH + BAR_WIDTH;
        }
        AddBar(m, x - GAP_WIDTH, y + GAP_WIDTH, GAP_WIDTH, BAR_WIDTH, THICKNESS, SRE_BLOCK_NO_LEFT | SRE_BLOCK_NO_RIGHT);
        y += GAP_WIDTH + BAR_WIDTH;
    }
    x = BORDER_WIDTH + GAP_WIDTH;
    for (int i = 0; i < NU_HOLES_X - 1; i++) {
        AddBar(m, x, y, BAR_WIDTH, GAP_WIDTH, THICKNESS, SRE_BLOCK_NO_FRONT | SRE_BLOCK_NO_BACK);
        x += GAP_WIDTH + BAR_WIDTH;
    }
//    printf("nu_vertices = %d, max_vertices = %d", m->nu_vertices, max_vertices);
    m->SetPositions(m->position);
    m->SetAttributeFlags(SRE_POSITION_MASK /* | SRE_TEXCOORDS_MASK */);
    m->flags |= SRE_LOD_MODEL_CONTAINS_HOLES;
    m->SortVertices(0); // Sort on x-coordinate.
    // Slight difference in the coordinates for some vertices caused by rounding
    // differences makes it necessary to weld vertices with almost similar
    // positions.
    m->WeldVertices();
    m->vertex_normal = new Vector3D[m->nu_vertices];
    m->CalculateNormalsNotSmooth();
    m->MergeIdenticalVertices();
//    m->CalculateTangentVectors();
}

sreModel *sreCreateGratingModel(sreScene *scene, int nu_holes_x, int nu_holes_y, float border_width, float gap_width,
float bar_width, float thickness) {
    sreModel *m = new sreModel;
    m->lod_model[0] = sreNewLODModel();
    sreInitializeGratingModel(m->lod_model[0], nu_holes_x, nu_holes_y, border_width,
        gap_width, bar_width, thickness);
    m->nu_lod_levels = 1;
    // Generate LOD-levels if the characteristics are right.
    float size_x = nu_holes_x * gap_width + (nu_holes_x - 1) * bar_width + border_width * 2.0;
//    float size_y = nu_holes_y * gap_width + (nu_holes_y - 1) * bar_width + border_width * 2.0;
    for (int lod = 1; lod < 4; lod++) {
        if (nu_holes_x != nu_holes_y)
            break;
        if (nu_holes_x == 1 || nu_holes_y == 1)
            break;
        // Doubling the bar width and border_width, divide the number of holes by 2.
        nu_holes_x = nu_holes_x / 2;
        nu_holes_y = nu_holes_y / 2;
        bar_width *= 2.0;
        border_width *= 2.0;
        // new_nu_holes_x * new_gap_width + (new_nu_holes_x - 1) * new_bar_width + border_width * 2 = size_x
        // new_gap_width = (size_x - (new_nu_holes_x - 1) * new_bar_width - border_width * 2) / new_nu_holes_x
        gap_width = (size_x - (nu_holes_x - 1) * bar_width - border_width * 2.0) / nu_holes_x;
        m->lod_model[lod] = sreNewLODModel();
        sreInitializeGratingModel(m->lod_model[lod], nu_holes_x, nu_holes_y, border_width,
            gap_width, bar_width, thickness);
        m->nu_lod_levels++;
    }
    m->CalculateBounds();
    m->collision_shape_static = SRE_COLLISION_SHAPE_BOX;
    m->collision_shape_dynamic = SRE_COLLISION_SHAPE_BOX;
    scene->RegisterModel(m);
    return m;
}

sreModel *sreCreateBlockModel(sreScene *scene, float xdim, float ydim, float zdim, int flags) {
    int max_triangles = 12;
    int max_vertices = max_triangles * 2;
    sreModel *model = new sreModel;
    sreLODModel *m = model->lod_model[0] = sreNewLODModel();
    model->nu_lod_levels = 1;
    m->position = dstNewAligned <Point3DPadded>(max_vertices, 16);
    m->triangle = new sreModelTriangle[max_triangles];
    m->texcoords = new Point2D[max_vertices];
    m->nu_vertices = 0;
    m->nu_triangles = 0;
    AddBar(m, 0, 0, xdim, ydim, zdim, flags);
    m->SetPositions(m->position);
    m->SetAttributeFlags(SRE_POSITION_MASK | SRE_TEXCOORDS_MASK);
    // If any side of the block is missing, the mode is not closed.
    if (flags != 0)
        m->flags |= SRE_LOD_MODEL_NOT_CLOSED;
    m->SortVertices(0); // Sort on x-coordinate.
//    m->MergeIdenticalVertices();
    m->vertex_normal = new Vector3D[m->nu_vertices];
    m->CalculateNormalsNotSmooth();
    m->CalculateTangentVectors();
    model->CalculateBounds();
    model->collision_shape_static = SRE_COLLISION_SHAPE_BOX;  // Should be BOX for efficiency.
    model->collision_shape_dynamic = SRE_COLLISION_SHAPE_BOX;
    scene->RegisterModel(model);
    return model;
}

// Create a rectangle (in z plane) with a repeating texture pattern.

sreModel *sreCreateRepeatingRectangleModel(sreScene *scene, float size, float unit_size) {
    sreModel *model = new sreModel;
    sreLODModel *m = model->lod_model[0] = sreNewLODModel();
    model->nu_lod_levels = 1;
    Point3D *mesh = new Point3D[2 * 2];
    for (int y = 0; y <= 1; y++)
        for (int x = 0; x <= 1; x++)
            mesh[y * 2 + x].Set(x * size, y * size, 0);
    m->nu_triangles = 2;
    m->nu_vertices = m->nu_triangles * 3;
    m->position = dstNewAligned <Point3DPadded>(m->nu_vertices, 16);
    m->triangle = new sreModelTriangle[m->nu_triangles];
    m->texcoords = new Point2D[m->nu_vertices];
    int i = 0;
    sreModelTriangle *t1 = &m->triangle[0];
    sreModelTriangle *t2 = &m->triangle[1];
    m->position[i] = mesh[0];
    m->position[i + 1] = mesh[1];
    m->position[i + 2] = mesh[2];
    t1->AssignVertices(i, i + 1, i + 2);
    m->texcoords[0].Set(0, 0);
    m->texcoords[1].Set(size / unit_size, 0);
    m->texcoords[2].Set(0, size / unit_size);
    i += 3;
    m->position[i] = mesh[1];
    m->position[i + 1] = mesh[3];
    m->position[i + 2] = mesh[2];
    t2->AssignVertices(i, i + 1, i + 2);
    m->texcoords[3].Set(size / unit_size, 0);
    m->texcoords[4].Set(size / unit_size, size / unit_size);
    m->texcoords[5].Set(0, size / unit_size);
    i += 3;
    delete [] mesh;
    m->SetPositions(m->position);
    m->SetAttributeFlags(SRE_POSITION_MASK);
    m->flags |= /* SRE_LOD_MODEL_NO_SHADOW_VOLUME_SUPPORT | */
        SRE_LOD_MODEL_NOT_CLOSED | SRE_TEXCOORDS_MASK | SRE_LOD_MODEL_SINGLE_PLANE;
    m->SortVertices(0); // Sort on x-coordinate.
    m->MergeIdenticalVertices();
    m->vertex_normal = new Vector3D[m->nu_vertices];
    m->CalculateNormalsNotSmooth();
    m->CalculateTangentVectors();
    model->CalculateBounds();
    model->collision_shape_static = SRE_COLLISION_SHAPE_BOX;
    model->collision_shape_dynamic = SRE_COLLISION_SHAPE_BOX;
    scene->RegisterModel(model);
    return model;
}

static sreModel *FinishPlaneRectangleModel(sreScene *scene, Point3D *mesh) {
    sreModel *model = new sreModel;
    sreLODModel *m = model->lod_model[0] = sreNewLODModel();
    model->nu_lod_levels = 1;
    m->nu_triangles = 2;
    m->nu_vertices = m->nu_triangles * 3;
    m->position = dstNewAligned <Point3DPadded>(m->nu_vertices, 16);
    m->triangle = new sreModelTriangle[m->nu_triangles];
    m->texcoords = new Point2D[m->nu_vertices];
    int i = 0;
    sreModelTriangle *t1 = &m->triangle[0];
    sreModelTriangle *t2 = &m->triangle[1];
    m->position[i] = mesh[0];
    m->position[i + 1] = mesh[1];
    m->position[i + 2] = mesh[2];
    t1->AssignVertices(i, i + 1, i + 2);
    m->texcoords[0].Set(0, 0);
    m->texcoords[1].Set(1.0f, 0);
    m->texcoords[2].Set(0, 1.0f);
    i += 3;
    m->position[i] = mesh[1];
    m->position[i + 1] = mesh[3];
    m->position[i + 2] = mesh[2];
    t2->AssignVertices(i, i + 1, i + 2);
    m->texcoords[3].Set(1.0f, 0);
    m->texcoords[4].Set(1.0f, 1.0f);
    m->texcoords[5].Set(0, 1.0f);
    i += 3;
    delete [] mesh;
    m->SetPositions(m->position);
    m->SetAttributeFlags(SRE_POSITION_MASK);
    m->flags |= /* SRE_LOD_MODEL_NO_SHADOW_VOLUME_SUPPORT | */
        SRE_LOD_MODEL_NOT_CLOSED | SRE_LOD_MODEL_SINGLE_PLANE | SRE_TEXCOORDS_MASK;
    m->SortVertices(0); // Sort on x-coordinate.
    m->MergeIdenticalVertices();
    m->vertex_normal = new Vector3D[m->nu_vertices];
    m->CalculateNormalsNotSmooth();
    m->CalculateTangentVectors();
    model->CalculateBounds();
    model->collision_shape_static = SRE_COLLISION_SHAPE_BOX;
    model->collision_shape_dynamic = SRE_COLLISION_SHAPE_BOX;
    scene->RegisterModel(model);
    return model;
}

sreModel *sreCreateCenteredXPlaneRectangleModel(sreScene *scene, float dimy, float dimz) {
    Point3D *mesh = new Point3D[2 * 2];
    for (int z = 0; z <= 1; z++)
        for (int y = 0; y <= 1; y++)
            mesh[z * 2 + y].Set(0, ((float)y - 0.5f) * dimy, ((float)z - 0.5f) * dimz);
    return FinishPlaneRectangleModel(scene, mesh);
}

sreModel *sreCreateCenteredYPlaneRectangleModel(sreScene *scene, float dimx, float dimz) {
    Point3D *mesh = new Point3D[2 * 2];
    for (int z = 0; z <= 1; z++)
        for (int x = 0; x <= 1; x++)
            mesh[z * 2 + x].Set(((float)x - 0.5f) * dimx, 0, ((float)z - 0.5f) * dimz);
    return FinishPlaneRectangleModel(scene, mesh);
}

sreModel *sreCreateCenteredZPlaneRectangleModel(sreScene *scene, float dimx, float dimy) {
    Point3D *mesh = new Point3D[2 * 2];
    for (int y = 0; y <= 1; y++)
        for (int x = 0; x <= 1; x++)
            mesh[y * 2 + x].Set(((float)x - 0.5f) * dimx, ((float)y - 0.5f) * dimy, 0);
    return FinishPlaneRectangleModel(scene, mesh);
}

static void sreInitializeCylinderModel(sreBaseModel *m, int LONGITUDE_SEGMENTS, float zdim,
bool include_top, bool include_bottom) {
    int *grid_vertex = (int *)alloca(sizeof(int) * (LONGITUDE_SEGMENTS + 1) * 4);
    int row_size = LONGITUDE_SEGMENTS + 1;
    float radius = 1;
    int vertex_index = 0;
    m->nu_vertices = (LONGITUDE_SEGMENTS + 1) * 2;
    if (include_top)
        m->nu_vertices += (LONGITUDE_SEGMENTS + 1) + 1;
    if (include_bottom)
        m->nu_vertices += (LONGITUDE_SEGMENTS + 1) + 1;
    m->position = dstNewAligned <Point3DPadded>(m->nu_vertices, 16);
    m->texcoords = new Point2D[m->nu_vertices];
    for (int i = 0; i <= LONGITUDE_SEGMENTS; i++) {
            float longitude = i * (360 / (float)LONGITUDE_SEGMENTS) * M_PI / 180;
            if (i == LONGITUDE_SEGMENTS) // Make sure vertex is exactly the same for longitude 360 as for longitude 0
                longitude = 0;
            float x, y;
            x = radius * cosf(longitude);
            y = radius * sinf(longitude);
            m->position[vertex_index].Set(x, y, zdim);
            m->position[vertex_index + 1].Set(x, y, 0.0);
            m->texcoords[vertex_index].Set((float)i / LONGITUDE_SEGMENTS, 0);
            m->texcoords[vertex_index + 1].Set((float)i / LONGITUDE_SEGMENTS, 1.0);
            grid_vertex[i] = vertex_index;
            grid_vertex[i + row_size] = vertex_index + 1;
            vertex_index += 2;
    }
    // Duplicate the vertices for the top and bottom (vertex normals will be different for the same vertex).
    if (include_top)
        for (int i = 0; i <= LONGITUDE_SEGMENTS; i++) {
            m->position[vertex_index] = m->position[i * 2];
            // Top is hard to texture map, provide an arbitrary mapping.
            m->texcoords[vertex_index].Set((float)i / LONGITUDE_SEGMENTS, 0);
            grid_vertex[i + row_size * 2] = vertex_index;
            vertex_index++;
        }
    if (include_bottom)
        for (int i = 0; i <= LONGITUDE_SEGMENTS; i++) {
            m->position[vertex_index] = m->position[i * 2 + 1];
            m->texcoords[vertex_index].Set((float)i / LONGITUDE_SEGMENTS, 0);
            grid_vertex[i + row_size * 3] = vertex_index;
            vertex_index++;
        }
    int top_center_vertex_index;
    if (include_top) {
        top_center_vertex_index = vertex_index;
        m->position[vertex_index].Set(0, 0, zdim);
        vertex_index++;
    }
    int bottom_center_vertex_index;
    if (include_bottom) {
        bottom_center_vertex_index = vertex_index;
        m->position[vertex_index].Set(0, 0, 0);
        vertex_index++;
    }
    m->nu_triangles = LONGITUDE_SEGMENTS * 2;
    if (include_top)
        m->nu_triangles += LONGITUDE_SEGMENTS;
    if (include_bottom)
        m->nu_triangles += LONGITUDE_SEGMENTS;
    m->triangle = new sreModelTriangle[m->nu_triangles];
    int triangle_index = 0;
    for (int i = 0; i < LONGITUDE_SEGMENTS; i++) {
        m->triangle[triangle_index].AssignVertices(grid_vertex[i], grid_vertex[row_size + i],
            grid_vertex[row_size + i + 1]);
        triangle_index++;
        m->triangle[triangle_index].AssignVertices(grid_vertex[i], grid_vertex[row_size + i + 1],
            grid_vertex[i + 1]);
        triangle_index++;
    }
    // Add the top.
    if (include_top)
        for (int i = 0; i < LONGITUDE_SEGMENTS; i++) {
            m->triangle[triangle_index].AssignVertices(top_center_vertex_index, grid_vertex[i + row_size * 2],
                grid_vertex[i + 1 + row_size * 2]);
            triangle_index++;
        }
    // Add the bottom.
    if (include_bottom)
        for (int i = 0; i < LONGITUDE_SEGMENTS; i++) {
            m->triangle[triangle_index].AssignVertices(bottom_center_vertex_index, grid_vertex[i + 1 + row_size * 3],
                grid_vertex[i + row_size * 3]);
            triangle_index++;
        }
    m->SetPositions(m->position);
    m->SetAttributeFlags(SRE_POSITION_MASK | SRE_TEXCOORDS_MASK);
    if (!(include_bottom && include_top))
        m->flags |= SRE_LOD_MODEL_NOT_CLOSED;
    m->SortVertices(0); // Sort on x-coordinate.
//    m->MergeIdenticalVertices(false);
    m->vertex_normal = new Vector3D[m->nu_vertices];
    m->CalculateNormalsNotSmooth();
//    m->CalculateTangentVectors();
}

// Create cylinder model. Axis runs from (0, 0, 0) to (0, 0, length).

sreModel *sreCreateCylinderModel(sreScene *scene, float zdim, bool include_top, bool include_bottom) {
    sreModel *m = new sreModel;
    m->lod_model[0] = sreNewLODModel();
    sreInitializeCylinderModel(m->lod_model[0], 64, zdim, include_top, include_bottom);
    m->lod_model[1] = sreNewLODModel();
    sreInitializeCylinderModel(m->lod_model[1], 32, zdim, include_top, include_bottom);
    m->lod_model[2] = sreNewLODModel();
    sreInitializeCylinderModel(m->lod_model[2], 16, zdim, include_top, include_bottom);
    m->lod_model[3] = sreNewLODModel();
    sreInitializeCylinderModel(m->lod_model[3], 8, zdim, include_top, include_bottom);
    m->nu_lod_levels = 4;
    m->CalculateBounds();
    m->collision_shape_static = SRE_COLLISION_SHAPE_CYLINDER;
    m->collision_shape_dynamic = SRE_COLLISION_SHAPE_CYLINDER;
    scene->RegisterModel(m);
    return m;
}

static void AddHalfEllipsoid(sreBaseModel *m, int LONGITUDE_SEGMENTS, int LATITUDE_SEGMENTS,
float cap_radius, float center_x, bool cap_x_positive, float radius_y, float radius_z) {
    int starting_latitude;
    int ending_latitude;
    if (cap_x_positive) {
        starting_latitude = - LATITUDE_SEGMENTS / 2;
        ending_latitude = 0;
    }
    else {
        starting_latitude = 0;
        ending_latitude = LATITUDE_SEGMENTS / 2;
    }
    int *grid_vertex = (int *)alloca(sizeof(int) * (LATITUDE_SEGMENTS / 2 + 1) * (LONGITUDE_SEGMENTS + 1));
    int row_size = LONGITUDE_SEGMENTS + 1;
    int vertex_index = m->nu_vertices;
    m->nu_vertices += row_size * (LATITUDE_SEGMENTS / 2 + 1);
    for (int i = 0; i <= LONGITUDE_SEGMENTS; i++) {
        for (int j = starting_latitude; j <= ending_latitude; j++) {
            float latitude = (j + LATITUDE_SEGMENTS / 2) * (180 / (float)LATITUDE_SEGMENTS) * M_PI / 180;
            float longitude = i * (360 / (float)LONGITUDE_SEGMENTS) * M_PI / 180;
            if (i == LONGITUDE_SEGMENTS) // Make sure vertex is exactly the same for longitude 360 as for longitude 0
                longitude = 0;
            float x, y, z;
            if (j == LATITUDE_SEGMENTS / 2) {
                x = - cap_radius + center_x;
                y = 0;
                z = 0;
            }
            else {
                x = cap_radius * cosf(latitude) + center_x;
                y = radius_y * sinf(latitude) * cosf(longitude);
                z = radius_z * sinf(latitude) * sinf(longitude);
            }
            if (x == -0)
                x = 0;
            if (y == -0)
                y = 0;
            if (z == -0)
                z = 0;
            m->vertex[vertex_index].Set(x, y, z);
            grid_vertex[(j - starting_latitude) * row_size + i] = vertex_index;
            vertex_index++;
        }
    }
    int triangle_index = m->nu_triangles;
    m->nu_triangles += LONGITUDE_SEGMENTS * (LATITUDE_SEGMENTS / 2) * 2;
    for (int i = 0; i < LONGITUDE_SEGMENTS; i++) {
        for (int j = 0; j < LATITUDE_SEGMENTS / 2; j++) {
            m->triangle[triangle_index].AssignVertices(grid_vertex[j * row_size + i], grid_vertex[(j + 1) * row_size + i],
                grid_vertex[(j + 1) * row_size + i + 1]);
            triangle_index++;
            m->triangle[triangle_index].AssignVertices(grid_vertex[j * row_size + i], grid_vertex[(j + 1) * row_size +
                i + 1], grid_vertex[j * row_size + i + 1]);
            triangle_index++;
        }
    }
}

static void AddSquashedCylinderHull(sreBaseModel *m, int LONGITUDE_SEGMENTS, float length,
float radius_y, float radius_z) {
    int *grid_vertex = (int *)alloca(sizeof(int) * (LONGITUDE_SEGMENTS + 1) * 2);
    int row_size = LONGITUDE_SEGMENTS + 1;
    int vertex_index = m->nu_vertices;
    m->nu_vertices += (LONGITUDE_SEGMENTS + 1) * 2;
    for (int i = 0; i <= LONGITUDE_SEGMENTS; i++) {
            float longitude = i * (360 / (float)LONGITUDE_SEGMENTS) * M_PI / 180;
            if (i == LONGITUDE_SEGMENTS) // Make sure vertex is exactly the same for longitude 360 as for longitude 0
                longitude = 0;
            float y, z;
            y = radius_y * sinf(longitude);
            z = radius_z * cosf(longitude);
            m->vertex[vertex_index].Set(- length * 0.5, y, z);
            m->vertex[vertex_index + 1].Set(length * 0.5, y, z);
            grid_vertex[i] = vertex_index;
            grid_vertex[i + row_size] = vertex_index + 1;
            vertex_index += 2;
    }
    int triangle_index = m->nu_triangles;
    m->nu_triangles += LONGITUDE_SEGMENTS * 2;
    for (int i = 0; i < LONGITUDE_SEGMENTS; i++) {
        m->triangle[triangle_index].AssignVertices(grid_vertex[i], grid_vertex[row_size + i],
            grid_vertex[row_size + i + 1]);
        triangle_index++;
        m->triangle[triangle_index].AssignVertices(grid_vertex[i], grid_vertex[row_size + i + 1],
            grid_vertex[i + 1]);
        triangle_index++;
    }
}

static void sreInitializeCapsuleModel(sreBaseModel *m, int LONGITUDE_SEGMENTS, int LATITUDE_SEGMENTS,
float cap_radius, float length, float radius_y, float radius_z) {
    int max_vertices = (LONGITUDE_SEGMENTS + 1) * (LATITUDE_SEGMENTS / 2 + 1) * 2 + // The ellipsoid caps.
        (LONGITUDE_SEGMENTS + 1) * 2; // The cylinder hull.
    m->position = dstNewAligned <Point3DPadded>(max_vertices, 16);
    m->nu_vertices = 0;
    int max_triangles = LONGITUDE_SEGMENTS * (LATITUDE_SEGMENTS / 2) * 2 * 2 + LONGITUDE_SEGMENTS * 2;
    m->triangle = new sreModelTriangle[max_triangles];
    m->nu_triangles = 0;
    AddHalfEllipsoid(m, LONGITUDE_SEGMENTS, LATITUDE_SEGMENTS, cap_radius, length * 0.5, true, radius_y, radius_z);
    AddSquashedCylinderHull(m, LONGITUDE_SEGMENTS, length, radius_y, radius_z);
    AddHalfEllipsoid(m, LONGITUDE_SEGMENTS, LATITUDE_SEGMENTS, cap_radius, - length * 0.5, false, radius_y, radius_z);
    m->SetPositions(m->position);
    m->SetAttributeFlags(SRE_POSITION_MASK);
    m->RemoveEmptyTriangles();
    m->SortVertices(2); // Sort on z-coordinate.
    m->MergeIdenticalVertices();
    m->vertex_normal = new Vector3D[m->nu_vertices];
    m->CalculateNormals();
//    m->CalculateTangentVectors();
}

sreModel *sreCreateCapsuleModel(sreScene *scene, float cap_radius, float length, float radius_y, float radius_z) {
    sreModel *m = new sreModel;
    m->lod_model[0] = sreNewLODModel();
    sreInitializeCapsuleModel(m->lod_model[0], 64, 32, cap_radius, length, radius_y, radius_z);
    m->lod_model[0]->cache_coherency_sorting_hint = 6;
    m->lod_model[1] = sreNewLODModel();
    sreInitializeCapsuleModel(m->lod_model[1], 32, 16, cap_radius, length, radius_y, radius_z);
    m->lod_model[1]->cache_coherency_sorting_hint = 30;
    m->lod_model[2] = sreNewLODModel();
    sreInitializeCapsuleModel(m->lod_model[2], 16, 8, cap_radius, length, radius_y, radius_z);
    m->lod_model[2]->cache_coherency_sorting_hint = 38;
    m->lod_model[3] = sreNewLODModel();
    sreInitializeCapsuleModel(m->lod_model[3], 8, 4, cap_radius, length, radius_y, radius_z);
    m->lod_model[3]->cache_coherency_sorting_hint = 12;
    m->nu_lod_levels = 4;
    m->CalculateBounds();
    m->collision_shape_static = SRE_COLLISION_SHAPE_CAPSULE;
    m->collision_shape_dynamic = SRE_COLLISION_SHAPE_CAPSULE;
    sreBoundingVolumeCapsule capsule;
    capsule.length = length;
    capsule.radius = cap_radius;
    capsule.radius_y = radius_y;
    capsule.radius_z = radius_z;
    capsule.center.Set(0, 0, 0);
    capsule.axis.Set(1.0, 0, 0);
    m->SetBoundingCollisionShapeCapsule(capsule);
    scene->RegisterModel(m);
    return m;
}

// Compound objects. Note: no support for multi-color. LOD level 0 only.

sreModel *sreCreateCompoundModel(sreScene *scene, bool has_texcoords, bool has_tangents, int lod_flags) {
    sreModel *m = new sreModel;
    sreLODModel *lm = m->lod_model[0] = sreNewLODModel();
    m->nu_lod_levels = 1;
    lm->nu_vertices = 0;
    lm->nu_triangles = 0;
    lm->flags |= SRE_POSITION_MASK | SRE_NORMAL_MASK;
    if (has_texcoords)
        lm->flags |= SRE_TEXCOORDS_MASK;
    if (has_tangents)
        lm->flags |= SRE_TANGENT_MASK;
    lm->flags |= lod_flags;
    return m;
}

static void sreAddToCompoundModel(sreBaseModel *compound_model, sreBaseModel *m, Point3D position,
Vector3D rotation, float scaling) {
    MatrixTransform rotation_transform;
    Matrix3D rotation_matrix;
    if (rotation.x == 0 && rotation.y == 0 && rotation.z == 0) {
        rotation_matrix.SetIdentity();
        rotation_transform.SetIdentity();
    }
    else {
        MatrixTransform rot_x_matrix;
        rot_x_matrix.AssignRotationAlongXAxis(rotation.x);
        MatrixTransform rot_y_matrix;
        rot_y_matrix.AssignRotationAlongYAxis(rotation.y);
        MatrixTransform rot_z_matrix;
        rot_z_matrix.AssignRotationAlongZAxis(rotation.z);
        rotation_transform = (rot_x_matrix * rot_y_matrix) * rot_z_matrix;
        rotation_matrix.Set(
            rotation_transform(0, 0), rotation_transform(0, 1), rotation_transform(0, 2),
            rotation_transform(1, 0), rotation_transform(1, 1), rotation_transform(1, 2),
            rotation_transform(2, 0), rotation_transform(2, 1), rotation_transform(2, 2));
    }

    MatrixTransform translation_transform;
    translation_transform.AssignTranslation(position);
    MatrixTransform model_transform;
    if (scaling == 1) {
        if (rotation.x == 0 && rotation.y == 0 && rotation.z == 0)
            model_transform = translation_transform;
        else
            model_transform = translation_transform * rotation_transform;
    }
    else {
        MatrixTransform scaling_transform;
        scaling_transform.AssignScaling(scaling);
        model_transform = (translation_transform * scaling_transform) * rotation_transform;
    }

    Point3DPadded *new_vertex = dstNewAligned <Point3DPadded>(compound_model->nu_vertices +
        m->nu_vertices, 16);
    memcpy(new_vertex, compound_model->vertex, sizeof(Point3DPadded) * compound_model->nu_vertices);
    Vector3D *new_vertex_normal = new Vector3D[compound_model->nu_vertices + m->nu_vertices];
    memcpy(new_vertex_normal, compound_model->vertex_normal, sizeof(Vector3D) * compound_model->nu_vertices);
    Point2D *new_texcoords;
    if (compound_model->flags & SRE_TEXCOORDS_MASK) {
        new_texcoords = new Point2D[compound_model->nu_vertices + m->nu_vertices];
        memcpy(new_texcoords, compound_model->texcoords, sizeof(Point2D) * compound_model->nu_vertices);
    }
    Vector4D *new_vertex_tangent;
    if (compound_model->flags & SRE_TANGENT_MASK) {
        new_vertex_tangent = new Vector4D[compound_model->nu_vertices + m->nu_vertices];
        memcpy(new_vertex_tangent, compound_model->vertex_tangent, sizeof(Vector4D) * compound_model->nu_vertices);
    }
    for (int i = 0; i < m->nu_vertices; i++) {
        new_vertex[compound_model->nu_vertices + i] = (model_transform * m->vertex[i]).GetPoint3D();
        new_vertex_normal[compound_model->nu_vertices + i] = rotation_matrix * m->vertex_normal[i];
        if (compound_model->flags & SRE_TEXCOORDS_MASK)
            new_texcoords[compound_model->nu_vertices + i] = m->texcoords[i];
        if (compound_model->flags & SRE_TANGENT_MASK)
            new_vertex_tangent[compound_model->nu_vertices + i] = Vector4D(rotation_matrix *
                m->vertex_tangent[i].GetVector3D(), m->vertex_tangent[i].w);
    }
    sreModelTriangle *new_triangle = new sreModelTriangle[compound_model->nu_triangles + m->nu_triangles];
    memcpy(new_triangle, compound_model->triangle, sizeof(sreModelTriangle) * compound_model->nu_triangles);
    for (int i = 0; i < m->nu_triangles; i++) {
        new_triangle[compound_model->nu_triangles + i].normal = rotation_matrix * m->triangle[i].normal;
        for (int j = 0; j < 3; j++)
            new_triangle[compound_model->nu_triangles + i].vertex_index[j] = m->triangle[i].vertex_index[j] +
                compound_model->nu_vertices;
    }
    if (compound_model->nu_vertices > 0)
        delete [] compound_model->vertex;
    compound_model->SetPositions(new_vertex);
    if (compound_model->nu_vertices > 0)
        delete [] compound_model->vertex_normal;
    compound_model->vertex_normal = new_vertex_normal;
    if (compound_model->flags & SRE_TEXCOORDS_MASK) {
        if (compound_model->nu_vertices > 0)
            delete [] compound_model->texcoords;
        compound_model->texcoords = new_texcoords;
    }
    if (compound_model->flags & SRE_TANGENT_MASK) {
        if (compound_model->nu_vertices > 0)
            delete [] compound_model->vertex_tangent;
        compound_model->vertex_tangent = new_vertex_tangent;
    }
    if (compound_model->nu_triangles > 0)
        delete [] compound_model->triangle;
    compound_model->triangle = new_triangle;
    compound_model->nu_vertices += m->nu_vertices;
    compound_model->nu_triangles += m->nu_triangles;
}

void sreAddToCompoundModel(sreModel *compound_model, sreModel *model, Point3D position,
Vector3D rotation, float scaling) {
    sreAddToCompoundModel(compound_model->lod_model[0], model->lod_model[0], position,
        rotation, scaling);
}

void sreFinalizeCompoundModel(sreScene *scene, sreModel *model) {
    sreLODModel *m = model->lod_model[0];
    m->WeldVertices(); // Weld vertices.
//    m->MergeIdenticalVertices();
    m->SortVerticesOptimalDimension();
    model->CalculateBounds();
    model->collision_shape_static = SRE_COLLISION_SHAPE_STATIC;
    model->collision_shape_dynamic = SRE_COLLISION_SHAPE_CONVEX_HULL;
    scene->RegisterModel(model);
}

// Create a new multi-color model from an existing model. Optionally shares
// the model data strucures amd OpenGL vertex buffers with the existing model.
// If the existing model is already multi-color, the new model will have new
// colors.

sreModel *sreCreateNewMultiColorModel(sreScene *scene, sreModel *m, int color_flags,
int nu_colors, Color *colors) {
    if (!(color_flags & SRE_MULTI_COLOR_FLAG_SHARE_RESOURCES)) {
        printf("Error -- SRE_MULTI_COLOR_FLAG_SHARE_RESOURCES must be set.\n");
        exit(1);
    }
    // Need to handle LOD levels.
    sreModel *new_model;
    bool need_new_colors;
    if (color_flags & SRE_MULTI_COLOR_FLAG_SHARE_RESOURCES) {
        // This instances LOD levels as well.
        new_model = m->CreateNewInstance();
        need_new_colors = true;
    }
    else {
        // Not yet fully implemented.
        new_model = new sreModel;
        new_model->lod_model[0] = m->lod_model[0]->AllocateNewOfSameType();
        new_model->nu_lod_levels = 1;
        // Create clone.
        m->lod_model[0]->Clone(new_model->lod_model[0]);
        if (!(m->lod_model[0]->flags & SRE_COLOR_MASK))
            need_new_colors = true;
        // This needs some work, copy other fields etc.
    }
    if (sre_internal_debug_message_level >= 2)
        printf("sreCreateNewMultiColorModel: %d LOD levels.\n",
            new_model->nu_lod_levels);
    for  (int l = 0; l < new_model->nu_lod_levels; l++) {
       if (need_new_colors) {
            new_model->lod_model[l]->colors =
                new Color[new_model->lod_model[l]->nu_vertices];
            new_model->lod_model[l]->flags |= SRE_COLOR_MASK;
        }
        // Apply new colors.
        if (sre_internal_debug_message_level >= 2)
            printf("Applying new colors to model (LOD level %d, %d triangles).\n",
                l, new_model->lod_model[l]->nu_triangles);
        for (int i = 0; i < new_model->lod_model[l]->nu_triangles; i++) {
            if (color_flags & SRE_MULTI_COLOR_FLAG_ASSIGN_RANDOM) {
                int color_index = rand() % nu_colors;
                new_model->lod_model[l]->colors[i] = colors[color_index];
            }
            else if (color_flags & SRE_MULTI_COLOR_FLAG_NEW_RANDOM) {
                Color c;
                c.SetRandom();
                new_model->lod_model[l]->colors[i] = c;
            }
        }
        new_model->lod_model[l]->instance_flags = SRE_COLOR_MASK;
    }
    scene->RegisterModel(new_model);
    return new_model;
}
