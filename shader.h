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


// sre_matrix.cpp

// The following matrices are only used in the shader modules.
// The general projection/view matrices are defined in sre_internal.h.
extern MatrixTransform shadow_map_matrix;
extern Matrix4D projection_shadow_map_matrix;
extern Matrix4D cube_shadow_map_matrix;
extern MatrixTransform shadow_map_lighting_pass_matrix;
extern Matrix4D projection_shadow_map_lighting_pass_matrix;
extern Vector3D sre_internal_up_vector;
extern Vector3D sre_internal_camera_vector;

// vertex_buffer.cpp

extern const int sre_internal_attribute_size[SRE_NU_VERTEX_ATTRIBUTES];
extern char sre_internal_interleaved_offset_table[(1 << SRE_NU_VERTEX_ATTRIBUTES)][SRE_NU_VERTEX_ATTRIBUTES + 1];
extern unsigned int sre_internal_attribute_list_table[1 << SRE_NU_VERTEX_ATTRIBUTES];

#define SRE_GET_INTERLEAVED_STRIDE(attribute_mask) \
    (sre_internal_interleaved_offset_table[attribute_mask][0])
#define SRE_GET_INTERLEAVED_ATTRIBUTE_OFFSET(attribute_mask, attribute_index) \
    (sre_internal_interleaved_offset_table[attribute_mask][attribute_index + 1])
#define SRE_GET_INTERLEAVED_OFFSET_LIST(attribute_mask) \
    (&sre_internal_interleaved_offset_table[attribute_mask][1])

// Shader definitions.

#define NU_SINGLE_PASS_SHADERS 10
#ifdef NO_SHADOW_MAP
#define NU_MULTI_PASS_SHADERS 13
#else
#define NU_MULTI_PASS_SHADERS 22
#endif

#define MAX_UNIFORMS 32

enum { SRE_SHADER_STATUS_UNINITIALIZED, SRE_SHADER_STATUS_INITIALIZED,
    SRE_SHADER_STATUS_LOADED };

class sreShader {
public :
    const char *name;
    SRE_GLUINT program;
    int status;
    int type;
    SRE_GLINT uniform_location[MAX_UNIFORMS];
    unsigned int uniform_mask;
    int attribute_mask;
    const char *vfilename;
    const char *ffilename;
    const char *prologue;

    sreShader();
    // When demand loading is disabled, Initialize() will load the shader.
    void Initialize(const char *_name, int _type, int _uniform_mask, int _attribute_mask,
        const char * _vsource, const char *_fsource, const char *_prologue);
    void Initialize(const char *vertex_shader, const char *fragment_shader, const char *prologue);
    void Load();
    void Validate() {
        if (status == SRE_SHADER_STATUS_LOADED)
            return;
        Load();
    }
    void BindAttributes();
    void InitializeUniformLocationsLightingShader();
    void InitializeUniformLocationsMiscShader();
    void InitializeUniformLocationsMiscShaderNew();
    // Initialization of uniforms that only need to be initialized once.
    void SetDefaultUniformValues();
};

extern bool sre_shader_load_on_demand; 

// Uniforms used in the lighting shaders.

#define UNIFORM_MVP 0
#define UNIFORM_MODEL_MATRIX 1
#define UNIFORM_MODEL_ROTATION_MATRIX 2
#define UNIFORM_DIFFUSE_REFLECTION_COLOR 3
#define UNIFORM_USE_MULTI_COLOR 4
#define UNIFORM_USE_TEXTURE_MAP 5
// #define UNIFORM_CURRENT_LIGHT 6
// Reuse unused uniform index.
// #define UNIFORM_SHADOW_MAP_DIMENSIONS 6
#define UNIFORM_AMBIENT_COLOR 7
#define UNIFORM_VIEWPOINT 8
#define UNIFORM_LIGHT_PARAMETERS 9
#define UNIFORM_SHADOW_MAP_PARAMETERS 10
#if 0
#define UNIFORM_LIGHT_POSITION 9
#define UNIFORM_LIGHT_ATT 10
#define UNIFORM_LIGHT_COLOR 11
#endif
#define UNIFORM_SPECULAR_REFLECTION_COLOR 12
#define UNIFORM_SPECULAR_EXPONENT 13
#define UNIFORM_TEXTURE_MAP_SAMPLER 14
#define UNIFORM_USE_NORMAL_MAP 15
#define UNIFORM_NORMAL_MAP_SAMPLER 16
#define UNIFORM_USE_SPECULARITY_MAP 17
#define UNIFORM_SPECULARITY_MAP_SAMPLER 18
#define UNIFORM_EMISSION_COLOR 19
#define UNIFORM_USE_EMISSION_MAP 20
#define UNIFORM_EMISSION_MAP_SAMPLER 21
#define UNIFORM_DIFFUSE_FRACTION 22
#define UNIFORM_ROUGHNESS 23
#define UNIFORM_ROUGHNESS_WEIGHTS 24
#define UNIFORM_ANISOTROPIC 25
#define UNIFORM_SHADOW_MAP_TRANSFORMATION_MATRIX 26
#define UNIFORM_SHADOW_MAP_SAMPLER 27
#define UNIFORM_CUBE_SHADOW_MAP_SAMPLER 28
// #define UNIFORM_SEGMENT_DISTANCE_SCALING 29
// #define UNIFORM_SPOTLIGHT 30
#define UNIFORM_UV_TRANSFORM 31
#if 1
#define UNIFORM_LIGHT_PARAMETERS_MASK ((1 << UNIFORM_LIGHT_PARAMETERS) | \
    (1 << UNIFORM_SPECULAR_REFLECTION_COLOR) | (1 << UNIFORM_SPECULAR_EXPONENT))
#else
#define UNIFORM_LIGHT_PARAMETERS_MASK ((1 << 9) | (1 << 10) | (1 << 11) | (1 << 12) | (1 << 13))
#endif
// The mask defines a set of commonly used uniforms, but does not include all uniforms.
#define UNIFORM_MASK_COMMON ((1 << UNIFORM_MVP) | (1 << UNIFORM_MODEL_MATRIX ) | \
    (1 << UNIFORM_MODEL_ROTATION_MATRIX) | (1 << UNIFORM_DIFFUSE_REFLECTION_COLOR) | \
    (1 << UNIFORM_USE_MULTI_COLOR) | (1 << UNIFORM_USE_TEXTURE_MAP) | \
    (1 << UNIFORM_AMBIENT_COLOR) | \
    (1 << UNIFORM_VIEWPOINT) | (1 << UNIFORM_LIGHT_PARAMETERS) | \
    (1 << UNIFORM_SPECULAR_REFLECTION_COLOR) | (1 << UNIFORM_SPECULAR_EXPONENT) | \
    (1 << UNIFORM_TEXTURE_MAP_SAMPLER) | (1 << UNIFORM_USE_NORMAL_MAP ) | \
    (1 << UNIFORM_NORMAL_MAP_SAMPLER) | (1 << UNIFORM_USE_SPECULARITY_MAP) | \
    (1 << UNIFORM_SPECULARITY_MAP_SAMPLER) | (1 << UNIFORM_EMISSION_COLOR) | \
    (1 << UNIFORM_USE_EMISSION_MAP ) | (1 << UNIFORM_EMISSION_MAP_SAMPLER ) | \
    ((unsigned int)1 << UNIFORM_UV_TRANSFORM))

// It would be better to treat the miscellaneous shaders in a more efficient in terms of storing
// the uniform locations. Combining the uniform mask has no great advantage, except for initialization
// (which can still take advantage of).

#define MAX_MISC_UNIFORMS 22

#define UNIFORM_MISC_MVP 0
#define UNIFORM_MISC_LIGHT_MODEL_SPACE 1
#define UNIFORM_MISC_VIEW_PROJECTION_MATRIX 2
#define UNIFORM_MISC_BASE_COLOR 3
#define UNIFORM_MISC_ASPECT_RATIO 4
#define UNIFORM_MISC_HALO_SIZE 5
#define UNIFORM_MISC_TEXTURE_SAMPLER 6
#define UNIFORM_MISC_LIGHT_POSITION 7
#define UNIFORM_MISC_MODEL_MATRIX 8
#define UNIFORM_MISC_SEGMENT_DISTANCE_SCALING 9
#define UNIFORM_MISC_AVERAGE_LUM_SAMPLER 10
#define UNIFORM_MISC_LUMINANCE_HISTORY_SLOT 11
#define UNIFORM_MISC_KEY_VALUE 12
#define UNIFORM_MISC_ARRAY_INDEX 13
#define UNIFORM_MISC_RECTANGLE 14
#define UNIFORM_MISC_UV_TRANSFORM 15
#define UNIFORM_MISC_MULT_COLOR 16
#define UNIFORM_MISC_ADD_COLOR 17
#define UNIFORM_MISC_SCREEN_SIZE_IN_CHARS 18
#define UNIFORM_MISC_STRING 19
#define UNIFORM_MISC_USE_EMISSION_MAP 20
#define UNIFORM_MISC_SHADOW_MAP_DIMENSIONS 21

// The misc shader-specific uniforms are packed in the order they appear in the generic misc
// uniforms list above. The indices do not correspond, the real uniform indices for each shader
// are explictly defined below. These constants are not yet used (each misc shader still has
// a full generic array of uniform locations).

enum { UNIFORM_TEXT1_TEXTURE_SAMPLER = 0, UNIFORM_TEXT1_BASE_COLOR };
enum { UNIFORM_TEXT2_TEXTURE_SAMPLER = 0, UNIFORM_TEXT2_RECTANGLE,
     UNIFORM_TEXT2_MULT_COLOR, UNIFORM_TEXT2_ADD_COLOR,
     UNIFORM_TEXT2_SCREEN_SIZE_IN_CHARS, UNIFORM_TEXT2_STRING };
// The four image shaders shader the same uniform identifiers.
enum { UNIFORM_IMAGE_TEXTURE_SAMPLER = 0, UNIFORM_IMAGE_RECTANGLE, UNIFORM_IMAGE_UV_TRANSFORM,
     UNIFORM_IMAGE_MULT_COLOR, UNIFORM_IMAGE_ADD_COLOR, UNIFORM_IMAGE_ARRAY_INDEX };
enum { UNIFORM_SHADOW_VOLUME_MPV = 0, UNIFORM_SHADOW_VOLUME_LIGHT_POS_MODEL_SPACE };
enum { UNIFORM_SHADOW_MAP_MVP = 0 };
enum { UNIFORM_SHADOW_MAP_TRANSPARENT_MVP = 0, UNIFORM_SHADOW_MAP_TRANSPARENT_TEXTURE_SAMPLER };
enum { UNIFORM_CUBE_SHADOW_MAP_MVP = 0, UNIFORM_CUBE_SHADOW_MAP_LIGHT_POSITION,
    UNIFORM_CUBE_SHADOW_MAP_MODEL_MATRIX, UNIFORM_CUBE_SHADOW_MAP_SEGMENT_DISTANCE_SCALING };
enum { UNIFORM_CUBE_SHADOW_MAP_TRANSPARENT_MVP = 0, UNIFORM_CUBE_SHADOW_MAP_TRANSPARENT_LIGHT_POSITION,
    UNIFORM_CUBE_SHADOW_MAP_TRANSPARENT_MODEL_MATRIX,
    UNIFORM_CUBE_SHADOW_MAP_TRANSPARENT_SEGMENT_DISTANCE_SCALING };
enum { UNIFORM_HALO_MVP = 0, UNIFORM_HALO_VIEW_PROJECTION_MATRIX,
    UNIFORM_HALO_BASE_COLOR, UNIFORM_HALO_ASPECT_RATIO, UNIFORM_HALO_HALO_SIZE };
enum { UNIFORM_PS_VIEW_PROJECTION_MATRIX = 0, UNIFORM_PS_BASE_COLOR, UNIFORM_PS_ASPECT_RATIO,
    UNIFORM_PS_HALO_SIZE };
enum { UNIFORM_HDR_LOG_LUMINANCE_TEXTURE_SAMPLER = 0 };
enum { UNIFORM_HDR_AVERAGE_LOG_LUMINANCE_TEXTURE_SAMPLER = 0 };
enum { UNIFORM_HDR_LUMINANCE_HISTORY_STORAGE_TEXTURE_SAMPLER = 0,
    UNIFORM_HDR_LUMINANCE_HISTORY_STORAGE_AVERAGE_LUM_SAMPLER };
enum { UNIFORM_HDR_LUMINANCE_HISTORY_TEXTURE_SAMPLER = 0, UNIFORM_HDR_LUMINANCE_HISTORY_SLOT };

// Only define misc shaders that can actually be used.
#ifdef NO_SHADOW_MAP
#define SRE_NU_MISC_SHADERS_SHADOW_MAP 0
#else
#define SRE_NU_MISC_SHADERS_SHADOW_MAP 8
#endif
#ifdef NO_HDR
#define SRE_NU_MISC_SHADERS_HDR 0
#else
#define SRE_NU_MISC_SHADERS_HDR 4
#endif
#ifdef OPENGL_ES2
#define SRE_NU_MISC_SHADERS_IMAGE_TEXTURE_ARRAY 0
#else
#define SRE_NU_MISC_SHADERS_IMAGE_TEXTURE_ARRAY 2
#endif
#define SRE_NU_MISC_SHADERS (7 + SRE_NU_MISC_SHADERS_SHADOW_MAP +\
    SRE_NU_MISC_SHADERS_HDR + SRE_NU_MISC_SHADERS_IMAGE_TEXTURE_ARRAY)

enum {
    SRE_MISC_SHADER_TEXT_16X16 = 0,
    SRE_MISC_SHADER_TEXT_32X8,
    SRE_MISC_SHADER_IMAGE_TEXTURE,
    SRE_MISC_SHADER_IMAGE_TEXTURE_ONE_COMPONENT,
#ifndef OPENGL_ES2
    SRE_MISC_SHADER_IMAGE_TEXTURE_ARRAY,
    SRE_MISC_SHADER_IMAGE_TEXTURE_ARRAY_ONE_COMPONENT,
#endif
    SRE_MISC_SHADER_SHADOW_VOLUME,
#ifndef NO_SHADOW_MAP
    SRE_MISC_SHADER_SHADOW_MAP,
    SRE_MISC_SHADER_SHADOW_MAP_NON_CLOSED_OBJECT,
    SRE_MISC_SHADER_SHADOW_MAP_TRANSPARENT,
    SRE_MISC_SHADER_SHADOW_MAP_NON_CLOSED_OBJECT_TRANSPARENT,
    SRE_MISC_SHADER_SPOTLIGHT_SHADOW_MAP,
    SRE_MISC_SHADER_SPOTLIGHT_SHADOW_MAP_TRANSPARENT,
    SRE_MISC_SHADER_CUBE_SHADOW_MAP,
    SRE_MISC_SHADER_CUBE_SHADOW_MAP_TRANSPARENT,
#endif
    SRE_MISC_SHADER_HALO,
    SRE_MISC_SHADER_BILLBOARD,
#ifndef NO_HDR
    SRE_MISC_SHADER_HDR_LOG_LUMINANCE,
    SRE_MISC_SHADER_HDR_AVERAGE_LUMINANCE,
    SRE_MISC_SHADER_HDR_LUMINANCE_HISTORY_STORAGE,
    SRE_MISC_SHADER_HDR_LUMINANCE_HISTORY_COMPARISON,
#endif
};

// shader_loading.cpp

extern sreShader multi_pass_shader[NU_MULTI_PASS_SHADERS];
extern sreShader single_pass_shader[NU_SINGLE_PASS_SHADERS];
extern sreShader misc_shader[SRE_NU_MISC_SHADERS];
extern sreShader HDR_tone_map_shader[SRE_NUMBER_OF_TONE_MAPPING_SHADERS];

void sreInitializeShaders(int shader_mask);
void sreValidateShadowVolumeShaders();
void sreValidateShadowMapShaders();
void sreValidateSpotlightShadowMapShaders();
void sreValidateCubeShadowMapShaders();
void sreValidateHDRShaders();

#ifdef SHADERS_BUILTIN

// shaders_builtin.cpp (auto-generated)

class sreBuiltinShaderTable {
public :
     const char *shader_filename;
     const char *shader_source;
};

extern int sre_nu_builtin_shader_sources;
extern const sreBuiltinShaderTable sre_builtin_shader_table[];

#endif

// shader_uniform.cpp

// void sreInitializeShader(const sreObject& so);
void sreInitializeLightingShaderUniformWithDefaultValue(int uniform_id, int loc);
void sreInitializeMiscShaderUniformWithDefaultValue(int uniform_id, int loc);
void sreInitializeShaderWithMesh(sreObject *so, sreModelMesh *mesh);
bool sreInitializeObjectShaderEmissionOnly(sreObject& so);
bool sreInitializeObjectShaderSinglePass(sreObject& so);
bool sreInitializeObjectShaderAmbientPass(sreObject& so);
bool sreInitializeObjectShaderMultiPassLightingPass(sreObject& so);
void sreInitializeObjectShaderLightHalo(const sreObject& so);
void sreInitializeObjectShaderBillboard(const sreObject& so);
bool sreInitializeObjectShaderMultiPassShadowMapLightingPass(sreObject& so);

