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


#if !defined(WINDOW_WIDTH) || !defined(WINDOW_HEIGHT)
#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 576
#endif

// Defined in main.cpp
extern sreScene *scene;
extern sreView *view;
extern bool lock_panning;
extern void (*RenderFunc)();
extern void (*TimeIterationFunc)(double time_previous, double time_current);
// Physics engine inputs.
extern int control_object;
extern bool jump_requested;
extern float input_acceleration;
extern float horizontal_acceleration;
extern float max_horizontal_velocity;
extern bool dynamic_gravity;
extern Point3D gravity_position;
extern bool no_gravity;
extern float hovering_height;
extern float hovering_height_acceleration;
extern bool no_ground_plane;
// Misc.
extern double demo_time;
extern bool fullscreen_mode;
extern bool jump_allowed;
extern bool demo_stop_signalled;
extern double text_message_time, text_message_timeout;
extern int nu_text_message_lines;
extern char *text_message[24];

void RunDemo();
// Text overlay function with FPS and settings menus
void DemoTextOverlay();

// Defined in demo1.cpp
void Demo1CreateScene();
void Demo1Render();
void Demo1TimeIteration(double time_previous, double time_current);

// Defined in demo2.cpp
void Demo2CreateScene();
void Demo2Render();
void Demo2TimeIteration(double time_previous, double time_current);

// Defined in demo3.cpp
extern double demo3_start_time;
extern double demo3_days_per_second;
extern double demo3_elapsed_time;
extern double demo3_time;
void Demo3CreateScene();
void Demo3Render();
void Demo3TimeIteration(double time_previous, double time_current);

// Defined in demo4.cpp
void Demo4CreateScene();
void Demo4Render();
void Demo4TimeIteration(double time_previous, double time_current);

// Defined in demo5.cpp
void Demo5CreateScene();
void Demo5Render();
void Demo5TimeIteration(double time_previous, double time_current);
void Demo6TimeIteration(double time_previous, double time_current);

// Defined in demo7.cpp
void Demo7CreateScene();
void Demo7Render();
void Demo7TimeIteration(double time_previous, double time_current);

// Defined in demo8.cpp
void Demo8CreateScene();
void Demo8Render();
void Demo8TimeIteration(double time_previous, double time_current);

// Defined in demo9.cpp
void Demo9CreateScene();
void Demo9Render();
void Demo9TimeIteration(double time_previous, double time_current);

// Defined in game.cpp
void RunGame();
void GameRender();
void GameTimeIteration(double time_previous, double time_current);

// Defined in texture_test.cpp
void TextureTestCreateScene(bool compressed);
void TextureTestRender();
void TextureTestTimeIteration(double previous_time, double current_time);

// Defined in demo10.cpp
void Demo10CreateScene();
void Demo10Render();
void Demo10TimeIteration(double time_previous, double time_current);

// Defined in demo11.cpp
void Demo11CreateScene();
void Demo11Render();
void Demo11TimeIteration(double time_previous, double time_current);

// Defined in texture_test.cpp
void TextDemoCreateScene();
void TextDemoRender();
void TextDemoTimeIteration(double previous_time, double current_time);

void TextureMemoryTest(bool compressed);

// Defined in bullet.cpp:
#ifdef USE_BULLET
void BulletInitialize();
void BulletDestroy();
#endif

