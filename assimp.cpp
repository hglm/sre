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

#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "assimp/cimport.h"

#include "sre.h"
#include "sre_internal.h"

// the global Assimp scene object
static const struct aiScene* ai_scene = NULL;

class AssimpVertex {
public :
    float x, y, z;
};

class AssimpColor {
public :
    float r, g, b, a;
};

class AssimpTexcoords {
public :
    float u, v;
};

class AssimpMesh {
public:
    int starting_vertex;
    int ending_vertex;
    bool color_set;
    AssimpColor color;
    char *texture;	// Diffuse texture; NULL if none
    char *normal_map; 	// Normal map; NULL if none
    char *specular_map;
};

static AssimpVertex *assimp_vertex;
static AssimpVertex *assimp_normal;
static AssimpColor *assimp_color;
static AssimpTexcoords *assimp_texcoords;
static AssimpVertex *assimp_tangent;
static int nu_vertices, max_vertices;
static bool normals_set;
static bool colors_set;
static bool texcoords_set;
static int nu_tangents_set;
static int nu_meshes, max_meshes;
static AssimpMesh *assimp_mesh;
static const char *assimp_base_path;

#define INITIAL_MAX_ASSIMP_VERTICES 1024
#define INITIAL_MAX_ASSIMP_MESHES 8

static void InitDataStructures() {
    assimp_vertex = new AssimpVertex[INITIAL_MAX_ASSIMP_VERTICES];
    assimp_normal = new AssimpVertex[INITIAL_MAX_ASSIMP_VERTICES];
    assimp_color = new AssimpColor[INITIAL_MAX_ASSIMP_VERTICES];
    assimp_texcoords = new AssimpTexcoords[INITIAL_MAX_ASSIMP_VERTICES];
    assimp_tangent = new AssimpVertex[INITIAL_MAX_ASSIMP_VERTICES];
    assimp_mesh = new AssimpMesh[INITIAL_MAX_ASSIMP_MESHES];
    max_vertices = INITIAL_MAX_ASSIMP_VERTICES;
    max_meshes = INITIAL_MAX_ASSIMP_MESHES;
    nu_vertices = 0;
    normals_set = false;
    colors_set = false;
    texcoords_set = false;
    nu_tangents_set = 0;
    nu_meshes = 0;
}

static void FreeDataStructures() {
    delete [] assimp_vertex;
    delete [] assimp_normal;
    delete [] assimp_color;
    delete [] assimp_texcoords;
    delete [] assimp_tangent;
    delete [] assimp_mesh;
}

static void SetVertex(float *v) {
    assimp_vertex[nu_vertices].x = v[0];
    assimp_vertex[nu_vertices].y = v[1];
    assimp_vertex[nu_vertices].z = v[2];
}

static void SetNormal(float *v) {
    assimp_normal[nu_vertices].x = v[0];
    assimp_normal[nu_vertices].y = v[1];
    assimp_normal[nu_vertices].z = v[2];
    normals_set = true;
}

static void SetColor(float *c) {
    assimp_color[nu_vertices].r = c[0];
    assimp_color[nu_vertices].g = c[1];
    assimp_color[nu_vertices].b = c[2];
    assimp_color[nu_vertices].a = c[3];
    colors_set = true;
}

static void SetTangent(float *v) {
    assimp_tangent[nu_vertices].x = v[0];
    assimp_tangent[nu_vertices].y = v[1];
    assimp_tangent[nu_vertices].z = v[2];
    nu_tangents_set++;
}

static void SetTexcoords(float *t) {
    assimp_texcoords[nu_vertices].u = t[0];
    assimp_texcoords[nu_vertices].v = t[1];
    texcoords_set = true;
}


static void NextVertex() {
    nu_vertices++;
    // Allocate more space when required.
    if (nu_vertices >= max_vertices) {
        max_vertices *= 2;
        AssimpVertex *new_assimp_vertex = new AssimpVertex[max_vertices];
        AssimpVertex *new_assimp_normal = new AssimpVertex[max_vertices];
        AssimpColor *new_assimp_color = new AssimpColor[max_vertices];
        AssimpTexcoords *new_assimp_texcoords = new AssimpTexcoords[max_vertices];
        AssimpVertex *new_assimp_tangent = new AssimpVertex[max_vertices];
        memcpy(new_assimp_vertex, assimp_vertex, sizeof(AssimpVertex) * nu_vertices);
        memcpy(new_assimp_normal, assimp_normal, sizeof(AssimpVertex) * nu_vertices);
        memcpy(new_assimp_color, assimp_color, sizeof(AssimpColor) * nu_vertices);
        memcpy(new_assimp_texcoords, assimp_texcoords, sizeof(AssimpTexcoords) * nu_vertices);
        memcpy(new_assimp_tangent, assimp_tangent, sizeof(AssimpVertex) * nu_vertices);
        delete [] assimp_vertex;
        delete [] assimp_normal;
        delete [] assimp_color;
        delete [] assimp_texcoords;
        delete [] assimp_tangent;
        assimp_vertex = new_assimp_vertex;
        assimp_normal = new_assimp_normal;
        assimp_color = new_assimp_color;
        assimp_texcoords = new_assimp_texcoords;
        assimp_tangent = new_assimp_tangent;
    }
}

static void SetMeshBegin() {
    assimp_mesh[nu_meshes].starting_vertex = nu_vertices;
}

static void SetMeshEnd() {
    assimp_mesh[nu_meshes].ending_vertex = nu_vertices - 1;
    nu_meshes++;
    if (nu_meshes >= max_meshes) {
        max_meshes *= 2;
        AssimpMesh *new_assimp_mesh = new AssimpMesh[max_meshes];
        memcpy(new_assimp_mesh, assimp_mesh, sizeof(AssimpMesh) * nu_meshes);
        delete [] assimp_mesh;
        assimp_mesh = new_assimp_mesh;
    }
}

static char *AllocateBasePathPlusFilename(const char *s) {
    int blength = strlen(assimp_base_path);
    int slength = strlen(s);
    char *result = (char *)malloc(blength + slength + 1);
    strcpy(result, assimp_base_path);
    strcpy(&result[blength], s);
    return result;
}

static void SetMeshTexture(const char *s) {
    assimp_mesh[nu_meshes].texture = AllocateBasePathPlusFilename(s);
}

static void SetMeshNormalMap(const char *s) {
    assimp_mesh[nu_meshes].normal_map = AllocateBasePathPlusFilename(s);
}

static void SetMeshSpecularMap(const char *s) {
    assimp_mesh[nu_meshes].specular_map = AllocateBasePathPlusFilename(s);
}

bool FileExists(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (f == NULL)
        return false;
    fclose(f);
    return true;
}

void color4_to_float4(const aiColor4D *c, float f[4])
{
	f[0] = c->r;
	f[1] = c->g;
	f[2] = c->b;
	f[3] = c->a;
}

// ----------------------------------------------------------------------------
void set_float4(float f[4], float a, float b, float c, float d)
{
	f[0] = a;
	f[1] = b;
	f[2] = c;
	f[3] = d;
}

static void ApplyMaterial(const struct aiMaterial *mtl) {
	float c[4];
	aiColor4D diffuse;
	aiColor4D specular;
	aiColor4D ambient;
	aiColor4D emission;
//	float shininess, strength;
	set_float4(c, 0.8f, 0.8f, 0.8f, 1.0f);
	if(AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_DIFFUSE, &diffuse)) {
                assimp_mesh[nu_meshes].color_set = true;
		color4_to_float4(&diffuse, (float *)&assimp_mesh[nu_meshes].color);
        }

	set_float4(c, 0.0f, 0.0f, 0.0f, 1.0f);
	if(AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_SPECULAR, &specular))
		color4_to_float4(&specular, c);

	set_float4(c, 0.2f, 0.2f, 0.2f, 1.0f);
	if(AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_AMBIENT, &ambient))
		color4_to_float4(&ambient, c);

	set_float4(c, 0.0f, 0.0f, 0.0f, 1.0f);
	if(AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_EMISSIVE, &emission))
		color4_to_float4(&emission, c);

#if 0
	max = 1;
	ret1 = aiGetMaterialFloatArray(mtl, AI_MATKEY_SHININESS, &shininess, &max);
	if(ret1 == AI_SUCCESS) {
	    	max = 1;
	    	ret2 = aiGetMaterialFloatArray(mtl, AI_MATKEY_SHININESS_STRENGTH, &strength, &max);
			if(ret2 == AI_SUCCESS)
				glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shininess * strength);
	        else
        	glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shininess);
	}
	else {
		glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 0.0f);
		set_float4(c, 0.0f, 0.0f, 0.0f, 0.0f);
		glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, c);
	}
#endif

	assimp_mesh[nu_meshes].texture = NULL;
        assimp_mesh[nu_meshes].normal_map = NULL;
        assimp_mesh[nu_meshes].specular_map = NULL;
	aiString path;
	int ret1 = aiGetMaterialTexture(mtl, aiTextureType_DIFFUSE, 0, &path, NULL, NULL, NULL, NULL, NULL);
	if (ret1 == AI_SUCCESS) {
		printf("Found diffuse texture %s.\n", path.data);
		SetMeshTexture(path.data);
	}
	ret1 = aiGetMaterialTexture(mtl, aiTextureType_NORMALS, 0, &path, NULL, NULL, NULL, NULL, NULL);
	if (ret1 == AI_SUCCESS) {
		printf("Found normal map texture %s.\n", path.data);
		SetMeshNormalMap(path.data);
	}
	ret1 = aiGetMaterialTexture(mtl, aiTextureType_SPECULAR, 0, &path, NULL, NULL, NULL, NULL, NULL);
	if (ret1 == AI_SUCCESS) {
		printf("Found specular texture %s.\n", path.data);
                SetMeshSpecularMap(path.data);
	}
	ret1 = aiGetMaterialTexture(mtl, aiTextureType_EMISSIVE, 0, &path, NULL, NULL, NULL, NULL, NULL);
	if (ret1 == AI_SUCCESS) {
		printf("Found emissive texture %s, but not using it.\n", path.data);
	}
	ret1 = aiGetMaterialTexture(mtl, aiTextureType_HEIGHT, 0, &path, NULL, NULL, NULL, NULL, NULL);
	if (ret1 == AI_SUCCESS) {
		if (assimp_mesh[nu_meshes].normal_map == NULL) {
			printf("Found height map texture %s, using it as a normal map.\n", path.data);
			SetMeshNormalMap(path.data);
                }
                else
			printf("Found height map texture %s, but not using it.\n", path.data);
	}
	ret1 = aiGetMaterialTexture(mtl, aiTextureType_UNKNOWN, 0, &path, NULL, NULL, NULL, NULL, NULL);
	if (ret1 == AI_SUCCESS) {
		printf("Found unknown texture %s, but not using it.\n", path.data);
	}
}


static void WalkRecursive(const struct aiScene *sc, const struct aiNode* nd) {
	int i;
	unsigned int n = 0, t;
//	aiMatrix4x4 m = nd->mTransformation;

	// draw all meshes assigned to this node
	for (; n < nd->mNumMeshes; ++n) {
		const struct aiMesh* mesh = ai_scene->mMeshes[nd->mMeshes[n]];

		ApplyMaterial(sc->mMaterials[mesh->mMaterialIndex]);

/*		if(mesh->mNormals == NULL) {
			glDisable(GL_LIGHTING);
		} else {
			glEnable(GL_LIGHTING);
		} */

		SetMeshBegin();

		for (t = 0; t < mesh->mNumFaces; ++t) {
			const struct aiFace* face = &mesh->mFaces[t];

			if (face->mNumIndices != 3) {
				printf("Assimp loader: only triangles supported.\n");
				exit(1);
			}

			for(i = 0; i < (int)face->mNumIndices; i++) {
				int index = face->mIndices[i];
				if (mesh->mColors[0] != NULL)
					SetColor((float *)&mesh->mColors[0][index]);
				if (mesh->mNormals != NULL) 
					SetNormal(&mesh->mNormals[index].x);
				if (mesh->mTextureCoords[0] != NULL)
					SetTexcoords((float *)&mesh->mTextureCoords[0][index]);
                                if (mesh->mTangents != NULL)
					SetTangent(&mesh->mTangents[index].x);
				SetVertex(&mesh->mVertices[index].x);
				NextVertex();
			}
		}
		SetMeshEnd();
	}

	// draw all children
	for (n = 0; n < nd->mNumChildren; ++n) {
		WalkRecursive(sc, nd->mChildren[n]);
	}

}

static void ProcessAssimpScene() {
	WalkRecursive(ai_scene, ai_scene->mRootNode);
}

// Convert the processed assimp model to an SRE LOD model.

static sreLODModel *ConvertToModel(int load_flags) {
    sreLODModel *m = sreNewLODModel();

    m->nu_vertices = nu_vertices;
    // Assign vertex positions.
    m->position = dstNewAligned <Point3DPadded>(m->nu_vertices, 16);
    for (int i = 0; i < m->nu_vertices; i++) {
        m->position[i].Set(assimp_vertex[i].x, assimp_vertex[i].y, assimp_vertex[i].z);
    }
    m->flags |= SRE_POSITION_MASK;
    // Assign triangles.
    m->nu_triangles = nu_vertices / 3;
    m->triangle = new sreModelTriangle[m->nu_triangles];
    for (int i = 0; i < nu_vertices / 3; i++) {
        m->triangle[i].AssignVertices(i * 3, i * 3 + 1, i * 3 + 2);
    }
    // Assign texcoords.
    if (texcoords_set && !(load_flags & SRE_MODEL_LOAD_FLAG_NO_TEXCOORDS)) {
        m->texcoords = new Point2D[m->nu_vertices];
        for (int i = 0; i < m->nu_vertices; i++)
            m->texcoords[i].Set(assimp_texcoords[i].u, 1 - assimp_texcoords[i].v);
        m->flags |= SRE_TEXCOORDS_MASK;
    }
    // Assign colors.
    if (colors_set && !(load_flags & SRE_MODEL_LOAD_FLAG_NO_COLORS)) {
       m->colors = new Color[m->nu_vertices];
       for (int i = 0; i < m->nu_vertices; i++) {
            m->colors[i].r = assimp_color[i].r;
            m->colors[i].g = assimp_color[i].g;
            m->colors[i].b = assimp_color[i].b;
       }
        m->flags |= SRE_COLOR_MASK;
    }
    m->vertex_normal = new Vector3D[m->nu_vertices];
    // Assign normals.
    if (normals_set && !(load_flags & SRE_MODEL_LOAD_FLAG_NO_VERTEX_NORMALS)) {
        printf("Copying normals from assimp object.\n");
        for (int i = 0; i < m->nu_vertices; i++) {
            m->vertex_normal[i].Set(assimp_normal[i].x, assimp_normal[i].y, assimp_normal[i].z);
        }
        m->flags |= SRE_NORMAL_MASK;
    }
    // Assign tangents.
//    nu_tangents_set = 0; // DEBUG
    if (nu_tangents_set == nu_vertices && !(load_flags & SRE_MODEL_LOAD_FLAG_NO_TANGENTS)) {
        printf("Copying tangents from assimp object.\n");
        m->vertex_tangent = new Vector4D[m->nu_vertices];
        for (int i = 0; i < m->nu_vertices; i++) {
            Vector3D t;
            t.Set(assimp_tangent[i].x, assimp_tangent[i].y, assimp_tangent[i].z);
            m->vertex_tangent[i].Set(t.x, t.y, t.z, 1.0);
        }
        m->flags |= SRE_TANGENT_MASK;
    }
    m->MergeIdenticalVertices();
    if ((m->flags & SRE_NORMAL_MASK) || (load_flags & SRE_MODEL_LOAD_FLAG_NO_VERTEX_NORMALS))
        // If the object has pre-defined vertex normals, or vertex normals are not desired,
        // only calculate face normals.
        m->CalculateTriangleNormals();
    else
        m->CalculateNormals();

    // Process textures.
    m->mesh = new sreModelMesh[nu_meshes];
    m->nu_meshes = nu_meshes;
    int texture_count = 0;
    int normal_map_count = 0;
    int specular_map_count = 0;
    for (int i = 0; i < nu_meshes; i++) {
        if (assimp_mesh[i].texture != NULL) {
            sreTexture *texture = new sreTexture(assimp_mesh[i].texture, TEXTURE_TYPE_NORMAL);
            // Assign texture id for SRE renderer.
            m->mesh[i].texture_opengl_id = texture->opengl_id;
            texture_count++;
        }
        if (assimp_mesh[i].normal_map != NULL) {
            normal_map_count++;
            if (!FileExists(assimp_mesh[i].normal_map)) {
                printf("Normal map texture not found, skipping whole mesh.\n");
                m->mesh[i].nu_vertices = 0;
                // Allow specular maps for the remaining meshes in this situation (specular_map_count will
                // be equal to nu_meshes).
                if (assimp_mesh[i].specular_map != NULL)
                    specular_map_count++;
                continue;
            }
            sreTexture *normal_map = new sreTexture(assimp_mesh[i].normal_map, TEXTURE_TYPE_NORMAL_MAP);
            // Assign texture id for SRE renderer.
            m->mesh[i].normal_map_opengl_id = normal_map->opengl_id;
        }
        if (assimp_mesh[i].specular_map != NULL) {
            specular_map_count++;
            sreTexture *specular_map = new sreTexture(assimp_mesh[i].specular_map, TEXTURE_TYPE_NORMAL_MAP);
            // Assign texture id for SRE renderer.
            m->mesh[i].specular_map_opengl_id = specular_map->opengl_id;
        }
        // Assign mesh details for SRE renderer.
        m->mesh[i].starting_vertex = assimp_mesh[i].starting_vertex;
        m->mesh[i].nu_vertices = assimp_mesh[i].ending_vertex + 1 - assimp_mesh[i].starting_vertex;
    }
    // When using a normal map, and tangents haven't been specified, calculate tangent vectors.
    if (normal_map_count == nu_meshes && nu_tangents_set != nu_vertices && 
    !(load_flags & SRE_MODEL_LOAD_FLAG_NO_TANGENTS)) {
        m->CalculateTangentVectors();
        m->flags |= SRE_TANGENT_MASK;
    }
    FreeDataStructures();
    return m;
}


sreModel *sreReadModelFromAssimpFile(sreScene *scene, const char *filename, const char *base_path,
int load_flags) {
    struct aiLogStream stream;

    // Get a handle to the predefined STDOUT log stream and attach
    // it to the logging system. It remains active for all further
    // calls to aiImportFile(Ex) and aiApplyPostProcessing.
    if (sre_internal_debug_message_level >= 1) {
       stream = aiGetPredefinedLogStream(aiDefaultLogStream_STDOUT, NULL);
       aiAttachLogStream(&stream);
    }

    assimp_base_path = base_path;
    ai_scene = aiImportFile(filename, aiProcessPreset_TargetRealtime_MaxQuality);

    InitDataStructures();
    ProcessAssimpScene();

    // cleanup - calling 'aiReleaseImport' is important, as the library 
    // keeps internal resources until the scene is freed again. Not 
    // doing so can cause severe resource leaking.
    aiReleaseImport(ai_scene);

    // We added a log stream to the library, it's our job to disable it
    // again. This will definitely release the last resources allocated
    // by Assimp.
    aiDetachAllLogStreams();

    sreModel *model = new sreModel;
    model->nu_lod_levels = 1;
    model->lod_model[0] = ConvertToModel(load_flags);
    model->CalculateBounds();
    model->collision_shape_static = SRE_COLLISION_SHAPE_STATIC;
    model->collision_shape_dynamic = SRE_COLLISION_SHAPE_CONVEX_HULL;
    scene->RegisterModel(model);
    return model;

}
