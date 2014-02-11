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

// Constructor function for Scene.

sreScene::sreScene(int _max_scene_objects, int _max_models, int _max_scene_lights) {
    nu_objects = 0;
    nu_models = 0;
    max_scene_objects = _max_scene_objects;
    sceneobject = new sreObject *[max_scene_objects];
    max_models = _max_models;
    model = new sreModel *[max_models];
    nu_lights = 0;
    max_scene_lights = _max_scene_lights;
    global_light = new sreLight *[max_scene_lights];
    ambient_color = Color(0.1, 0.1, 0.1);
    // Scene building helpers.
    SetDiffuseReflectionColor(Color(1.0, 1.0, 1.0));
    current_flags = 0;
    current_texture = NULL;
    SetSpecularReflectionColor(Color(1.0, 1.0, 1.0));
    current_specular_exponent = 60.0;
    current_specularity_map = NULL;
    current_normal_map = NULL;
    current_emission_color = Color(0, 0, 0);
    current_emission_map = NULL;
    current_texture3d_scale = 1.0;
    current_texture3d_type = 0;
    current_mass = 1.0;
    current_roughness_values = Vector2D(0.15, 1.0);
    current_roughness_weights = Vector2D(1.0, 0);
    current_diffuse_fraction = 0.6;
    current_anisotropic = false;
    current_lod_flags = SRE_LOD_DYNAMIC;
    current_lod_level = 0;
    current_lod_threshold_scaling = 1.0;
    current_UV_transformation_matrix = sre_internal_standard_UV_transformation_matrix;
    deleted_ids = new sreObjectList;
    // No rendering object arrays allocated yet.
    max_visible_objects = 0;
    max_final_pass_objects = 0; 
    max_shadow_caster_objects = 0;
    max_visible_lights = 0;
}

// Make an already existing scene empty. Models are not affected.
// CreateOctrees() must be called before attempting to render a
// scene again.

void sreScene::Clear() {
    // The storage for the fast octrees is freed, but they are invalid
    // until CreateOctrees() is called again.
    fast_octree_static.Destroy();
    fast_octree_dynamic.Destroy();
    fast_octree_static_infinite_distance.Destroy();
    fast_octree_dynamic_infinite_distance.Destroy();
    for (int i = 0; i < nu_objects; i++)
        delete sceneobject[i];
    nu_objects = 0;
    for (int i = 0; i < nu_lights; i++)
        delete global_light[i];
    nu_lights = 0;
    deleted_ids->MakeEmpty();
}

void sreScene::PrepareForRendering(bool preprocess_static_scenery) {
    CreateOctrees();
    if (preprocess_static_scenery)
        Preprocess();
    RemoveUnreferencedModels();

    // Temporarily allocate visual object and shadow caster object arrays
    // with full capacity for shadow volume calculation.
    nu_visible_objects = 0;
    visible_object = new int[max_scene_objects];
    nu_shadow_caster_objects = 0;
    shadow_caster_object = new int[max_scene_objects];
    // Use visible_object and shadow_caster_object arrays as scratch memory.
    CalculateStaticLightObjectLists();
    delete [] visible_object;
    delete [] shadow_caster_object;

    // Set reasonable limits for number of visible objects/lights during
    // rendering. If the world is large, this can be much lower than the
    // total number of objects. The capacity will be automatically
    // increased when a limit is encountered.
    max_visible_objects = mini(SRE_DEFAULT_MAX_VISIBLE_OBJECTS, nu_objects);
    max_final_pass_objects = mini(SRE_DEFAULT_MAX_FINAL_PASS_OBJECTS, nu_objects);
    max_shadow_caster_objects = mini(SRE_DEFAULT_MAX_SHADOW_CASTER_OBJECTS, nu_objects);
    max_visible_lights = mini(SRE_DEFAULT_MAX_VISIBLE_LIGHTS, nu_lights);

    nu_visible_objects = 0;
    visible_object = new int[max_visible_objects];
    nu_shadow_caster_objects = 0;
    shadow_caster_object = new int[max_shadow_caster_objects];
    nu_final_pass_objects = 0;
    final_pass_object = new int[max_final_pass_objects];
    nu_visible_lights = 0;
    visible_light = new int[max_visible_lights];

    // Upload models to GPU memory.
    UploadModels();
}

// Scene builder helper functions.

void sreScene::SetColor(Color color) {
    current_diffuse_reflection_color = color;
}

void sreScene::SetFlags(int flags) {
    current_flags = flags;
}

void sreScene::SetDiffuseReflectionColor(Color color) {
    current_diffuse_reflection_color = color;
}

void sreScene::SetSpecularReflectionColor(Color color) {
    current_specular_reflection_color = color;
}

void sreScene::SetSpecularExponent(float exponent) {
    current_specular_exponent = exponent;
}

void sreScene::SetTexture(sreTexture *texture) {
    current_texture = texture;
}

void sreScene::SetSpecularityMap(sreTexture *texture) {
    current_specularity_map = texture;
}

void sreScene::SetNormalMap(sreTexture *texture) {
    current_normal_map = texture;
}

void sreScene::SetEmissionColor(Color color) {
    current_emission_color = color;
}

void sreScene::SetEmissionMap(sreTexture *texture) {
    current_emission_map = texture;
}

void sreScene::SetTexture3DTypeAndScale(int type, float scale) {
    current_texture3d_type = type;
    current_texture3d_scale = scale;
}

void sreScene::SetUVTransform(Matrix3D *matrix) {
    if (matrix == NULL)
        current_UV_transformation_matrix = sre_internal_standard_UV_transformation_matrix;
    else
        current_UV_transformation_matrix = matrix;
}

void sreScene::SetBillboardSize(float width, float height) {
    current_billboard_width = width;
    current_billboard_height = height;
}

void sreScene::SetHaloSize(float size) {
    current_halo_size = size;
}

void sreScene::SetMass(float m) {
    current_mass = m;
}

void sreScene::SetMicrofacetParameters(float diffuse_fraction, float roughness_value1, float weight1,
float roughness_value2, float weight2, bool anisotropic) {
    current_diffuse_fraction = diffuse_fraction;
    current_roughness_values = Vector2D(roughness_value1, roughness_value2);
    current_roughness_weights = Vector2D(weight1, weight2);
    current_anisotropic = anisotropic;
}

void sreScene::SetLevelOfDetail(int flags, int level, float scaling) {
    current_lod_flags = flags;
    current_lod_level = level;
    current_lod_threshold_scaling = scaling;
}

void sreScene::SetAmbientColor(Color color) {
    ambient_color = color;
}

// Object instantiation.

void sreScene::FinishObjectInstantiation(sreObject& so, bool rotated) const {
    so.inverted_model_matrix = Inverse(so.model_matrix);
    // Update attached light.
    if (so.attached_light != - 1) {
        ChangeLightPosition(so.attached_light,
            (so.model_matrix * Vector4D(so.attached_light_model_position, 1.0)).GetPoint3D());
    }
    // Update bounds.
    if (so.flags & SRE_OBJECT_PARTICLE_SYSTEM) {
        so.sphere.center = so.position;
        return;
    }
    if (so.flags & (SRE_OBJECT_LIGHT_HALO | SRE_OBJECT_BILLBOARD)) {
        so.sphere.center = so.position;
        return;
    }
    // Calculate bounding volume center in world space.
    so.sphere.center = (so.model_matrix * so.model->sphere.center).GetPoint3D();
    so.sphere.radius = so.model->sphere.radius * so.scaling;
    so.box.center = (so.model_matrix * so.model->box_center).GetPoint3D();
    // Rotate and scale principal component axi.
    so.box.PCA[0].vector = (so.rotation_matrix * so.model->PCA[0].vector) * so.model->PCA[0].size * so.scaling;
    so.box.PCA[1].vector = (so.rotation_matrix * so.model->PCA[1].vector) * so.model->PCA[1].size * so.scaling;
    so.box.PCA[0].scale_factor = 1.0f / (so.model->PCA[0].size * so.scaling);
    so.box.PCA[1].scale_factor = 1.0f / (so.model->PCA[1].size * so.scaling);
    if (so.model->PCA[2].size == 0.0) {
        so.box.PCA[2].SetSizeZero();
        so.box.T_normal = (so.rotation_matrix * so.model->PCA[2].vector);
    }
    else {
        so.box.PCA[2].vector = (so.rotation_matrix * so.model->PCA[2].vector) *
            so.model->PCA[2].size * so.scaling;
        so.box.PCA[2].scale_factor = 1.0f / (so.model->PCA[2].size * so.scaling);
    }
    if (so.model->bounds_flags & SRE_BOUNDS_PREFER_SPECIAL) {
        so.bv_special.type = so.model->bv_special.type;
        if (so.model->bv_special.type == SRE_BOUNDING_VOLUME_ELLIPSOID) {
            if (so.bv_special.ellipsoid == NULL)
               so.bv_special.ellipsoid = new sreBoundingVolumeEllipsoid;
            so.bv_special.ellipsoid->center = (so.model_matrix * so.model->bv_special.ellipsoid->center).GetPoint3D();
            so.bv_special.ellipsoid->PCA[0].vector =
                (so.rotation_matrix * so.model->bv_special.ellipsoid->PCA[0].vector) * so.scaling;
            so.bv_special.ellipsoid->PCA[1].vector =
                (so.rotation_matrix * so.model->bv_special.ellipsoid->PCA[1].vector) * so.scaling;
            so.bv_special.ellipsoid->PCA[2].vector =
                (so.rotation_matrix * so.model->bv_special.ellipsoid->PCA[2].vector) * so.scaling;
        }
        else if (so.model->bv_special.type ==  SRE_BOUNDING_VOLUME_CYLINDER) {
            if (so.bv_special.cylinder == NULL)
               so.bv_special.cylinder = new sreBoundingVolumeCylinder;
            so.bv_special.cylinder->center = (so.model_matrix * so.model->bv_special.cylinder->center).GetPoint3D();
            so.bv_special.cylinder->radius = so.model->bv_special.cylinder->radius * so.scaling;
            so.bv_special.cylinder->length = so.model->bv_special.cylinder->length * so.scaling;
            so.bv_special.cylinder->axis = so.rotation_matrix * so.model->bv_special.cylinder->axis;
        }
    }
    so.box.flags = so.model->bounds_flags;
    so.box.CalculatePlanes();
    if (!(so.flags & (SRE_OBJECT_DYNAMIC_POSITION | SRE_OBJECT_INFINITE_DISTANCE))) {
        // Static object.
        if ((so.model->bounds_flags & SRE_BOUNDS_PREFER_AABB) && (!rotated ||
        so.rotation_matrix.RotationMatrixPreservesAABB())) {
            so.box.flags |= SRE_BOUNDS_IS_AXIS_ALIGNED;
            // The actual AABB is calculated during Octree creation.
//            printf("AABB allowed as bounding box for object %d.\n", so.id);
        }
    }
}

void sreScene::InstantiateObject(int oi) const {
    sreObject *so = sceneobject[oi];
    float rot_x = so->rotation.x;
    float rot_y = so->rotation.y;
    float rot_z = so->rotation.z;
    float scaling = so->scaling;

    MatrixTransform rotation_matrix;
    bool rotated = true;
    if (rot_x == 0 && rot_y == 0 && rot_z == 0) {
        rotation_matrix.SetIdentity();
        so->rotation_matrix.SetIdentity();
        rotated = false;
    }
    else {
        MatrixTransform rot_x_matrix;
        rot_x_matrix.AssignRotationAlongXAxis(rot_x);
        MatrixTransform rot_y_matrix;
        rot_y_matrix.AssignRotationAlongYAxis(rot_y);
        MatrixTransform rot_z_matrix;
        rot_z_matrix.AssignRotationAlongZAxis(rot_z);
        rotation_matrix = (rot_x_matrix * rot_y_matrix) * rot_z_matrix;
        so->rotation_matrix.Set(
            rotation_matrix(0, 0), rotation_matrix(0, 1), rotation_matrix(0, 2),
            rotation_matrix(1, 0), rotation_matrix(1, 1), rotation_matrix(1, 2),
            rotation_matrix(2, 0), rotation_matrix(2, 1), rotation_matrix(2, 2));
    }

    MatrixTransform translation_matrix;
    translation_matrix.AssignTranslation(so->position);
    so->only_translation = false;
    if (scaling == 1) {
        if (rot_x == 0 && rot_y == 0 && rot_z == 0) {
            so->model_matrix = translation_matrix;
            so->only_translation = true;
        }
        else
            so->model_matrix = translation_matrix * rotation_matrix;
    }
    else {
        MatrixTransform scaling_matrix;
        scaling_matrix.AssignScaling(scaling);
        so->model_matrix = (translation_matrix * scaling_matrix) * rotation_matrix;
    }
    FinishObjectInstantiation(*so, rotated);
}

void sreScene::InstantiateObjectRotationMatrixAlreadySet(int oi) const {
    sreObject *so = sceneobject[oi];
    float scaling = so->scaling;

    MatrixTransform rotation_matrix;
    rotation_matrix.Set(
        so->rotation_matrix(0, 0), so->rotation_matrix(0, 1), so->rotation_matrix(0, 2), 0,
        so->rotation_matrix(1, 0), so->rotation_matrix(1, 1), so->rotation_matrix(1, 2), 0,
        so->rotation_matrix(2, 0), so->rotation_matrix(2, 1), so->rotation_matrix(2, 2), 0 /*,
        0, 0, 0, 1 */);

    MatrixTransform translation_matrix;
    translation_matrix.AssignTranslation(so->position);
    so->only_translation = false;
    if (scaling == 1) {
        so->model_matrix = translation_matrix * rotation_matrix;
    }
    else {
        MatrixTransform scaling_matrix;
        scaling_matrix.AssignScaling(scaling);
        so->model_matrix = (translation_matrix * scaling_matrix) * rotation_matrix;
    }
    FinishObjectInstantiation(*so, true);
}

int sreScene::AddObject(sreModel *model, float dx, float dy, float dz,
float rot_x, float rot_y, float rot_z, float scaling) {
    if (nu_objects == max_scene_objects && deleted_ids->head == NULL) {
        if (sre_internal_debug_message_level >= 1)
            printf("Maximum number of scene objects reached -- doubling capacity to %d\n",
                max_scene_objects * 2);
        sreObject **new_so_objects = new sreObject *[max_scene_objects * 2];
        memcpy(new_so_objects, sceneobject, sizeof(sreObject *) * max_scene_objects);
        delete [] sceneobject;
        sceneobject = new_so_objects;
        max_scene_objects *= 2;
    }
    int i;
    if (deleted_ids->head == NULL) {
        i = nu_objects;
        nu_objects++;
        // Create the sreObject
        sceneobject[i] = new sreObject;
    }
    else {
        i = deleted_ids->Pop();
        // The deleted sreObject allocation will be reused.
    }
    sreObject *so = sceneobject[i];
    so->model = model;
    so->exists = true;
    so->position.Set(dx, dy, dz);
    so->rotation.Set(rot_x, rot_y, rot_z);
    so->scaling = scaling;
    so->diffuse_reflection_color = current_diffuse_reflection_color;
    so->flags = current_flags;
    so->specular_reflection_color = current_specular_reflection_color;
    so->specular_exponent = current_specular_exponent;
    so->texture = current_texture;
    so->specularity_map = current_specularity_map;
    so->normal_map = current_normal_map;
    so->emission_map = current_emission_map;
    so->UV_transformation_matrix = current_UV_transformation_matrix;
    so->emission_color = current_emission_color;
    so->texture3d_scale = current_texture3d_scale;
    so->texture3d_type = current_texture3d_type;
    so->billboard_width = current_billboard_width;
    so->billboard_height = current_billboard_height;
    so->halo_size = current_halo_size;
    so->mass = current_mass;
    so->diffuse_fraction = current_diffuse_fraction;
    so->roughness_values = current_roughness_values;
    so->roughness_weights = current_roughness_weights;
    so->anisotropic = current_anisotropic;
    so->lod_flags = current_lod_flags;
    so->lod_level = current_lod_level;
    so->lod_threshold_scaling = current_lod_threshold_scaling;

    so->id = i;
    so->attached_light = - 1;
    so->nu_shadow_volumes = 0;
    so->most_recent_position_change = 0;
    so->most_recent_transformation_change = 0;
    so->rapid_change_flags = 0;
    so->bv_special.ellipsoid = NULL;
    so->most_recent_frame_visible = - 1;
    so->geometry_scissors_cache_timestamp = - 1;

    if ((so->flags & (SRE_OBJECT_DYNAMIC_POSITION | SRE_OBJECT_NO_PHYSICS)) ==
    (SRE_OBJECT_DYNAMIC_POSITION))
        // If the object is dynamic and affected by physics, force use of the object shadow volume
        // cache for stencil shadow volumes. In this case, the orientation is likely to
        // be changing a lot and will be unique for the object. Note that when the object changes
        // continuously (which is true for physics objects in motion), the shadow cache will be
        // skipped anyway. However, when the object is not in motion, the physics engine will no
        // longer update the transformation after a few seconds, so the shadow cache can be used.
        so->flags |= SRE_OBJECT_USE_OBJECT_SHADOW_CACHE;
    // Mark the model as referenced.
    model->referenced = true;
    // Determine which LOD model objects can be used by this object, and set the referenced flag for them.
    if (model->nu_lod_levels == 1)
        model->lod_model[0]->referenced = true;
    else {
        if (so->lod_flags & SRE_LOD_FIXED)
            model->lod_model[so->lod_level]->referenced = true;
        else
            for (int i = so->lod_level; i < model->nu_lod_levels; i++)
                model->lod_model[i]->referenced = true;
    }
    InstantiateObject(i);
    // When adding a billboard object, make sure the bounding sphere is properly set.
    if (so->flags & (SRE_OBJECT_LIGHT_HALO | SRE_OBJECT_BILLBOARD)) {
        Vector3D X = 0.5f * so->billboard_width * Vector3D(1.0f, 0, 0);
        Vector3D Y = 0.5f * so->billboard_height * Vector3D(0, 0, 1.0f);
        so->sphere.radius = Magnitude(X + Y);
        so->model->bounds_flags = SRE_BOUNDS_PREFER_SPHERE;
    }
    return i;
}

int sreScene::AddObject(sreModel *model, Point3D pos, Vector3D rot, float scaling) {
    return AddObject(model, pos.x, pos.y, pos.z, rot.x, rot.y, rot.z, scaling);
}

int sreScene::AddParticleSystem(sreModel *object, int _nu_particles, Point3D center,
float worst_case_bounding_sphere_radius, Vector3D *particles) {
    int i = AddObject(object, center.x, center.y, center.z, 0, 0, 0, 1.0f);
    // Override the bounding sphere radius (which was set for a single billboard/particle.
    sceneobject[i]->sphere.radius = worst_case_bounding_sphere_radius;
    sceneobject[i]->nu_particles = _nu_particles;
    sceneobject[i]->particles = particles;
    return i;
}

void sreScene::DeleteObject(int soi) {
    sreObject *so = sceneobject[soi];
    if (so->flags & SRE_OBJECT_PARTICLE_SYSTEM)
        delete so->particles;
    deleted_ids->AddElement(so->id);
    so->exists = false;
}

// Scene object dynamic change helper functions.

static void UpdateChangeTracking(sreObject& so, int mask) {
   // Do not change the position changing_every_frame flag when the position
   // was changed earlier during the same frame.
   if ((mask & SRE_OBJECT_POSITION_CHANGE) &&
   so.most_recent_position_change != sre_internal_current_frame) {
       // When the object position as changed since the last frame, set the flag.
       if (so.most_recent_position_change == sre_internal_current_frame - 1)
           so.rapid_change_flags |= SRE_OBJECT_POSITION_CHANGE;
       else 
           so.rapid_change_flags &= ~SRE_OBJECT_POSITION_CHANGE;
       so.most_recent_position_change = sre_internal_current_frame;
   }
   // Do not change the transformation changing_every_frame flag when the transformation
   // was changed earlier during the same frame.
   if ((mask & SRE_OBJECT_TRANSFORMATION_CHANGE) &&
   so.most_recent_transformation_change != sre_internal_current_frame) {
       if (so.most_recent_transformation_change == sre_internal_current_frame - 1)
           so.rapid_change_flags |= SRE_OBJECT_TRANSFORMATION_CHANGE;
       else
           so.rapid_change_flags &= ~SRE_OBJECT_TRANSFORMATION_CHANGE;
       so.most_recent_transformation_change = sre_internal_current_frame;
   }
}

void sreScene::ChangePosition(int soi, Point3D pos) const {
    if (pos == sceneobject[soi]->position)
        // Position didn't actually change.
        return;
    sceneobject[soi]->position= pos;
    InstantiateObject(soi);
    UpdateChangeTracking(*sceneobject[soi], SRE_OBJECT_POSITION_CHANGE);
}

void sreScene::ChangePosition(int soi, float x, float y, float z) const {
    ChangePosition(soi, Vector3D(x, y, z));
}

void sreScene::ChangeRotation(int soi, float rotx, float roty, float rotz) const {
#if 0
    // Since the rotation angles aren't updated when the rotation matrix is
    // changed, just skip the check. In practice (physics) the rotation martix
    // update method will be used.
    if (Vector3D(rotx, roty, rotz) == sceneobject[soi]->rotation)
        // Rotation didn't actually change.
        return;
#endif
    sceneobject[soi]->rotation.Set(rotx, roty, rotz);
    InstantiateObject(soi);
    UpdateChangeTracking(*sceneobject[soi], SRE_OBJECT_TRANSFORMATION_CHANGE);
}

void sreScene::ChangeRotationMatrix(int soi, const Matrix3D& rot) const {
    if (rot == sceneobject[soi]->rotation_matrix)
        // Position and rotation didn't actually change.
        return;
    sceneobject[soi]->rotation_matrix = rot;
    InstantiateObjectRotationMatrixAlreadySet(soi);
    UpdateChangeTracking(*sceneobject[soi], SRE_OBJECT_TRANSFORMATION_CHANGE);
}

void sreScene::ChangePositionAndRotation(int soi, float x, float y, float z,
float rotx, float roty, float rotz) const {
    // Since the rotation angles aren't updated when the rotation matrix is
    // changed, just assume the rotation has changed. In practice (physics)
    // the rotation martix update method will be used.
    int flags = SRE_OBJECT_TRANSFORMATION_CHANGE;
    if (Vector3D(x, y, z) != sceneobject[soi]->position)
        flags |= SRE_OBJECT_POSITION_CHANGE;
    sceneobject[soi]->position.Set(x, y, z);
    sceneobject[soi]->rotation.Set(rotx, roty, rotz);
    InstantiateObject(soi);
    UpdateChangeTracking(*sceneobject[soi], flags);
}

void sreScene::ChangePositionAndRotationMatrix(int soi, float x, float y, float z,
const Matrix3D& m_rot) const {
    int flags = 0;
    if (Vector3D(x, y, z) != sceneobject[soi]->position)
        flags |= SRE_OBJECT_POSITION_CHANGE;
    if (m_rot != sceneobject[soi]->rotation_matrix)
        flags |= SRE_OBJECT_TRANSFORMATION_CHANGE;
    if (flags == 0)
        // Position and rotation didn't actually change.
        return;
    sceneobject[soi]->position.Set(x, y, z);
    sceneobject[soi]->rotation_matrix = m_rot;
    InstantiateObjectRotationMatrixAlreadySet(soi);
    UpdateChangeTracking(*sceneobject[soi], flags);
}

void sreScene::ChangeBillboardSize(int object_index, float bb_width, float bb_height) const {
    sceneobject[object_index]->billboard_width = bb_width;
    sceneobject[object_index]->billboard_height = bb_height;
    Vector3D X = 0.5 * sceneobject[object_index]->billboard_width * Vector3D(1.0, 0, 0);
    Vector3D Y = 0.5 * sceneobject[object_index]->billboard_height * Vector3D(0, 0, 1.0);
    // Set bounding sphere.
    if (!sceneobject[object_index]->model->is_static)
        // Be careful because for static objects, the position may be set to (0, 0, 0)
        // if preprocessing is enabled.
        sceneobject[object_index]->sphere.center = sceneobject[object_index]->position;
    sceneobject[object_index]->sphere.radius = Magnitude(X + Y);
}

void sreScene::ChangeHaloSize(int object_index, float size) const {
    sceneobject[object_index]->halo_size = size;
}

void sreScene::ChangeDiffuseReflectionColor(int object_index, Color color) const {
    sceneobject[object_index]->diffuse_reflection_color = color;
}

void sreScene::ChangeSpecularReflectionColor(int object_index, Color color) const {
    sceneobject[object_index]->specular_reflection_color = color;
}

void sreScene::ChangeEmissionColor(int object_index, Color color) const {
    sceneobject[object_index]->emission_color = color;
}

void sreScene::ChangeSpecularExponent(int object_index, float exponent) const {
    sceneobject[object_index]->specular_exponent = exponent;
}

void sreScene::ChangeMicrofacetParameters(int object_index, float diffuse_fraction,
float roughness_value1, float weight1, float roughness_value2, float weight2,
bool anisotropic) const {
    sceneobject[object_index]->diffuse_fraction = diffuse_fraction;
    sceneobject[object_index]->roughness_values = Vector2D(roughness_value1, roughness_value2);
    sceneobject[object_index]->roughness_weights = Vector2D(weight1, weight2);
    sceneobject[object_index]->anisotropic = anisotropic;
    // The anisotropic setting can affect shader vertex attributes.
    InvalidateLightingShaders(object_index);
}

void sreScene::AttachLight(int soi, int light_index, Vector3D model_position) const {
    sceneobject[soi]->attached_light = light_index;
    sceneobject[soi]->attached_light_model_position = model_position;
}

void sreScene::InvalidateShaders(int soi) const {
    for (int i = 0; i < SRE_NU_SHADER_LIGHT_TYPES; i++)
        sceneobject[soi]->current_shader[i] = - 1;
    if (sre_internal_shadows == SRE_SHADOWS_SHADOW_MAPPING)
        for (int i = 0; i < SRE_NU_SHADER_LIGHT_TYPES; i++)
            sceneobject[soi]->current_shader_shadow_map[i] = - 1;
}

// Invalidate all shaders except ambient.

void sreScene::InvalidateLightingShaders(int soi) const {
    for (int i = 0; i <= SRE_SHADER_LIGHT_TYPE_SPOT_OR_BEAM; i++)
        sceneobject[soi]->current_shader[i] = - 1;
    // Aat the moment when shadow mapping is enabled, all shaders,
    // are invalidated anyway, so we only need to reset the cached
    // shaders when shadow mapping is already enabled.
    if (sre_internal_shadows == SRE_SHADOWS_SHADOW_MAPPING)
        for (int i = 0; i <= SRE_SHADER_LIGHT_TYPE_SPOT_OR_BEAM; i++)
            sceneobject[soi]->current_shader_shadow_map[i] = - 1;
}

// Implementation of sreObjectList (actually a general linked list
// for integers).

sreObjectList::sreObjectList() {
    head = NULL;
    tail = NULL;
}

void sreObjectList::AddElement(int so) {
// printf("Adding element with id %d.\n", so);
    if (tail == NULL) {
        head = tail = new sreObjectListElement;
        head->next = NULL;
        head->so = so;
        return;
    }
    sreObjectListElement *e = new sreObjectListElement;
    tail->next = e;
    e->next = NULL;
    tail = e;
    e->so = so;
}

void sreObjectList::DeleteElement(int so) {
// printf("Deleting element with id %d.\n", so);
    // Handle the case where the element to be deleted is first in the list.
    if (head->so == so) {
        sreObjectListElement *e = head;
        head = head->next;
        if (head == NULL)
            tail = NULL;
        delete e;
        return;
    }
    sreObjectListElement *e = head;
    while (e->next != NULL) {
        if (e->next->so == so) {
            sreObjectListElement *f = e->next;
            e->next = f->next;
            delete f;
            if (e->next == NULL)
                tail = e;
            return;
        }
        e = e->next;
    }
    printf("Error in DeleteElement -- could not find sceneobject id in list.\n");
    exit(1);
}

int sreObjectList::Pop() {
// printf("Popping element with id %d.\n", head->so);
    int id = head->so;
    sreObjectListElement *e = head->next;
    if (tail == head)
        tail = NULL;
    delete head;
    head = e;
    return id;
}

void sreObjectList::MakeEmpty() {
    while (head != NULL) {
        int id = Pop();
    }
}

// sreObject member functions.

sreObject::sreObject() {
    // Make sure shader will be reselected when the object is first drawn.
    for (int i = 0; i < SRE_NU_SHADER_LIGHT_TYPES; i++) {
        current_shader[i] = - 1;
        current_shader_shadow_map[i] = - 1;
    }
}

sreObject::~sreObject() {
    // Any object that was added to the scene will have the
    // nu_shadow_volumes field initialized. There may be zero
    // or more static shadow volumes that were calculated in
    // sreScene::CalculateStaticLightObjectLists(). These are dynamically
    // allocated structured that can be freed.
    for (int i = 0; i < nu_shadow_volumes; i++)
        // This should trigger the destructor of sreShadowVolume,
        // freeing additional allocated structures.
        delete shadow_volume[i];
    // Free the array of shadow volume pointers.
    delete [] shadow_volume;
}

void sreObject::AddShadowVolume(sreShadowVolume *sv) {
    sreShadowVolume **new_shadow_volume = new sreShadowVolume *[nu_shadow_volumes + 1];
    if (nu_shadow_volumes > 0) {
        memcpy(new_shadow_volume, shadow_volume, sizeof(sreShadowVolume *) * nu_shadow_volumes);
        delete [] shadow_volume;
    }
    new_shadow_volume[nu_shadow_volumes] = sv;
    shadow_volume = new_shadow_volume;
    nu_shadow_volumes++;
}

sreShadowVolume *sreObject::LookupShadowVolume(int light_index) const {
    // This function is called either when shadow volume visibility testing is enabled during
    // shadow volume construction, or at an earlier stage when geometry scissors are enabled.
    // In the latter case, it does not appear to be sensible to check whether the visibility
    // test is enabled (precalculated static object shadow volumes, especially for point/spot
    // lights, save processing time).
//    if (!(sre_internal_rendering_flags & SRE_RENDERING_FLAG_SHADOW_VOLUME_VISIBILITY_TEST))
//        return NULL;
    // Just iterate all shadow volumes and look for the right light.
    // Not optimal, but objects tend to have only a limited number of
    // static lights affect them.
    for (int i = 0; i < nu_shadow_volumes; i++)
        if (shadow_volume[i]->light == light_index) {
            return shadow_volume[i];
        }
    return NULL;
}
