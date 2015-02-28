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

// Shader matrix calculations.

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

Matrix4D sre_internal_projection_matrix;
MatrixTransform sre_internal_view_matrix;
Matrix4D sre_internal_view_projection_matrix;

MatrixTransform shadow_map_matrix;
Matrix4D projection_shadow_map_matrix;
Matrix4D cube_shadow_map_matrix;
MatrixTransform shadow_map_lighting_pass_matrix;
Matrix4D projection_shadow_map_lighting_pass_matrix;
// Matrix4D sre_internal_geometry_matrix_scissors_projection_matrix;
Vector3D sre_internal_up_vector;
Vector3D sre_internal_camera_vector;
// The sre_internal_aspect_changed flag will be set at the time of
// the the first projection matrix set-up, and subsequently when the
// aspect ratio changes due to window resizes etc. The value of the
// sre_internal_aspect_ratio variable should be initialized to zero
// or the actual aspect ratio value by sreInitialize before any
// shaders are loaded.
float sre_internal_aspect_ratio;

void srePerspective(float fov, float aspect, float nearp, float farp) {
#if 0
    float f = 1 / tan((fov * M_PI/ 180) / 2);
    projection_matrix.Set(
        f / aspect, 0.0f, 0.0f, 0,
        0.0f, f, 0.0f, 0,
        0.0f, 0.0f, (farp + nearp) / (nearp - farp), 2 * farp * nearp / (nearp - farp),
        0.0f, 0.0f, -1.0f, 0.0f);
#endif
    if (aspect != sre_internal_aspect_ratio) {
        sre_internal_aspect_ratio = aspect;
        sre_internal_aspect_changed = true;
    }
    float e = 1 / tanf((fov * M_PI / 180) / 2);
    float n = nearp;
    float l = - n / e;
    float r = n / e;
    float b = - (1 / aspect) * n / e;
    float t = (1 / aspect) * n / e;
    // Set up a projection matrix with an infinite view frustum. We use depth clamping.
    sre_internal_projection_matrix.Set(
        2 * n / (r - l), 0.0f, (r + l) / (r - l), 0.0f,
        0.0f, 2 * n / (t - b), (t + b) / (t - b), 0.0f,
        0.0f, 0.0f, - 1.0f, - 2 * n,
        0.0f, 0.0f, - 1.0f, 0.0f);
}

void srePerspectiveTweaked(float fov, float aspect, float nearp, float farp) {
    if (aspect != sre_internal_aspect_ratio) {
        sre_internal_aspect_ratio = aspect;
        sre_internal_aspect_changed = true;
    }
    float e = 1 / tanf((fov * M_PI / 180) / 2);
    float n = nearp;
    float l = - n / e;
    float r = n / e;
    float b = - (1 / aspect) * n / e;
    float t = (1 / aspect) * n / e;
    // Set up a projection matrix with an infinite view frustum. Tweaked with small constant epsilon.
    const float epsilon = 0.001f;
    sre_internal_projection_matrix.Set(
        2 * n / (r - l), 0.0f, (r + l) / (r - l), 0.0f,
        0.0f, 2 * n / (t - b), (t + b) / (t - b), 0.0f,
        0.0f, 0.0f, epsilon - 1.0f, n * (epsilon - 2.0f),
        0.0f, 0.0f, - 1.0f, 0.0f);
}

void sreLookAt(float viewpx, float viewpy, float viewpz, float lookx, float looky, float lookz,
float upx, float upy, float upz) {
    Vector3D F = Vector3D(lookx, looky, lookz) - Vector3D(viewpx, viewpy, viewpz);
    Vector3D Up = Vector3D(upx, upy, upz);
    Vector3D f = F.Normalize();
    sre_internal_camera_vector = f;
    Up.Normalize();
    sre_internal_up_vector = Up;
    Vector3D s = Cross(f, Up);
    Vector3D u = Cross(s, f);
    MatrixTransform M;
    M.Set(
        s.x, s.y, s.z, 0.0f,
        u.x, u.y, u.z, 0.0f,
        - f.x, - f.y, - f.z, 0.0f);
    MatrixTransform T;
    T.AssignTranslation(Vector3D(- viewpx, - viewpy, -viewpz));
    sre_internal_view_matrix = M * T;
    sre_internal_view_projection_matrix = sre_internal_projection_matrix * sre_internal_view_matrix;
//    printf("View-projection matrix:\n");
//    for (int row = 0; row < 4; row++)
//        for (int column = 0; column < 4; column++)
//            printf("%f, ", sre_internal_view_projection_matrix(row, column));
//    printf("\n");
}

// Calculated othographic shadow map transformation based on light direction,
// range within that direction, and a local x and y coordinate system.

void GL3CalculateShadowMapMatrix(Vector3D viewp, Vector3D light_direction, Vector3D x_direction,
Vector3D y_direction, Vector3D dim_min, Vector3D dim_max) {
    MatrixTransform M;
    // Note that the y direction has to be negated in order to preserve the handedness of
    // triangles when rendering the shadow map.
#if 0
    M.Set(
        x_direction.x, y_direction.x, - light_direction.x, 0.0f,
	x_direction.x, y_direction.y, - light_direction.y, 0.0f,
	x_direction.z, y_direction.z, - light_direction.z, 0.0f);
#else
    M.Set(
        x_direction.x, x_direction.y, x_direction.z, 0.0f,
        - y_direction.x, - y_direction.y, - y_direction.z, 0.0f,
        - light_direction.x, - light_direction.y, - light_direction.z, 0.0f);
#endif
    MatrixTransform T;
    T.AssignTranslation(- viewp);
    // Set orthographic projection matrix.
    MatrixTransform orthographic_shadow_map_projection_matrix;
    orthographic_shadow_map_projection_matrix.Set(
        2.0f / (dim_max.x - dim_min.x), 0.0f, 0.0f, - (dim_max.x + dim_min.x) / (dim_max.x - dim_min.x),
        0.0f, 2.0f / (dim_max.y - dim_min.y), 0.0f, - (dim_max.y + dim_min.y) / (dim_max.y - dim_min.y),
        0.0f, 0.0f, - 2.0f / dim_max.z, - 1.0f);
    shadow_map_matrix = orthographic_shadow_map_projection_matrix * (M * T);
    // Calculate viewport matrix for lighting pass with shadow map.
    MatrixTransform shadow_map_viewport_matrix;
    shadow_map_viewport_matrix.Set(
        0.5f, 0.0f, 0.0f, 0.5f,
        0.0f, 0.5f, 0.0f, 0.5f,
        0.0f, 0.0f, 0.5f, 0.5f);
    shadow_map_lighting_pass_matrix = shadow_map_viewport_matrix * shadow_map_matrix;

#if 0
    char *dim_max_str = dim_max.GetString();
    sreMessage(SRE_MESSAGE_LOG, "dim_max = %s", dim_max_str);
    delete [] dim_max_str;
    Point3D P1 = viewp + light_direction * dim_max.z;
    Point3D P2 = viewp;
    Point3D P3 = viewp + x_direction * dim_max.x + y_direction * dim_max.y;
    Vector4D P1_proj = shadow_map_matrix * P1;
    Vector4D P2_proj = shadow_map_matrix * P2;
    Vector4D P3_proj = shadow_map_matrix * P3;
    Vector3D P1_norm = P1_proj.GetVector3D();
    Vector3D P2_norm = P2_proj.GetVector3D();
    Vector3D P3_norm = P3_proj.GetVector3D();
    char *P1_norm_str = P1_norm.GetString();
    char *P2_norm_str = P2_norm.GetString();
    char *P3_norm_str = P3_norm.GetString();
    sreMessage(SRE_MESSAGE_LOG, "CalculateShadowMapMatrix: Point transformations "
        "%s, %s and %s.", P1_norm_str, P2_norm_str, P3_norm_str);
    delete P1_norm_str;
    delete P2_norm_str;
    delete P3_norm_str;
#endif
}

void GL3CalculateCubeShadowMapMatrix(Vector3D light_position, Vector3D zdir,
Vector3D cube_s_vector, Vector3D cube_t_vector, float zmin, float zmax) {
    MatrixTransform M;
    M.Set(
        cube_s_vector.x, cube_s_vector.y, cube_s_vector.z, 0.0f,
        cube_t_vector.x, cube_t_vector.y, cube_t_vector.z, 0.0f,
        - zdir.x, - zdir.y, - zdir.z, 0.0f);
    MatrixTransform T;
    T.AssignTranslation(- light_position);
    // Calculate the projection matrix with a field of view of 90 degrees.
    float aspect = 1.0;
    float e = 1 / tanf((90.0f * M_PI / 180) / 2);
    float n = zmin;
    float f = zmax;
    float l = - n / e;
    float r = n / e;
    float b = - (1.0f / aspect) * n / e;
    float t = (1.0f / aspect) * n / e;
    Matrix4D shadow_map_projection_matrix;
    shadow_map_projection_matrix.Set(
        2 * n / (r - l), 0.0f, (r + l) / (r - l), 0.0f,
        0.0f, 2 * n / (t - b), (t + b) / (t - b), 0.0f,
        0.0f, 0.0f, - (f + n) / (f - n), - 2 * n * f / (f - n),
        0.0f, 0.0f, - 1.0f, 0.0f);
    cube_shadow_map_matrix = shadow_map_projection_matrix * (M * T);
}

// Calculate projection shadow map matrix, used for generating spotlight shadow maps.

void GL3CalculateProjectionShadowMapMatrix(Vector3D viewp, Vector3D light_direction,
Vector3D x_direction, Vector3D y_direction, float zmin, float zmax) {
#if 0
    char *s1 = light_direction.GetString();
    char *s2 = x_direction.GetString();
    char *s3 = y_direction.GetString();
    sreMessage(SRE_MESSAGE_LOG, "GL3CalculateProjectionShadowMapMatrix: light_dir = %s, "
        "x_dir = %s, y_dir = %s, dot products: %f, %f, %f", s1, s2, s3,
        Dot(light_direction, x_direction), Dot(light_direction, y_direction),
        Dot(x_direction, y_direction));
    delete s1;
    delete s2;
    delete s3;
#endif
    Vector3D fvec = light_direction;
    Vector3D s = x_direction;
    Vector3D u = y_direction;
    Matrix4D M;
    // Note that the y direction has to be negated in order to preserve the handedness of
    // triangles when rendering the shadow map.
    M.Set(
        s.x, s.y, s.z, 0,
        - u.x, - u.y, - u.z, 0,
        - fvec.x, - fvec.y, - fvec.z, 0,
        0.0f, 0.0f, 0.0f, 1.0f);
    Matrix4D T;
    T.AssignTranslation(- viewp);
    // Calculate the projection matrix with a field of view of 90 degrees.
    float aspect = 1.0;
    float e = 1 / tanf((90.0 * M_PI / 180) / 2);
    float n = zmin;
    float f = zmax;
    float l = - n / e;
    float r = n / e;
    float b = - (1.0f / aspect) * n / e;
    float t = (1.0f / aspect) * n / e;
    Matrix4D projection_matrix;
    projection_matrix.Set(
        2 * n / (r - l), 0.0f, (r + l) / (r - l), 0.0f,
        0.0f, 2 * n / (t - b), (t + b) / (t - b), 0.0f,
        0.0f, 0.0f, - (f + n) / (f - n), - 2 * n * f / (f - n),
        0.0f, 0.0f, - 1.0f, 0.0f);
    projection_shadow_map_matrix = projection_matrix * (M * T);
    MatrixTransform shadow_map_viewport_matrix;
    shadow_map_viewport_matrix.Set(
        0.5f, 0.0f, 0.0f, 0.5f,
        0.0f, 0.5f, 0.0f, 0.5f,
        0.0f, 0.0f, 0.5f, 0.5f);
    projection_shadow_map_lighting_pass_matrix = shadow_map_viewport_matrix *
        projection_shadow_map_matrix;
//    projection_shadow_map_lighting_pass_matrix = projection_shadow_map_matrix;

#if 0
    Point3D P1 = viewp + light_direction * n;
    Point3D P2 = viewp + light_direction * f;
    Point3D P3 = viewp + light_direction * f + x_direction * f + y_direction * f;
    Vector4D P1_proj = projection_shadow_map_matrix * P1;
    Vector4D P2_proj = projection_shadow_map_matrix * P2;
    Vector4D P3_proj = projection_shadow_map_matrix * P3;
    Vector3D P1_norm = P1_proj.GetVector3D() / P1_proj.w;
    Vector3D P2_norm = P2_proj.GetVector3D() / P2_proj.w;
    Vector3D P3_norm = P3_proj.GetVector3D() / P3_proj.w;
    char *P1_norm_str = P1_norm.GetString();
    char *P2_norm_str = P2_norm.GetString();
    char *P3_norm_str = P3_norm.GetString();
    sreMessage(SRE_MESSAGE_LOG, "CalculateProjectionShadowMapMatrix: Point transformations "
        "%s, %s and %s.", P1_norm_str, P2_norm_str, P3_norm_str);
    delete P1_norm_str;
    delete P2_norm_str;
    delete P3_norm_str;
#endif
}

void GL3CalculateShadowMapMatrixAlwaysLight() {
    // Set a matrix that produces shadow map coordinates that are out of bounds in x and y, with w coordinate 1
    // and a z-coordinate of 0.5. In the pixel shader, this produces no shadow.
    shadow_map_lighting_pass_matrix.Set(
        0.0f, 0.0f, 0.0f, -2.0f,
        0.0f, 0.0f, 0.0f, -2.0,
        0.0f, 0.0f, 0.0f, 0.5f);
}

