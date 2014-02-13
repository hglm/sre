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
#include <ctype.h>
#include <math.h>

#include "sre.h"
#include "sreRandom.h"

#include "demo.h"

#define NU_TESTS 5
#define TEST_DURATION 5.0
#define ASPECT_RATIO (16.0f / 9.0f)

typedef void (*TextDemoInitFuncType)();
typedef void (*TextDemoDrawFuncType)();
typedef void (*TextDemoSetTextFuncType)(double dt);


static sreRNG *rng;

static int current_test = - 1;
static double test_start_time, test_time;

class TextTestInfo {
public :
    const char *name;
    int shader_type;
    TextDemoInitFuncType init_func;
    TextDemoSetTextFuncType set_text_func;
    TextDemoDrawFuncType draw_func;
};

extern const TextTestInfo test_info[NU_TESTS];

#define GRID_WIDTH 32
#define GRID_HEIGHT 18

static char *grid;

static void GridSet(int x, int y, int c) {
    grid[y * (GRID_WIDTH + 1) + x] = c;
}

static int GridGet(int x, int y) {
    return grid[y * (GRID_WIDTH + 1) + x];
}

static void GridSwap(int x1, int y1, int x2, int y2) {
    int char1 = GridGet(x1, y1);
    int char2 = GridGet(x2, y2);
    GridSet(x1, y1, char2);
    GridSet(x2, y2, char1);
}

static void SetGridPattern() {
    int i = 0;
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            grid[i] = 'A' + i % 26;
            i++;
        }
        grid[i] = '\n';
        i++;
    }
    grid[i] = '\0';
}

#define MAX_RANDOM_COLORS 256

static Color random_color[MAX_RANDOM_COLORS];
static const Vector4D test_colors[4][2] = {
    { Vector4D(1.0f, 1.0f, 1.0f, 1.0f), Vector4D(0, 0, 0, 0) },
    { Vector4D(1.0f, 0.2f, 0.2f, 1.0f), Vector4D(0, 0, 0, 0) },
    { Vector4D(1.0f, 1.0f, 0.2f, 1.0f), Vector4D(0, 0, 0, 0) }, 
    { Vector4D(0.2f, 0.2f, 1.0f, 1.0f), Vector4D(0, 0, 0, 0) }
};

static const double font_aspect_ratio = 2.0f; // Assume standard 32x16 pixel font.

// Return a random postion avoiding the top and bottom edges of the screen and
// taking into account the character size.
 
static void GetRandomPosition(float char_width, float char_height,
float *x, float *y) {
    *x = rng->RandomFloat(1.0 - char_width);
    *y = 0.1 + rng->RandomFloat(0.8 - char_height);
}

static void NoopVoid() {
}

static void NoopSetText(double dt) {
}

// Test 0, characters in a predefined grid are swapped at random.
// The amount of swapping is independent of framerate.
// After 5 seconds, the grid begins to move and make circles.

#define TEST_GRID_SWAP_RATE 1000.0
#define TEST_GRID_SWAP_CIRCLE_PERIOD 2.0

static double swaps_to_go = 0;

void TextDemoGridSwapSetText(double dt) {
    // Given a high framerate the amount of swaps per frame is low
    // or rounded to zero, so keep track of the rate in float format.
    swaps_to_go += (int)(dt * TEST_GRID_SWAP_RATE);
    int n = trunc(swaps_to_go);
    for (int i = 0; i < n; i++) {
        unsigned int r = rng->Random32();
        // It is important that r is unsigned, otherwise the sign
        // bit will be retained and the modulo result will be unexpected.
        int x1 = r % GRID_WIDTH;
        int y1 = (r >> 8) % GRID_HEIGHT;
        int x2 = (r >> 16) % GRID_WIDTH;
        int y2 = (r >> 24) % GRID_HEIGHT;
        GridSwap(x1, y1, x2, y2);
    }
    swaps_to_go -= n;
}


// Test result.
//
// Old text shader:            1700 fps
// New text shader (text2):    2000 fps

void TextDemoGridSwapDraw() {
    if (test_info[current_test].shader_type == 1) {
        if (current_test & 1)
            sreSetImageBlendingMode(SRE_IMAGE_BLEND_OPAQUE);
        else
            sreSetImageBlendingMode(SRE_IMAGE_BLEND_ADDITIVE);
    }
#if 0
    else if (current_test & 1) {
        // The second iteration is the same as the first but with opaque text (no blending).
        // By default blend mode is enabled before calling this function, so
        // we have to explictly disable text blending each frame.
        sreSetTextParameters(0, 0, Color(1.0, 1.0, 1.0), SRE_TEXT_FLAG_OPAQUE);
    }
#endif
    float cw = 1.0f / GRID_WIDTH;
    float ch = 0.8f / GRID_HEIGHT;
    float x = 0;
    float y = 0.1;
    if (test_time >= TEST_DURATION * 0.5) {
        // Get the phase from 0 to 1.0.
        double t = 2 * M_PI * fmod(demo_time, TEST_GRID_SWAP_CIRCLE_PERIOD) / TEST_GRID_SWAP_CIRCLE_PERIOD;
        // Move in a circle with a radius 0.1. To start in the original position (no abrupt
        // move), start with 0.25 * PI.
        t += M_PI * 0.25;
        x += cos(t) * 0.05;
        y += sin(t) * 0.05 * ASPECT_RATIO;
    }
    if (test_info[current_test].shader_type == 1) {
        // Use the new text shader. It doesn't support multiple lines, but is fast.
        // The font size used depends on the grid size, it may not match the original font
        // aspect ratio.
        Vector2D font_size = Vector2D(cw, ch);
        // test_colors[2] is yellow.
        sreSetTextParameters(SRE_IMAGE_SET_COLORS | SRE_TEXT_SET_FONT_SIZE,
            test_colors[2], &font_size);
        for (int i = 0; i < GRID_HEIGHT; i++) {
            sreDrawTextN(&grid[i * (GRID_WIDTH + 1)], GRID_WIDTH,
                x, y + i * ch);
        }
    }
#if 0
    else
        sreDrawText(grid, x, y, cw, ch);
#endif
    if (test_info[current_test].shader_type == 1 && (current_test & 1))
        sreSetImageBlendingMode(SRE_IMAGE_BLEND_ADDITIVE);
}

void TextDemoGridSwapNewShaderInit() {
    // Set the default font.
    sreSetFont(NULL);
}

// Test 1, random postion drawing. Because we redraw all text
// every frame we need to keep track of the positions.

#define TEST_RANDOM_POSITION_MAX_CHARACTERS 500
#define TEST_RANDOM_POSITION_RATE 1000.0
#define TEST_RANDOM_POSITION_CHAR_WIDTH 0.03
#define TEST_RANDOM_POSITION_CHAR_HEIGHT (0.03 * font_aspect_ratio)
#define TEST_RANDOM_POSITION_NU_COLORS 16

static int test1_nu_characters = 0;
static float position_table[TEST_RANDOM_POSITION_MAX_CHARACTERS][2];
static char character_table[TEST_RANDOM_POSITION_MAX_CHARACTERS];
static double items_to_go = 0;
static int oldest_item = 0;

void TextDemoRandomPositionSetText(double dt) {
    items_to_go += (int)(dt * TEST_RANDOM_POSITION_RATE);
    int n = trunc(items_to_go);
    for (int i = 0 ; i < n; i++) {
        float x, y;
        GetRandomPosition(TEST_RANDOM_POSITION_CHAR_WIDTH, TEST_RANDOM_POSITION_CHAR_HEIGHT, &x, &y);
        int c = 'A' + (rng->RandomInt(26));
        int j;
        if (test1_nu_characters < TEST_RANDOM_POSITION_MAX_CHARACTERS) {
            // Still room in the table.
            j = test1_nu_characters;
            test1_nu_characters++;
        }
        else {
            // Replace the oldest item.
            // Item 0 will be the oldest when the first
            // replacement occurs. The index will roll
            // around.
            j = oldest_item;
            oldest_item = (oldest_item + 1) % TEST_RANDOM_POSITION_MAX_CHARACTERS;
        }
        position_table[j][0] = x;
        position_table[j][1] = y;
        character_table[j] = c;
    }
    items_to_go -= n;
}

void TextDemoRandomPositionDraw() {
    int text1_shader_blend_flags;
    if (test_info[current_test].shader_type == 1) {
        if (current_test & 1)
            sreSetImageBlendingMode(SRE_IMAGE_BLEND_OPAQUE);
        else
            sreSetImageBlendingMode(SRE_IMAGE_BLEND_ADDITIVE);
    }
#if 0
    else {
        text1_shader_blend_flags = 0;
        if (current_test & 1) {
            // The second iteration is the same as the first but with opaque text (no blending).
            // By default blend mode is enabled before calling this function, so
            // we have to explictly disable text blending each frame.
            sreSetTextParameters(0, 0, Color(1.0, 1.0, 1.0), SRE_TEXT_FLAG_OPAQUE);
            text1_shader_blend_flags = SRE_TEXT_FLAG_OPAQUE;
        }
    }
#endif
    char text[4];
    text[0] = 'A'; text[1] = '\0';
    float dx = 0;
    float dy = 0;
    double local_time = test_time;
    if (local_time >= TEST_DURATION * 0.5) {
        double t = (local_time - TEST_DURATION * 0.5);
        // Oscillate every two seconds.
        double offset = sin(t * 2.0 * M_PI / 2.0) * 0.1;
        if (current_test == 1)
            dx += offset;
        else
            dy += offset;
    }
    if (test_info[current_test].shader_type == 1) {
        Vector2D font_size = Vector2D(TEST_RANDOM_POSITION_CHAR_WIDTH, TEST_RANDOM_POSITION_CHAR_HEIGHT);
        sreSetTextParameters(SRE_TEXT_SET_FONT_SIZE, NULL, &font_size);
    }
    for (int i = 0; i < test1_nu_characters; i++) {
        text[0] = character_table[i];
#if 0
        if (test_info[current_test].shader_type == 0) {
            sreSetTextParameters(0, 0, random_color[i % TEST_RANDOM_POSITION_NU_COLORS],
                text1_shader_blend_flags);
            sreDrawText(text, position_table[i][0] + dx, position_table[i][1] + dy,
                TEST_RANDOM_POSITION_CHAR_WIDTH, TEST_RANDOM_POSITION_CHAR_HEIGHT);
        }
        else {
#endif
            Vector4D colors[2];
            colors[0] = Vector4D(random_color[i % TEST_RANDOM_POSITION_NU_COLORS], 0); // mult_color
            colors[1].Set(0, 0, 0, 0); // add_color
            sreSetTextParameters(SRE_IMAGE_SET_COLORS, colors, NULL);
            sreDrawTextN(text, 1, position_table[i][0] + dx, position_table[i][1] + dy);
//        }
    }
    if (test_info[current_test].shader_type == 1 && (current_test & 1))
        sreSetImageBlendingMode(SRE_IMAGE_BLEND_ADDITIVE);
}

// Demo 3 uses scaling to a big size, smoothly modulating the scaling factor
// based on demo_time.

#define TEST_SCALE_PERIOD (TEST_DURATION * 0.5)

void TextDemoScaleDraw() {
    char text[2];
    text[1] = '\0';
    // Get the phase from 0 to 1.0.
    double t = fmod(demo_time, TEST_SCALE_PERIOD) / TEST_SCALE_PERIOD;
    // Run the alphabet from A to Z during the test duration.
    text[0] = 'A' + trunc(26.0 * fmod(demo_time, TEST_DURATION) / TEST_DURATION);
    // Smoothly modulate the scaling factor with sinus function,
    // ranging from 0.5 to 1.1.
    double scale = 0.5 + 0.3 * sin(t * M_PI * 2.0);
    // Center the image. Fonts texture needs to be symmetrical
    // for good results.
    float x = 0.5 - 0.5 * scale;
    float scale_y = scale * font_aspect_ratio;
    float y = 0.5 - 0.5 * scale_y;
    Vector2D font_size = Vector2D(scale, scale_y);
    sreSetTextParameters(SRE_IMAGE_SET_COLORS | SRE_TEXT_SET_FONT_SIZE,
        test_colors[text[0] & 3], &font_size);
    sreDrawTextN(text, 1, x, y);
}

// Default multiply and addition colors for image and text shaders.

#define NOT_INVERSE

#ifdef NOT_INVERSE

static const Vector4D default_image_colors[2] = {
    Vector4D(1.0f, 1.0f, 1.0f, 1.0f),
    Vector4D(0.0f, 0.0f, 0.0f, 0.0f)
};

#else

// Yellow background, black text.
static const Vector4D default_image_colors[2] = {
    Vector4D(- 1.0f, - 1.0f, - 1.0f, 0.0f),
    Vector4D(1.0f, 1.0f, 0.2f, 1.0f)
};

#endif

static const Vector2D default_font_size = Vector2D(0.02, 0.03);

static void SetStandardTextParameters() {
#if 0
    if (test_info[current_test].shader_type == 0)
        sreSetTextParameters(0, 0, Color(1.0, 1.0, 1.0), 0);
    else
#endif
        sreSetTextParameters(SRE_TEXT_SET_FONT_SIZE | SRE_IMAGE_SET_COLORS,
            default_image_colors, &default_font_size);
}

const TextTestInfo test_info[NU_TESTS] = {
#if 0
    { "Grid character swap", 0,
      NoopVoid, TextDemoGridSwapSetText, TextDemoGridSwapDraw },
    { "Grid character swap (opaque)", 0,
      NoopVoid, TextDemoGridSwapSetText, TextDemoGridSwapDraw },
    { "Random position", 0,
      NoopVoid, TextDemoRandomPositionSetText, TextDemoRandomPositionDraw },
    { "Random position (no blend)", 0,
      NoopVoid, TextDemoRandomPositionSetText, TextDemoRandomPositionDraw },
#endif
    { "Grid character swap", 1,
      TextDemoGridSwapNewShaderInit, TextDemoGridSwapSetText, TextDemoGridSwapDraw },
    { "Grid character swap (opaque)", 1,
      TextDemoGridSwapNewShaderInit, TextDemoGridSwapSetText, TextDemoGridSwapDraw },
    { "Random position", 1,
      NoopVoid, TextDemoRandomPositionSetText, TextDemoRandomPositionDraw },
    { "Random position (no blend)", 1,
      NoopVoid, TextDemoRandomPositionSetText, TextDemoRandomPositionDraw },
    { "Scale", 1,
      NoopVoid, NoopSetText, TextDemoScaleDraw }
};

static void DrawTextOverlay() {
    // Skip the very first frame.
    if (current_test < 0)
        return;
    test_time = demo_time - test_start_time;
#if 0
    if (test_info[current_test].shader_type == 0) {
        SetStandardTextParameters();
        sreBeginText();
        test_info[current_test].draw_func();
        sreEndText();
        SetStandardTextParameters();
        sreBeginText();
        // Draw the name of the test at the bottom.
        char s[80];
        sprintf(s, "Test %d: %s", current_test, test_info[current_test].name);
        sreSetTextParameters(0, 0, Color(1.0f, 1.0f, 1.0f), 0);
        sreDrawTextCentered(s, 0, 1.0f, 0.95f, 0.03f, 0.05f);
        sreEndText();
    }
    else {
#endif
        // Ensure the standard font will be bound.
        sreSetFont(NULL);
        test_info[current_test].draw_func();
        // Draw the name of the test at the bottom.
        char s[80];
        sprintf(s, "Test %d: %s", current_test, test_info[current_test].name);
        Vector2D font_size = Vector2D(0.03f, 0.05f);
        sreSetTextParameters(SRE_TEXT_SET_FONT_SIZE | SRE_IMAGE_SET_COLORS,
            default_image_colors, &font_size);
        sreDrawTextCentered(s, 0,  0.95f, 1.0f);
//    }
    // Add the default GUI (fps etc).
    DemoTextOverlay();
}

void TextDemoCreateScene() {
    // Scene doesn't matter.
#if 0
    sreModel *sphere_model = sreCreateSphereModel(scene, 0);
    // Add player sphere as scene object 0.
    scene->SetFlags(SRE_OBJECT_DYNAMIC_POSITION | SRE_OBJECT_NO_PHYSICS);
    Color c;
    c.r = 0.00;
    c.g = 0.75;
    c.b = 1.0;
    scene->SetColor(c);
    scene->AddObject(sphere_model, 0, 0, 3.0, 0, 0, 0, 3.0);
    scene->AddDirectionalLight(0, Vector3D(0.1, - 0.5, 1.0), Color(1.0, 1.0, 1.0));
#endif

    // Frame-indepedent test initialization

    rng = sreGetDefaultRNG();

    // Test 0.
    // GRID_WIDTH x GRID_HEIGHT character grid, with space for newlines and terminator.
    grid = new char[(GRID_WIDTH + 1) * GRID_HEIGHT + 1];
    SetGridPattern();

    // Test 1.
    for (int i = 0; i < MAX_RANDOM_COLORS; i++)
        for (;;) {
            // Keep trying until we get a reasonable intensity.
            random_color[i] = Color().SetRandom();
            float intensity = random_color[i].SRGBIntensity();
            if (intensity >= 0.3)
                break;
        };

    // The main purpose of this demo.
    sreSetDrawTextOverlayFunc(DrawTextOverlay);

    // The order of the demo function is Render, TextOverlay, TimeIteration.
    // The current test will be set in the TimeIteration function.
    // Therefore, we need check for current_test == - 1 (invalid)
    // in the very first frame in the TextOverlay function.
}

void TextDemoRender() {
    scene->Render(view);
}

void TextDemoTimeIteration(double previous_time, double current_time) {
   float dt = current_time - previous_time;
   int test_number = (int)truncf(demo_time / TEST_DURATION) % NU_TESTS;
   if (test_number != current_test) {
       current_test = test_number;
       test_info[current_test].init_func();
       test_start_time = demo_time;
   }
   test_info[current_test].set_text_func(dt);
}
