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
#include <math.h>
#include <float.h>

#include "sre.h"
#include "sre_internal.h"
#include "sre_bounds.h"
#include "win32_compat.h"

// This file contains functions for creating the main rendering octrees. Seperate
// octrees are created for static and dynamic (position) entities (an entity is
// either an object or a light), and for static and dynamic infinite distance entities.
// The octrees are converted from an unoptimized temporary format into a more
// efficient format used for rendering.

// Unoptimized octree used for initial octree creation.

class Octree : public sreOctreeNodeBounds {
public :
//    sreBoundingVolumeAABB AABB;
//    sreBoundingVolumeBox box;
//    float sphere_radius;
    Octree *subnode[8];
    sreSceneEntityList entity_list;  // Linked list of entities used during creation process.
    int nu_entities;              // Converted to array of entities for performance.
    sreSceneEntity *entity_array;

    Octree();
    Octree(const Vector3D& dim_min, const Vector3D& dim_max);
    void Initialize(const Vector3D& dmin, const Vector3D& dmax);
    void AddsreObject(sreObject& so);
    void AddsreObjectAtRootLevel(sreObject& so);
    void AddLight(sreLight& light);
    void AddLightAtRootLevel(sreLight& light);
    void MakeEmpty();
    void CountNodes(int& counted_nodes, int& counted_leafs, int& counted_entities) const;
    void AddEntitiesBalanced(int nu_input_entiies, sreSceneEntity *input_entity_array, int depth);
    void AddEntitiesBalancedAtRootLevel(int nu_input_entities, sreSceneEntity *input_entity_array);
    void AddEntityIntoBalancedOctree(const sreSceneEntity& entity);
    void AddEntityIntoBalancedOctreeAtRootLevel(const sreSceneEntity& entity);
    void ConvertToFastOctree(sreFastOctree& fast_oct);
private :
    void AddEntityRecursive(sreSceneEntity *entity, int depth);
    void ConvertToArrays(int& counted_nodes, int& counted_leafs, int& counted_entities);
    void ConvertToFastOctreeRecursive(sreFastOctree& fast_oct) const;
    void ConvertToFastOctree(sreFastOctree& fast_oct, int counted_nodes, int counted_leafs,
        int counted_entities) const;
};


/*
 * Octree benchmarks on example scene (with thin geometry in x/y plane):
 *
 *                        #nodes   size     Root      FPS         Notes
 *                                          (objects)
 * QUADTREE_XY_STRICT     5371     24238    82        80/81       Geometry scissors error messages
 *                        2096     11138    82        83.5        MAX_DEPTH = 6
 * QUADTREE_XY_BALANCED   8332     36082    68        79/79.5     Geometry scissors error messages
 * OCTREE_STRICT          3353     16166    146       84          
 * OCTREE_BALANCED        4712     21602    68        85          Geometry scissors error messages
 *                        1518      8826    68        85/86       MAX_DEPTH = 6
 * OCTREE_MIXED_WITH_QUAD 4438     20506    34        84/84.5
 * QUADTREE_XY_BALANCED   4083     19086    68        82          MAX_DEPTH = 8
 *                        1547      8942    68        84.5        MAX_DEPTH = 6
 *
 * MAX_DEPTH = 12, Optimization for last object.
 *
 * QUADTREE_XY_STRICT     3833     18086    82        85
 * QUADTREE_XY_BALANCED   4205     19574    68        84
 * OCTREE_BALANCED        3909     18390              85
 * OCTREE_STRICT          3722     17642    146       84
 * OCTREE_MIXED_WITH_QUAD 3711     17598    34        85
 *
 * Better optimization for last object.
 * 
 * OCTREE_MIXED_WITH_QUAD 2707     13582    34        86.5
 * QUADTREE_XY_BALANCED   2715     13614    68        84.5
 * OCTREE_BALANCED        2498     12746    68        85
 *
 * Don't clamp average position.
 * 
 * OCTREE_BALANCED        2501     12758    68        86
 * QUADTREE_XY_BALANCED   2713     13606    68        85
 *
 * Optimization for two objects. Possibly doesn't work because intersection tests for the object themselves
 * will not be avoided.
 *
 * QUADTREE_XY_BALANCED   2620     13234    68        84
 * OCTREE_BALANCED        2562     13002              84
 * 
 * Put last two objects for subnodes in the array for the current node.
 *
 * OCTREE_BALANCED        1698      9546    68        86
 *
 * Optimization when small dimension detected.
 *
 * OCTREE_BALANCED        1667      9422     6        85/86, no shadow/no geometry scissors: 150/151
 * OCTREE_MIXED_WITH_QUAD 1761      9798     6        151/152
 * OCTREE_STRICT (no opt) 1529      8870   146        153
 *
 * New scene with lots of objects, few collisions.
 *                                                            far plane
 * OCTREE_BALANCED         5242    37236    24        27.5     99.5
 * OCTREE_STRICT           5440    38028    171       27.2     96.8
 * QUADTREE_XY_STRICT     10344    57644    102       26.8
 * QUADTREE_XY_BALANCED   15110    76708    88        27.3
 * OCTREE_MIXED_WITH_QUAD  9521    54352    24        27.5    100.0
 *
 * OCTREE_STRICT node bounds computed on the fly.
 *
 * OCTREE_STRICT                                              96.0/97.5/100.0
 * OCTREE_STRICT -O2					      109/110.5
 *    optimization 2                                          108.5/111/110
 *    optimization 1b                                         109/110
 *    optimization 1c                                         112
 *    not on the fly                                          110/112
 *
 * OCTREE_STRICT_OPTIMIZED no node information:
 *                         5440    32588
 *
 * OCTREE_BALANCED         5242    37236                      109/110
 * optimization for two left over entities:
 *                         5538    38420                      112
 * optimization any entity alone in a subnode:
 *                         5086    36612
 */

class Factor {
public:
    float minx, maxx, miny, maxy, minz, maxz;
};

static const Factor factor[8] = {
    { 0, 0.5f, 0, 0.5f, 0, 0.5f },
    { 0.5f, 1.0f, 0, 0.5f, 0, 0.5f },
    { 0, 0.5f, 0.5f, 1.0f, 0, 0.5f },
    { 0.5f, 1.0f, 0.5f, 1.0f, 0, 0.5f },
    { 0, 0.5f, 0, 0.5f, 0.5f, 1.0f },
    { 0.5f, 1.0f, 0, 0.5f, 0.5f, 1.0f },
    { 0, 0.5f, 0.5f, 1.0f, 0.5f, 1.0f },
    { 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f }
};


static void CalculateNodeDimensions(int i, const Vector3D& dim_min, const Vector3D& dim_max,
Vector3D& dmin_out, Vector3D& dmax_out) {
    dmin_out.x = dim_min.x + factor[i].minx * (dim_max.x - dim_min.x);
    dmax_out.x = dim_min.x + factor[i].maxx * (dim_max.x - dim_min.x);
    dmin_out.y = dim_min.y + factor[i].miny * (dim_max.y - dim_min.y);
    dmax_out.y = dim_min.y + factor[i].maxy * (dim_max.y - dim_min.y);
    dmin_out.z = dim_min.z + factor[i].minz * (dim_max.z - dim_min.z);
    dmax_out.z = dim_min.z + factor[i].maxz * (dim_max.z - dim_min.z);
}

// sreObject octree follows.

static bool sreObjectFitsNode(const sreObject& so, const Vector3D& dmin, const Vector3D& dmax) {
    if (so.AABB.dim_min.x < dmin.x)
        return false;
    if (so.AABB.dim_max.x > dmax.x)
        return false;
    if (so.AABB.dim_min.y < dmin.y)
        return false;
    if (so.AABB.dim_max.y > dmax.y)
        return false;
    if (so.AABB.dim_min.z < dmin.z)
        return false;
    if (so.AABB.dim_max.z > dmax.z)
        return false;
    return true;
}

static bool sreSceneEntityFitsNode(sreSceneEntity *entity, const Vector3D& dmin, const Vector3D& dmax) {
   if (entity->type == SRE_ENTITY_OBJECT)
      return sreObjectFitsNode(*entity->so, dmin, dmax);
   if (entity->type == SRE_ENTITY_LIGHT) {
      sreBoundingVolumeAABB AABB;
      AABB.dim_min = dmin;
      AABB.dim_max = dmax;
      bool r = IsCompletelyInside(*entity->light, AABB);
      if (r)
          return true;
      else
          return false;
   }
   return true;
}

Octree::Octree() {
    for (int i = 0; i < 8; i++)
        subnode[i] = NULL;
    entity_array = NULL;
}

void Octree::Initialize(const Vector3D& dmin, const Vector3D& dmax) {
    AABB.dim_min = dmin;
    AABB.dim_max = dmax;
    for (int i = 0; i < 8; i++)
        subnode[i] = NULL;
    entity_array = NULL;
    nu_entities = 0;
    sphere.center = (AABB.dim_min + AABB.dim_max) * 0.5f;
    // Precalculate the approximate bounding volume of the octree using a sphere.
    sphere.radius = Magnitude(sphere.center - Point3D(AABB.dim_min.x, AABB.dim_min.y, AABB.dim_min.z));
#if 0
    // Set the box parameters.
    box.center = sphere.center;
    box.plane[0].Set(1.0, 0, 0, - AABB.dim_min.x);	// Left plane.
    box.plane[1].Set(- 1.0, 0, 0, AABB.dim_max.x); 	// Right plane.
    box.plane[2].Set(0, 1.0, 0, - AABB.dim_min.y);	// Near plane.
    box.plane[3].Set(0, - 1.0, 0, AABB.dim_max.y); 	// Far plane.
    box.plane[4].Set(0, 0, 1.0, - AABB.dim_min.z);	// Bottom plane.
    box.plane[5].Set(0, 0, - 1.0, AABB.dim_max.z); 	// Top plane.
    // We skip the box scale factors (PCA[i].scale_factor).
    box.PCA[0].Set(AABB.dim_max.x - AABB.dim_min.x, 0, 0);
    box.PCA[1].Set(0, AABB.dim_max.y - AABB.dim_min.y, 0);
    box.PCA[2].Set(0, 0, AABB.dim_max.z - AABB.dim_min.z);
    box.flags = 0;
#endif
}

Octree::Octree(const Vector3D& dmin, const Vector3D& dmax) {
    Initialize(dmin, dmax);
}

void Octree::AddEntityRecursive(sreSceneEntity *entity, int depth) {
    if (depth >= SRE_MAX_OCTREE_DEPTH) {
        entity_list.AddElement(entity);
        if (entity->type == SRE_ENTITY_OBJECT)
            entity->so->octree_list = &entity_list;
        return;
    }

    // Check whether the object fits in one of the octants.
    for (int i = 0; i < 8; i++) {
        Vector3D dmin, dmax;
        CalculateNodeDimensions(i, AABB.dim_min, AABB.dim_max, dmin, dmax);
        if (sreSceneEntityFitsNode(entity, dmin, dmax)) {
            if (subnode[i] == NULL)
                subnode[i] = new Octree(dmin, dmax);
            subnode[i]->AddEntityRecursive(entity, depth + 1);
            return;
        }
    }

    // If it does not fit, add the object to the list of this node.
    entity_list.AddElement(entity);
    if (entity->type == SRE_ENTITY_OBJECT)
        entity->so->octree_list = &entity_list;
}

void Octree::AddsreObject(sreObject& so) {
    sreSceneEntity *entity = new sreSceneEntity;
    entity->type = SRE_ENTITY_OBJECT;
    entity->so = &so;
    AddEntityRecursive(entity, 0);
}

void Octree::AddsreObjectAtRootLevel(sreObject& so) {
    // Add the object to the list of this node.
    sreSceneEntity *entity = new sreSceneEntity;
    entity->type = SRE_ENTITY_OBJECT;
    entity->so = &so;
    entity_list.AddElement(entity);
    so.octree_list = &entity_list;
}

void Octree::AddLight(sreLight& light) {
    sreSceneEntity *entity = new sreSceneEntity;
    entity->type = SRE_ENTITY_LIGHT;
    entity->light = &light;
    AddEntityRecursive(entity, 0);
}

void Octree::AddLightAtRootLevel(sreLight& light) {
    // Add the object to the list of this node.
    sreSceneEntity *entity = new sreSceneEntity;
    entity->type = SRE_ENTITY_LIGHT;
    entity->light = &light;
    entity_list.AddElement(entity);
}

void Octree::MakeEmpty() {
    for (int i = 0; i < 8; i++)
        if (subnode[i] != NULL) {
            subnode[i]->MakeEmpty();
            delete subnode[i];
            subnode[i] = NULL;
        }
    entity_list.MakeEmpty();
    if (entity_array != NULL)
        delete [] entity_array;
}

// Convert the octree node object/light lists to arrays for performance.
// At the same, also count nodes and entities.

void Octree::ConvertToArrays(int& counted_nodes, int& counted_leafs, int& counted_entities) {
    int count = 0;
    for (sreSceneEntityListElement *e = entity_list.head; e != NULL; e = e->next)
        count++;
    entity_array = new sreSceneEntity[count];
//    sreSceneEntityListElement *e = entity_list.head;
    for (int i = 0; i < count; i++) {
        sreSceneEntity *entity = entity_list.Pop();
//        sreSceneEntity *entity = e->entity;
        entity_array[i].type = entity->type;
        entity_array[i].so = entity->so; // This copies the union of sreObject and Light pointers.
        delete entity;
//        e = e->next;
    }
    nu_entities = count;
    // Update counting statistics.
    counted_nodes++;
    counted_entities += nu_entities;
    for (int i = 0; i < 8; i++)
        if (subnode[i] != NULL) {
            counted_leafs++;
            subnode[i]->ConvertToArrays(counted_nodes, counted_leafs, counted_entities);
        }
}

// Count nodes of an Octree filled with arrays of entities.

void Octree::CountNodes(int& counted_nodes, int& counted_leafs, int& counted_entities) const {
    counted_nodes++;
    counted_entities += nu_entities;
    for (int i = 0; i < 8; i++)
        if (subnode[i] != NULL) {
            counted_leafs++;
            subnode[i]->CountNodes(counted_nodes, counted_leafs, counted_entities);
        }
}

static void CalculateOctantAABBs(int nu_octants, const sreBoundingVolumeAABB& AABB, Point3D middle_point,
sreBoundingVolumeAABB *octant_AABB) {
    // Calculate the AABB for each octant.
    if (nu_octants == 4 && middle_point.x == AABB.dim_max.x) {
        // Quadtree with no split in x.
        octant_AABB[0].dim_min = Vector3D(AABB.dim_min.x, AABB.dim_min.y, AABB.dim_min.z);
        octant_AABB[0].dim_max = Vector3D(AABB.dim_max.x, middle_point.y, middle_point.z);
        octant_AABB[1].dim_min = Vector3D(AABB.dim_min.x, middle_point.y, AABB.dim_min.z);
        octant_AABB[1].dim_max = Vector3D(AABB.dim_max.x, AABB.dim_max.y, middle_point.z);
        octant_AABB[2].dim_min = Vector3D(AABB.dim_min.x, AABB.dim_min.y, middle_point.z);
        octant_AABB[2].dim_max = Vector3D(AABB.dim_max.x, middle_point.y, AABB.dim_max.z);
        octant_AABB[3].dim_min = Vector3D(AABB.dim_min.x, middle_point.y, middle_point.z);
        octant_AABB[3].dim_max = Vector3D(AABB.dim_max.x, AABB.dim_max.y, AABB.dim_max.z);
        return;
    }
    if (nu_octants == 4 && middle_point.y == AABB.dim_max.y) {
        // Quadtree with no split in y.
        octant_AABB[0].dim_min = Vector3D(AABB.dim_min.x, AABB.dim_min.y, AABB.dim_min.z);
        octant_AABB[0].dim_max = Vector3D(middle_point.x, AABB.dim_max.y, middle_point.z);
        octant_AABB[1].dim_min = Vector3D(middle_point.x, AABB.dim_min.y, AABB.dim_min.z);
        octant_AABB[1].dim_max = Vector3D(AABB.dim_max.x, AABB.dim_max.y, middle_point.z);
        octant_AABB[2].dim_min = Vector3D(AABB.dim_min.x, AABB.dim_min.y, middle_point.z);
        octant_AABB[2].dim_max = Vector3D(middle_point.x, AABB.dim_max.y, AABB.dim_max.z);
        octant_AABB[3].dim_min = Vector3D(middle_point.x, AABB.dim_min.y, middle_point.z);
        octant_AABB[3].dim_max = Vector3D(AABB.dim_max.x, AABB.dim_max.y, AABB.dim_max.z);
        return;
    }
    // Octree, or quadtree with no split in z (middle_point.z == AABB.dim_max.z).
    octant_AABB[0].dim_min = AABB.dim_min;
    octant_AABB[0].dim_max = middle_point;
    octant_AABB[1].dim_min = Vector3D(middle_point.x, AABB.dim_min.y, AABB.dim_min.z);
    octant_AABB[1].dim_max = Vector3D(AABB.dim_max.x, middle_point.y, middle_point.z);
    octant_AABB[2].dim_min = Vector3D(AABB.dim_min.x, middle_point.y, AABB.dim_min.z);
    octant_AABB[2].dim_max = Vector3D(middle_point.x, AABB.dim_max.y, middle_point.z);
    octant_AABB[3].dim_min = Vector3D(middle_point.x, middle_point.y, AABB.dim_min.z);
    octant_AABB[3].dim_max = Vector3D(AABB.dim_max.x, AABB.dim_max.y, middle_point.z);
    if (nu_octants == 4)
        return;
    octant_AABB[4].dim_min = Vector3D(AABB.dim_min.x, AABB.dim_min.y, middle_point.z);
    octant_AABB[4].dim_max = Vector3D(middle_point.x, middle_point.y, AABB.dim_max.z);
    octant_AABB[5].dim_min = Vector3D(middle_point.x, AABB.dim_min.y, middle_point.z);
    octant_AABB[5].dim_max = Vector3D(AABB.dim_max.x, middle_point.y, AABB.dim_max.z);
    octant_AABB[6].dim_min = Vector3D(AABB.dim_min.x, middle_point.y, middle_point.z);
    octant_AABB[6].dim_max = Vector3D(middle_point.x, AABB.dim_max.y, AABB.dim_max.z);
    octant_AABB[7].dim_min = Vector3D(middle_point.x, middle_point.y, middle_point.z);
    octant_AABB[7].dim_max = Vector3D(AABB.dim_max.x, AABB.dim_max.y, AABB.dim_max.z);

}

// Defined the offsets from the center of the octree for the middle point that are tried.
// The following results in middle points at coordinates 1/3th and 2/3th into the octree.
#define MIDDLE_OFFSET (0.5f - 1.0f / 3.0f)
// Middle points at coordinates 1/4th and 3/4th into the octree.
// #define MIDDLE_OFFSET 0.25f

void Octree::AddEntitiesBalanced(int nu_input_entities, sreSceneEntity *input_entity_array,
int depth) {
//    printf("AddEntitiesBalanced (nu_input_entities = %d)\n", nu_input_entities);
//    printf("Octree dimensions %f, %f, %f.\n", AABB.dim_max.x - AABB.dim_min.x, AABB.dim_max.y
//      - AABB.dim_min.y, AABB.dim_max.z - AABB.dim_min.z);
    if (depth >= SRE_MAX_OCTREE_DEPTH) {
        entity_array = new sreSceneEntity[nu_input_entities];
        nu_entities = 0;
        for (int i = 0; i < nu_input_entities; i++) {
            entity_array[nu_entities] = input_entity_array[i];
            nu_entities++;
        }
//        printf("Balanced octree: %d entities in node of depth %d\n", nu_entities, depth);
        if (depth > 0)
            delete [] input_entity_array;
        return;
    }
#if 0
    if (nu_input_entities == 1 && sre_internal_octree_type != SRE_OCTREE_STRICT &&
    sre_internal_octree_type != SRE_QUADTREE_XY_STRICT) {
        // When there is only one entity left, simply set the current node with the bounding AABB of the entity.
        // The octree implementation is flexible enough to handle this.
        if (input_entity_array[0].type == SRE_ENTITY_OBJECT)
            Initialize(input_entity_array[0].so->AABB.dim_min, input_entity_array[0].so->AABB.dim_max);
        else
            Initialize(input_entity_array[0].light->AABB.dim_min, input_entity_array[0].light->AABB.dim_max);
        nu_entities = 1;
        entity_array = new sreSceneEntity[1];
        entity_array[0] = input_entity_array[0];
        printf("Balanced octree: 1 entity (last object) in node of depth %d\n", depth);
        // It may be better to store the object in the list for the node above.
    }
    if (nu_input_entities == 2 && (sre_internal_octree_type == SRE_OCTREE_BALANCED ||
    sre_internal_octree_type == SRE_OCTREE_MIXED_WITH_QUADTREE || sre_internal_octree_type == SRE_QUADTREE_XY_BALANCED)) {
        // When there are only entities left, try to split them.
        sreBoundingVolumeAABB *AABB1p, *AABB2p;
        if (input_entity_array[0].type == SRE_ENTITY_OBJECT)
            AABB1p = &input_entity_array[0].so->AABB;
        else
            AABB1p = &input_entity_array[0].light->AABB;
        if (input_entity_array[1].type == SRE_ENTITY_OBJECT)
            AABB2p = &input_entity_array[1].so->AABB;
        else
            AABB2p = &input_entity_array[1].light->AABB;
        if (!Intersects(*AABB1p, *AABB2p)) {
            // The do not intersect.
            // Split them in x coordinate.
            float x_split;
            bool is_left;
            if (AABB1p->dim_max.x <= AABB2p->dim_min.x) {
                // AABB1 is "to the left" of AABB2.
                x_split = (AABB1p->dim_max.x + AABB2p->dim_min.x) * 0.5f;
                is_left = true;
            }
            else {
                // AABB2 must be on the other side of AABB1
                x_split = (AABB2p->dim_max.x + AABB1p->dim_min.x) * 0.5f;
                is_left = false;
            }
            nu_entities = 0;
            subnode[0] = new Octree(AABB.dim_min, Vector3D(x_split, AABB.dim_max.y, AABB.dim_max.z));
            subnode[1] = new Octree(Vector3D(x_split, AABB.dim_min.y, AABB.dim_min.z), AABB.dim_max);
            subnode[0]->nu_entities = 1;
            subnode[1]->nu_entities = 1;
            subnode[0]->entity_array = new sreSceneEntity[1];
            subnode[1]->entity_array = new sreSceneEntity[1];
            if (is_left) {
                subnode[0]->entity_array[0] = input_entity_array[0];
                subnode[1]->entity_array[0] = input_entity_array[1];
            }
            else {
                subnode[0]->entity_array[0] = input_entity_array[1];
                subnode[1]->entity_array[0] = input_entity_array[0];
            }
            printf("Balanced octree: 2 entities (last objects) split in nodes of depth %d\n", depth + 1);
            if (depth > 0)
                delete [] input_entity_array;
            return;
        }
    }
#endif
    sreBoundingVolumeAABB octant_AABB[8];
    int best_nu_octants;
    int custom_smallest_dimension = - 1;
    if (sre_internal_octree_type == SRE_OCTREE_BALANCED || sre_internal_octree_type == SRE_OCTREE_MIXED_WITH_QUADTREE) {
        // Calculate the unified AABB of the objects that are to be inserted.
        sreBoundingVolumeAABB unified_AABB;
        if (input_entity_array[0].type == SRE_ENTITY_OBJECT)
            unified_AABB = input_entity_array[0].so->AABB;
        else
            unified_AABB = input_entity_array[0].light->AABB;
        for (int i = 1; i < nu_input_entities; i++)
            if (input_entity_array[i].type == SRE_ENTITY_OBJECT)
                UpdateAABB(unified_AABB, input_entity_array[i].so->AABB);
            else
                UpdateAABB(unified_AABB, input_entity_array[i].light->AABB);
        Vector3D dim = unified_AABB.dim_max - unified_AABB.dim_min;
//        printf("Unified octree dimensions %f, %f, %f.\n", dim.x, dim.y, dim.z);
        // Sort dimensions in order of size.
        int dimension[3];
        if (dim.x < dim.y)
            if (dim.x < dim.z) {
                dimension[0] = 0;
                if (dim.y < dim.z) {
                    dimension[1] = 1;
                    dimension[2] = 2;
                }
                else { 
                    dimension[1] = 2;
                    dimension[2] = 1;
                }
            }
            else {
                dimension[0] = 2;
                dimension[1] = 0;
                dimension[2] = 1;
            }
        else
            if (dim.y < dim.z) {
                dimension[0] = 1;
                if (dim.x < dim.z) {
                    dimension[1] = 0;
                    dimension[2] = 2;
                }
                else {
                    dimension[1] = 2;
                    dimension[2] = 0;
                }
            }
            else {
                dimension[0] = 2;
                dimension[1] = 1;
                dimension[2] = 0;
            }
//        printf("AABB dimensions %f, %f, %f.\n", dim[dimension[0]], dim[dimension[1]], dim[dimension[2]]);
#if 1
        // If the greatest dimension of the unified AABB is smaller than two times the smallest dimension,
        // set the AABB of the current node to the unified AABB.
        if (dim[dimension[2]] / dim[dimension[0]] <= 2.0) {
//            printf("Setting node AABB to unified entity AABB.\n");
            AABB = unified_AABB;
            sphere.center = (AABB.dim_min + AABB.dim_max) * 0.5f;
        }
        else
        if (sre_internal_octree_type == SRE_OCTREE_MIXED_WITH_QUADTREE) {
            // Otherwise, create subnodes in the unified AAAB that are roughly square.
//            printf("Smallest dimension * 2.0 < largest dimension.\n");
            float r = dim[dimension[2]] / dim[dimension[1]];
            if (r <= 2.0 && r >= 0.5f) {
                // Create four subnodes in a square (2x2).
                // Calculate the extent of the smallest dimension that will still allow roughly square subnodes (to
                // within a factor of two).
                float max_dim0 = unified_AABB.dim_min[dimension[0]] + maxf(dim[dimension[1]], dim[dimension[2]]) * 0.5f;
#if 0
                float middle_dim1 = (unified_AABB.dim_min[dimension[1]] + unified_AABB.dim_max[dimension[1]]) * 0.5f;
                float middle_dim2 = (unified_AABB.dim_min[dimension[2]] + unified_AABB.dim_max[dimension[2]]) * 0.5f;
                // Start at the negative side of the middle and largest dimensions.
                octant_AABB[0].dim_min[dimension[0]] = unified_AABB.dim_min[dimension[0]];
                octant_AABB[0].dim_min[dimension[1]] = unified_AABB.dim_min[dimension[1]];
                octant_AABB[0].dim_min[dimension[2]] = unified_AABB.dim_min[dimension[2]];
                octant_AABB[0].dim_max[dimension[0]] = max_dim0;
                octant_AABB[0].dim_max[dimension[1]] = middle_dim1; 
                octant_AABB[0].dim_max[dimension[2]] = middle_dim2;
                // Positive side of the middle dimension, negative side of the largest dimension.
                octant_AABB[1].dim_min[dimension[0]] = unified_AABB.dim_min[dimension[0]];
                octant_AABB[1].dim_min[dimension[1]] = middle_dim1;
                octant_AABB[1].dim_min[dimension[2]] = unified_AABB.dim_min[dimension[2]];
                octant_AABB[1].dim_max[dimension[0]] = max_dim0;
                octant_AABB[1].dim_max[dimension[1]] = unified_AABB.dim_max[dimension[1]]; 
                octant_AABB[1].dim_max[dimension[2]] = middle_dim2;
                // Negative side of the middle and positive side of the middle dimension.
                octant_AABB[2].dim_min[dimension[0]] = unified_AABB.dim_min[dimension[0]];
                octant_AABB[2].dim_min[dimension[1]] = unified_AABB.dim_min[dimension[1]];
                octant_AABB[2].dim_min[dimension[2]] = middle_dim2;
                octant_AABB[2].dim_max[dimension[0]] = max_dim0;
                octant_AABB[2].dim_max[dimension[1]] = middle_dim1; 
                octant_AABB[2].dim_max[dimension[2]] = unified_AABB.dim_max[dimension[2]];
                // Positive side of the middle dimension, positive side of the middle dimension.
                octant_AABB[3].dim_min[dimension[0]] = unified_AABB.dim_min[dimension[0]];
                octant_AABB[3].dim_min[dimension[1]] = middle_dim1;
                octant_AABB[3].dim_min[dimension[2]] = middle_dim2;
                octant_AABB[3].dim_max[dimension[0]] = max_dim0;
                octant_AABB[3].dim_max[dimension[1]] = unified_AABB.dim_max[dimension[1]]; 
                octant_AABB[3].dim_max[dimension[2]] = unified_AABB.dim_max[dimension[2]];
                best_nu_octants = 4;
#endif
//                printf("Small dimension detected, creating four subnodes.\n");
                custom_smallest_dimension = dimension[0];
                // Set the AABB of the node to the custom AABB.
                AABB.dim_min[dimension[0]] = unified_AABB.dim_min[dimension[0]];
                AABB.dim_min[dimension[1]] = unified_AABB.dim_min[dimension[1]];
                AABB.dim_min[dimension[2]] = unified_AABB.dim_min[dimension[2]];
                AABB.dim_max[dimension[0]] = max_dim0;
                AABB.dim_max[dimension[1]] = unified_AABB.dim_max[dimension[1]]; 
                AABB.dim_max[dimension[2]] = unified_AABB.dim_max[dimension[2]];
                sphere.center = (AABB.dim_min + AABB.dim_max) * 0.5f;
            }
        }
#endif
    }

    // Calculate the average center position of all entities that are to be inserted.
    Point3D average_center = Point3D(0, 0, 0);
    for (int i = 0; i < nu_input_entities; i++)
        if (input_entity_array[i].type == SRE_ENTITY_OBJECT)
            average_center += input_entity_array[i].so->sphere.center;
        else
            average_center += input_entity_array[i].light->sphere.center;
    average_center /= nu_input_entities;
    // Don't allow the middle point to deviate more than the offset amount from the geometrical center of the node.
#if 0
    average_center.x = maxf(average_center.x, sphere.center.x - (AABB.dim_max.x - AABB.dim_min.x) * MIDDLE_OFFSET);
    average_center.y = maxf(average_center.y, sphere.center.y - (AABB.dim_max.y - AABB.dim_min.y) * MIDDLE_OFFSET);
    average_center.z = maxf(average_center.z, sphere.center.z - (AABB.dim_max.z - AABB.dim_min.z) * MIDDLE_OFFSET);
    average_center.x = minf(average_center.x, sphere.center.x + (AABB.dim_max.x - AABB.dim_min.x) * MIDDLE_OFFSET);
    average_center.y = minf(average_center.y, sphere.center.y + (AABB.dim_max.y - AABB.dim_min.y) * MIDDLE_OFFSET);
    average_center.z = minf(average_center.z, sphere.center.z + (AABB.dim_max.z - AABB.dim_min.z) * MIDDLE_OFFSET);
#endif
    // Try different middle points: the geometrical center, the average center position of the entities,
    // and offsets into the eight octants, plus quadtree configurations for each dimension with the geometrical
    // center of the split dimensions, the average center, and four offsets into the four quarters.
    // If no object fits any node, this will select the geometrical center octree.
    // If all entities fit into nodes, this will select the first middle point for which that is the case
    // (priority for the geometrical center octree because it is first).
    int min_left_over_entities = nu_input_entities + 1;
    Point3D best_middle_point;
    best_nu_octants = 8;
    for (int k = 0; k < 28; k++) {
        Point3D middle_point;
        int nu_octants;
        nu_octants = 8;
        if (custom_smallest_dimension >= 0) {
            switch (custom_smallest_dimension) {
            case 0 : // x is the smallest dimension
                if (k != 12 && k != 13 && (k < 20 || k >= 24))
                    continue;
                break;
            case 1 : // y is the smallest dimension
                if (k != 14 && k != 15 && k < 24)
                    continue;
                break;
            case 2 : // z is the smallest dimension
                if (k != 10 && k != 11 && (k < 16 || k >= 20))
                    continue;
                break;
            }
        }
        else
        switch (sre_internal_octree_type) {
        case SRE_OCTREE_STRICT :
        case SRE_OCTREE_STRICT_OPTIMIZED :
            if (k > 0)
                continue;
            break;
        case SRE_OCTREE_BALANCED :
            if (k >= 10)
                continue;
            break;
        case SRE_QUADTREE_XY_STRICT :
        case SRE_QUADTREE_XY_STRICT_OPTIMIZED :
            if (k != 10)
                continue;
            break;
        case SRE_QUADTREE_XY_BALANCED :
            if (k != 10 && k != 11 && (k < 16 || k >= 20))
                continue;
            break;
        case SRE_OCTREE_MIXED_WITH_QUADTREE :
            break;
        }
        switch (k) {
        case 0 :
            middle_point = sphere.center; // The geometrical center.
            break;
        case 1 :
            middle_point = average_center; // The average center position of the entities.
            break;
        case 10 :
            // Try a quadtree subdivision with geometrical x/y center position as middle point.
            middle_point = Point3D(sphere.center.x, sphere.center.y, AABB.dim_max.z);
            nu_octants = 4;
            break;
        case 11 :
            // Try a quadtree subdivision with the average x/y center position as middle point.
            middle_point = Point3D(average_center.x, average_center.y, AABB.dim_max.z);
            nu_octants = 4;
            break;
        case 12 :
            // Try a quadtree subdivision with geometrical y/z center position as middle point.
            middle_point = Point3D(AABB.dim_max.x, sphere.center.y, sphere.center.z);
            nu_octants = 4;
            break;
        case 13 :
            // Try a quadtree subdivision with the average y/z center position as middle point.
            middle_point = Point3D(AABB.dim_max.x, average_center.y, average_center.z);
            nu_octants = 4;
            break;
        case 14 :
            // Try a quadtree subdivision with geometrical x/z center position as middle point.
            middle_point = Point3D(sphere.center.x, AABB.dim_max.y, sphere.center.z);
            nu_octants = 4;
            break;
        case 15 :
            // Try a quadtree subdivision with the average x/z center position as middle point.
            middle_point = Point3D(average_center.x, AABB.dim_max.y, average_center.z);
            nu_octants = 4;
            break;
        default :
            if (k >= 2 && k < 10) {
                // Try a middle point offset into each of the eight octants.
                float dx = ((float)((k - 2) & 1) * 2.0f - 1.0f) * (AABB.dim_max.x - AABB.dim_min.x) * MIDDLE_OFFSET;
                float dy = ((float)((k - 2) & 2) * 1.0f - 1.0f) * (AABB.dim_max.y - AABB.dim_min.y) * MIDDLE_OFFSET;
                float dz = ((float)((k - 2) & 4) * 0.5f - 1.0f) * (AABB.dim_max.z - AABB.dim_min.z) * MIDDLE_OFFSET;
                middle_point = Point3D(sphere.center.x + dx, sphere.center.y + dy, sphere.center.z + dz);
                break;
            }
            if (k >= 16 && k < 20) {
                // Try a middle point offset into each of the four x/y quadtree quarters.
                float dx = ((float)((k - 16) & 1) * 2.0f - 1.0f) * (AABB.dim_max.x - AABB.dim_min.x) * MIDDLE_OFFSET;
                float dy = ((float)((k - 16) & 2) * 1.0f - 1.0f) * (AABB.dim_max.y - AABB.dim_min.y) * MIDDLE_OFFSET;
                middle_point = Point3D(sphere.center.x + dx, sphere.center.y + dy, AABB.dim_max.z);
                nu_octants = 4;
                break;
            }
            if (k >= 20 && k < 24) {
                // Try a middle point offset into each of the four y/z quadtree quarters.
                float dy = ((float)((k - 20) & 1) * 2.0f - 1.0f) * (AABB.dim_max.y - AABB.dim_min.y) * MIDDLE_OFFSET;
                float dz = ((float)((k - 20) & 2) * 1.0f - 1.0f) * (AABB.dim_max.z - AABB.dim_min.z) * MIDDLE_OFFSET;
                middle_point = Point3D(AABB.dim_max.x, sphere.center.y + dy, sphere.center.z + dz);
                nu_octants = 4;
                break;
            }
            if (k >= 24 && k < 28) {
                // Try a middle point offset into each of the four x/z quadtree quarters.
                float dx = ((float)((k - 24) & 1) * 2.0f - 1.0f) * (AABB.dim_max.x - AABB.dim_min.x) * MIDDLE_OFFSET;
                float dz = ((float)((k - 24) & 2) * 1.0f - 1.0f) * (AABB.dim_max.z - AABB.dim_min.z) * MIDDLE_OFFSET;
                middle_point = Point3D(sphere.center.x + dx, AABB.dim_max.y, sphere.center.z + dz);
                nu_octants = 4;
                break;
            }
        }
        // In the case of a quadtree subdivision four of the octants will be empty.
        CalculateOctantAABBs(nu_octants, AABB, middle_point, octant_AABB);
        char *middle_point_str = middle_point.GetString();
//        sreMessage(SRE_MESSAGE_LOG, "Trying middle point %s.", middle_point_str);
        delete [] middle_point_str;
        // Count the number of entities that do not fit entirely in one subnode.
        int left_over_entities = nu_input_entities;
        for (int i = 0; i < nu_input_entities; i++)
            for (int j = 0; j < nu_octants; j++)
                if (input_entity_array[i].type == SRE_ENTITY_OBJECT) {
                    if (IsCompletelyInside(input_entity_array[i].so->AABB, octant_AABB[j])) {
                        left_over_entities--;
                        break;
                    }
                }
                else
                if (IsCompletelyInside(*input_entity_array[i].light, octant_AABB[j])) {
                    left_over_entities--;
                    break;
                }
//        sreMessage(SRE_MESSAGE_LOG, "Octree node split method %d has %d left over entities.",
//            k, min_left_over_entities);
        if (left_over_entities < min_left_over_entities) {
            min_left_over_entities = left_over_entities;
            best_middle_point = middle_point;
            best_nu_octants = nu_octants;
            if (min_left_over_entities == 0)
                // If all entities fit into nodes, no need to try other configurations.
                break;
        }
    }
    CalculateOctantAABBs(best_nu_octants, AABB, best_middle_point, octant_AABB);
    sreMessage(SRE_MESSAGE_LOG, "Octree node split with %d left over entities.",
        min_left_over_entities);

    // Add entities that fit entirely in one node into the array for that node.
    sreSceneEntity *subnode_entity_array[8];
    int nu_subnode_entities[8];
    unsigned char *fits_in_node = new unsigned char[nu_input_entities];
    for (int i = 0; i < best_nu_octants; i++)
        subnode_entity_array[i] = new sreSceneEntity[nu_input_entities];
    for (int i = 0; i < best_nu_octants; i++)
        nu_subnode_entities[i] = 0;
    int left_over_entities = nu_input_entities;
    for (int i = 0; i < nu_input_entities; i++) {
        fits_in_node[i] = 0xFF;
        for (int j = 0; j < best_nu_octants; j++) {
            bool fits;
            if (input_entity_array[i].type == SRE_ENTITY_OBJECT)
                fits = IsCompletelyInside(input_entity_array[i].so->AABB, octant_AABB[j]);
            else
                fits = IsCompletelyInside(*input_entity_array[i].light, octant_AABB[j]);
            if (fits) {
                subnode_entity_array[j][nu_subnode_entities[j]] = input_entity_array[i];
                nu_subnode_entities[j]++;
                fits_in_node[i] = j;
                left_over_entities--;
                break;
            }
        }
    }
#if 0
    if (nu_input_entities - min_left_over_entities <= 2) {
        // When there are two or less entities left for subnodes, and they don't all fit into the same subnode,
        // add them all to the array of the current node along with the entities that do not fit into subnode.
        bool in_same_node = true;
        if (nu_input_entities - min_left_over_entities == 2) {
            int j = - 1;
            for (int i = 0; i < nu_input_entities; i++)
                if (fits_in_node[i] != 0xFF) {
                    if (j == - 1)
                        j = fits_in_node[i];
                    else
                        if (fits_in_node[i] != j) {
                            in_same_node = false;
                            break;
                        }
                }
        }
        if (!in_same_node) {
            entity_array = new sreSceneEntity[nu_input_entities];
            nu_entities = 0;
            for (int i = 0; i < nu_input_entities; i++) {
                entity_array[nu_entities] = input_entity_array[i];
                nu_entities++;
            }
            printf("Balanced octree: %d entities in node of depth %d\n", nu_input_entities, depth);
            delete [] fits_in_node;
            for (int i = 0; i < best_nu_octants; i++)
                delete [] subnode_entity_array[i];
            if (depth > 0)
                delete [] input_entity_array;
            return;
        }
    }
#endif
#define NO_SINGLE_ENTITY_NODES
#ifdef NO_SINGLE_ENTITY_NODES
    int nodes_with_single_entity = 0;
    for (int i = 0; i < best_nu_octants; i++)
        if (nu_subnode_entities[i] == 1)
            nodes_with_single_entity++;
#endif
    // Put the left over entities that do not fit entirely into a subnode into the array of entities for this node.
    // Also add entities that fit into a subnode but are the only entity selected for that subnode.
    nu_entities = 0;
#ifdef NO_SINGLE_ENTITY_NODES
    if (left_over_entities + nodes_with_single_entity > 0) {
        entity_array = new sreSceneEntity[left_over_entities + nodes_with_single_entity];
#else
    if (left_over_entities > 0) {
        entity_array = new sreSceneEntity[left_over_entities];
#endif
        for (int i = 0; i < nu_input_entities; i++) {
            bool only_entity_in_node = false;
#ifdef NO_SINGLE_ENTITY_NODES
            if (fits_in_node[i] != 0xFF)
                if (nu_subnode_entities[fits_in_node[i]] == 1)
                    only_entity_in_node = true;
#endif
            if (fits_in_node[i] == 0xFF || only_entity_in_node) {
                entity_array[nu_entities] = input_entity_array[i];
                nu_entities++;
            }
        }
    }
//    printf("Balanced octree: %d entities in node of depth %d, %d octants\n", nu_entities, depth, best_nu_octants);
    delete [] fits_in_node;
    if (depth > 0)
        delete [] input_entity_array;
    // Recursively process the subnodes.
    for (int i = 0; i < best_nu_octants; i++) {
        // Note that for subnodes with only one entity, the entity was added to the array of the current node,
        // so no subnode is necessary.
#ifdef NO_SINGLE_ENTITY_NODES
        if (nu_subnode_entities[i] <= 1) {
#else
        if (nu_subnode_entities[i] == 0) {
#endif
            subnode[i] = NULL; // Not strictly necessary, should be already initialized with NULL.
            delete [] subnode_entity_array[i];
        }
        else {
            subnode[i] = new Octree(octant_AABB[i].dim_min, octant_AABB[i].dim_max);
            subnode[i]->AddEntitiesBalanced(nu_subnode_entities[i], subnode_entity_array[i], depth + 1);
        }
    }
}

void Octree::AddEntitiesBalancedAtRootLevel(int nu_input_entities, sreSceneEntity *input_entity_array) {
    sreSceneEntity *new_entity_array = new sreSceneEntity[nu_entities + nu_input_entities];
    for (int i = 0; i < nu_entities; i++)
        new_entity_array[i] = entity_array[i];
    for (int i = 0; i < nu_input_entities; i++)
        new_entity_array[nu_entities + i] = input_entity_array[i];
    if (nu_entities > 0)
        delete [] entity_array;
    entity_array = new_entity_array;
    nu_entities += nu_input_entities;
}

// Add an entity to a pre-existing balanced octree, no new nodes are created.

void Octree::AddEntityIntoBalancedOctree(const sreSceneEntity& entity) {
    // Check whether the entity fits in one of the octants.
    for (int i = 0; i < 8; i++) {
        if (subnode[i] != NULL) {
            bool fits = false;
            if (entity.type == SRE_ENTITY_OBJECT)
                fits = IsCompletelyInside(entity.so->AABB, subnode[i]->AABB);
            else
                fits = IsCompletelyInside(*entity.light, subnode[i]->AABB);
            if (fits) {
                subnode[i]->AddEntityIntoBalancedOctree(entity);
                return;
            }
        }
    }

    // If it does not fit, add the entity to the array of this node.
    AddEntityIntoBalancedOctreeAtRootLevel(entity);
}

void Octree::AddEntityIntoBalancedOctreeAtRootLevel(const sreSceneEntity& entity) {
    sreSceneEntity *new_entity_array = new sreSceneEntity[nu_entities + 1];
    for (int i = 0; i < nu_entities; i++)
        new_entity_array[i] = entity_array[i];
    new_entity_array[nu_entities] = entity;;
    delete [] entity_array;
    entity_array = new_entity_array;
    nu_entities++;
}

// Fast octree implementation.

static int array_index;
static int node_index;

// Conversion to optimized "fast" octree.

void Octree::ConvertToFastOctreeRecursive(sreFastOctree& fast_oct) const {
    // Copy node bounds information.
    fast_oct.node_bounds[node_index].AABB = AABB;
    fast_oct.node_bounds[node_index].sphere = sphere;
    // Check which octants are present.
    // The number of non-empty octants is stored in count.
    int octant_flags = 0;
    int b = 1;
    int count = 0;
    int first_octant = - 1;
    for (int i = 0; i < 8; i++) {
        if (subnode[i] != NULL) {
            octant_flags |= b;
            count++;
            if (first_octant == -1)
                first_octant = i;
        }
        b <<= 1;
    }
    // Write the encoded node data.
    if (sre_internal_octree_type == SRE_OCTREE_STRICT_OPTIMIZED || sre_internal_octree_type ==
    SRE_QUADTREE_XY_STRICT_OPTIMIZED) {
        // Don't store the node index for the optimized strict octree/quadtree.
        // Encode the first integer with the number of octants in the first byte
        // and the indices of non-empty octants in bits 8-31 (3 bits per octant, value 0-7).
        unsigned int value = count;
        for (int i = 0; i < 8; i++)
            if (octant_flags & (1 << i))
                value += i << (i * 3);
        fast_oct.array[array_index + 0] = value;
        fast_oct.array[array_index + 1] = nu_entities;
        array_index += 2;
    }
    else {
        // In the regular fast octree, only the number of octants is stored; octants
        // do not have a specific order, and can have any AABB (as long as it is inside
        // the current node's AABB). The AABB for each node is stored seperately.
        fast_oct.array[array_index] = node_index;
        fast_oct.array[array_index + 1] = count;
        fast_oct.array[array_index + 2] = nu_entities;
        array_index += 3;
    }
    node_index++;
    // Write the encoded entities.
    for (int i = 0; i < nu_entities; i++) {
        if (entity_array[i].type == SRE_ENTITY_OBJECT)
            fast_oct.array[array_index] = entity_array[i].so->id;
        else if (entity_array[i].type == SRE_ENTITY_LIGHT)
            fast_oct.array[array_index] = entity_array[i].light->id | 0x80000000;
        array_index++;
    }
    // Return when there are no non-empty subnodes.
    if (count == 0)
        return;
    // Remember the location where the array indices of the non-empty octant subnode
    // data are stored. They will be filled in later.
    int octant_indices_location = array_index;
    // Set the current global array index to just beyond the current node (after
    // the octant array indices). Subnode data will be written there.
    array_index += count;
    // Keep index of the original octree's subnode octant in j (range from 0 to 7).
    int j = 0;
    // Iterate over the non-empty octants.
    for (int i = 0; i < count; i++) {
        // Find a non-empty subnode the source octree (it is guaranteed that at
        // least one is left).
        while (subnode[j] == NULL)
            j++;
        // Set the array index pointer in the current node to where the data of the subnode
        // is stored. The subnode data will be written at the current global array index.
        fast_oct.array[octant_indices_location + i] = array_index;
        // Recursively convert the subnode. Data will be written at the current global array
        // index.
        subnode[j]->ConvertToFastOctreeRecursive(fast_oct);
        // Go to the next octant of the original octree's node.
        j++;
    }
}

// Convert octree to fast octree with the given number of nodes, leafs and entities.

void Octree::ConvertToFastOctree(sreFastOctree& fast_oct, int counted_nodes, int counted_leafs,
int counted_entities) const {
    int size;
    if (sre_internal_octree_type == SRE_OCTREE_STRICT_OPTIMIZED || sre_internal_octree_type ==
    SRE_QUADTREE_XY_STRICT_OPTIMIZED)
        size = counted_nodes * 2 + counted_leafs + counted_entities;
    else
        size = counted_nodes * 3 + counted_leafs + counted_entities;
    sreMessage(SRE_MESSAGE_INFO,
        "Creating fast octree (%d nodes, %d leafs, %d entities), array size = %d.",
        counted_nodes, counted_leafs, counted_entities, size);
    fast_oct.node_bounds = new sreOctreeNodeBounds[counted_nodes];
    fast_oct.array = new unsigned int[size];
    array_index = 0;
    node_index = 0;
    ConvertToFastOctreeRecursive(fast_oct);
//    printf("node_index = %d, array_index = %d\n", node_index, array_index);
}

// Convert octree to fast octree, first counting the required number of nodes, leafs
// and entities.

void Octree::ConvertToFastOctree(sreFastOctree& fast_oct) {
    int counted_nodes, counted_leafs, counted_entities;
    counted_nodes = 0;
    counted_leafs = 0;
    counted_entities = 0;
    CountNodes(counted_nodes, counted_leafs, counted_entities);
    ConvertToFastOctree(fast_oct, counted_nodes, counted_leafs, counted_entities);
    // Free the original octree.
    MakeEmpty();
}

void sreFastOctree::Destroy() {
    delete [] node_bounds;
    delete [] array;
}

// Create the scene octrees (in sreFastOctree format).

void sreScene::CreateOctrees() {
    sreMessage(SRE_MESSAGE_INFO, "Creating octrees.");
    // First calculate the AABB for all static geometry objects and static (or bound) lights and
    // determine the maximum extents of all static entities combined.
    sreBoundingVolumeAABB AABB;
    AABB.dim_min = Vector3D(POSITIVE_INFINITY_FLOAT, POSITIVE_INFINITY_FLOAT, POSITIVE_INFINITY_FLOAT);
    AABB.dim_max = Vector3D(NEGATIVE_INFINITY_FLOAT, NEGATIVE_INFINITY_FLOAT, NEGATIVE_INFINITY_FLOAT);
    for (int i = 0; i < nu_objects; i++)
        if (!((object[i]->flags & SRE_OBJECT_DYNAMIC_POSITION) ||
        (object[i]->flags & SRE_OBJECT_INFINITE_DISTANCE))) {
            object[i]->CalculateAABB();
            UpdateAABB(AABB, object[i]->AABB);
        }
    for (int i = 0; i < nu_lights; i++)
        if (!(light[i]->type & SRE_LIGHT_DIRECTIONAL) &&
        (!(light[i]->type & SRE_LIGHT_DYNAMIC_LIGHT_VOLUME) ||
        (light[i]->type & SRE_LIGHT_WORST_CASE_BOUNDS_SPHERE)))
            UpdateAABB(AABB, light[i]->AABB);
    if (AABB.dim_min.x == POSITIVE_INFINITY_FLOAT || AABB.dim_max.x == NEGATIVE_INFINITY_FLOAT) {
        AABB.dim_min.Set(0, 0, 0);
        AABB.dim_max.Set(0, 0, 0);
    }

    sreBoundingVolumeAABB root_AABB;
    if (sre_internal_octree_type == SRE_OCTREE_BALANCED ||
    sre_internal_octree_type == SRE_QUADTREE_XY_BALANCED) {
        // Octree is of a type that is dynamically balanced during creation by varying the
        // middle point of each node.
        // Calculate the extents in all three dimensions.
        Vector3D extents = AABB.dim_max - AABB.dim_min;
        int largest_dim = 0;
        if (extents.y > extents.x) {
            largest_dim = 1;
            if (extents.z > extents.y)
                largest_dim = 2;
        }
        else if (extents.z > extents.x)
            largest_dim = 2;
        float max_extents = extents[largest_dim];
        max_extents += 0.0001f * max_extents;
        root_AABB.dim_min[largest_dim] = AABB.dim_min[largest_dim];
        root_AABB.dim_max[largest_dim] = AABB.dim_max[largest_dim];
        // Put the scene contents for the two non-largest dimensions in the center of the root AABB.
        // Balancing will make sure this won't be a problem.
        for (int i = 0; i < 3; i++)
            if (i != largest_dim) {
                float space = (max_extents - extents[i]) * 0.5f;
                root_AABB.dim_min[i] = AABB.dim_min[i] - space;
                root_AABB.dim_max[i] = AABB.dim_max[i] + space;
            }
        goto create_octrees;
    }

    // The octree is of a more regular type.
    {
    float minxyz = min3f(AABB.dim_min.x, AABB.dim_min.y, AABB.dim_min.z);
    float maxxyz = max3f(AABB.dim_max.x, AABB.dim_max.y, AABB.dim_max.z);
    float max_dim = maxxyz - minxyz;
    // Make it a little larger so that intersection tests at the borders produce expected results.
    minxyz -= 0.001f * max_dim;
    maxxyz += 0.001f * max_dim;
    max_dim = maxxyz - minxyz;
    
    // When there is one relatively flat dimension (commonly z in a scene with a objects on the
    // ground), we want to avoid to have the ground level (z = 0) very close to a top-level octree
    // node boundary level because most objects on the ground are likely to have bounding volumes
    // just extending below ground level, which would result in those objects all being stored in a
    // top-level octree node. To remedy this, we try to align octree extents for any smaller dimensions
    // so that all entities fit comfortably in a single octree node in that dimension for a certain
    // octree depth (octree nodes at that depth may of course be further subdivided during octree
    // creation).
    // Note: This only helps for regular power-of-2 octrees like SRE_OCTREE_STRICT; it won't help
    // for the default SRE_OCTREE_BALANCED octrees because the node size is variable. However, the
    // balanced octree should be able for the most part to avoid the problem automatically.
    for (int i = 0; i < 3; i++) {
        float dim_offset = 0;
        // Calculate the deepest octree depth (smallest octree node size) for which all
        // entities would still fit with a little room within the node size for the dimension.
        float octree_depth = floorf(log2f(max_dim / (AABB.dim_max[i] - AABB.dim_min[i])) - 0.01f);
        // For the largest dimensions, octree_depth will be 0. However, for smaller dimensions,
        // (much) less octree nodes across that dimension may be required.
        if (octree_depth > 0) {
            octree_depth = minf(octree_depth, SRE_MAX_OCTREE_DEPTH);
            // Calculate the size of the nodes at that depth.
            float node_size = max_dim / powf(2.0f, octree_depth);
#if 0
            printf("octree_depth = %lf, node_size = %lf, AABB range = [%lf, %lf], minxyz = %lf, "
                "maxxyz = %lf\n",
                octree_depth, node_size, AABB.dim_min[i], AABB.dim_max[i], minxyz, maxxyz);
#endif
            // Calculate an offset for the octree extents in the dimension so that the
            // entities fit neatly into a single octree node in that dimension.
            // Note: We add 0.1% of the node size so that there is some room at the node boundary; because
            // a little margin was used when calculating octree_depth, the entities should still fit
            // comfortably.
            dim_offset = node_size * 0.001f + node_size - fmodf(AABB.dim_min[i] - minxyz, node_size);
            // If the calculation went correctly, [minxyz + dim_offset, maxxyz + dim_offset]
            // should still comfortably cover the AABB range for the dimension.
            if (minxyz + dim_offset > AABB.dim_min[i] || maxxyz + dim_offset < AABB.dim_max[i]) {
               printf("Unexpected error aligning octree extents for dimension %d (displacement %lf); "
                   "applying no alignment.\n", i, dim_offset);
               dim_offset = 0;
            }
            else if (sre_internal_debug_message_level >= 1)
                printf("Octree shifted by %.4f in dimension %d to align entities with nodes.\n",
                    dim_offset, i);
               
        }
        root_AABB.dim_min[i] = minxyz + dim_offset;
        root_AABB.dim_max[i] = maxxyz + dim_offset;
    }
    }

create_octrees :
    char *min_str = root_AABB.dim_min.GetString();
    char *max_str = root_AABB.dim_max.GetString();
    sreMessage(SRE_MESSAGE_LOG, "Root octree dimensions: min %s, max %s.", min_str, max_str);
    delete [] min_str;
    delete [] max_str;

    // Create static octree and set dimensions.
    Octree octree_static;
    octree_static.Initialize(root_AABB.dim_min, root_AABB.dim_max);

    // Add the static objects to an entity array for use by the balanced tree building function.
    sreSceneEntity *entity_array = new sreSceneEntity[nu_objects + nu_lights]; // Upper limit of the size.
    int size = 0;
    for (int i = 0; i < nu_objects; i++)
        if (!((object[i]->flags & SRE_OBJECT_DYNAMIC_POSITION) ||
        (object[i]->flags & SRE_OBJECT_INFINITE_DISTANCE))) {
            entity_array[size].type = SRE_ENTITY_OBJECT;
            entity_array[size].so = object[i];
            size++;
        }
    // Add static local lights (any light that has a bounded light volume).
    for (int i = 0; i < nu_lights; i++)
        if (!(light[i]->type & SRE_LIGHT_DIRECTIONAL) &&
        (!(light[i]->type & SRE_LIGHT_DYNAMIC_LIGHT_VOLUME) ||
        (light[i]->type & SRE_LIGHT_WORST_CASE_BOUNDS_SPHERE))) {
            entity_array[size].type = SRE_ENTITY_LIGHT;
            entity_array[size].light = light[i];
            size++;
        }

    // Add the entities (create an octree with the entities).
    if (size > 0)
        octree_static.AddEntitiesBalanced(size, entity_array, 0);

    // Create a dynamic octree (with just a root node) and reset it.
    Octree octree_dynamic;
    octree_dynamic.nu_entities = 0;

    // Add dynamic objects to an entity array.
    size = 0;
    for (int i = 0 ; i < nu_objects; i++)
        if (object[i]->flags & SRE_OBJECT_DYNAMIC_POSITION) {
            entity_array[size].type = SRE_ENTITY_OBJECT;
            entity_array[size].so = object[i];
            size++;
        }
    // Add dynamic lights (any light with a dynamic light volume that do
    // not have worst caase bounds.
    for (int i = 0; i < nu_lights; i++)
        if (!(light[i]->type & SRE_LIGHT_DIRECTIONAL) &&
        ((light[i]->type & SRE_LIGHT_DYNAMIC_LIGHT_VOLUME) &&
        !(light[i]->type & SRE_LIGHT_WORST_CASE_BOUNDS_SPHERE))) {
            entity_array[size].type = SRE_ENTITY_LIGHT;
            entity_array[size].light = light[i];
            size++;
        }

    // Add the dynamic objects at the root node of the dynamic entity octree.
    if (size > 0)
        octree_dynamic.AddEntitiesBalancedAtRootLevel(size, entity_array);

    // Create an octree for static infinite distance objects and reset it.
    // Ideally, it should define a (large) bounding volume and allow a few subnodes.
    Octree octree_static_infinite_distance;
    octree_static_infinite_distance.nu_entities = 0;

    // Add static infinite distance objects and directional lights that do not change direction
    // to the static infinite distance octree.
    size = 0;
    for (int i = 0; i < nu_objects; i++)
        if ((object[i]->flags & SRE_OBJECT_INFINITE_DISTANCE) &&
        !(object[i]->flags & SRE_OBJECT_DYNAMIC_POSITION)) {
            entity_array[size].type = SRE_ENTITY_OBJECT;
            entity_array[size].so = object[i];
            size++;
        }
    for (int i = 0; i < nu_lights; i++)
        if ((light[i]->type & SRE_LIGHT_DIRECTIONAL) &&
        !(light[i]->type & SRE_LIGHT_DYNAMIC_DIRECTION)) {
            entity_array[size].type = SRE_ENTITY_LIGHT;
            entity_array[size].light = light[i];
            size++;
        }
    // Add the entities.
    if (size > 0)
        octree_static_infinite_distance.AddEntitiesBalancedAtRootLevel(size, entity_array);

    // Create an octree for dynamic infinite distance objects and reset it.
    Octree octree_dynamic_infinite_distance;
    octree_dynamic_infinite_distance.nu_entities = 0;

    // Add dynamic infinite distance objects/dynamic directional lights
    // to the dynamic infinite distance octree.
    size = 0;
    for (int i = 0; i < nu_objects; i++)
        if ((object[i]->flags & SRE_OBJECT_INFINITE_DISTANCE) &&
        !(object[i]->flags & SRE_OBJECT_DYNAMIC_POSITION)) {
            entity_array[size].type = SRE_ENTITY_OBJECT;
            entity_array[size].so = object[i];
            size++;
        }
    for (int i = 0; i < nu_lights; i++)
        if ((light[i]->type & SRE_LIGHT_DIRECTIONAL) &&
        (light[i]->type & SRE_LIGHT_DYNAMIC_DIRECTION)) {
            entity_array[size].type = SRE_ENTITY_LIGHT;
            entity_array[size].light = light[i];
            size++;
        }
    // Add the entities.
    if (size > 0)
        octree_dynamic_infinite_distance.AddEntitiesBalancedAtRootLevel(size, entity_array);

    delete [] entity_array;

    // Convert the static octree, dynamic octree and both infinite distance octrees to
    // the "fast" octrees for the scene.
    octree_static.ConvertToFastOctree(fast_octree_static);
    octree_dynamic.ConvertToFastOctree(fast_octree_dynamic);
    octree_static_infinite_distance.ConvertToFastOctree(fast_octree_static_infinite_distance);
    octree_dynamic_infinite_distance.ConvertToFastOctree(fast_octree_dynamic_infinite_distance);

#if 0
    // Old implementation.
    // Add the static objects to the octree.
    for (int i = 0; i < nu_objects; i++)
        if (!((object[i]->flags & SRE_OBJECT_DYNAMIC_POSITION) ||
        (object[i]->flags & SRE_OBJECT_INFINITE_DISTANCE)))
            octree.AddsreObject(*object[i]);
    // Add the dynamic objects to the octree at root level.
    for (int i = 0; i < nu_objects; i++)
        if (object[i]->flags & SRE_OBJECT_DYNAMIC_POSITION)
            octree.AddsreObjectAtRootLevel(*object[i]);
    for (int i = 0; i < nu_objects; i++)
        if (object[i]->flags & SRE_OBJECT_INFINITE_DISTANCE)
            octree_infinite_distance.AddsreObjectAtRootLevel(*object[i]);
    // Add lights.
    for (int i = 0; i < nu_lights; i++)
        if (!(light[i]->type & SRE_LIGHT_DIRECTIONAL) &&
        !(light[i]->type & SRE_LIGHT_DYNAMIC_POSITION))
            octree.AddLight(*light[i]);
    for (int i = 0; i < nu_lights; i++)
        if (!(light[i]->type & SRE_LIGHT_DIRECTIONAL) &&
        (light[i]->type & SRE_LIGHT_DYNAMIC_POSITION))
            octree.AddLightAtRootLevel(*light[i]);
    for (int i = 0; i < nu_lights; i++)
        if (light[i]->type & SRE_LIGHT_DIRECTIONAL)
            octree_infinite_distance.AddLightAtRootLevel(*light[i]);
    int counted_nodes, counted_leafs, counted_entities;
    counted_nodes = 0;
    counted_leafs = 0;
    counted_entities = 0;
    octree.ConvertToArrays(counted_nodes, counted_leafs, counted_entities);
    octree.ConvertToFastOctree(fast_octree, counted_nodes, counted_leafs, counted_entities);
    octree.MakeEmpty();
    counted_nodes = 0;
    counted_leafs = 0;
    counted_entities = 0;
    octree_infinite_distance.ConvertToArrays(counted_nodes, counted_leafs, counted_entities);
    octree_infinite_distance.ConvertToFastOctree(fast_octree_infinite_distance, counted_nodes,
        counted_leafs, counted_entities);
    octree_infinite_distance.MakeEmpty();
#endif
}

// Implementation of sreSceneEntityList.

sreSceneEntityList::sreSceneEntityList() {
    head = NULL;
    tail = NULL;
}

void sreSceneEntityList::AddElement(sreSceneEntity *entity) {
// printf("Adding element with id %d.\n", so);
    if (tail == NULL) {
        head = tail = new sreSceneEntityListElement;
        head->next = NULL;
        head->entity = entity;
        return;
    }
    sreSceneEntityListElement *e = new sreSceneEntityListElement;
    tail->next = e;
    e->next = NULL;
    tail = e;
    e->entity = entity;
}

void sreSceneEntityList::DeleteElement(sreSceneEntity *entity) {
// printf("Deleting element with id %d.\n", so);
    // Handle the case where the element to be deleted is first in the list.
    if (head->entity == entity) {
        sreSceneEntityListElement *e = head;
        head = head->next;
        if (head == NULL)
            tail = NULL;
        delete e;
        return;
    }
    sreSceneEntityListElement *e = head;
    while (e->next != NULL) {
        if (e->next->entity == entity) {
            sreSceneEntityListElement *f = e->next;
            e->next = f->next;
            delete f;
            if (e->next == NULL)
                tail = e;
            return;
        }
        e = e->next;
    }
    printf("Error in DeleteElement -- could not find sreSceneEntity id in list.\n");
    exit(1);
}

void sreSceneEntityList::DeletesreObject(sreObject *so) {
// printf("Deleting element with id %d.\n", so);
    // Handle the case where the element to be deleted is first in the list.
    if (head->entity->type == SRE_ENTITY_OBJECT && head->entity->so == so) {
        sreSceneEntityListElement *e = head;
        head = head->next;
        if (head == NULL)
            tail = NULL;
        delete e->entity;
        delete e;
        return;
    }
    sreSceneEntityListElement *e = head;
    while (e->next != NULL) {
        if (e->next->entity->type == SRE_ENTITY_OBJECT && e->next->entity->so == so) {
            sreSceneEntityListElement *f = e->next;
            e->next = f->next; 
            delete f->entity;
            delete f;
            if (e->next == NULL)
                tail = e;
            return;
        }
        e = e->next;
    }
    printf("Error in DeleteElement -- could not find sreSceneEntity id in list.\n");
    exit(1);
}

sreSceneEntity *sreSceneEntityList::Pop() {
// printf("Popping element with id %d.\n", head->so);
    sreSceneEntity *entity = head->entity;
    sreSceneEntityListElement *e = head->next;
    if (tail == head)
        tail = NULL;
    delete head;
    head = e;
    return entity;
}

void sreSceneEntityList::MakeEmpty() {
    while (head != NULL) {
        sreSceneEntity *entity = Pop();
        delete entity;
    }
}

