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

// Load/save model file in SRE-specific binary format (fast loading).

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>

#include "sre.h"
#include "sre_internal.h"

void fread_with_check(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t n = fread(ptr, size, nmemb, stream);
    if (n != nmemb)
        sreFatalError("Error reading from file.");
}

void fwrite_with_check(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t n = fwrite(ptr, size, nmemb, stream);
    if (n != nmemb)
        sreFatalError("Error writing to file.");
}

#define SRE_BINARY_MODEL_SIGNATURE ((unsigned int)'S' + (unsigned int)'R' * 0x100 + \
    (unsigned int)'E' * 0x10000 + (unsigned int)'M' * 0x1000000)

#define SRE_BINARY_LOD_MODEL_SIGNATURE ((unsigned int)'S' + (unsigned int)'R' * 0x100 + \
    (unsigned int)'E' * 0x10000 + (unsigned int)'L' * 0x1000000)

class sreBinaryModelHeader {
public :
    uint32_t signature;
    uint32_t nu_lod_levels;
    float lod_threshold_scaling;
    int32_t collision_shape_static;
    int32_t collision_shape_dynamic;
    uint32_t bounds_flags;
    union {
        sreBoundingVolumeType special_collision_shape_type;
        uint32_t reserved6;
    };
    union {
        struct {
            uint32_t data[(sizeof(sreBoundingVolumeCylinder) + 3) & (~3)];
        } cylinder;
        struct {
            uint32_t data[(sizeof(sreBoundingVolumeEllipsoid) + 3) & (~3)];
        } ellipsoid;
        struct {
            uint32_t data[(sizeof(sreBoundingVolumeCapsule) + 3) & (~3)];
        } capsule;
        uint32_t reserved[25];
    };
};

class sreBinaryLODModelHeader {
public :
    uint32_t signature;
    uint32_t flags;
    int32_t nu_vertices;
    int32_t nu_triangles;
    int32_t sorting_dimension;
    uint32_t reserved[27];
};

static void RemoveUnwantedAttributes(sreLODModel *lm, int load_flags) {
    if ((lm->flags & SRE_NORMAL_MASK) && (load_flags & SRE_MODEL_LOAD_FLAG_NO_VERTEX_NORMALS)) {
        delete lm->vertex_normal;
        lm->flags &= (~SRE_NORMAL_MASK);
    }
    if ((lm->flags & SRE_TANGENT_MASK) && (load_flags & SRE_MODEL_LOAD_FLAG_NO_TANGENTS)) {
        delete lm->vertex_tangent;
        lm->flags &= (~SRE_TANGENT_MASK);
    }
    if ((lm->flags & SRE_TEXCOORDS_MASK) && (load_flags & SRE_MODEL_LOAD_FLAG_NO_TEXCOORDS)) {
        delete lm->texcoords;
        lm->flags &= (~SRE_TEXCOORDS_MASK);
    }
    if ((lm->flags & SRE_COLOR_MASK) && (load_flags & SRE_MODEL_LOAD_FLAG_NO_COLORS)) {
        delete lm->colors;
        lm->flags &= (~SRE_COLOR_MASK);
    }
}

static sreLODModel *sreReadLODModelFromSREBinaryLODModelFile(FILE *fp, int load_flags) {
        sreBinaryLODModelHeader lod_header;
        fread_with_check(&lod_header, 1, sizeof(sreBinaryLODModelHeader), fp);

        if (lod_header.signature != SRE_BINARY_LOD_MODEL_SIGNATURE)
            sreFatalError("Invalid signature when attempting to read LOD model from "
                ".srebinarymodel or .srebinarylodmodel file.");

        sreLODModel *lm = sreNewLODModel();
        lm->nu_meshes = 1;
        lm->flags = lod_header.flags;
        lm->nu_vertices = lod_header.nu_vertices;
        lm->nu_triangles = lod_header.nu_triangles;
        lm->sorting_dimension = lod_header.sorting_dimension;

        // Read the vertex attribute data, in order.
        if (lm->flags & SRE_POSITION_MASK) {
            Point3D *vertex = new Point3D[lm->nu_vertices];
            fread_with_check(vertex, 1, sizeof(Point3D) * lm->nu_vertices, fp);
            lm->position = dstNewAligned <Point3DPadded>((size_t)lm->nu_vertices, 16);
            for (int i = 0; i < lm->nu_vertices; i++)
	            lm->position[i] = vertex[i];
            delete [] vertex;
        }
        if (lm->flags & SRE_NORMAL_MASK) {
            lm->vertex_normal = new Vector3D[lm->nu_vertices];
            fread_with_check(lm->vertex_normal, 1, sizeof(Vector3D) * lm->nu_vertices, fp);
        }
        if (lm->flags & SRE_TANGENT_MASK) {
            lm->vertex_tangent = new Vector4D[lm->nu_vertices];
            fread_with_check(lm->vertex_tangent, 1, sizeof(Point3D) * lm->nu_vertices, fp);
        }
        if (lm->flags & SRE_TEXCOORDS_MASK) {
            lm->texcoords = new Point2D[lm->nu_vertices];
            fread_with_check(lm->texcoords, 1, sizeof(Point2D) * lm->nu_vertices, fp);
        }
        if (lm->flags & SRE_COLOR_MASK) {
            lm->colors = new Color[lm->nu_vertices];
            fread_with_check(lm->colors, 1, sizeof(Color) * lm->nu_vertices, fp);
        }

        RemoveUnwantedAttributes(lm, load_flags);

        // Read the triangle data.
        lm->triangle = new sreModelTriangle[lm->nu_triangles];
        fread_with_check(lm->triangle, 1, sizeof(sreModelTriangle) * lm->nu_triangles, fp);
        return lm;
}

sreLODModel *sreReadLODModelFromSREBinaryLODModelFile(const char *pathname, int load_flags) {
    sreMessage(SRE_MESSAGE_INFO, "Loading LOD model file %s.", pathname);
     FILE *fp = fopen(pathname, "rb");
     if (fp == NULL)
         sreFatalError("Could not open file %s.", pathname);
     sreLODModel *lm = sreReadLODModelFromSREBinaryLODModelFile(fp, load_flags);
     fclose(fp);
     return lm;
}

sreModel *sreReadModelFromSREBinaryModelFile(sreScene *scene, const char *pathname,
int load_flags) {
    sreMessage(SRE_MESSAGE_INFO, "Loading model file %s.", pathname);
    FILE *fp = fopen(pathname, "rb");
     if (fp == NULL)
         sreFatalError("Could not open file %s.", pathname);
    sreBinaryModelHeader header;
    fread_with_check(&header, 1, sizeof(sreBinaryModelHeader), fp);

    if (header.signature != SRE_BINARY_MODEL_SIGNATURE)
        sreFatalError("Invalid signature when attempting to read .srebinarymodel file.");

    sreModel *m = new sreModel;
    m->nu_lod_levels = header.nu_lod_levels;
    m->lod_threshold_scaling = header.lod_threshold_scaling;
    m->collision_shape_static = header.collision_shape_static;
    m->collision_shape_dynamic = header.collision_shape_dynamic;
    m->bounds_flags = 0;
    if (header.bounds_flags & SRE_BOUNDS_SPECIAL_SRE_COLLISION_SHAPE) {
        m->special_collision_shape = new sreBoundingVolume;
        m->special_collision_shape->type = header.special_collision_shape_type;
        m->special_collision_shape->is_complete = true;
        if (header.special_collision_shape_type == SRE_COLLISION_SHAPE_CYLINDER) {
            m->special_collision_shape->cylinder = new sreBoundingVolumeCylinder;
            memcpy(m->special_collision_shape->cylinder, &header.cylinder,
                sizeof(sreBoundingVolumeCylinder));
        }
        else if (header.special_collision_shape_type == SRE_COLLISION_SHAPE_ELLIPSOID) {
            m->special_collision_shape->ellipsoid = new sreBoundingVolumeEllipsoid;
            memcpy(m->special_collision_shape->ellipsoid, &header.ellipsoid,
                sizeof(sreBoundingVolumeEllipsoid));
        }
        else if (header.special_collision_shape_type == SRE_COLLISION_SHAPE_CAPSULE) {
            m->special_collision_shape->capsule = new sreBoundingVolumeCapsule;
            memcpy(m->special_collision_shape->capsule, &header.capsule,
                sizeof(sreBoundingVolumeCapsule));
        }
        else
            sreFatalError("Special collision shape type not supported when reading "
                ".srebinarmodel file.");
        m->bounds_flags = SRE_BOUNDS_SPECIAL_SRE_COLLISION_SHAPE;
        // The rest of the bounds/bounds_flags will be recalculated.
    }

    for (int i = 0 ; i < header.nu_lod_levels; i++) {
        sreLODModel *lm = sreReadLODModelFromSREBinaryLODModelFile(fp, load_flags);

        m->lod_model[i] = lm;
    }
    fclose(fp);

    m->CalculateBounds();
    scene->RegisterModel(m);
    return m;
}

static void sreSaveLODModelToSREBinaryLODModelFile(sreLODModel *lm, FILE *fp, int save_flags) {
        sreBinaryLODModelHeader lod_header;
	lod_header.signature = SRE_BINARY_LOD_MODEL_SIGNATURE;
        lod_header.flags = lm->flags;
        lod_header.nu_vertices = lm->nu_vertices;
        lod_header.nu_triangles = lm->nu_triangles;
        lod_header.sorting_dimension = lm->sorting_dimension;

        fwrite_with_check(&lod_header, 1, sizeof(sreBinaryLODModelHeader), fp);

        // Write the vertex attribute data, in order.
        if (lm->flags & SRE_POSITION_MASK) {
            fwrite_with_check(lm->vertex, sizeof(Point3D) * lm->nu_vertices, 1, fp);
        }
        if (lm->flags & SRE_NORMAL_MASK) {
            if (save_flags & SRE_MODEL_LOAD_FLAG_NO_VERTEX_NORMALS)
                lod_header.flags &= (~SRE_NORMAL_MASK);
            else
                fwrite_with_check(lm->vertex_normal, sizeof(Vector3D) * lm->nu_vertices, 1, fp);
        }
        if (lm->flags & SRE_TANGENT_MASK) {
            if (save_flags & SRE_MODEL_LOAD_FLAG_NO_TANGENTS)
                lod_header.flags &= (~SRE_TANGENT_MASK);
            else
                fwrite_with_check(lm->vertex_tangent, sizeof(Point3D) * lm->nu_vertices, 1, fp);
        }
        if (lm->flags & SRE_TEXCOORDS_MASK) {
            if (save_flags & SRE_MODEL_LOAD_FLAG_NO_TEXCOORDS)
                lod_header.flags &= (~SRE_TEXCOORDS_MASK);
            else
                fwrite_with_check(lm->texcoords, sizeof(Point2D) * lm->nu_vertices, 1, fp);
        }
        if (lm->flags & SRE_COLOR_MASK) {
            if (save_flags & SRE_MODEL_LOAD_FLAG_NO_COLORS)
                lod_header.flags &= (~SRE_NORMAL_MASK);
            else
                fwrite_with_check(lm->colors, sizeof(Color) * lm->nu_vertices, 1, fp);
        }

        // Write the triangle data.
        fwrite_with_check(lm->triangle, sizeof(sreModelTriangle) * lm->nu_triangles, 1, fp);
}

void sreSaveLODModelToSREBinaryLODModelFile(sreLODModel *lm, const char *pathname,
int save_flags) {
    sreMessage(SRE_MESSAGE_INFO, "Saving LOD model file %s.", pathname);
    FILE *fp = fopen(pathname, "wb");
    if (fp == NULL)
        sreFatalError("Could not open file %s for writing.", pathname);
    sreSaveLODModelToSREBinaryLODModelFile(lm, fp, save_flags);
    fclose(fp);
}

void sreSaveModelToSREBinaryModelFile(sreModel *m, const char *pathname,
int save_flags) {
    sreMessage(SRE_MESSAGE_INFO, "Saving model file %s.", pathname);
    FILE *fp = fopen(pathname, "wb");
    if (fp == NULL)
        sreFatalError("Could not open file %s for writing.", pathname);

    sreBinaryModelHeader header;
    memset(&header, 0, sizeof(sreBinaryModelHeader));
    header.signature = SRE_BINARY_MODEL_SIGNATURE;
    header.nu_lod_levels = m->nu_lod_levels;
    header.lod_threshold_scaling = m->lod_threshold_scaling;
    header.collision_shape_static = m->collision_shape_static;
    header.collision_shape_dynamic = m->collision_shape_dynamic;
    header.bounds_flags = m->bounds_flags & SRE_BOUNDS_SPECIAL_SRE_COLLISION_SHAPE;

    if (header.bounds_flags & SRE_BOUNDS_SPECIAL_SRE_COLLISION_SHAPE) {
        header.special_collision_shape_type = m->special_collision_shape->type;
        if (header.special_collision_shape_type == SRE_COLLISION_SHAPE_CYLINDER) {
            memcpy(&header.cylinder, m->special_collision_shape->cylinder,
                sizeof(sreBoundingVolumeCylinder));
        }
        else if (header.special_collision_shape_type == SRE_COLLISION_SHAPE_ELLIPSOID) {
             memcpy(&header.ellipsoid, m->special_collision_shape->ellipsoid,
                sizeof(sreBoundingVolumeEllipsoid));
        }
        else if (header.special_collision_shape_type == SRE_COLLISION_SHAPE_CAPSULE) {
            memcpy(&header.capsule, m->special_collision_shape->capsule,
                sizeof(sreBoundingVolumeCapsule));
        }
        else
            sreFatalError("Collision shape type not supported while writing "
                ".srebinarymodel file.");
        // The rest of the bounds/bounds_flags will be recalculated when the model
        // is loaded.
    }
    fwrite_with_check(&header, 1, sizeof(sreBinaryModelHeader), fp);

    for (int i = 0 ; i < header.nu_lod_levels; i++) {
        sreLODModel *lm = m->lod_model[i];
        sreSaveLODModelToSREBinaryLODModelFile(lm, fp, save_flags);
    }
    fclose(fp);
}
