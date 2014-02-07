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
#include <math.h>
#include <float.h>

#include "sre.h"
#include "sre_bounds.h"
#include "demo.h"
#include "gui-common.h"

#define DAY_INTERVAL 1000.0
#define YEAR_INTERVAL (DAY_INTERVAL * 365.0)

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

//#define RESOLUTION_16K
#define SPHERE
//#define CLOSED_SEGMENTS_FOR_SHADOW_VOLUMES
//#define NIGHT
// The heightmap texture size is 16200x8100 if RESOLUTION_16K is set, 8192x4096 otherwise.
// The mesh size. MESH_WIDTH and MESH_HEIGHT must be a factor of the heightmap width and height, respectively, minus one.
#define MESH_WIDTH 1023
#define MESH_HEIGHT 511
// #define MESH_WIDTH (4050 - 1)
// #define MESH_HEIGHT (2025 - 1)
// The size of submeshes. Maximum MESH_WIDTH + 2 and MESH_HEIGHT + 2.
#define SUB_MESH_WIDTH 200
#define SUB_MESH_HEIGHT 200
// Zoom must be a power of two >= 1.
#define ZOOM 1
// LONGITUDE and LATITUDE, in degrees, define the center of the part of the world that is shown. Negative longitude is
// west.
#define LONGITUDE 0
#define LATITUDE 0
// Scaling defines the coordinate size in the world.
#define X_SCALE 10000.0f
#define Z_SCALE (5.0f * ZOOM)

#define MESH_TEXTURE_WIDTH (earth_heightmap->width / (MESH_WIDTH + 1))
#define MESH_TEXTURE_HEIGHT (earth_heightmap->height / (MESH_HEIGHT + 1))
#define SUB_MESHES_X ((MESH_WIDTH + SUB_MESH_WIDTH - 1) / (SUB_MESH_WIDTH - 1))
#define SUB_MESHES_Y ((MESH_HEIGHT + SUB_MESH_HEIGHT - 1) / (SUB_MESH_HEIGHT - 1))
#define X_OFFSET ((LONGITUDE + 180.0) * (MESH_WIDTH + 1) / 360.0 - (MESH_WIDTH + 1) / 2 / ZOOM)
#define Y_OFFSET ((LATITUDE + 90.0) * (MESH_HEIGHT + 1) / 180.0 - (MESH_HEIGHT + 1) / 2 / ZOOM)
#define Y_SCALE (X_SCALE * (MESH_WIDTH + 1) / (MESH_HEIGHT + 1) * 0.5)
#ifdef RESOLUTION_16K
static const char *earth_texture_filename = "4_no_ice_clouds_mts_16k";
static const char *earth_heightmap_filename = "elev_bump_16k";
static const char *earth_night_light_filename = "cities_16k";
static const char *earth_specularity_filename = "water_16k";
#else
static const char *earth_texture_filename = "4_no_ice_clouds_mts_8k";
static const char *earth_heightmap_filename = "elev_bump_8k";
static const char *earth_night_light_filename = "cities_8k";
static const char *earth_specularity_filename = "water_8k";
#endif

static sreTexture *earth_texture;
static sreTexture *earth_heightmap;
static sreTexture *earth_specularity;
static sreTexture *earth_night_light_texture;

static int player_object;
static int spacecraft_object;
static int sun_object;
static int directional_light;
static int spacecraft_spot_light;
static float saved_hovering_height;
static Matrix3D saved_spacecraft_rotation_matrix;

void CreateMeshObjects(sreModel *mesh_model[SUB_MESHES_Y][SUB_MESHES_X]) {
    printf("Creating mesh objects.\n");
    printf("Calculating vertices.\n");
    int v = 0;
    Point3D *vertex = new Point3D[MESH_WIDTH * MESH_HEIGHT + MESH_HEIGHT * 2 + 2];
    Point2D *texcoords = new Point2D[MESH_WIDTH * MESH_HEIGHT + MESH_HEIGHT * 2 + 2];
    for (int y = 0; y < MESH_HEIGHT; y++)
        for (int x = 0; x < MESH_WIDTH; x++) {
            // Calculate the average height in the area of size MESH_TEXTURE_WIDTH * MESH_TEXTURE_HEIGHT.
            long int h = 0;
            for (int i = 0; i < MESH_TEXTURE_HEIGHT / ZOOM; i++)
                for (int j = 0; j < MESH_TEXTURE_WIDTH / ZOOM; j++)
                    h += earth_heightmap->LookupPixel(x * (MESH_TEXTURE_WIDTH / ZOOM) + j + X_OFFSET * MESH_TEXTURE_WIDTH,
                        earth_heightmap->height - Y_OFFSET * MESH_TEXTURE_HEIGHT - (MESH_TEXTURE_HEIGHT / ZOOM) -
                            y * (MESH_TEXTURE_HEIGHT / ZOOM) + i);
#ifdef SPHERE
            float longitude = ((float)x + 0.5) / MESH_WIDTH * 2.0 * M_PI - M_PI;
            float latitude = ((float)y + 0.5) / MESH_HEIGHT * M_PI - 0.5 * M_PI;
            float radius = 0.5 * X_SCALE + (float)h * Z_SCALE / (500000.0 * (MESH_TEXTURE_WIDTH / ZOOM) *
                (MESH_TEXTURE_HEIGHT / ZOOM));
            float xcoord = radius * cosf(latitude) * cosf(longitude);
            float ycoord = radius * cosf(latitude) * sinf(longitude);
            float zcoord = radius * sinf(latitude);
#else
            float xcoord = (x - MESH_WIDTH / 2) * X_SCALE / MESH_WIDTH;
            float ycoord = (y - MESH_HEIGHT / 2) * Y_SCALE / MESH_WIDTH;
            float zcoord = (float)h * Z_SCALE / (2000000.0 * (MESH_TEXTURE_WIDTH / ZOOM) * (MESH_TEXTURE_HEIGHT / ZOOM));
#endif
            vertex[v].Set(xcoord, ycoord, zcoord);
            // Set the texcoords to the middle of the sampled area.
            // First calculate the texcoords in pixel units.
            float texcoords_x = x * (MESH_TEXTURE_WIDTH / ZOOM) + 0.5 * (MESH_TEXTURE_WIDTH / ZOOM) - 0.5 +
                X_OFFSET * MESH_TEXTURE_WIDTH;
            float texcoords_y = earth_heightmap->height - Y_OFFSET * MESH_TEXTURE_HEIGHT - (MESH_TEXTURE_HEIGHT / ZOOM) -
                y * (MESH_TEXTURE_HEIGHT / ZOOM) + 0.5 * (MESH_TEXTURE_HEIGHT / ZOOM) + 0.5;
            texcoords[v].Set(texcoords_x / earth_texture->width, texcoords_y / earth_texture->height);
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
        texcoords[v].Set(0, texcoords_y / earth_texture->height);
        v++;
    }
    // Define special column of vertices at 180 degrees longitude.
    int vertex_index_longitude_180 = v;
    for (int y = 0; y < MESH_HEIGHT; y++) {
        vertex[v].Set(0.5 * (vertex[y * MESH_WIDTH].x + vertex[y * MESH_WIDTH + MESH_WIDTH - 1].x),
            0, 0.5 * (vertex[y * MESH_WIDTH].z + vertex[y * MESH_WIDTH + MESH_WIDTH - 1].z));
        float texcoords_y = earth_heightmap->height - Y_OFFSET * MESH_TEXTURE_HEIGHT - (MESH_TEXTURE_HEIGHT / ZOOM) -
            y * (MESH_TEXTURE_HEIGHT / ZOOM) + 0.5 * (MESH_TEXTURE_HEIGHT / ZOOM) + 0.5;
        texcoords[v].Set(1.0, texcoords_y / earth_texture->height);
        v++;
    }
    // Define special vertices at the south and north polar caps.
    int vertex_index_latitude_minus_90 = v;
    int height = earth_heightmap->LookupPixel(earth_heightmap->width / 2, earth_heightmap->height - 1);
    vertex[v].Set(0, 0, - 0.5 * X_SCALE - (float)height * Z_SCALE / 500000.0);
    texcoords[v].Set(0, 1.0);
    v++;
    int vertex_index_latitude_90 = v;
    height = earth_heightmap->LookupPixel(earth_heightmap->width / 2, 0);
    vertex[v].Set(0, 0, 0.5 * X_SCALE + (float)height * Z_SCALE / 500000.0);
    texcoords[v].Set(0, 0);
    v++;
#endif
    ModelTriangle *triangle = new ModelTriangle[(MESH_WIDTH - 1) * (MESH_HEIGHT - 1) * 2];
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
            sreLODModel *m = model->lod_model[0];
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
            m->nu_vertices = w * h;
#ifdef CLOSED_SEGMENTS_FOR_SHADOW_VOLUMES
            m->nu_vertices += w * 2 + w * 2 + h * 2 + h * 2 + 8 + 4; // Extra edge vertices.
#endif
            m->vertex = new Point3D[m->nu_vertices];
            m->texcoords = new Point2D[m->nu_vertices];
            m->vertex_normal = new Vector3D[m->nu_vertices];
            float zmin = FLT_MAX;
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++) {
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
            m->triangle = new ModelTriangle[m->nu_triangles];
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
            m->ReduceTriangleCount(0.5, 0.03, true, 0.995);
            total_triangle_count_reduced += m->nu_triangles;
            m->SortVertices(1);
            // Vertex normals cannot be recalculated because it would result in discrepancies at the edges.
            m->CalculateTriangleNormals();
            model->lod_model[0] = m;
            model->lod_model[1] = sreNewLODModel();
            model->nu_lod_levels = 2;
            m->Clone(model->lod_model[1]);
            model->lod_model[1]->ReduceTriangleCount(0.2, 0.5, true, 0.98);
            model->lod_model[1]->SortVertices(1);
            model->lod_model[1]->CalculateTriangleNormals();
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
    spacecraft_emission_map->UploadGL(TEXTURE_TYPE_LINEAR);
}

void Demo4CreateScene() {
    Vector3D initial_ascend_vector = Vector3D(1.0, 0, 0);
//    Vector3D initial_ascend_vector = Vector3D(0, 0, 1.0);
    initial_ascend_vector.Normalize();
#ifdef SPHERE
    scene->SetAmbientColor(Color(0.01, 0.01, 0.01));
#else
    scene->SetAmbientColor(Color(0.2, 0.2, 0.2));
#endif
    sreModel *globe_model = sreCreateSphereModel(scene, 0);
    // Add player sphere as scene object 0.
    scene->SetFlags(SRE_OBJECT_DYNAMIC_POSITION | SRE_OBJECT_CAST_SHADOWS);
    scene->SetDiffuseReflectionColor(Color(0, 0.75, 1.0));
#ifdef SPHERE
    Point3D pos = Point3D(0, 0, 0) + (0.5 * X_SCALE + 200.0) * initial_ascend_vector;
    player_object = scene->AddObject(globe_model, pos.x, pos.y, pos.z, 0, 0, 0, 3.0);
#else
    player_object = scene->AddObject(globe_model, 0, 0, 200.0, 0, 0, 0, 3.0);
#endif
    // Add player spacecraft as scene object 1.
    sreModel *spacecraft_model = sreCreateEllipsoidModel(scene, 1.0, 0.3);
    scene->SetDiffuseReflectionColor(Color(0.1, 0.5, 0.1));
    scene->SetSpecularReflectionColor(Color(0.2, 0.2, 0.2));
    CreateSpacecraftTexture();
    scene->SetEmissionColor(Color(1.0, 1.0, 1.0));  
    scene->SetEmissionMap(spacecraft_emission_map);
    scene->SetFlags(SRE_OBJECT_DYNAMIC_POSITION | SRE_OBJECT_CAST_SHADOWS | SRE_OBJECT_USE_EMISSION_MAP);
#ifdef SPHERE
    Point3D spacecraft_pos = Point3D(0, 0, 0) + (0.5 * X_SCALE + 300.0) * initial_ascend_vector;
    spacecraft_object = scene->AddObject(spacecraft_model, spacecraft_pos.x, spacecraft_pos.y, spacecraft_pos.z, 0, 0, 0, 8.0);
#else
    spacecraft_object = scene->AddObject(spacecraft_model, 0, 0, 300.0, 0, 0, 0, 8.0);
#endif
    scene->SetSpecularReflectionColor(Color(1.0, 1.0, 1.0));
    // Add sun sphere.
    scene->SetFlags(SRE_OBJECT_DYNAMIC_POSITION | SRE_OBJECT_NO_PHYSICS | SRE_OBJECT_EMISSION_ONLY | SRE_OBJECT_INFINITE_DISTANCE);
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
        SRE_OBJECT_USE_SPECULARITY_MAP | SRE_OBJECT_CAST_SHADOWS);
#else
#ifdef SPHERE
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_EARTH_SHADER |
        SRE_OBJECT_USE_SPECULARITY_MAP | SRE_OBJECT_CAST_SHADOWS);
#else
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | /* SRE_OBJECT_NO_PHYSICS | */
        SRE_OBJECT_USE_SPECULARITY_MAP | SRE_OBJECT_CAST_SHADOWS);
#endif
#endif
    earth_heightmap = new sreTexture(earth_heightmap_filename, TEXTURE_TYPE_WILL_MERGE_LATER);
    sreModel *mesh_model[SUB_MESHES_Y][SUB_MESHES_X];
    for (int y = 0; y < SUB_MESHES_Y; y++)
        for (int x = 0; x < SUB_MESHES_X; x++)
             mesh_model[y][x] = new sreModel;
    CreateMeshObjects(mesh_model);
    delete earth_heightmap;
    scene->SetSpecularExponent(120.0);
    earth_specularity = new sreTexture(earth_specularity_filename, TEXTURE_TYPE_SPECULARITY_MAP);
    scene->SetSpecularityMap(earth_specularity);
#if defined(SPHERE) && !defined(NIGHT)
    // Set city light emission map for earth shader.
    earth_night_light_texture = new sreTexture(earth_night_light_filename, TEXTURE_TYPE_NORMAL);
    scene->SetEmissionMap(earth_night_light_texture);
#endif
    scene->SetLevelOfDetail(SRE_LOD_DYNAMIC, 0, 10.0);
    for (int y = 0; y < SUB_MESHES_Y; y++)
        for (int x = 0; x < SUB_MESHES_X; x++)
            scene->AddObject(mesh_model[y][x], 0, 0, 0, 0, 0, 0, 1.0);
#if 0
    // Miniature.
    for (int y = 0; y < SUB_MESHES_Y; y++)
        for (int x = 0; x < SUB_MESHES_X; x++) {
            scene->AddObject(mesh_model[y][x], 0, 0, 10.0, 0, 0, 0, 0.01);
        }
#endif

//    int l = scene->AddPointSourceLight(SRE_LIGHT_DYNAMIC_POSITION, Point3D(0, 0, 100.0), 300.0, Color(1.0, 1.0, 1.0));
//    scene->AttachLight(player_object, l, Vector3D(0, 0, 0));
#ifndef NIGHT
    directional_light = scene->AddDirectionalLight(SRE_LIGHT_DYNAMIC_DIRECTION, Vector3D(- 0.6, - 0.8, - 0.5),
        Color(0.5, 0.5, 0.5));
#endif
#if defined(NIGHT) || defined(SPHERE)
    spacecraft_spot_light = scene->AddSpotLight(SRE_LIGHT_DYNAMIC_POSITION | SRE_LIGHT_DYNAMIC_DIRECTION,
        spacecraft_pos - Vector3D(0, 0, 2.4), - initial_ascend_vector, 3.0, 300.0, Color(0.5, 0.5, 0.2));
    scene->AttachLight(spacecraft_object, spacecraft_spot_light, Vector3D(0, 0, - 2.4));
#endif

#ifdef SPHERE
    dynamic_gravity = true;
    gravity_position.Set(0, 0, 0);
//    sreSetFarPlaneDistance(X_SCALE);
    no_ground_plane = true;
#else
    view->SetViewModeFollowObject(0, 40.0, Vector3D(0, 0, 10.0));
#endif
    control_object = 1;
    no_gravity = true;
#ifdef SPHERE
    hovering_height = Magnitude(ProjectOnto(scene->sceneobject[control_object]->position, initial_ascend_vector));
#else
    hovering_height = scene->sceneobject[control_object]->position.z;
#endif
    sreSetHDRKeyValue(0.2);
}

void Demo4Render() {
    Vector3D ascend_vector = Vector3D(0, 0, 1.0);
    float view_distance;
    if (control_object == spacecraft_object)
        view_distance = 100.0;
    else
        view_distance = 40.0;
#ifdef SPHERE
    // Set viewing direction.
    Vector3D up_vector = scene->sceneobject[control_object]->position;
    up_vector.Normalize();
    view->SetAscendVector(up_vector);
    ascend_vector = up_vector;
    Vector3D forward_vector;
//            float xcoord = radius * cosf(latitude) * cosf(longitude);
//            float ycoord = radius * cosf(latitude) * sinf(longitude);
//            float zcoord = radius * sinf(latitude);
    // Define the basal forward direction as looking down a meridian from the north pole (negative latitude direction).
    float latitude = asinf(ascend_vector.z);
    float longitude = atan2(ascend_vector.y, ascend_vector.x);

    Vector3D angles;
    view->GetViewAngles(angles);
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

    Vector3D right_vector = great_circle_normal;
    forward_vector = Cross(right_vector, up_vector);
    forward_vector.Normalize();
#if 0
    Matrix3D r1;
    r1.AssignRotationAlongAxis(up_vector, angles.z * M_PI / 180);
    forward_vector = r1 * forward_vector;
    Vector3D right_vector = Cross(forward_vector, up_vector);
    right_vector.Normalize();
#endif
    view->SetForwardVector(forward_vector);
    Matrix3D r2; 
    r2.AssignRotationAlongAxis(right_vector, - angles.x * M_PI / 180.0);
    Vector3D view_direction = r2 * forward_vector;
    Point3D viewpoint = scene->sceneobject[control_object]->position - view_distance * view_direction; // + up_vector *
//        view_distance * 0.25;
    Point3D lookat = scene->sceneobject[control_object]->position; // viewpoint + view_direction;
    up_vector = r2 * up_vector;
    view->SetViewModeLookAt(viewpoint, lookat, up_vector);
    view->SetMovementMode(SRE_MOVEMENT_MODE_USE_FORWARD_AND_ASCEND_VECTOR);
    // Let spacecraft spot light point the right way.
    scene->ChangeSpotLightDirection(spacecraft_spot_light, - ascend_vector);
#endif
#ifndef SPHERE
    view->SetViewModeFollowObject(control_object, view_distance, Vector3D(0, 0, 0) /* Vector3D(0, 0, view_distance * 0.25) */);
#endif
    scene->Render(view);
    if (no_gravity) {
        if (control_object == player_object)
            hovering_height = saved_hovering_height;
        control_object = spacecraft_object;
    }
    else {
        if (control_object == spacecraft_object) {
            // Drop the player from the spacecraft.
            scene->BulletChangeVelocity(spacecraft_object, Vector3D(0, 0, 0));
            Point3D new_pos = scene->sceneobject[spacecraft_object]->position - ascend_vector * 15.0;
//            scene->ChangePosition(player_object, new_pos);
            scene->BulletChangeVelocity(player_object, Vector3D(0, 0, 0));
            scene->BulletChangePosition(player_object, new_pos);
        }
        control_object = player_object;
    }
    Matrix3D spin_matrix;
    spin_matrix.AssignRotationAlongZAxis(fmod(demo_time, 4.0) * 2.0 * M_PI / 4.0);
#ifdef SPHERE
    // Try to keep the spacecraft upright, parallel to the surface.
    if (control_object == spacecraft_object) {
        Matrix3D rot_matrix;
        rot_matrix.Set(ascend_vector, forward_vector, right_vector);
        Matrix3D r;
        r.AssignRotationAlongYAxis(M_PI / 2.0);
//        rot_matrix.SetRow(0, right_vector);
//        rot_matrix.SetRow(1, forward_vector);
//        rot_matrix.SetRow(2, ascend_vector);
        saved_spacecraft_rotation_matrix = (rot_matrix * r) * spin_matrix;         
        scene->BulletChangeRotationMatrix(control_object, saved_spacecraft_rotation_matrix);
    }
    else
        // The spacecraft is not being controlled, but should rotate.
        scene->BulletChangeRotationMatrix(spacecraft_object, saved_spacecraft_rotation_matrix * spin_matrix);
#else
     scene->BulletChangeRotationMatrix(spacecraft_object, spin_matrix);
#endif
    // Set the maximum horizontal velocity (over the surface), for the spacecraft it increases as the height increases.
    if (control_object == player_object)
        max_horizontal_velocity = 100.0;
    else {
        float height = Magnitude(ProjectOnto(scene->sceneobject[spacecraft_object]->position, ascend_vector));
#ifdef SPHERE
        height -= 0.5 * X_SCALE;
#endif
        max_horizontal_velocity = 100.0 + height * 0.5;
        horizontal_acceleration = max_horizontal_velocity;
        // The ascend/descend controls are also sensitive to the height above the surface.
        hovering_height_acceleration = 100.0 + height * 0.5;
    }
#ifdef SPHERE
    // Set viewing distance, clip distance increases as height increases.
    float player_dist = Magnitude(scene->sceneobject[control_object]->position) - 0.5 * X_SCALE;
    float far_plane_dist = maxf(2000.0, X_SCALE * 0.1 + player_dist * X_SCALE / 5000.0);
    sreSetFarPlaneDistance(far_plane_dist);
    // Also increase the shadow mapping region as height increases.
    float factor;
    if (control_object == spacecraft_object)
        factor = far_plane_dist / 2000.0 + powf((far_plane_dist - 2000.0) / 2000.0, 0.2) * 4.0;
    else
        factor = 0.5;
    sreSetShadowMapRegion(Point3D(- 400.0, - 400.0, - 600.0) * factor, Point3D(400.0, 400.0, 200.0) * factor);
#endif
}

char message[80];

void Demo4TimeIteration(double time_previous, double time_current) {
#if defined(SPHERE) && !defined(NIGHT)
    Matrix3D r1;
    r1.AssignRotationAlongZAxis(fmodf(- demo_time + 7.0 * DAY_INTERVAL / 24.0, DAY_INTERVAL) * 2.0 * M_PI / DAY_INTERVAL);
    Matrix3D r2;
    r2.AssignRotationAlongXAxis(23.4 * M_PI / 180.0);
    Matrix3D r3;
    r3.AssignRotationAlongZAxis(fmodf(demo_time, YEAR_INTERVAL) * 2.00 * M_PI / YEAR_INTERVAL);
    Point3D sun_pos = Point3D(1000000.0, 0, 0);
    sun_pos = (r3 * (r2 * r1)) * sun_pos;
    Vector3D light_dir = (- sun_pos).Normalize();
    scene->ChangeDirectionalLightDirection(directional_light, light_dir);
    scene->ChangePosition(sun_object, sun_pos.x, sun_pos.y, sun_pos.z);
    float hour = - fmodf(- demo_time - 8.0 * DAY_INTERVAL / 24.0, DAY_INTERVAL) * 24.0 / DAY_INTERVAL;
    sprintf(message, "%02d:%02dh Day %d", (int)floorf(hour),  (int)floorf((hour - (int)floorf(hour)) * 60.0 / 100.0),
        (int)(floorf(fmodf(demo_time, YEAR_INTERVAL) * 365.0 / YEAR_INTERVAL) + 1));
    text_message[0] = message; 
    text_message_time = GUIGetCurrentTime();
    nu_text_message_lines = 1;
#endif
    saved_hovering_height = hovering_height;
}

