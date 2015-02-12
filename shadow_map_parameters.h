// Shadow map uniform parameters.

#define NU_SHADOW_MAP_PARAMETERS_DIRECTIONAL_BEAM_LIGHT 4
#define NU_SHADOW_MAP_PARAMETERS_SPOT_LIGHT 2
#define NU_SHADOW_MAP_PARAMETERS_POINT_LIGHT 4
#define NU_SHADOW_MAP_PARAMETERS_MAX 4

// Size of shadow map in pixels
#define SHADOW_MAP_SIZE 0
// The precision of the stored depth values.
#define SHADOW_MAP_DEPTH_PRECISION 1
// Indication of the size of the shadow map in world coordinates.
#define SHADOW_MAP_DIMENSIONS_X 2
// Depth of the shadow map in world coordinates.
#define SHADOW_MAP_DIMENSIONS_Z 3

// Precalculated (f + n) / (f - n) for the shadow cube map shader when
// configured in depth storing mode.
#define SHADOW_MAP_F_N_COEFFICIENT_1 2
// Precalculated 2 * f * n / (f - n) for the shadow cube map shader when
// configured in depth storing mode.
#define SHADOW_MAP_F_N_COEFFICIENT_2 3
// Segment distance scaling (1.0 / shadow map depth) for the shadow cube
// map shader when configured in distance storing mode.
#define SHADOW_MAP_SEGMENT_DISTANCE_SCALING 2

