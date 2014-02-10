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


// Read 3D model file formats and convert them to an sreModel.
// Support the .OBJ format, but the framework is in place to add
// more formats.

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <limits.h>

#include "sre.h"
#include "sre_internal.h"

static int max_words = 0;
static int nu_words;
static char **words;

static void AddWord(char *s) {
    if (nu_words == max_words) {
        max_words += 16;
        if (max_words == 16)
            words = (char **)malloc(sizeof(char *) * max_words);
        else
            words = (char **)realloc(words, sizeof(char *) * max_words);
    }
    words[nu_words] = s;
    nu_words++;
}

static inline bool IsWhiteSpace(int c) {
    return c == ' ' || c == '\t' || c == '\n';
}

#define MAX_LINE_LENGTH 512

static char *sre_internal_word_str = NULL;

// Decode a string into words delimited by whitespace. Stores pointers
// into str, which is patched with null bytes at the end of words.

static void GetWordsFromString(char *str) {
    nu_words = 0;
    int word_length = 0;
    for (int i = 0;; i++) {
        int c = str[i];
        bool is_white_space = IsWhiteSpace(c);
        if (c == '\0' || is_white_space) {
            // Check to see we have a word.
            if (word_length > 0) {
                // Patch any white space character with a null byte.
                str[i] = '\0';
                // Add the word.
                AddWord(&str[i - word_length]);
                word_length = 0;
            }
            if (c == '\0')
               return;
        }
        else
            word_length++;
    }
}

// Read a line from a source file and store the words. Returns a pointer
// to the string data (NULL when EOF is reached), and the global variables
// nu_words and words[] are updated.

static char *GetWords(FILE *fp) {
    if (sre_internal_word_str == NULL)
        sre_internal_word_str = (char *)malloc(MAX_LINE_LENGTH * sizeof(char));
    char *result = fgets(sre_internal_word_str, MAX_LINE_LENGTH, fp);
    if (result == NULL)
        return NULL;
    GetWordsFromString(sre_internal_word_str);
    return sre_internal_word_str;
}

// Remove comments by limiting the list of words to those
// before the first word starting with a comment character.

static void RemoveComments() {
    int i = 0;
    while (i < nu_words) {
        if (words[i][0] == '#') {
            nu_words = i;
            return;
        }
        i++;
    }
}

// Read up to four coordinates (floats) from successive words starting from
// starting_word. No error checking is done.

static int GetCoordinates(int starting_word, float *coord) {
    int n = 0;
    for (int i = starting_word; i < nu_words; i++) {
        coord[n] = atof(words[i]);
        n++;
        if (n == 4)
            break;
    }
    return n;
}

static int nu_attribute_vertices[SRE_NU_VERTEX_ATTRIBUTES];
static int max_attribute_vertices[SRE_NU_VERTEX_ATTRIBUTES];
// Store every attribute in a Vector4D for simplicity.
static Vector4D *vertex_attributes[SRE_NU_VERTEX_ATTRIBUTES];

static void AddVertexAttribute(int attribute, float *coord, int n) {
    if (nu_attribute_vertices[attribute] == max_attribute_vertices[attribute]) {
        max_attribute_vertices[attribute] *= 2;
        if (max_attribute_vertices[attribute] == 0) {
            max_attribute_vertices[attribute] = 16384;
            vertex_attributes[attribute] = (Vector4D *)malloc(
                sizeof(Vector4D) * max_attribute_vertices[attribute]);
        }
        else
            vertex_attributes[attribute] = (Vector4D *)realloc(
                vertex_attributes[attribute], sizeof(Vector4D) * max_attribute_vertices[attribute]);
    }
    vertex_attributes[attribute][nu_attribute_vertices[attribute]].x = coord[0];
    if (n >= 2) {
        vertex_attributes[attribute][nu_attribute_vertices[attribute]].y = coord[1];
        if (n >= 3) {
            vertex_attributes[attribute][nu_attribute_vertices[attribute]].z = coord[2];
            if (n >= 4)
                vertex_attributes[attribute][nu_attribute_vertices[attribute]].w = coord[3];
            else
                vertex_attributes[attribute][nu_attribute_vertices[attribute]].w = 1.0f;
        }
    }
    nu_attribute_vertices[attribute]++;
}

class sreFace {
public :
    int nu_vertices;
    int max_vertices;
    int attribute_mask;
    int attributes_allocated_mask;
    int *attribute_vertex_index[SRE_NU_VERTEX_ATTRIBUTES];

    void AllocateVertices(int n);
    void DestroyVertices();
};

// Allocate or reallocate (depending on whether max_vertices > 0)
// vertex index storage using the currently defined attribute mask.

void sreFace::AllocateVertices(int n) {
    for (int i = 0; i < SRE_NU_VERTEX_ATTRIBUTES; i++) {
        if (attributes_allocated_mask & (1 << i)) {
            if (max_vertices == 0)
                attribute_vertex_index[i] = (int *)malloc(sizeof(int) * n);
            else
                attribute_vertex_index[i] = (int *)realloc(attribute_vertex_index[i], sizeof(int) * n);
        }
    }
    max_vertices = n;
}

void sreFace::DestroyVertices() {
    for (int i = 0; i < SRE_NU_VERTEX_ATTRIBUTES; i++)
        if (attributes_allocated_mask & (1 << i))
            free(attribute_vertex_index[i]);
}

static int nu_faces;
static int max_faces;
static sreFace *face;

// Add a new face; vertices must subsequently be added with AddFaceVertex,
// after which EndFace must be called.
// The nu_vertices_hint argument is the number of vertices that will be
// allocated initially; vertex information will be dynamically reallocated
// when more vertices are added. A value of 0 means no vertices are
// initially allocated.
// The attribute_mask argument defines the vertex attributes that are
// defined for the face and for which space will be allocated,
// for example SRE_POSITION_MASK | SRE_NORMAL_MASK.

static void BeginFace(int nu_vertices_hint, int attribute_mask) {
    if (nu_faces == max_faces) {
        max_faces *= 2;
        if (max_faces == 0) {
            max_faces = 16384;
            face = (sreFace *)malloc(sizeof(sreFace) * max_faces);
        }
        else
            face = (sreFace *)realloc(face, sizeof(sreFace) * max_faces);
    }
    face[nu_faces].max_vertices = 0;
    face[nu_faces].nu_vertices = 0;
    face[nu_faces].attributes_allocated_mask = attribute_mask;
    // The actual attributes defined will be known after AddFaceVertex is called.
    face[nu_faces].attribute_mask = 0;
    if (nu_vertices_hint > 0)
        face[nu_faces].AllocateVertices(nu_vertices_hint);
}

// Add a new face vertex.

static void AddFaceVertex(const int *attribute_order, int *indices) {
    // Make sure there are enough vertices allocated.
    if (face[nu_faces].nu_vertices == face[nu_faces].max_vertices)
        // Allocate space for four more vertices.
        face[nu_faces].AllocateVertices(face[nu_faces].nu_vertices + 4);
    // For each attribute listed in attribute_order (terminated with - 1),
    // assign the vertex index for the attribute when it is present
    // (when not present, the vertex index is - 1).
    for (int i = 0; attribute_order[i] >= 0; i++) {
        int attribute = attribute_order[i];
        // Ignore attributes that were not allocated.
        if ((face[nu_faces].attributes_allocated_mask & (1 << attribute)) == 0)
            continue;
        // The value assigned is either a real index, or - 1 (not present).
        face[nu_faces].attribute_vertex_index[attribute][face[nu_faces].nu_vertices] =
            indices[i];
        // Update the attribute mask of attributes present. Normally, the same attributes
        // will be defined for all vertices, but exceptions are allowed.
        if (indices[i] >= 0)
            face[nu_faces].attribute_mask |= (1 << attribute);
    }
    face[nu_faces].nu_vertices++;
}

static void EndFace() {
    nu_faces++;
}

static void ModelFileReadError(const char *s) {
    printf("Error reading model file: %s.\n", s);
    exit(1);
}

static void ResetSourceModelData() {
    nu_faces = 0;
    max_faces = 0;
    for (int i = 0; i < SRE_NU_VERTEX_ATTRIBUTES; i++) {
        nu_attribute_vertices[i] = 0;
        max_attribute_vertices[i] = 0;
    }
}

static void FreeSourceModelData() {
    for (int i = 0; i < SRE_NU_VERTEX_ATTRIBUTES; i++)
        if (max_attribute_vertices[i] > 0)
            free(vertex_attributes[i]);
    for (int i = 0; i < nu_faces; i++)
        face[i].DestroyVertices();
    free(face);
}

// Initialize and allocate the sreLODModel based on the
// current face data read from the model source file. The
// actual triangles must still be added after calling this function.

static void InitializeModelFromFaceData(sreLODModel *m) {
    // Assuming faces of three or four vertices.
    int triangle_count = 0;
    for (int i = 0; i < nu_faces; i++)
        if (face[i].nu_vertices == 3)
            triangle_count++;
        else if (face[i].nu_vertices == 4)
            triangle_count += 2;
        else {
            printf("Invalid number of vertices in face in obj file.\n");
            exit(-1);
         }
    m->triangle = new sreModelTriangle[triangle_count];
    m->nu_triangles = 0;

    // Just create new vertices and vertex normals from every triangle vertex.
    // We will merge identical vertices later.
    int vertex_count = triangle_count * 3;
    m->vertex = new Point3D[vertex_count];
    m->nu_vertices = 0;
    m->flags |= SRE_POSITION_MASK;
    // Always allocate the normal array (CalculateNormals() will be called
    // when no normals are defined in the source file).
    m->vertex_normal = new Vector3D[vertex_count];
    if (nu_attribute_vertices[SRE_ATTRIBUTE_NORMAL] > 0)
        m->flags |= SRE_NORMAL_MASK;
    if (nu_attribute_vertices[SRE_ATTRIBUTE_TEXCOORDS] > 0) {
        m->texcoords = new Point2D[vertex_count];
        m->flags |= SRE_TEXCOORDS_MASK;
    }
}

// Add a triangle to an sreLODModel, with data from face i and the specified vertices of
// that face.

static void AddsreModelTriangle(sreLODModel *m, int i, int vertex0, int vertex1, int vertex2) {
    // Add new vertices.
    int vindex[3];
    vindex[0] = vertex0;
    vindex[1] = vertex1;
    vindex[2] = vertex2;
    for (int k = 0; k < 3; k++) {
        if (nu_attribute_vertices[SRE_ATTRIBUTE_POSITION] > 0 &&
        (face[i].attribute_mask & SRE_POSITION_MASK))
            m->vertex[m->nu_vertices + k] =
                vertex_attributes[SRE_ATTRIBUTE_POSITION][face[i].
                    attribute_vertex_index[SRE_ATTRIBUTE_POSITION][vindex[k]]].GetPoint3D();
        if (nu_attribute_vertices[SRE_ATTRIBUTE_NORMAL] > 0 &&
        (face[i].attribute_mask & SRE_NORMAL_MASK))
            m->vertex_normal[m->nu_vertices + k] =
                vertex_attributes[SRE_ATTRIBUTE_NORMAL][face[i].
                attribute_vertex_index[SRE_ATTRIBUTE_NORMAL][vindex[k]]].GetVector3D();
        if (nu_attribute_vertices[SRE_ATTRIBUTE_TEXCOORDS] > 0 &&
        (face[i].attribute_mask & SRE_TEXCOORDS_MASK))
            m->texcoords[m->nu_vertices + k] =
                vertex_attributes[SRE_ATTRIBUTE_TEXCOORDS][face[i].
                attribute_vertex_index[SRE_ATTRIBUTE_TEXCOORDS][vindex[k]]].GetVector3D().GetPoint2D();
    }
    m->triangle[m->nu_triangles].vertex_index[0] = m->nu_vertices;
    m->triangle[m->nu_triangles].vertex_index[1] = m->nu_vertices + 1;
    m->triangle[m->nu_triangles].vertex_index[2] = m->nu_vertices + 2;
    m->nu_vertices += 3;
    m->nu_triangles++;
}

// Add all faces read from the model source file to the sreLODModel.
// Only supports faces with three or four vertices.

static void AddFacesToModel(sreLODModel *m) {
    for (int i = 0; i < nu_faces; i++)
        if (face[i].nu_vertices == 3)
            // Add triangle from face i, creating new vertices.
            AddsreModelTriangle(m, i, 0, 1, 2);
        else if (face[i].nu_vertices == 4) {
            // Add two triangles from face i (which has four vertices).
            AddsreModelTriangle(m, i, 0, 1, 2);
            AddsreModelTriangle(m, i, 0, 2, 3);
        }
        else
            ModelFileReadError("More than four vertices in a face not supported");
}

// OBJ file import.

// The order in which attribute vertex indices appear in the face
// definitions in OBJ files.

static const int OBJ_attributes[4] = {
    SRE_ATTRIBUTE_POSITION,
    SRE_ATTRIBUTE_TEXCOORDS,
    SRE_ATTRIBUTE_NORMAL,
    - 1
};


// Decode a single word with indices delimited by slashes (OBJ face specification).
// Up to four indices are stored; the value INT_MAX indicates the index is not
// present.

static void DecodeOBJFaceIndices(char *word_str, int *indices) {
    indices[0] = INT_MAX;
    indices[1] = INT_MAX;
    indices[2] = INT_MAX;
    int i = 0;
    int i_start = 0;
    int nu_indices = 0;
    for (;; i++) {
        int c = word_str[i];
        if (c == '/' || c == '\0') {
            // Keep value of -1 when index is not specified.
            if (i != i_start) {
                word_str[i] = '\0';
                indices[nu_indices] = atoi(&word_str[i_start]);
            }
            nu_indices++;
            if (nu_indices == 3 || c == '\0')
                return;
            i_start = i + 1;
        }
    }
}

static void ReadOBJ(const char *filename) {
    FILE *fp = fopen(filename, "r");
    for (;;) {
        char *str = GetWords(fp);
        if (str == NULL)
            // End of file.
            break;
        RemoveComments();
        if (nu_words == 0)
            continue;
        int command = - 1;
        if (strcmp(words[0], "v") == 0)
           command = 0;
        else if (strcmp(words[0], "vn") == 0)
           command = 1;
        else if (strcmp(words[0], "vt") == 0)
           command = 2;
        else if (strcmp(words[0], "f") == 0)
           command = 3;
        if (command < 0)
            // First word not recognized.
            continue;
        if (command <= 2) {
           // Get up to four coordinates.
           float coord[4];
           int n = GetCoordinates(1, coord);
           if (command == 0)
               AddVertexAttribute(SRE_ATTRIBUTE_POSITION, coord, n);
           else if (command == 1)
               AddVertexAttribute(SRE_ATTRIBUTE_NORMAL, coord, n);
           else
               AddVertexAttribute(SRE_ATTRIBUTE_TEXCOORDS, coord, n);
        }
        else {
            // Face defition.
            BeginFace(4, SRE_POSITION_MASK | SRE_NORMAL_MASK | SRE_TEXCOORDS_MASK);
            for (int word_index = 1; word_index < nu_words; word_index++) {
                int vertex_index[3];
                DecodeOBJFaceIndices(words[word_index], vertex_index);
                for (int k = 0; k < 3; k++) {
                    // Special value INT_MAX means not used; AddFace expects - 1
                    // for unused attributes.
                    if (vertex_index[k] == INT_MAX)
                        vertex_index[k] = - 1;
                     else {
                        if (vertex_index[k] > 0)
                            // Regular index; counting starts at 1 in OBJ files.
                            vertex_index[k]--;
                        else if (vertex_index[k] < 0)
                            // Negative numer is relative index.
                            vertex_index[k] += nu_attribute_vertices[OBJ_attributes[k]];
                        else
                            ModelFileReadError("Vertex index of 0 not allowed in OBJ file");
                    }
                }
                AddFaceVertex(OBJ_attributes, vertex_index);
            }
            EndFace();
        }
    }
    fclose(fp);
}

sreLODModel *sreReadMultiDirectoryLODModelFromFile(const char *filename, const char *base_path,
int model_type, int load_flags) {
    // Read vertex attribute and face information.
    switch (model_type) {
    case SRE_MODEL_FILE_TYPE_OBJ :
        ResetSourceModelData();
        ReadOBJ(filename);
        break;
    default :
        ModelFileReadError("Model file format not supported");
        exit(1);
        break;
    }

    sreLODModel *m = sreNewLODModel();

    InitializeModelFromFaceData(m);

    AddFacesToModel(m);

    FreeSourceModelData();

    if (nu_attribute_vertices[SRE_ATTRIBUTE_NORMAL] == 0) {
        // If no normals were specified in the file, calculate them.
        m->CalculateNormals();
    }
    else
        // Vertex normals are already set, calculate just triangle normals.
        m->CalculateTriangleNormals();

    // Because we duplicated every vertex when adding the triangles, there
    // is likely to be potential for optimization, which is handled by
    // this library function.
    m->MergeIdenticalVertices();

    printf("Loaded .obj file %s, %d vertices, %d triangles\n", filename,
        m->nu_vertices, m->nu_triangles);
    return m;
}

sreLODModel *sreReadLODModelFromFile(const char *filename, int model_type, int load_flags) {
    return sreReadMultiDirectoryLODModelFromFile(filename, NULL, model_type, load_flags);
}

sreModel *sreReadMultiDirectoryModelFromFile(sreScene *scene, const char *filename, const char *base_path,
int model_type, int load_flags) {
#ifdef ASSIMP_SUPPORT
    // When assimp is available, use it for any file type (including .OBJ), except when a
    // flag indicates using the native SRE model loading function.
    if (!(load_flags & SRE_MODEL_LOAD_FLAG_USE_SRE))
        return sreReadModelFromAssimpFile(scene, filename, base_path, load_flags);
#endif

    sreModel *model = new sreModel;
    model->lod_model[0] = sreReadLODModelFromFile(filename, model_type, load_flags);
    model->nu_lod_levels = 1;
    model->CalculateBounds();
    model->collision_shape_static = SRE_COLLISION_SHAPE_STATIC;
    model->collision_shape_dynamic = SRE_COLLISION_SHAPE_CONVEX_HULL;
    scene->RegisterModel(model);
    return model;
}

sreModel *sreReadModelFromFile(sreScene *scene, const char *filename, int model_type, int load_flags) {
#ifdef ASSIMP_SUPPORT
    // When assimp is available, use it for any file type (including .OBJ), except when a
    // flag indicates using the native SRE model loading function.
    if (!(load_flags & SRE_MODEL_LOAD_FLAG_USE_SRE))
        return sreReadModelFromAssimpFile(scene, filename, "", load_flags);
#endif

    return sreReadMultiDirectoryModelFromFile(scene, filename, NULL, model_type, load_flags);
}
