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

// Defined in demo1.cpp
void Demo1CreateScene(sreScene *scene, sreView *view);
void Demo1Step(sreScene *scene, double demo_time);

// Defined in demo2.cpp
void Demo2CreateScene(sreScene *scene, sreView *view);
void Demo2Step(sreScene *scene, double demo_time);

// Defined in demo4.cpp
void Demo4CreateScene(sreScene *scene, sreView *view);
void Demo4Step(sreScene *scene, double demo_time);
void Demo4StepBeforePhysics(sreScene *scene, double demo_time);
void Demo4SetParameters(float day_interval, bool display_time, bool physics,
    bool create_spacecraft, bool show_spacecraft, float sun_light_factor,
    float extra_lod_threshold_scaling);

void Demo4bCreateScene(sreScene *scene, sreView *view);
void Demo4cCreateScene(sreScene *scene, sreView *view);
void Demo4bStep(sreScene *scene, double demo_time);
void Demo4cStep(sreScene *scene, double demo_time);

// Defined in demo5.cpp
void Demo5CreateScene(sreScene *scene, sreView *view);
void Demo5Step(sreScene *scene, double demo_time);

// Defined in demo5.cpp
void Demo6Step(sreScene *scene, double demo_time);

// Defined in demo7.cpp
void Demo7CreateScene(sreScene *scene, sreView *view);
void Demo7Step(sreScene *scene, double demo_time);

// Defined in demo8.cpp
void Demo8CreateScene(sreScene *scene, sreView *view);
void Demo8Step(sreScene *scene, double demo_time);

// Defined in demo9.cpp
void Demo9CreateScene(sreScene *scene, sreView *view);
void Demo9Step(sreScene *scene, double demo_time);

// Defined in demo10.cpp
void Demo10CreateScene(sreScene *scene, sreView *view);
void Demo10Step(sreScene *scene, double demo_time);

// Defined in demo11.cpp
void Demo11CreateScene(sreScene *scene, sreView *view);
void Demo11Step(sreScene *scene, double demo_time);

// Defined in demo12.cpp
void Demo12CreateScene(sreScene *scene, sreView *view);
void Demo12Step(sreScene *scene, double demo_time);

// Defined in demo13.cpp
void Demo13CreateScene(sreScene *scene, sreView *view);
void Demo13Step(sreScene *scene, double demo_time);

// Defined in texture_test.cpp
void TextureTestCreateScene(sreScene *scene, bool compressed);
void TextureTestStep(sreScene *scene, double demo_time);
void TextureMemoryTest(bool compressed);

// Defined in textdemo.cpp
void TextDemoCreateScene(sreScene *scene, sreView *view);
void TextDemoStep(sreScene *scene, double demo_time);


