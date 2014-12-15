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

// demo4.cpp -- Earth relief mesh demo.

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <float.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include "sre.h"
#include "sre_bounds.h"
#include "sreBackend.h"
#include "gui-common.h"

class Demo4Application : public sreBulletPhysicsApplication {
    virtual void Step(sreScene *scene, double demo_time);
    virtual void DoPhysics(sreScene *scene, double previous_time, double current_time);
};

// Number of seconds for a complete rotation of the earth.
#define DEFAULT_DAY_INTERVAL 1000.0f
#define YEAR_INTERVAL (DEFAULT_DAY_INTERVAL * 365.0f)

// A submesh of size 129x129 takes 17685 * (4 * 4 + 3 * 4 + 2 * 4) + 33806 * (3 * 2) = 636660 + 202836 = 839496 bytes,
// or 839496 + 282960 = 1122456 bytes with shadow vertices.
// A mesh size of 4096x2048 takes approximately 32 * 16 * 839496 = 409.91MB, or 574.70MB with shadow vertices.
// A mesh size of 4050x4050 takes approximately 840.44 MB, or 1124.72MB with shadow vertices.
// A mesh size of 4050x2025 takes approximately 420.22 MB, or 561.86MB with shadow vertices.
// Submesh of size 129x129 with removed unused vertices has 17433 vertices.
// The 16K earth texture takes 62.57MB, the specularity texture also 62.57MB.
// A mesh of 4050x2025 with shadow vertices and 16K textures shows about 166MB free GPU memory.
// Cached shadow volumes use a relatively small amount of GPU memory.
//
// 902002/1081718 (83%) removed.
// 901850/1081718 (83%) removed with restriction on invalid triangles (EPSILON = 0.0001).
// 901820/1081718 (83%) removed (EPSILON = 0.005).
// 3659036/4276768 (85%) removed.
// 3658470/4276768 (85%) removed with restriction of vertex normal difference (threshold = 0.9).
// 3611666/4276768 (84%) removed (threshold = 0.99).
// 3613116/4276768 (84%) removed with enhanced restriction of vertex normal difference (treshold = 0.99).
// 3285444/4276768 (76%) removed (treshold = 0.999).
// 3561292/4276768 (83%) removed (threshold = 0.995).

// When RESOLUTION_16K is defined, a specific 16K Earth texture data set is used with
// resolution of 16200x8100. When RESOLUTION_8K is defined, a specific 8K Earth texture
// data set is used with a resolution of 8192x4096. When RESOLUTION_16384 is set, power-
// of-two upscaled versions of the 16K textures are used. When neither is set, custom textures
// can be used.
#define RESOLUTION_16384
#define SPHERE
//#define CLOSED_SEGMENTS_FOR_SHADOW_VOLUMES
//#define NIGHT
// ELEVATION_MAP_DETAIL_FACTOR is the number of elevation map samples that are combined
// in both dimensions. It must be a power of two. A value of one produces the most complex
// mesh, a value two halves the complexity, etc.
// Since the elevation map texture size is 16200x8100 if RESOLUTION_16K is set, the maximum
// value is four in this case.
// When RESOLUTION_8K is set, the elevation map used has a resolution of 8192x4096, so any
// level of detail is allowed.
// When ELEVATION_MAP_16_BIT is set, a high-precision elevation map with a resolution of
// 10800x5400 is used, so the maximum detail divisor value is eight.
#define ELEVATION_MAP_16_BIT
#define ELEVATION_MAP_DETAIL_FACTOR 1
// The size of submeshes. Maximum MESH_WIDTH + 2 and MESH_HEIGHT + 2.
#define SUB_MESH_WIDTH 200
#define SUB_MESH_HEIGHT 200
// Zoom must be a power of two >= 1.
#define ZOOM 1
// LONGITUDE and LATITUDE, in degrees, define the center of the part of the world that is shown. Negative longitude is
// west.
#define LONGITUDE 0
#define LATITUDE 0
// Scaling defines the coordinate size in the world (for a sphere, it is the radius * 2).
#define X_SCALE 10000.0f
// Z_SCALE defines the range of the height map. The source value from the height map
// ranges from 0 to 1 (usually normalized from a byte value from 0 to 255). This
// normalized value to scaled so that the maximum possible height is bounded by Z_SCALE.
// #define Z_SCALE (200.0f * ZOOM)
// #define Z_SCALE (150.0f * ZOOM)
// Changed to reduce relief.
#define Z_SCALE (50.0f * ZOOM)

// Whether to show the user-controlled spacecraft in the center of the window.
// #define SHOW_SPACECRAFT

// End of configurable parameters.

// The mesh size:
// MESH_WIDTH and MESH_HEIGHT must be equal to the elevation map width and height, divided by
// a power of two, with one subtracted.
#ifdef ELEVATION_MAP_16_BIT
#define ELEVATION_MAP_WIDTH 10800
#define ELEVATION_MAP_HEIGHT 5400
#elif defined(RESOLUTION_16384) || defined(RESOLUTION_16K)
#define ELEVATION_MAP_WIDTH 16200
#define ELEVATION_MAP_HEIGHT 8100
#elif defined(RESOLUTION_8K)
#define ELEVATION_MAP_WIDTH 8192
#define ELEVATION_MAP_HEIGHT 4096
#else
#define ELEVATION_MAP_WIDTH 8192
#define ELEVATION_MAP_HEIGHT 4096
#endif
#define MESH_WIDTH (ELEVATION_MAP_WIDTH / ELEVATION_MAP_DETAIL_FACTOR - 1)
#define MESH_HEIGHT (ELEVATION_MAP_HEIGHT / ELEVATION_MAP_DETAIL_FACTOR - 1)
#define MESH_TEXTURE_WIDTH (earth_heightmap->width / (MESH_WIDTH + 1))
#define MESH_TEXTURE_HEIGHT (earth_heightmap->height / (MESH_HEIGHT + 1))
#define SUB_MESHES_X ((MESH_WIDTH + SUB_MESH_WIDTH - 1) / (SUB_MESH_WIDTH - 1))
#define SUB_MESHES_Y ((MESH_HEIGHT + SUB_MESH_HEIGHT - 1) / (SUB_MESH_HEIGHT - 1))
#define X_OFFSET ((LONGITUDE + 180.0) * (MESH_WIDTH + 1) / 360.0 - (MESH_WIDTH + 1) / 2 / ZOOM)
#define Y_OFFSET ((LATITUDE + 90.0) * (MESH_HEIGHT + 1) / 180.0 - (MESH_HEIGHT + 1) / 2 / ZOOM)
#define Y_SCALE (X_SCALE * (MESH_WIDTH + 1) / (MESH_HEIGHT + 1) * 0.5)

#ifdef RESOLUTION_16384
static const char *earth_texture_filename = "6_merged_color_ice_16384";
#define ELEVATION_MAP_FILENAME "elev_bump_16k";
static const char *earth_night_light_filename = "cities_16384";
static const char *earth_specularity_filename = "water_16384";
#elif defined(RESOLUTION_16K)
// static const char *earth_texture_filename = "4_no_ice_clouds_mts_16k";
static const char *earth_texture_filename = "6_merged_color_ice_16k";
#define ELEVATION_MAP_FILENAME "elev_bump_16k";
static const char *earth_night_light_filename = "cities_16k_one_component";
static const char *earth_specularity_filename = "water_16k_one_component";
#elif defined(RESOLUTION_8K)
static const char *earth_texture_filename = "4_no_ice_clouds_mts_8k";
#define ELEVATION_MAP_FILENAME "elev_bump_8k";
static const char *earth_night_light_filename = "cities_8k";
static const char *earth_specularity_filename = "water_8k";
#else
static const char *earth_texture_filename = "custom_planet_color_map";
// The elevation map is expected to be an image file in which each red component
// (normalized from range 0-255) represents the height (e.g. a red or white monochrome
// texture). Higher precision is not yet supported.
#define ELEVATION_MAP_FILENAME "custom_planet_elevation_map";
// The dark side texture is used (additively) when there is no illumination from the sun.
// To disable, use a small black texture.
static const char *earth_night_light_filename = "custom_planet_dark_side_color_map";
// The specularity map is expected to be an image file in which each texel represents
// the specular reflection color. Black means no specular reflection, (1.0, 1.0, 1.0)
// is full white specular reflection.
// Use a small black texture to disable specularity entirely or a small texture with
// a fixed color for constant specularity.
static const char *earth_specularity_filename = "custom_planet_specularity_map";
#endif
#ifdef ELEVATION_MAP_16_BIT
#define ELEVATION_MAP_FILENAME "16_bit_elevation";
#endif
static const char *earth_heightmap_filename = ELEVATION_MAP_FILENAME;

static sreTexture *earth_texture;
static sreTexture *earth_heightmap;
static sreTexture *earth_specularity;
static sreTexture *earth_night_light_texture;

static int player_object = -1;
static int spacecraft_object = -1;
static int sun_object;
static int directional_light;
static int spacecraft_spot_light;
static float saved_hovering_height;
static Matrix3D saved_spacecraft_rotation_matrix;

static float day_interval = DEFAULT_DAY_INTERVAL;
static bool display_time = true;
static bool physics = true;
static bool create_spacecraft = true;
#ifdef SHOW_SPACECRAFT
static bool show_spacecraft = true;
#else
static bool show_spacecraft = false;
#endif
static float sun_light_factor = 1.0f;
static float extra_lod_threshold_scaling = 5.0f;

void CreateMeshObjects(sreScene *scene, sreModel *mesh_model[SUB_MESHES_Y][SUB_MESHES_X]) {
    printf("Creating mesh objects.\n");
    printf("Calculating vertices.\n");
    int v = 0;
    Point3D *vertex = new Point3D[MESH_WIDTH * MESH_HEIGHT + MESH_HEIGHT * 2 + 2];
    Point2D *texcoords = new Point2D[MESH_WIDTH * MESH_HEIGHT + MESH_HEIGHT * 2 + 2];
    int elevation_bits;
    float elevation_scale, elevation_offset;
#ifdef ELEVATION_MAP_16_BIT
    elevation_bits = 16;
    elevation_scale = 65535.0f;
    // The 16-bit elevation map sea-level is at 186.
    elevation_offset = - 186.0f * Z_SCALE / 65535.0f;
#else
    elevation_bits = 8;
    elevation_scale = 255.0f;
    // The sea-level value for the 8-bit elevation maps is 23.
    elevation_offset = - 23.0f * Z_SCALE / 255.0f;
#endif
    for (int y = 0; y < MESH_HEIGHT; y++)
        for (int x = 0; x < MESH_WIDTH; x++) {
            // Calculate the average height in the area of size MESH_TEXTURE_WIDTH * MESH_TEXTURE_HEIGHT.
            double h = 0;
            for (int i = 0; i < MESH_TEXTURE_HEIGHT / ZOOM; i++)
                for (int j = 0; j < MESH_TEXTURE_WIDTH / ZOOM; j++) {
                    // Assume the first color component (red) is a value from 0 to 255 (or 65535)
                    // representing the height.
                    unsigned int heightmap_value = earth_heightmap->LookupPixel(x * (MESH_TEXTURE_WIDTH / ZOOM) + j + X_OFFSET * MESH_TEXTURE_WIDTH,
                        earth_heightmap->height - Y_OFFSET * MESH_TEXTURE_HEIGHT - (MESH_TEXTURE_HEIGHT / ZOOM) -
                        y * (MESH_TEXTURE_HEIGHT / ZOOM) + i)
                        & (((unsigned int)1 << elevation_bits) - 1);
                    h += heightmap_value;
                }
#ifdef SPHERE
            float longitude = ((float)x + 0.5) / MESH_WIDTH * 2.0 * M_PI - M_PI;
            float latitude = ((float)y + 0.5) / MESH_HEIGHT * M_PI - 0.5 * M_PI;
            float normalized_height = h / (elevation_scale *
                (MESH_TEXTURE_WIDTH / ZOOM) * (MESH_TEXTURE_HEIGHT / ZOOM));
            float radius = 0.5f * X_SCALE + normalized_height * Z_SCALE + elevation_offset;
            float xcoord = radius * cosf(latitude) * cosf(longitude);
            float ycoord = radius * cosf(latitude) * sinf(longitude);
            float zcoord = radius * sinf(latitude);
#else
            float xcoord = (x - MESH_WIDTH / 2) * X_SCALE / MESH_WIDTH;
            float ycoord = (y - MESH_HEIGHT / 2) * Y_SCALE / MESH_WIDTH;
            float zcoord = h * Z_SCALE / (elevation_scale *
                (MESH_TEXTURE_WIDTH / ZOOM) * (MESH_TEXTURE_HEIGHT / ZOOM)) + elevation_offset;
#endif
            vertex[v].Set(xcoord, ycoord, zcoord);
            // Set the texcoords to the middle of the sampled area.
            // First calculate the texcoords in pixel units.
            float texcoords_x = x * (MESH_TEXTURE_WIDTH / ZOOM) + 0.5 * (MESH_TEXTURE_WIDTH / ZOOM) - 0.5 +
                X_OFFSET * MESH_TEXTURE_WIDTH;
            float texcoords_y = earth_heightmap->height - Y_OFFSET * MESH_TEXTURE_HEIGHT - (MESH_TEXTURE_HEIGHT / ZOOM) -
                y * (MESH_TEXTURE_HEIGHT / ZOOM) + 0.5 * (MESH_TEXTURE_HEIGHT / ZOOM) + 0.5;
            // The color/specularity/nightlight textures may have a different size, but the
            // normalized texture coordinates will be the same.
            texcoords[v].Set(texcoords_x / earth_heightmap->width, texcoords_y /
                earth_heightmap->height);
            v++;
        }
#ifdef SPHERE
    // Define special column of vertices at - 180 degrees longitude.
    int vertex_index_longitude_minus_180 = v;
    for (int y = 0; y < MESH_HEIGHT; y++) {
        vertex[v].Set(0.5 * (vertex[y * MESH_WIDTH].x + vertex[y * MESH_WIDTH + MESH_WIDTH - 1].x),
            0, 0.5 * (vertex[y * MESH_WIDTH].z + vertex[y * MESH_WIDTH + MESH_WIDTH - 1].z));
        float texcoords_y = earth_heightmap->height - Y_OFFSET * MESH_TEXTURE_HEIGHT - (MESH_TEXTURE_HEIGHT / ZOOM) -
            y * (MESH_TEXTURE_HEIGHT / ZOOM) + 0.5 * (MESH_TEXTURE_HEIGHT / ZOOM) + 0.5;
        texcoords[v].Set(0, texcoords_y / earth_heightmap->height);
        v++;
    }
    // Define special column of vertices at 180 degrees longitude.
    int vertex_index_longitude_180 = v;
    for (int y = 0; y < MESH_HEIGHT; y++) {
        vertex[v].Set(0.5 * (vertex[y * MESH_WIDTH].x + vertex[y * MESH_WIDTH + MESH_WIDTH - 1].x),
            0, 0.5 * (vertex[y * MESH_WIDTH].z + vertex[y * MESH_WIDTH + MESH_WIDTH - 1].z));
        float texcoords_y = earth_heightmap->height - Y_OFFSET * MESH_TEXTURE_HEIGHT - (MESH_TEXTURE_HEIGHT / ZOOM) -
            y * (MESH_TEXTURE_HEIGHT / ZOOM) + 0.5 * (MESH_TEXTURE_HEIGHT / ZOOM) + 0.5;
        texcoords[v].Set(1.0, texcoords_y / earth_heightmap->height);
        v++;
    }
    // Define special vertices at the south and north polar caps.
    int vertex_index_latitude_minus_90 = v;
    unsigned int height = earth_heightmap->LookupPixel(earth_heightmap->width / 2, earth_heightmap->height - 1);
    float radius = 0.5f * X_SCALE + height * Z_SCALE / (elevation_scale *
        (MESH_TEXTURE_WIDTH / ZOOM) * (MESH_TEXTURE_HEIGHT / ZOOM)) + elevation_offset;
    vertex[v].Set(0, 0, - radius);
    texcoords[v].Set(0, 1.0);
    v++;
    int vertex_index_latitude_90 = v;
    height = earth_heightmap->LookupPixel(earth_heightmap->width / 2, 0);
    radius = 0.5f * X_SCALE + height * Z_SCALE / (elevation_scale *
        (MESH_TEXTURE_WIDTH / ZOOM) * (MESH_TEXTURE_HEIGHT / ZOOM)) + elevation_offset;
    vertex[v].Set(0, 0, radius);
    texcoords[v].Set(0, 0);
    v++;
#endif
    sreModelTriangle *triangle = new sreModelTriangle[(MESH_WIDTH - 1) * (MESH_HEIGHT - 1) * 2];
    int t = 0;
    for (int y = 0; y < MESH_HEIGHT - 1; y++)
        for (int x = 0; x < MESH_WIDTH - 1; x++) {
            triangle[t].AssignVertices(y * MESH_WIDTH + x, y * MESH_WIDTH + x + 1, (y + 1) *
                MESH_WIDTH + x + 1);
            triangle[t + 1].AssignVertices(y * MESH_WIDTH + x, (y + 1) * MESH_WIDTH + x + 1,
                (y + 1) * MESH_WIDTH + x);
            t += 2;
        }
    printf("Calculating normals.\n");
    for (int i = 0; i < t; i++) {
        triangle[i].normal = CalculateNormal(vertex[triangle[i].vertex_index[0]],
            vertex[triangle[i].vertex_index[1]], vertex[triangle[i].vertex_index[2]]);
#ifndef SPHERE
        float sqmag = SquaredMag(triangle[i].normal);
        if (sqmag < 0.999 || sqmag > 1.001) {
            printf("Invalid normal encountered.\n");
            exit(1);
        }
#endif
    }
    Vector3D *vertex_normal = new Vector3D[MESH_WIDTH * MESH_HEIGHT + MESH_HEIGHT * 2 + 2];
    for  (int y = 0; y < MESH_HEIGHT; y++)
        for (int x = 0; x < MESH_WIDTH; x++) {
            int t1 = y * (MESH_WIDTH - 1) * 2 + x * 2;
            int t2 = t1 + 1;
            Vector3D sum;
            sum.Set(0, 0, 0);
            if (x > 0) {
                if (y < MESH_HEIGHT - 1) {
                    sum += triangle[t1 - 2].normal;
                }
                if (y > 0) {
                   sum += triangle[t1 - (MESH_WIDTH - 1) * 2 - 2].normal;
                   sum += triangle[t2 - (MESH_WIDTH - 1) * 2 - 2].normal;
                }
            }
            if (x < MESH_WIDTH - 1) {
                if (y < MESH_HEIGHT - 1) {
                    sum += triangle[t1].normal;
                    sum += triangle[t2].normal;
                }
                if (y > 0)
                   sum += triangle[t2 - (MESH_WIDTH - 1) * 2].normal;
            }
            vertex_normal[y * MESH_WIDTH + x] = sum.Normalize();
#ifndef SPHERE
        float sqmag = SquaredMag(vertex_normal[y * MESH_WIDTH + x]) ;
        if (sqmag < 0.999 || sqmag > 1.001) {
            printf("Invalid vertex normal encountered.\n");
            exit(1);
        }
#endif
        }
    delete [] triangle;
#ifdef SPHERE
    // Calculate vertex normals for special columns of vertices at - 180 and 180 degrees longitude.
    for (int y = 0; y < MESH_HEIGHT; y++) {
        vertex_normal[vertex_index_longitude_minus_180 + y] = (vertex_normal[y * MESH_WIDTH] +
            vertex_normal[y * MESH_WIDTH + MESH_WIDTH - 1]).Normalize();
        vertex_normal[vertex_index_longitude_180 + y] = (vertex_normal[y * MESH_WIDTH] +
            vertex_normal[y * MESH_WIDTH + MESH_WIDTH - 1]).Normalize();
    }
    vertex_normal[vertex_index_latitude_minus_90].Set(0, 0, - 1.0);
    vertex_normal[vertex_index_latitude_90].Set(0, 0, 1.0);
#endif
    printf("Assigning submeshes.\n");
    int total_triangle_count = 0;
    int total_triangle_count_reduced = 0;
    for (int sub_mesh_y = 0; sub_mesh_y < SUB_MESHES_Y; sub_mesh_y++)
        for (int sub_mesh_x = 0; sub_mesh_x < SUB_MESHES_X; sub_mesh_x++) {
            sreModel *model = mesh_model[sub_mesh_y][sub_mesh_x];
            sreLODModel *m = sreNewLODModel();
            int w = SUB_MESH_WIDTH;
            int h = SUB_MESH_HEIGHT;
            
            int x_offset = 0;
            int y_offset = 0;
#ifdef SPHERE
            // At longitude - 180 degrees, we need special vertices to cover the gap to 180 degrees.
            if (sub_mesh_x == 0) {
                w++;
                x_offset = 1;
            }
#endif
            if (sub_mesh_x * (SUB_MESH_WIDTH - 1) + w > MESH_WIDTH) {
                w = MESH_WIDTH - sub_mesh_x * (SUB_MESH_WIDTH - 1);
#ifdef SPHERE
                // Similarly, at longitude 180 degrees, we need to cover the gap to - 180 degrees.
                w++;
#endif
            }
#ifdef SPHERE
            if (sub_mesh_y == 0) {
                h++;
                y_offset = 1;
            }
#endif
            if (sub_mesh_y * (SUB_MESH_HEIGHT - 1) + h > MESH_HEIGHT) {
                h = MESH_HEIGHT - sub_mesh_y * (SUB_MESH_HEIGHT - 1);
#ifdef SPHERE
                // At latitude 90 degrees, we need to cover the gap to the north polar cap.
                h++;
#endif
            }
            // Use a smaller number of longitudinal vertices at higher latitudes.
            // This requires more work and is disabled.
            int longitudinal_step = 1;
#if defined(SPHERE) && false
            if (90.0f * sub_mesh_y * (SUB_MESH_HEIGHT - 1) / MESH_HEIGHT > 60.0f ||
            90.f * (sub_mesh_y * (SUB_MESH_HEIGHT - 1) + SUB_MESH_HEIGHT - 1) < - 60.0f)
                longitudual_step = 2;
#endif
            m->nu_vertices = w * h;
#ifdef CLOSED_SEGMENTS_FOR_SHADOW_VOLUMES
            m->nu_vertices += w * 2 + w * 2 + h * 2 + h * 2 + 8 + 4; // Extra edge vertices.
#endif
            m->vertex = new Point3D[m->nu_vertices];
            m->texcoords = new Point2D[m->nu_vertices];
            m->vertex_normal = new Vector3D[m->nu_vertices];
            float zmin = FLT_MAX;
            for (int y = 0; y < h; y++)
                for (int x = 0;; x += longitudinal_step) {
                    // Make sure the rightmost vertex is always included.
                    if (longitudinal_step > 1 && x >= w && x < w - 1 + longitudinal_step)
                        x = w - 1;
                    else if (x >= w)
                        break;
                    int mesh_x = sub_mesh_x * (SUB_MESH_WIDTH - 1);
                    int mesh_y = sub_mesh_y * (SUB_MESH_HEIGHT - 1);
                    int index;
#ifdef SPHERE
                    if (mesh_y + y == 0)
                        // South polar cap vertex.
                        index = vertex_index_latitude_minus_90;
                    else
                    if (mesh_y + y == MESH_HEIGHT)
                        // North polar cap vertex.
                        index = vertex_index_latitude_90;
                    // Special check for sphere to link up both sides at 180 / -180 degrees longitude.
                    else
                    if (mesh_x + x == 0)
                        // Use one of the extra vertices defined at longitude - 180 degrees.
                        index = vertex_index_longitude_minus_180 + (mesh_y + y);
                    else
                    if (mesh_x + x == MESH_WIDTH)
                        // Use one of the extra vertices defined at longitude 180 degrees.
                        index = vertex_index_longitude_180 + (mesh_y + y);
                    else
                        index = (mesh_y + y - y_offset) * MESH_WIDTH + (mesh_x + x - x_offset);
#else
                    index = (mesh_y + y) * MESH_WIDTH + (mesh_x + x);
#endif
                    if (index < 0 || index >= MESH_WIDTH * MESH_HEIGHT + MESH_HEIGHT * 2 + 2) {
                        printf("Index out of bounds (%d).\n", index);
                        exit(1);
                    }
                    m->vertex[y * w + x] = vertex[index];
                    m->texcoords[y * w + x] = texcoords[index];
                    m->vertex_normal[y * w + x] = vertex_normal[index];
#ifdef CLOSED_SEGMENTS_FOR_SHADOW_VOLUMES
                    if (vertex[index].z < zmin)
                        zmin = vertex[index].z;
#endif
                }
#ifdef CLOSED_SEGMENTS_FOR_SHADOW_VOLUMES
            zmin -= 0.001 * Z_SCALE;
            if (zmin <= 0) {
                printf("zmin out of bounds.\n");
                exit(1);
            }
            // Duplicate vertices at the sides and add vertices on the ground.
            // Side at y = 0.
            int v = w * h;
            int *vertex_index_ymin_ground = new int[w];
            int *vertex_index_ymin_edge = new int[w];
            for (int x = 0; x < w; x++) {
                m->vertex[v] = m->vertex[x];
                m->vertex_normal[v] = Vector3D(0, -1.0, 0);
                m->texcoords[v] = Point2D(0, 0);
                vertex_index_ymin_edge[x] = v;
                v++;
                m->vertex[v] = Point3D(m->vertex[x].x, m->vertex[x].y, zmin);
                m->vertex_normal[v] = Vector3D(0, -1.0, 0);
                m->texcoords[v] = Point2D(0, 0);
                vertex_index_ymin_ground[x] = v;
                v++;
            }
            // Side at y = h - 1.
            int *vertex_index_ymax_ground = new int[w];
            int *vertex_index_ymax_edge = new int[w];
            for (int x = 0; x < w; x++) {
                m->vertex[v] = m->vertex[(h - 1) * w + x];
                m->vertex_normal[v] = Vector3D(0, 1.0, 0);
                m->texcoords[v] = Point2D(0, 0);
                vertex_index_ymax_edge[x] = v;
                v++;
                m->vertex[v] = Point3D(m->vertex[(h - 1) * w + x].x, m->vertex[(h - 1) * w + x].y, zmin);
                m->vertex_normal[v] = Vector3D(0, 1.0, 0);
                m->texcoords[v] = Point2D(0, 0);
                vertex_index_ymax_ground[x] = v;
                v++;
            }
            // Side at x = 0.
            int *vertex_index_xmin_ground = new int[h];
            int *vertex_index_xmin_edge = new int[h];
            for (int y = 0; y < h; y++) {
                m->vertex[v] = m->vertex[y * w];
                m->vertex_normal[v] = Vector3D(- 1.0, 0, 0);
                m->texcoords[v] = Point2D(0, 0);
                vertex_index_xmin_edge[y] = v;
                v++;
                m->vertex[v] = Point3D(m->vertex[y * w].x, m->vertex[y * w].y, zmin);
                m->vertex_normal[v] = Vector3D(- 1.0, 0, 0);
                m->texcoords[v] = Point2D(0, 0);
                vertex_index_xmin_ground[y] = v;
                v++;
            }
            // Side at x = w - 1.
            int *vertex_index_xmax_ground = new int[h];
            int *vertex_index_xmax_edge = new int[h];
            for (int y = 0; y < h; y++) {
                m->vertex[v] = m->vertex[y * w + w - 1];
                m->vertex_normal[v] = Vector3D(1.0, 0, 0);
                m->texcoords[v] = Point2D(0, 0);
                vertex_index_xmax_edge[y] = v;
                v++;
                m->vertex[v] = Point3D(m->vertex[y * w + w - 1].x, m->vertex[y * w + w - 1].y, zmin);
                m->vertex_normal[v] = Vector3D(1.0, 0, 0);
                m->texcoords[v] = Point2D(0, 0);
                vertex_index_xmax_ground[y] = v;
                v++;
            }
            // Create bottom corner vertices for each side.
            int ymin_corner_vertex_index = v;
            m->vertex[v] =  Point3D(m->vertex[vertex_index_ymin_ground[0]].x, m->vertex[vertex_index_ymin_ground[0]].y, 0);
            m->vertex_normal[v] = Vector3D(0, - 1.0, 0);
            m->texcoords[v] = Vector2D(0, 0);
            v++;
            m->vertex[v] = Point3D(m->vertex[vertex_index_ymin_ground[w - 1]].x,
                m->vertex[vertex_index_ymin_ground[w - 1]].y, 0);
            m->vertex_normal[v] = Vector3D(0, - 1.0, 0);
            m->texcoords[v] = Vector2D(0, 0);
            v++;
            int ymax_corner_vertex_index = v;
            m->vertex[v] = Point3D(m->vertex[vertex_index_ymax_ground[w - 1]].x,
                m->vertex[vertex_index_xmax_ground[h - 1]].y, 0);
            m->vertex_normal[v] = Vector3D(0, 1.0, 0);
            m->texcoords[v] = Vector2D(0, 0);
            v++;
            m->vertex[v] =  Point3D(m->vertex[vertex_index_ymax_ground[0]].x,
                m->vertex[vertex_index_xmin_ground[h - 1]].y, 0);
            m->vertex_normal[v] = Vector3D(0, 1.0, 0);
            m->texcoords[v] = Vector2D(0, 0);
            v++;
            int xmin_corner_vertex_index = v;             
            m->vertex[v] = Point3D(m->vertex[vertex_index_xmin_ground[h - 1]].x,
                m->vertex[vertex_index_xmin_ground[h - 1]].y, 0);
            m->vertex_normal[v] = Vector3D(- 1.0, 0, 0);
            m->texcoords[v] = Vector2D(0, 0);
            v++;
            m->vertex[v] =  Point3D(m->vertex[vertex_index_xmin_ground[0]].x, m->vertex[vertex_index_xmin_ground[0]].y, 0);
            m->vertex_normal[v] = Vector3D(- 1.0, 0, 0);
            m->texcoords[v] = Vector2D(0, 0);
            v++;
            int xmax_corner_vertex_index = v;
            m->vertex[v] = Point3D(m->vertex[vertex_index_xmax_ground[0]].x, m->vertex[vertex_index_xmax_ground[0]].y, 0);
            m->vertex_normal[v] = Vector3D(1.0, 0, 0);
            m->texcoords[v] = Vector2D(0, 0);
            v++;
            m->vertex[v] = Point3D(m->vertex[vertex_index_xmax_ground[h - 1]].x,
                m->vertex[vertex_index_xmax_ground[h - 1]].y, 0);
            m->vertex_normal[v] = Vector3D(1.0, 0, 0);
            m->texcoords[v] = Vector2D(0, 0);
            v++;
            // Create bottom corner vertices for bottom.
            int bottom_corner_vertex_index = v;
            m->vertex[v] =  Point3D(m->vertex[vertex_index_xmin_ground[0]].x, m->vertex[vertex_index_xmin_ground[0]].y, 0);
            m->vertex_normal[v] = Vector3D(0, 0, - 1.0);
            m->texcoords[v] = Vector2D(0, 0);
            v++;
            m->vertex[v] = Point3D(m->vertex[vertex_index_xmax_ground[0]].x, m->vertex[vertex_index_xmax_ground[0]].y, 0);
            m->vertex_normal[v] = Vector3D(0, 0, - 1.0);
            m->texcoords[v] = Vector2D(0, 0);
            v++;
            m->vertex[v] = Point3D(m->vertex[vertex_index_xmin_ground[h - 1]].x,
                m->vertex[vertex_index_xmin_ground[h - 1]].y, 0);
            m->vertex_normal[v] = Vector3D(0, 0, - 1.0);
            m->texcoords[v] = Vector2D(0, 0);
            v++;
            m->vertex[v] = Point3D(m->vertex[vertex_index_xmax_ground[h - 1]].x,
                m->vertex[vertex_index_xmax_ground[h - 1]].y, 0);
            m->vertex_normal[v] = Vector3D(0, 0, - 1.0);
            m->texcoords[v] = Vector2D(0, 0);
            v++;
            if (v != m->nu_vertices) {
                printf("Mismatch in the number of vertices (%d vs %d).\n", v, m->nu_vertices);
                exit(1);
            }
#endif
            m->nu_triangles = 2 * (w - 1) * (h - 1);
#ifdef CLOSED_SEGMENTS_FOR_SHADOW_VOLUMES
            // Extra triangles at the y = 0 side.
            int side_triangles_ymin = 1 + (w - 3) / 2 + ((w - 3) / 2) * 2 + 3;
            if ((w - 3) & 1)
                side_triangles_ymin++; // One extra triangle in the middle section (x == uneven).
            m->nu_triangles += side_triangles_ymin;
            // Triangle fan. Number of vertices in the triangle fan is equal to the number of side vertices created + 1.
            // Side vertices are created at the start (two), one when x is even.
            // is uneven.
            m->nu_triangles += 2 + (w - 3) / 2 + 1;
            // Extra triangles at the y = h - 1 side.
            int side_triangles_ymax = 2 + (w - 3) / 2 + ((w - 3) / 2) * 2 + 1;
            if ((w - 3) & 1)
                side_triangles_ymax += 2; // Two extra triangles in the middle section (x == even)
            if (!((w - 3) & 1))
                side_triangles_ymax++; // One extra triangle at the end.
            m->nu_triangles += side_triangles_ymax;
            // Triangle fan. Number of vertices in the triangle fan is equal to the number of side vertices created + 1.
            // Side vertices are created at the start (two), one when x is even.
            m->nu_triangles += 2 + (w - 3) / 2 + 1;
            // Extra triangles at the x = 0 side.
            m->nu_triangles += 2 + (h - 3) / 2 + ((h - 3) / 2) * 2 + 1;
            if ((h - 3) & 1)
                m->nu_triangles += 2; // Two extra triangles in the middle section (y == even)
            if (!((h - 3) & 1))
                m->nu_triangles++; // One extra triangle at the end.
            // Triangle fan.
            m->nu_triangles += 2 + (h - 3) / 2 + 1;
            // Extra triangles at the x = w - 1 side.
            m->nu_triangles += 1 + (h - 3) / 2 + ((h - 3) / 2) * 2 + 3;
            if ((h - 3) & 1)
                m->nu_triangles++; // One extra triangle in the middle section (y == uneven)
            // Triangle fan.
            m->nu_triangles += 2 + (h - 3) / 2 + 1;
            m->nu_triangles += 2; // Two triangles for the bottom.
            bool *ymin_ground_vertex_used = new bool[w];
            bool *ymax_ground_vertex_used = new bool[w];
            bool *xmin_ground_vertex_used = new bool[h];
            bool *xmax_ground_vertex_used = new bool[h];
            for (int i = 0; i < w; i++) {
                ymin_ground_vertex_used[i] = false;
                ymax_ground_vertex_used[i] = false;
            }
            for (int i = 0; i < h; i++) {
                xmin_ground_vertex_used[i] = false;
                xmax_ground_vertex_used[i] = false;
            }
            // Create side triangles.
            int counted_triangles_ymin = 0;
            int counted_triangles_ymax = 0;
#endif
            m->triangle = new sreModelTriangle[m->nu_triangles];
            int t = 0;
            for (int y = 0; y < h - 1; y++)
                for (int x = 0; x < w - 1; x++) {
                    m->triangle[t].AssignVertices(y * w + x, y * w + x + 1, (y + 1) * w + x + 1);
                    m->triangle[t + 1].AssignVertices(y * w + x, (y + 1) * w + x + 1, (y + 1) * w + x);
                    t += 2;
#ifdef CLOSED_SEGMENTS_FOR_SHADOW_VOLUMES
                    // If we are on a side, add triangles to make a closed volume for shadow volumes.
                    if (y == 0) {
                        if (x == 0) {
                            m->triangle[t].AssignVertices(vertex_index_ymin_edge[x], vertex_index_ymin_ground[x],
                                vertex_index_ymin_ground[x + 1]);
                            ymin_ground_vertex_used[x] = true;
                            ymin_ground_vertex_used[x + 1] = true;
                            t++;
                            counted_triangles_ymin++;
                        }
                        else
                        if (x & 1) {
                            m->triangle[t].AssignVertices(vertex_index_ymin_edge[x], vertex_index_ymin_edge[x - 1],
                                vertex_index_ymin_ground[x]);
                            ymin_ground_vertex_used[x] = true;
                            t++;
                            counted_triangles_ymin++;
                            if (x == w - 2) {
                                m->triangle[t].AssignVertices(vertex_index_ymin_edge[x + 1], vertex_index_ymin_edge[x],
                                    vertex_index_ymin_ground[x]);
                                m->triangle[t + 1].AssignVertices(vertex_index_ymin_edge[x + 1], vertex_index_ymin_ground[x],
                                    vertex_index_ymin_ground[x + 1]);
                                ymin_ground_vertex_used[x + 1] = true;
                                t += 2;
                                counted_triangles_ymin += 2;
                            }
                        }
                        else {
                            m->triangle[t].AssignVertices(vertex_index_ymin_edge[x], vertex_index_ymin_edge[x - 1],
                                vertex_index_ymin_ground[x - 1]);
                            m->triangle[t + 1].AssignVertices(vertex_index_ymin_edge[x], vertex_index_ymin_ground[x - 1],
                                vertex_index_ymin_ground[x + 1]);
                            ymin_ground_vertex_used[x - 1] = true;
                            ymin_ground_vertex_used[x + 1] = true;
                            t += 2;
                            counted_triangles_ymin += 2;
                            if (x == w - 2) {
                                m->triangle[t].AssignVertices(vertex_index_ymin_edge[x + 1], vertex_index_ymin_edge[x],
                                    vertex_index_ymin_ground[x + 1]);
                                t++;
                                counted_triangles_ymin++;
                            }
                        }
                    }
                    if (y == h - 2) {
                        if (x == w - 2) {
                            m->triangle[t].AssignVertices(vertex_index_ymax_edge[x + 1], vertex_index_ymax_ground[x + 1],
                                vertex_index_ymax_ground[x]);
                            m->triangle[t + 1].AssignVertices(vertex_index_ymax_edge[x], vertex_index_ymax_edge[x + 1],
                                vertex_index_ymax_ground[x]);
                            ymax_ground_vertex_used[x] = true;
                            ymax_ground_vertex_used[x + 1] = true;
                            t += 2;
                            counted_triangles_ymax += 2;
                        }
                        else
                        if (x == 0 && ((w - 3) & 1)) {
                            m->triangle[t].AssignVertices(vertex_index_ymax_edge[x], vertex_index_ymax_edge[x + 1],
                                vertex_index_ymax_ground[x]);
                            ymax_ground_vertex_used[x] = true;
                            t++;
                            counted_triangles_ymax++;
                        }
                        else
                        if (x == 0 && !((w - 3) & 1)) {
                            m->triangle[t].AssignVertices(vertex_index_ymax_edge[x], vertex_index_ymax_edge[x + 1],
                                vertex_index_ymax_ground[x]);
                            m->triangle[t + 1].AssignVertices(vertex_index_ymax_edge[x + 1], vertex_index_ymax_ground[x + 1],
                                vertex_index_ymax_ground[x]);
                            ymax_ground_vertex_used[x] = true;
                            ymax_ground_vertex_used[x + 1] = true;
                            t += 2;
                            counted_triangles_ymax += 2;
                        }
                        else
                        if ((w - 3 - x) & 1) {
                            m->triangle[t].AssignVertices(vertex_index_ymax_edge[x], vertex_index_ymax_edge[x + 1],
                                vertex_index_ymax_ground[x]);
                            ymax_ground_vertex_used[x] = true;
                            t++;
                            counted_triangles_ymax++;
                        }
                        else {
                            m->triangle[t].AssignVertices(vertex_index_ymax_edge[x], vertex_index_ymax_edge[x + 1],
                                vertex_index_ymax_ground[x + 1]);
                            m->triangle[t + 1].AssignVertices(vertex_index_ymax_edge[x], vertex_index_ymax_ground[x + 1],
                                vertex_index_ymax_ground[x - 1]);
                            ymax_ground_vertex_used[x - 1] = true;
                            ymax_ground_vertex_used[x + 1] = true;
                            t += 2;
                            counted_triangles_ymax += 2;
                        }
                    }
                    if (x == 0) {
                        if (y == h - 2) {
                            m->triangle[t].AssignVertices(vertex_index_xmin_edge[y + 1], vertex_index_xmin_ground[y + 1],
                                vertex_index_xmin_ground[y]);
                            m->triangle[t + 1].AssignVertices(vertex_index_xmin_edge[y], vertex_index_xmin_edge[y + 1],
                                vertex_index_xmin_ground[y]);
                            xmin_ground_vertex_used[y] = true;
                            xmin_ground_vertex_used[y + 1] = true;
                            t += 2;
                        }
                        else
                        if (y == 0 && ((h - 3) & 1)) {
                            m->triangle[t].AssignVertices(vertex_index_xmin_edge[y], vertex_index_xmin_edge[y + 1],
                                vertex_index_xmin_ground[y]);
                            xmin_ground_vertex_used[y] = true;
                            t++;
                        }
                        else
                        if (y == 0 && !((h - 3) & 1)) {
                            m->triangle[t].AssignVertices(vertex_index_xmin_edge[y], vertex_index_xmin_edge[y + 1],
                                vertex_index_xmin_ground[y]);
                            m->triangle[t + 1].AssignVertices(vertex_index_xmin_edge[y + 1], vertex_index_xmin_ground[y + 1],
                                vertex_index_xmin_ground[y]);
                            xmin_ground_vertex_used[y] = true;
                            xmin_ground_vertex_used[y + 1] = true;
                            t += 2;
                        }
                        else
                        if ((h - 3 - y) & 1) {
                            m->triangle[t].AssignVertices(vertex_index_xmin_edge[y], vertex_index_xmin_edge[y + 1],
                                vertex_index_xmin_ground[y]);
                            xmin_ground_vertex_used[y] = true;
                            t++;
                        }
                        else {
                            m->triangle[t].AssignVertices(vertex_index_xmin_edge[y], vertex_index_xmin_edge[y + 1],
                                vertex_index_xmin_ground[y + 1]);
                            m->triangle[t + 1].AssignVertices(vertex_index_xmin_edge[y], vertex_index_xmin_ground[y + 1],
                                vertex_index_xmin_ground[y - 1]);
                            xmin_ground_vertex_used[y - 1] = true;
                            xmin_ground_vertex_used[y + 1] = true;
                            t += 2;
                        }
                    }
                    if (x == w - 2) {
                        if (y == 0) {
                            m->triangle[t].AssignVertices(vertex_index_xmax_edge[y], vertex_index_xmax_ground[y],
                                vertex_index_xmax_ground[y + 1]);
                            xmax_ground_vertex_used[y] = true;
                            xmax_ground_vertex_used[y + 1] = true;
                            t++;
                        }
                        else
                        if (y & 1) {
                            m->triangle[t].AssignVertices(vertex_index_xmax_edge[y], vertex_index_xmax_edge[y - 1],
                                vertex_index_xmax_ground[y]);
                            xmax_ground_vertex_used[y] = true;
                            t++;
                            if (y == h - 2) {
                                m->triangle[t].AssignVertices(vertex_index_xmax_edge[y + 1], vertex_index_xmax_edge[y],
                                    vertex_index_xmax_ground[y]);
                                m->triangle[t + 1].AssignVertices(vertex_index_xmax_edge[y + 1], vertex_index_xmax_ground[y],
                                    vertex_index_xmax_ground[y + 1]);
                                xmax_ground_vertex_used[y + 1] = true;
                                t += 2;
                            }
                        }
                        else {
                            m->triangle[t].AssignVertices(vertex_index_xmax_edge[y], vertex_index_xmax_edge[y - 1],
                                vertex_index_xmax_ground[y - 1]);
                            m->triangle[t + 1].AssignVertices(vertex_index_xmax_edge[y], vertex_index_xmax_ground[y - 1],
                                vertex_index_xmax_ground[y + 1]);
                            xmax_ground_vertex_used[y - 1] = true;
                            xmax_ground_vertex_used[y + 1] = true;
                            t += 2;
                            if (y == h - 2) {
                                m->triangle[t].AssignVertices(vertex_index_xmax_edge[y + 1], vertex_index_xmax_edge[y],
                                    vertex_index_xmax_ground[y + 1]);
                                t++;
                            }
                        }
                    }
#endif
                }
#ifdef CLOSED_SEGMENTS_FOR_SHADOW_VOLUMES
            if (counted_triangles_ymin != side_triangles_ymin) {
                printf("Mismatch in number of ymin side triangles (%d vs %d).\n", counted_triangles_ymin,
                    side_triangles_ymin);
                exit(1);
            }
            if (counted_triangles_ymax != side_triangles_ymax) {
                printf("Mismatch in number of ymax side triangles (%d vs %d).\n", counted_triangles_ymax,
                    side_triangles_ymax);
                exit(1);
            }
            // Create triangle fans for the bottom part of each side.
            int previous_vertex = vertex_index_ymin_ground[0];
            for (int x = 1; x < w; x++)
                if (ymin_ground_vertex_used[x]) {
                    m->triangle[t].AssignVertices(ymin_corner_vertex_index, vertex_index_ymin_ground[x], previous_vertex);
                    previous_vertex = vertex_index_ymin_ground[x];
                    t++;
                }
            m->triangle[t].AssignVertices(ymin_corner_vertex_index, ymin_corner_vertex_index + 1, previous_vertex);
            t++;
            previous_vertex = vertex_index_ymax_ground[w - 1];
            for (int x = w - 2; x >= 0; x--)
                if (ymax_ground_vertex_used[x]) {
                    m->triangle[t].AssignVertices(ymax_corner_vertex_index, vertex_index_ymax_ground[x], previous_vertex);
                    previous_vertex = vertex_index_ymax_ground[x];
                    t++;
                }
            m->triangle[t].AssignVertices(ymax_corner_vertex_index, ymax_corner_vertex_index + 1, previous_vertex);
            t++;
            previous_vertex = vertex_index_xmin_ground[h - 1];
            for (int y = h - 2; y >= 0; y--)
                if (xmin_ground_vertex_used[y]) {
                    m->triangle[t].AssignVertices(xmin_corner_vertex_index, vertex_index_xmin_ground[y], previous_vertex);
                    previous_vertex = vertex_index_xmin_ground[y];
                    t++;
                }
            m->triangle[t].AssignVertices(xmin_corner_vertex_index, xmin_corner_vertex_index + 1, previous_vertex);
            t++;
            previous_vertex = vertex_index_xmax_ground[0];
            for (int y = 1; y < h; y++)
                if (xmax_ground_vertex_used[y]) {
                    m->triangle[t].AssignVertices(xmax_corner_vertex_index, vertex_index_xmax_ground[y], previous_vertex);
                    previous_vertex = vertex_index_xmax_ground[y];
                    t++;
                }
            m->triangle[t].AssignVertices(xmax_corner_vertex_index, xmax_corner_vertex_index + 1, previous_vertex);
            t++;
            // Create the bottom.
            m->triangle[t].AssignVertices(bottom_corner_vertex_index, bottom_corner_vertex_index + 2,
                bottom_corner_vertex_index + 1);
            m->triangle[t + 1].AssignVertices(bottom_corner_vertex_index + 2, bottom_corner_vertex_index + 3,
                bottom_corner_vertex_index + 1);
            t += 2;
            if (t != m->nu_triangles) {
                printf("Mismatch in the number of triangles (%d vs %d), w = %d, h = %d.\n", t, m->nu_triangles, w, h);
                exit(1);
            }
            delete [] vertex_index_ymin_ground;
            delete [] vertex_index_ymin_edge;
            delete [] vertex_index_ymax_ground;
            delete [] vertex_index_ymax_edge;
            delete [] vertex_index_xmin_ground;
            delete [] vertex_index_xmin_edge;
            delete [] vertex_index_xmax_ground;
            delete [] vertex_index_xmax_edge;
            delete [] ymin_ground_vertex_used;
            delete [] ymax_ground_vertex_used;
            delete [] xmin_ground_vertex_used;
            delete [] xmax_ground_vertex_used;
#endif
            m->flags = SRE_POSITION_MASK | SRE_TEXCOORDS_MASK | SRE_NORMAL_MASK;
#ifndef CLOSED_SEGMENTS_FOR_SHADOW_VOLUMES
//            m->flags |= SRE_LOD_MODEL_NO_SHADOW_VOLUME_SUPPORT;
            m->flags |= SRE_LOD_MODEL_NOT_CLOSED;
#endif
            m->RemoveEmptyTriangles();
            m->RemoveUnusedVertices();
            m->CalculateTriangleNormals();
            total_triangle_count += m->nu_triangles;
            m->ReduceTriangleCount(0.5f, 0.05f, true, 0.995); // Cost threshold was 0.03.
            total_triangle_count_reduced += m->nu_triangles;
            m->SortVerticesOptimalDimension();
            // Vertex normals cannot be recalculated because it would result in discrepancies at the edges.
            m->CalculateTriangleNormals();
            model->lod_model[0] = m;
            model->lod_model[1] = sreNewLODModel();
            m->Clone(model->lod_model[1]);
            model->lod_model[1]->ReduceTriangleCount(0.2f, 0.4f, true, 0.97f);
            model->lod_model[1]->SortVerticesOptimalDimension();
            model->lod_model[1]->CalculateTriangleNormals();
            // Create a third LOD model.
            model->lod_model[2] = sreNewLODModel();
            model->lod_model[1]->Clone(model->lod_model[2]);
            model->lod_model[2]->ReduceTriangleCount(0.1f, 0.8f, true, 0.9f);
            model->lod_model[2]->SortVerticesOptimalDimension();
            model->lod_model[2]->CalculateTriangleNormals();
            model->nu_lod_levels = 3;

            // Mandate a significant (> 30%) reduction in triangle count between
            // LOD levels.
            float ratio2_0 = (float)model->lod_model[2]->nu_triangles /
                (float)model->lod_model[0]->nu_triangles;
            float ratio1_0 = (float)model->lod_model[1]->nu_triangles /
                (float)model->lod_model[0]->nu_triangles;
            float ratio2_1 = (float)model->lod_model[2]->nu_triangles /
                (float)model->lod_model[1]->nu_triangles;
            model->lod_threshold_scaling = 1.0f;
            if (ratio2_0 >= 0.7f) {
                model->nu_lod_levels = 1;
                delete model->lod_model[1];
                delete model->lod_model[2];
            }
            else if (ratio1_0 < 0.7f) {
                if (ratio2_1 < 0.7f) {
                    // Both LOD levels reduce the count noticeably, keep all three.
                }
                else {
                    // Discard level 2 since it does not offer a significantly reduced
                    // count.
                    delete model->lod_model[2];
                    model->nu_lod_levels = 2;
                }
            }
            else {
                // Only the ratio between levels 2 and 0 reaches 70%.
                // Use level 2 as level 1, and increase threshold scaling so that
                // level 1 is triggered as would otherwise be level 2.
                delete model->lod_model[1];
                model->lod_model[1] = model->lod_model[2];
                model->nu_lod_levels = 2;
                model->lod_threshold_scaling =
                    SRE_LOD_LEVEL_1_THRESHOLD / SRE_LOD_LEVEL_2_THRESHOLD;
            }
            printf("Using %d out of 3 LOD levels.\n", model->nu_lod_levels);

            model->CalculateBounds();
            model->collision_shape_static = SRE_COLLISION_SHAPE_STATIC;
            model->collision_shape_dynamic = SRE_COLLISION_SHAPE_CONVEX_HULL;
            scene->RegisterModel(model);
//            printf("%d vertices, %d triangles.\n", m->nu_vertices, m->nu_triangles);
        }
    delete [] vertex;
    delete [] texcoords;
    delete [] vertex_normal;
    printf("%d of %d triangles (%d%%) removed by edge collapse.\n", total_triangle_count - total_triangle_count_reduced,
        total_triangle_count, (total_triangle_count - total_triangle_count_reduced) * 100 / total_triangle_count);
}

// Loading/saving of Earth model meshes.

static char *GetMeshModelFileName(int x, int y) {
    int color_w, color_h;
#ifdef RESOLUTION_16K
    color_w = 16200;
    color_h = 8100;
#elif defined(RESOLUTION_16384)
    color_w = 16384;
    color_h = 8192;
#else
    color_w = 8192;
    color_h = 4096;
#endif
    char *filename = new char[256];
    sprintf(filename, "earth-meshes/earth-mesh-x%dy%d-elevation-map-%s-%dx%d-detail-%d"
        ".srebinarymodel",
        x, y, earth_heightmap_filename,
        ELEVATION_MAP_WIDTH, ELEVATION_MAP_HEIGHT, ELEVATION_MAP_DETAIL_FACTOR);
    char *path = new char[256];
    int path_size = 256;
    while (getcwd(path, path_size) == NULL) {
        delete [] path;
        path_size *= 2;
        path = new char[path_size];
    }
    char *full_path = new char[strlen(path) + strlen(filename) + 2];
    strcpy(full_path, path);
    delete [] path;
    strcat(full_path, "/");
    strcat(full_path, filename);
    delete [] filename;
    return full_path;
}

static bool FileExists(const char *filename) {
#ifdef __GNUC__
    struct stat stat_buf;
    int r = stat(filename, &stat_buf);
    if (r == - 1)
        return false;
    return true;
#else
    FILE *f = fopen(filename, "rb");
    if (f == NULL)
	return false;
    fclose(f);
    return true;
#endif
}

bool MeshObjectFilesExist() {
    for (int y = 0; y < SUB_MESHES_Y; y++)
        for (int x = 0; x < SUB_MESHES_X; x++) {
            char *filename = GetMeshModelFileName(x, y);
            bool exists = FileExists(filename);
            delete [] filename;
            if (!exists)
                return false;
        }
    return true;
}

static bool LoadMeshObjects(sreScene *scene, sreModel *mesh_model[SUB_MESHES_Y][SUB_MESHES_X]) {
    if (!MeshObjectFilesExist())
        return false;
    for (int y = 0; y < SUB_MESHES_Y; y++)
        for (int x = 0; x < SUB_MESHES_X; x++) {
            char *filename = GetMeshModelFileName(x, y);
            mesh_model[y][x] = sreReadModelFromSREBinaryModelFile(scene, filename, 0);
            delete [] filename;
        }
    return true;
}

static void SaveMeshObjects(sreScene *scene, sreModel *mesh_model[SUB_MESHES_Y][SUB_MESHES_X]) {
    if (!FileExists("earth-meshes")) {
        system("mkdir earth-meshes");
    }
    for (int y = 0; y < SUB_MESHES_Y; y++)
        for (int x = 0; x < SUB_MESHES_X; x++) {
            char *filename = GetMeshModelFileName(x, y);
            sreSaveModelToSREBinaryModelFile(mesh_model[y][x], filename, 0);
            delete [] filename;
        }
}

static sreTexture *spacecraft_emission_map;

static void Fill(sreTexture *texture, int x, int y, int w, int h, unsigned int color) {
    for (int i = y; i < y + h; i++)
        for (int j = x; j < x + w; j++)
            texture->SetPixel(j, i, color);
}

static void CreateSpacecraftTexture() {
    spacecraft_emission_map = new sreTexture(256, 128);
    Fill(spacecraft_emission_map, 0, 0, 256, 128, 0);
    unsigned int yellow = 150 | (150 << 8) | 0xFF000000;
    unsigned int bright_yellow = 255 | (255 << 8) | 0xFF000000;
    unsigned int grey = 150 | (150 << 8) | (150 << 16) | 0xFF000000;
    unsigned int red = 150 | 0xFF000000;
    Fill(spacecraft_emission_map, 0, 0, 256, 8, grey);     // Top circle.
    Fill(spacecraft_emission_map, 0, 120, 256, 8, bright_yellow); // Bottom circle.
    for (int i = 0; i < 16; i++) {
        Fill(spacecraft_emission_map, i * 16, 61, 6, 6, yellow); // Colored windows on the sides.
        Fill(spacecraft_emission_map, i * 16 + 8, 61, 6, 6, red);
    }
    for (int j = 0; j < 26; j += 4)
        for (int i = 0; i < 64; i++) {
            if ((i & 7) == 7)
                continue;
            Fill(spacecraft_emission_map, i * 4, 24 + j, 2, 2, yellow); // Small windows.
            Fill(spacecraft_emission_map, i * 4, 103 - j, 2, 2, yellow);
        }
    spacecraft_emission_map->type = TEXTURE_TYPE_LINEAR;
    spacecraft_emission_map->UploadGL(false);
}

void Demo4CreateScene(sreScene *scene, sreView *view) {
    // Disable shadow volumes should improve performance a little
    // (shadow volumes work, but show some artifacts). 16-bit vertices
    // indices can be used for more mesh models, and some GPU memory is
    // freed.
    sreSetShadowVolumeSupport(false);
    int physics_flag = 0;
    if (!physics)
        physics_flag = SRE_OBJECT_NO_PHYSICS;
    int hidden_flag = 0;
    int shadows_flag = SRE_OBJECT_CAST_SHADOWS;
    if (!show_spacecraft) {
        hidden_flag = SRE_OBJECT_HIDDEN;
        shadows_flag = 0;
    }
    Vector3D initial_ascend_vector = Vector3D(1.0, 0, 0);
//    Vector3D initial_ascend_vector = Vector3D(0, 0, 1.0);
    initial_ascend_vector.Normalize();
#ifdef SPHERE
    scene->SetAmbientColor(Color(0.03f, 0.03f, 0.03f));
#else
    scene->SetAmbientColor(Color(0.2, 0.2, 0.2));
#endif
    sreModel *globe_model = sreCreateSphereModel(scene, 0);
    if (create_spacecraft) {
        // Add player sphere as scene object 0.
        scene->SetFlags(SRE_OBJECT_DYNAMIC_POSITION | shadows_flag |
            SRE_OBJECT_USE_TEXTURE | physics_flag | hidden_flag);
        scene->SetTexture(sreCreateStripesTexture(TEXTURE_TYPE_LINEAR,
            256, 256, 32, Color(0, 0.5f, 0.8f), Color(0.9f, 0.9f, 1.0f)));
        scene->SetDiffuseReflectionColor(Color(1.0f, 1.0f, 1.0f));
        scene->SetMass(3.0f);
#ifdef SPHERE
        Point3D pos = Point3D(0, 0, 0) + (0.5 * X_SCALE + 200.0) * initial_ascend_vector;
        player_object = scene->AddObject(globe_model, pos.x, pos.y, pos.z, 0, 0, 0, 3.0);
#else
        player_object = scene->AddObject(globe_model, 0, 0, 200.0, 0, 0, 0, 3.0);
#endif
    }
    // Add player spacecraft as scene object 1.
    Point3D spacecraft_pos;
    sreModel *spacecraft_model;
    if (!create_spacecraft)
        goto skip_spacecraft;
    spacecraft_model = sreCreateEllipsoidModel(scene, 1.0, 0.3);
    scene->SetDiffuseReflectionColor(Color(0.1, 0.5, 0.1));
    scene->SetSpecularReflectionColor(Color(0.2, 0.2, 0.2));
    CreateSpacecraftTexture();
    scene->SetEmissionColor(Color(1.0, 1.0, 1.0));  
    scene->SetEmissionMap(spacecraft_emission_map);
    scene->SetFlags(SRE_OBJECT_DYNAMIC_POSITION | shadows_flag |
        SRE_OBJECT_USE_EMISSION_MAP | physics_flag | hidden_flag);
#ifdef SPHERE
    spacecraft_pos = Point3D(0, 0, 0) + (0.5 * X_SCALE + 300.0) * initial_ascend_vector;
    spacecraft_object = scene->AddObject(spacecraft_model, spacecraft_pos.x, spacecraft_pos.y, spacecraft_pos.z, 0, 0, 0, 8.0);
#else
    spacecraft_object = scene->AddObject(spacecraft_model, 0, 0, 300.0, 0, 0, 0, 8.0);
#endif
skip_spacecraft :
    scene->SetSpecularReflectionColor(Color(1.0, 1.0, 1.0));
    // Add sun sphere.
    scene->SetFlags(SRE_OBJECT_DYNAMIC_POSITION | SRE_OBJECT_NO_PHYSICS |
        SRE_OBJECT_EMISSION_ONLY | SRE_OBJECT_INFINITE_DISTANCE);
    scene->SetEmissionColor(Color(3.0, 3.0, 2.4));
#ifdef SPHERE
    sun_object = scene->AddObject(globe_model, 1000000.0, 0, 0, 0, 0, 0, 30000.0);
#else
    Vector3D sun_N = Vector3D(0.6, 0.8, 0.5).Normalize();
    sun_object = scene->AddObject(globe_model, sun_N.x * 1000000.0, sun_N.y * 1000000.0, sun_N.z * 1000000.0, 0, 0, 0,
        30000.0);
#endif
    scene->SetEmissionColor(Color(0, 0, 0));

    // Add terrain.
    scene->SetDiffuseReflectionColor(Color(1.0, 1.0, 1.0));
    earth_texture = new sreTexture(earth_texture_filename, TEXTURE_TYPE_NORMAL);
    scene->SetTexture(earth_texture);
#ifdef NIGHT
    earth_night_light_texture = new sreTexture(earth_night_light_filename, TEXTURE_TYPE_NORMAL);
    scene->SetEmissionMap(earth_night_light_texture);
    scene->SetEmissionColor(Color(1.0, 1.0, 1.0));
    scene->SetFlags(SRE_OBJECT_USE_EMISSION_MAP | SRE_OBJECT_USE_TEXTURE |
        SRE_OBJECT_USE_SPECULARITY_MAP | SRE_OBJECT_CAST_SHADOWS |
        physics_flag);
#else
#ifdef SPHERE
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_EARTH_SHADER |
        SRE_OBJECT_USE_SPECULARITY_MAP | SRE_OBJECT_CAST_SHADOWS | physics_flag);
#else
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | /* SRE_OBJECT_NO_PHYSICS | */
        SRE_OBJECT_USE_SPECULARITY_MAP | SRE_OBJECT_CAST_SHADOWS | physics_flag);
#endif
#endif
    earth_heightmap = new sreTexture(earth_heightmap_filename, TEXTURE_TYPE_WILL_MERGE_LATER);
    sreModel *mesh_model[SUB_MESHES_Y][SUB_MESHES_X];
    for (int y = 0; y < SUB_MESHES_Y; y++)
        for (int x = 0; x < SUB_MESHES_X; x++)
             mesh_model[y][x] = new sreModel;
    if (!LoadMeshObjects(scene, mesh_model)) {
        CreateMeshObjects(scene, mesh_model);
        SaveMeshObjects(scene, mesh_model);
    }
    delete earth_heightmap;
    scene->SetSpecularExponent(120.0);
    earth_specularity = new sreTexture(earth_specularity_filename, TEXTURE_TYPE_SPECULARITY_MAP);
    scene->SetSpecularityMap(earth_specularity);
#if defined(SPHERE) && !defined(NIGHT)
    // Set city light emission map for earth shader.
    earth_night_light_texture = new sreTexture(earth_night_light_filename, TEXTURE_TYPE_NORMAL);
    scene->SetEmissionMap(earth_night_light_texture);
#endif
    for (int y = 0; y < SUB_MESHES_Y; y++)
        for (int x = 0; x < SUB_MESHES_X; x++) {
            // Dynamic LOD, starting from level 0 with threshold scale factor of 6.0;
            // there are one, two or three LOD levels, depending on the gains at
            // subsequent LOD levels; the second highest level is used for physics.
            int physics_lod_level = mesh_model[y][x]->nu_lod_levels - 2;
            physics_lod_level = maxi(0, physics_lod_level);
            scene->SetLevelOfDetail(SRE_LOD_DYNAMIC, 0, - 1,
                extra_lod_threshold_scaling, physics_lod_level);
            scene->AddObject(mesh_model[y][x], 0, 0, 0, 0, 0, 0, 1.0f);
        }
#if 0
    // Miniature.
    for (int y = 0; y < SUB_MESHES_Y; y++)
        for (int x = 0; x < SUB_MESHES_X; x++) {
            scene->AddObject(mesh_model[y][x], 0, 0, 10.0, 0, 0, 0, 0.01);
        }
#endif
    scene->SetLevelOfDetail(SRE_LOD_DYNAMIC, 0, - 1, 1.0, 0);

    // Lights.
#ifndef NIGHT
    Vector3D lightdir = Vector3D(- 0.6, - 0.8, - 0.5).Normalize();
    lightdir.Normalize();
    directional_light = scene->AddDirectionalLight(SRE_LIGHT_DYNAMIC_DIRECTION,
        lightdir, Color(0.7f, 0.7f, 0.7f) * sun_light_factor);
#endif
#if defined(NIGHT) || defined(SPHERE)
    if (create_spacecraft) {
        Vector3D spot_dir = Vector3D(0, 1.0f, - 0.5f).Normalize();
        Vector3D rel_pos = Vector3D(0, 0, 0); // Vector3D(0, 0, - 2.4f);
        spacecraft_spot_light = scene->AddSpotLight(
            SRE_LIGHT_DYNAMIC_POSITION | SRE_LIGHT_DYNAMIC_DIRECTION,
            spacecraft_pos - rel_pos, spot_dir, 27.0f, 500.0f, Color(1.2f, 1.2f, 1.2f));
        scene->AttachLight(spacecraft_object, spacecraft_spot_light, Vector3D(0, 0, 0),
            spot_dir);
    }
#endif

#ifdef SPHERE
    sre_internal_application->SetFlags(sre_internal_application->GetFlags() |
        SRE_APPLICATION_FLAG_DYNAMIC_GRAVITY | SRE_APPLICATION_FLAG_NO_GROUND_PLANE);
    sre_internal_application->gravity_position.Set(0, 0, 0);
#else
    view->SetViewModeFollowObject(0, 40.0, Vector3D(0, 0, 10.0));
#endif
    if (create_spacecraft) {
        sre_internal_application->control_object = spacecraft_object;
#ifdef SPHERE
        sre_internal_application->hovering_height = Magnitude(ProjectOnto(scene->object[
            sre_internal_application->control_object]->position,
            initial_ascend_vector));
#else
        hovering_height = scene->object[sre_internal_application->control_object]->position.z;
#endif
    }
    sreSetHDRKeyValue(0.2f);

    sre_internal_application->SetFlags(sre_internal_application->GetFlags() |
        SRE_APPLICATION_FLAG_NO_GRAVITY);
}

static char message[80], message2[80];
static Vector3D forward_vector;
static Vector3D ascend_vector;
static Vector3D right_vector;
#define HOUR_OFFSET 11.0f

void Demo4Step(sreScene *scene, double demo_time) {
#if defined(SPHERE) && !defined(NIGHT)
    float h = demo_time + HOUR_OFFSET * day_interval / 24.0f;
    float hour = fmodf(h, day_interval) * 24.0f / day_interval;
    float day = (int)floor(fmod(hour / 24.0, 365.0));
    Matrix3D sr1;
    sr1.AssignRotationAlongZAxis(- hour * 2.0 * M_PI / 24.0);
    Matrix3D sr2;
    sr2.AssignRotationAlongXAxis(23.4 * M_PI / 180.0);
    Matrix3D sr3;
    sr3.AssignRotationAlongZAxis(fmodf(demo_time, (day_interval * 365.0)) * 2.00 * M_PI /
        (day_interval * 365.0));
    Point3D sun_pos = Point3D(- X_SCALE * 1000.0f, 0, 0);
    sun_pos = ((sr3 * sr2) * sr1) * sun_pos;
    Vector3D light_dir = (- sun_pos.GetVector3D()).Normalize();
    scene->ChangeDirectionalLightDirection(directional_light, light_dir);
    scene->ChangePosition(sun_object, sun_pos.x, sun_pos.y, sun_pos.z);
    if (display_time) {
         sprintf(message, "%02d:%02dh Day %d", (int)floorf(hour),
            (int)floorf((hour - (int)floorf(hour)) * 60.0 / 100.0),
            (int)(floor(day) + 1));
        sre_internal_application->text_message[0] = message; 
        sre_internal_application->text_message_time = sre_internal_backend->GetCurrentTime();
        sre_internal_application->nu_text_message_lines = 1;
    }
#endif
    if (!create_spacecraft)
        return;

    saved_hovering_height = sre_internal_application->hovering_height;

    ascend_vector = Vector3D(0, 0, 1.0);
    float view_distance;
    if (sre_internal_application->control_object == spacecraft_object) {
        if (show_spacecraft)
            view_distance = 100.0;
        else
            view_distance = 0.1f;
        // Hide the player (ball) object.
        scene->object[player_object]->flags |= SRE_OBJECT_HIDDEN;
        scene->object[player_object]->flags &= ~SRE_OBJECT_CAST_SHADOWS;
        // Hide the spacecraft object when required.
        if (!show_spacecraft) {
            scene->object[spacecraft_object]->flags |= SRE_OBJECT_HIDDEN;
            scene->object[spacecraft_object]->flags &= ~SRE_OBJECT_CAST_SHADOWS;
        }
    }
    else {
        view_distance = 40.0;
        // Show the player (ball) object.
        scene->object[player_object]->flags &= ~SRE_OBJECT_HIDDEN;
        scene->object[player_object]->flags |= SRE_OBJECT_CAST_SHADOWS;
        // Also show the spacecraft up in the air.
        scene->object[spacecraft_object]->flags &= ~SRE_OBJECT_HIDDEN;
        scene->object[spacecraft_object]->flags |= SRE_OBJECT_CAST_SHADOWS;
    }
#ifdef SPHERE
    // Set viewing direction.
    Vector3D up_vector = scene->object[sre_internal_application->control_object]->position;
    up_vector.Normalize();
    sre_internal_application->view->SetAscendVector(up_vector);
    ascend_vector = up_vector;
    // Define the basal forward direction as looking down a meridian from the north pole (negative latitude direction).
    float latitude = asinf(ascend_vector.z);
    float longitude = atan2(ascend_vector.y, ascend_vector.x);
    sprintf(message2, "%.2f%c %.2f%c", fabsf(latitude * 180.0f / M_PI), latitude < 0 ? 'S' : 'N',
        fabsf(longitude * 180.0f / M_PI), longitude < 0 ? 'W' : 'E');
    sre_internal_application->text_message[1] = message2; 
    sre_internal_application->text_message_time = sre_internal_backend->GetCurrentTime();
    sre_internal_application->nu_text_message_lines = 2;

    Vector3D angles;
    sre_internal_application->view->GetViewAngles(angles);
#if 0
    printf("Longitude = %f, latitude = %f\n", longitude * 180.0 / M_PI, latitude * 180 / M_PI);
    float latitude2 = latitude + 0.01 * cosf(latitude) * cosf(angles.z * M_PI / 180.0);
    float longitude2 = longitude - 0.01 * cosf(latitude) * sinf(angles.z * M_PI / 180.0);
    Vector3D P2 = Vector3D(cosf(latitude2) * cosf(longitude2), cos(latitude2) * sinf(longitude2), sinf(latitude2));
    forward_vector = P2 - ascend_vector;
    // Use the differentation of the coordinate formulas, negated (because we looking in negative latitude direction)).
//    forward_vector = Vector3D(sinf(latitude) * cosf(longitude), sinf(latitude) * sinf(longitude), - cosf(latitude));
//    forward_vector = Cross(up_vector, Vector3D(1.0, 0, 0));
    forward_vector.Normalize();
#endif
        // Define two arbitrary points on the great circle defined by the up_vector and thetaz (angles.z).
        // The base direction is negative latitude.
        float latitude1 = latitude + M_PI / 4.0;
        float longitude1 = longitude;
        if (latitude1 < - 0.5 * M_PI) {
            latitude1 = - M_PI - latitude1;
            longitude1 += M_PI;
        }
        Point3D P1 = Vector3D(cosf(latitude1) * cosf(longitude1), cos(latitude1) * sinf(longitude1), sinf(latitude1));
        float latitude2 = latitude - M_PI / 4.0;
        float longitude2 = longitude;
        if (latitude2 > 0.5 * M_PI) {
            latitude2 = M_PI - latitude2;
            longitude2 += M_PI;
        }
        Point3D P2 = Vector3D(cosf(latitude2) * cosf(longitude2), cos(latitude2) * sinf(longitude2), sinf(latitude2));
        Matrix3D r1;
        float theta = angles.z * M_PI / 180.0f;
        r1.AssignRotationAlongAxis(up_vector, theta);
        P1 = r1 * P1;
        P2 = r1 * P2;
        // Calculate the normal of the new great circle.
        Vector3D great_circle_normal = CalculateNormal(Point3D(up_vector), P1, P2);
//        if (longitude < 0)
//            great_circle_normal = - great_circle_normal;
//        if (latitude < 0)
//            great_circle_normal = - great_circle_normal;

    right_vector = great_circle_normal;
    forward_vector = Cross(right_vector, up_vector);
    forward_vector.Normalize();
#if 0
    Matrix3D r1;
    r1.AssignRotationAlongAxis(up_vector, angles.z * M_PI / 180);
    forward_vector = r1 * forward_vector;
    Vector3D right_vector = Cross(forward_vector, up_vector);
    right_vector.Normalize();
#endif
    sre_internal_application->view->SetForwardVector(forward_vector);
    Matrix3D r2; 
    r2.AssignRotationAlongAxis(right_vector, - angles.x * M_PI / 180.0);
    Vector3D view_direction = r2 * forward_vector;
    Point3D viewpoint = scene->object[sre_internal_application->control_object]->position - view_distance * view_direction;
    // + up_vector * view_distance * 0.25;
    Point3D lookat = scene->object[sre_internal_application->control_object]->position; // viewpoint + view_direction;
    up_vector = r2 * up_vector;
    sre_internal_application->view->SetViewModeLookAt(viewpoint, lookat, up_vector);
    sre_internal_application->view->SetMovementMode(SRE_MOVEMENT_MODE_USE_FORWARD_AND_ASCEND_VECTOR);
    // Let spacecraft spot light point the right way.
    // This is now handled automatically by libsre.
//    scene->ChangeSpotOrBeamLightDirection(spacecraft_spot_light, - ascend_vector);
#endif
#ifndef SPHERE
    view->SetViewModeFollowObject(sre_internal_application->control_object, view_distance,
        Vector3D(0, 0, 0) /* Vector3D(0, 0, view_distance * 0.25) */);
#endif
}

void Demo4StepBeforePhysics(sreScene *scene, double demo_time) {
    if (sre_internal_application->flags & SRE_APPLICATION_FLAG_NO_GRAVITY) {
        if (sre_internal_application->control_object == player_object) {
            sre_internal_application->hovering_height = saved_hovering_height;
            sre_internal_application->hovering_height_acceleration = 0;
            scene->BulletChangeVelocity(spacecraft_object, Vector3D(0, 0, 0));
        }
        sre_internal_application->control_object = spacecraft_object;
    }
    else {
        if (sre_internal_application->control_object == spacecraft_object) {
            // Drop the player from the spacecraft.
            scene->BulletChangeVelocity(spacecraft_object, Vector3D(0, 0, 0));
            Point3D new_pos = scene->object[spacecraft_object]->position -
                ascend_vector * 15.0f;
//            scene->ChangePosition(player_object, new_pos);
            scene->BulletChangeVelocity(player_object, Vector3D(0, 0, 0));
            scene->BulletChangePosition(player_object, new_pos);
        }
        sre_internal_application->control_object = player_object;
    }
    Matrix3D spin_matrix;
    if (show_spacecraft)
        spin_matrix.AssignRotationAlongZAxis(fmod(demo_time, 4.0) * 2.0 * M_PI / 4.0);
    else {
        spin_matrix.SetIdentity();
    }
#ifdef SPHERE
    // Try to keep the spacecraft upright, parallel to the surface.
    if (sre_internal_application->control_object == spacecraft_object) {
        Matrix3D rot_matrix;
        rot_matrix.Set(ascend_vector, forward_vector, right_vector);
        Matrix3D r;
        r.AssignRotationAlongYAxis(M_PI / 2.0);
//        rot_matrix.SetRow(0, right_vector);
//        rot_matrix.SetRow(1, forward_vector);
//        rot_matrix.SetRow(2, ascend_vector);
        saved_spacecraft_rotation_matrix = (rot_matrix * r) * spin_matrix;         
        scene->BulletChangeRotationMatrix(sre_internal_application->control_object, saved_spacecraft_rotation_matrix);
    }
    else
        // The spacecraft is not being controlled, but should rotate.
        scene->BulletChangeRotationMatrix(spacecraft_object, saved_spacecraft_rotation_matrix * spin_matrix);
#else
     scene->BulletChangeRotationMatrix(spacecraft_object, spin_matrix);
#endif
    // Set the maximum horizontal velocity (over the surface), for the spacecraft it increases as the height increases.
    if (sre_internal_application->control_object == player_object)
        sre_internal_application->max_horizontal_velocity = 5.0f;
    else {
        float height = Magnitude(ProjectOnto(scene->object[spacecraft_object]->position, ascend_vector));
#ifdef SPHERE
        height -= 0.5f * X_SCALE;
#endif
        sre_internal_application->max_horizontal_velocity = 5.0f + height * 0.005f;
        sre_internal_application->horizontal_acceleration =
            sre_internal_application->max_horizontal_velocity * 2.0f;
        // The ascend/descend controls are also sensitive to the height above the surface.
        sre_internal_application->hovering_height_acceleration = 100.0f + height * 0.5f;
    }
#ifdef SPHERE
    // Set viewing distance, clip distance increases as height increases.
    float player_dist = Magnitude(scene->object[sre_internal_application->control_object]->position) - 0.5 * X_SCALE;
    float far_plane_dist = maxf(2000.0, X_SCALE * 0.1 + player_dist * X_SCALE / 5000.0);
    sreSetFarPlaneDistance(far_plane_dist);
    // Also increase the shadow mapping region as height increases.
    float factor;
    if (sre_internal_application->control_object == spacecraft_object)
        factor = far_plane_dist / 2000.0 + powf((far_plane_dist - 2000.0) / 2000.0, 0.2) * 4.0;
    else
        factor = 0.5;
    sreSetShadowMapRegion(Point3D(- 1000.0f, - 1000.0f, - 1000.0f) * factor,
        Point3D(1000.0f, 1000.0f, 200.0f) * factor);
//    sreSetShadowMapRegion(Point3D(- 400.0, - 400.0, - 600.0) * factor, Point3D(400.0, 400.0, 200.0) * factor);
#endif
}

void Demo4SetParameters(float interval, bool _display_time, bool _physics,
bool _create_spacecraft, bool _show_spacecraft, float _sun_light_factor,
float _extra_lod_threshold_scaling) {
   day_interval = interval;
   display_time = _display_time;
   physics = _physics;
   create_spacecraft = _create_spacecraft;
   show_spacecraft = _show_spacecraft;
   sun_light_factor = _sun_light_factor;
   extra_lod_threshold_scaling = _extra_lod_threshold_scaling;
}
