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
#ifdef OPENGL_ES2
#include <GLES2/gl2.h>
#endif
#ifdef OPENGL
#include <GL/glew.h>
#include <GL/gl.h>
#endif

#include "sre.h"
#include "sre_internal.h"


//============================================================================
//
// Listing 15.1
//
// Mathematics for 3D Game Programming and Computer Graphics, 3rd ed.
// By Eric Lengyel
//
// The code in this file may be freely used in any software. It is provided
// as-is, with no warranty of any kind.
//
//============================================================================



sreFluid::sreFluid(int n, int m, float d, float t, float c, float mu)
{
	width = n;
	height = m;
	int count = n * m;
	
	buffer[0] = new Vector3D[count];
	buffer[1] = new Vector3D[count];
	renderBuffer = 0;
	
	normal = new Vector3D[count];
	tangent = new Vector3D[count];
	
	// Precompute constants for Equation (15.25).
	float f1 = c * c * t * t / (d * d);
	float f2 = 1.0F / (mu * t + 2);
	k1 = (4.0F - 8.0F * f1) * f2;
	k2 = (mu * t - 2) * f2;
	k3 = 2.0F * f1 * f2;
	
	// Initialize buffers.
	int a = 0;
	for (int j = 0; j < m; j++)
	{
		float y = d * j;
		for (int i = 0; i < n; i++)
		{
			buffer[0][a].Set(d * i, y, 0.0F);
			buffer[1][a] = buffer[0][a];
			normal[a].Set(0.0F, 0.0F, 2.0F * d);
			tangent[a].Set(2.0F * d, 0.0F, 0.0F);
			a++;
		}
	}
}

sreFluid::~sreFluid()
{
	delete[] tangent;
	delete[] normal;
	delete[] buffer[1];
	delete[] buffer[0];
}

void sreFluid::Evaluate(void)
{
	// Apply Equation (15.25).
	for (int j = 1; j < height - 1; j++)
	{
		const Vector3D *crnt = buffer[renderBuffer] + j * width;
		Vector3D *prev = buffer[1 - renderBuffer] + j * width;
		
		for (int i = 1; i < width - 1; i++)
		{
			prev[i].z = k1 * crnt[i].z + k2 * prev[i].z +
				k3 * (crnt[i + 1].z + crnt[i - 1].z +
				crnt[i + width].z + crnt[i - width].z);
		}
	}
	
	// Swap buffers.
	renderBuffer = 1 - renderBuffer;
	
	// Calculate normals and tangents.
	for (int j = 1; j < height - 1; j++)
	{
		const Vector3D *next = buffer[renderBuffer] + j * width;
		Vector3D *nrml = normal + j * width;
		Vector3D *tang = tangent + j * width;
		
		for (int i = 1; i < width - 1; i++)
		{
			nrml[i].x = next[i - 1].z - next[i + 1].z;
			nrml[i].y = next[i - width].z - next[i + width].z;
			tang[i].z = next[i + 1].z - next[i - 1].z;
		}
	}
}

void sreFluid::CreateDisturbance(int x, int y, float z) {
    Vector3D *buf1 = buffer[renderBuffer];
    Vector3D *buf2 = buffer[1 - renderBuffer];
    buf1[y * width + x].z += z;
    buf2[y * width + x].z += z;
    if (x > 0) {
       buf1[y * width + x - 1].z += z * 0.5;
       buf2[y * width + x - 1].z += z * 0.5;
    }
    if (x < width - 1) {
       buf1[y * width + x + 1].z += z * 0.5;
       buf2[y * width + x + 1].z += z * 0.5;
    }
    if (y > 0) {
       buf1[(y - 1) * width + x].z += z * 0.5;
       buf2[(y - 1) * width + x].z += z * 0.5;
    }
    if (y < height - 1) {
       buf1[(y + 1) * width + x].z += z * 0.5;
       buf2[(y + 1) * width + x].z += z * 0.5;
    }
}

sreLODModelFluid::sreLODModelFluid() {
    flags = SRE_LOD_MODEL_IS_FLUID_MODEL;
}

sreLODModelFluid::~sreLODModelFluid() {
    delete fluid;
}

void sreLODModelFluid::Evaluate() {
    // Update the fluid state.
    fluid->Evaluate();
    // Update vertex buffers.
    // Since the vertex buffer has four-dimensions vertices, we have to convert from the
    // fluid state's three-dimensional vertices.
    float *fvertices = new float[nu_vertices * 4];
    Vector3D *vert = fluid->buffer[fluid->renderBuffer];
    for (int i = 0; i < nu_vertices; i++) {
        fvertices[i * 4] = vert[i].x;
        fvertices[i * 4 + 1] = vert[i].y;
        fvertices[i * 4 + 2] = vert[i].z;
        fvertices[i * 4 + 3] = 1.0;
    }
    glBindBuffer(GL_ARRAY_BUFFER, GL_attribute_buffer[SRE_ATTRIBUTE_POSITION]);
    glBufferData(GL_ARRAY_BUFFER, nu_vertices * sizeof(float) * 4, &fvertices[0], GL_DYNAMIC_DRAW);
    delete fvertices;
    // Update normals.
    glBindBuffer(GL_ARRAY_BUFFER, GL_attribute_buffer[SRE_ATTRIBUTE_NORMAL]);
    glBufferData(GL_ARRAY_BUFFER, nu_vertices * sizeof(float) * 3, fluid->normal, GL_DYNAMIC_DRAW);
}

void sreEvaluateModelFluid(sreModel *m) {
    ((sreLODModelFluid *)m->lod_model[0])->Evaluate();
}

void sreCreateModelFluidDisturbance(sreModel *m, int x, int y, float z) {
    ((sreLODModelFluid *)m->lod_model[0])->fluid->CreateDisturbance(x, y, z);
}

#define mesh(x, y) ((y) * (width + 1) + (x)) 

// Create fluid mesh, width and height must be multiple of 2.

sreModel *sreCreateFluidModel(sreScene *scene, int width, int height, float d, float t, float c, float mu) {
    // Do a sanity check on t, c, and mu.
    if (c < 0 || c >= d * sqrtf(mu * t + 2) / (2 * t)) {
        sreFatalError("Fluid c parameter out of range.\n");
    }
    if (t < 0 || t >= (mu + sqrtf(mu * mu + 32 * c * c / (d * d))) / (8 * c * c / (d * d))) {
        sreFatalError("Fluid t parameter out of range.\n");
    }
    sreModel *m = new sreModel;
    sreLODModelFluid *lm = new sreLODModelFluid;
    m->lod_model[0] = lm;
    m->nu_lod_levels = 1;
    // Create object vertices and triangles.
    lm->nu_vertices = (width + 1) * (height + 1);
    lm->vertex = new Point3D[lm->nu_vertices];
    lm->texcoords = new Point2D[lm->nu_vertices];
    lm->nu_triangles = (height / 2) * (width / 2) * 8;
    lm->triangle = new sreModelTriangle[lm->nu_triangles];
    for (int y = 0; y <= height; y++)
        for (int x = 0; x <= width; x++) {
            lm->vertex[y * (width + 1) + x].Set(x * d, y * d, 0);
            lm->texcoords[y * (width + 1) + x].Set((float)x / width, (float)y / height);
        }
    int i = 0;
    for (int y = 0; y < height; y += 2)
        for (int x = 0; x < width; x += 2) {
            lm->triangle[i].AssignVertices(mesh(x, y), mesh(x + 1, y), mesh(x + 1, y + 1));
            lm->triangle[i + 1].AssignVertices(mesh(x, y), mesh(x + 1, y  + 1), mesh(x, y + 1));
            lm->triangle[i + 2].AssignVertices(mesh(x + 1, y), mesh(x + 2, y), mesh(x + 1, y + 1));
            lm->triangle[i + 3].AssignVertices(mesh(x + 2, y), mesh(x + 2, y + 1), mesh(x + 1, y + 1));
            lm->triangle[i + 4].AssignVertices(mesh(x, y + 1), mesh(x + 1, y + 1), mesh(x, y + 2));
            lm->triangle[i + 5].AssignVertices(mesh(x + 1, y + 1), mesh(x + 1, y + 2), mesh(x, y + 2));
            lm->triangle[i + 6].AssignVertices(mesh(x + 1, y + 1), mesh(x + 2, y + 1), mesh(x + 2, y + 2));
            lm->triangle[i + 7].AssignVertices(mesh(x + 1, y + 1), mesh(x + 2, y + 2), mesh(x + 1, y + 2));
            i += 8;
        }
    lm->fluid = new sreFluid(width + 1, height + 1, d, t, c, mu);
    lm->flags = SRE_POSITION_MASK | SRE_TEXCOORDS_MASK | SRE_LOD_MODEL_VERTEX_BUFFER_DYNAMIC |
        SRE_LOD_MODEL_IS_FLUID_MODEL;
    lm->vertex_normal = new Vector3D[lm->nu_vertices];
    lm->CalculateNormals(); // Will set SRE_NORMAL_MASK.
    // Bounding box z extent should depend on parameters (fix me).
    sreBoundingVolumeAABB AABB;
    AABB.dim_min = Vector3D(0, 0, - 2.0f);
    AABB.dim_max = Vector3D(width * d, height * d, 2.0f);
    m->SetOBBWithAABBBounds(AABB);
    // Bounding sphere is approximate, because the fluid is moving.
    m->CalculateBoundingSphere();
    scene->RegisterModel(m);
    m->collision_shape_static = SRE_COLLISION_SHAPE_STATIC;
    m->collision_shape_dynamic = SRE_COLLISION_SHAPE_STATIC; 
    return m;
}

