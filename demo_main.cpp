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

// SRE demos -- shared main program.


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "sre.h"
#include "sreBackend.h"
#include "demo.h"

class DemoApplication : public sreBulletPhysicsApplication {
    virtual void StepBeforeRender(double demo_time);
    virtual void StepBeforePhysics(double demo_time);
};

class Demo {
public :
    const char *name;
    void (*CreateScene)(sreScene *scene, sreView *view);
    void (*Step)(sreScene *scene, double relative_time);
};

static const Demo demo_table[] = {
    { "textdemo", TextDemoCreateScene, TextDemoStep },
    { "demo1", Demo1CreateScene, Demo1Step },
    { "demo2", Demo2CreateScene, Demo2Step },
    { "demo4", Demo4CreateScene, Demo4Step },
    { "demo4b", Demo4bCreateScene, Demo4bStep },
    { "demo5", Demo5CreateScene, Demo5Step },
    { "demo6", Demo5CreateScene, Demo6Step },
    { "demo7", Demo7CreateScene, Demo7Step },
    { "demo8", Demo8CreateScene, Demo8Step },
    { "demo9", Demo9CreateScene, Demo9Step },
    { "demo10", Demo10CreateScene, Demo10Step },
    { "demo11", Demo11CreateScene, Demo11Step },
    { "demo4c", Demo4cCreateScene, Demo4cStep },
};

#define NU_DEMOS (sizeof(demo_table) / sizeof(demo_table[0]))

static int demo_index;

void DemoApplication::StepBeforeRender(double demo_time) {
    demo_table[demo_index].Step(scene, demo_time);
}

void DemoApplication::StepBeforePhysics(double demo_time) {
    if (demo_index == 3)
        Demo4StepBeforePhysics(scene, demo_time);  
}

int main(int argc, char **argv) {
    if (argc == 1) {
        sreSelectBackend(SRE_BACKEND_DEFAULT);
        const char *text1 =
            "Option --shadow-volumes enables stencil shadows at start-up.\n"
#ifdef OPENGL
            "Option --shadow-mapping enables shadow mapping at start-up.\n"
#endif
            "Option --no-shadows disables shadows at start-up.\n"
            "Option --multiple-lights enables multiple lights in the scene. Implies\n"
            "--multi-pass.\n"
            "Option --single-light limits the scene to one light.\n"
            "Option --multi-pass enables multi-pass rendering (for multiple lights).\n"
            "Option --single-pass disables multi-pass rendering.\n"
            "Option --benchmark makes the application quit automatically after 20s of\n"
            "rendering, displaying the number of frames per second.\n"
            "Option --preprocess performs T-junction elimination on all static scenery at start-up.\n"
            "Option --demand-load-shaders enables demand-loading of shaders (experimental).\n";
        const char *text2;
        if (strcmp(sre_internal_backend->name, "GLFW") == 0)
            text2 = 
                "Option --full-screen enables full-screen mode (GLFW only). Not recommended, \n"
                "changes video mode and is not perfect. Better to maximize the window and use\n"
                "mouse panning (press F).\n";
        else
            text2 = "";
        const char *text3 =
            "Options --debug1, --debug2 and --debug3 set the SRE libary debug message level to\n"
            "1, 2 or 3 (default 0).\n"
            "demo1 shows a scene with textured, bump-mapped blocks and fluid animation\n"
            "(on OpenGL3). A large number of colored spotlights are present with OpenGL.\n"
            "demo2 shows a large scene with numerous point lights scattered across the landscape.\n"
            "demo4 is an advanced Earth terrain demo. Only works with OpenGL\n"
            "and requires large texture data files.\n"
            "demo5 shows a torus landscape with some ramps and a ball to push and multiple lights.\n"
            "demo6 is a circumnavigating view of the demo5 scene.\n"
            "demo7 shows a simple scene with a grating to test stencil shadows.\n"
            "demo8 has a large scene with numerous point lights and moving spheres in a central field.\n"
            "demo10 is a simple scene optimized for OpenGL ES2.0 with geometric objects\n"
            "that can be moved.\n"
            "texturememorytest reports the number of textures that can be loaded until video memory is exhausted.\n"
            "texturememorytestcompressed reports the number of compressed textures that can be loaded.\n"
            "texturetest is a performance test for uncompressed textures.\n"
            "texturetestcompressed is a performance test for compressed textures.\n"
            "game is a simple game where a ball has to be pushed to the ground. This demo\n"
            "hasn't been updated for a while and may result in errors on some platforms.\n"
            "\n";
         const char *text4;
#ifdef OPENGL
         if (true)
#else
         if (strcmp(sre_internal_backend->name, "GLES2_X11") == 0)
#endif
             text4 =
                "Keyboard < is pan left and > is pan right, H is pan up and N is pan down.\n"
                "Keyboard / is jump.\n"
                "Press keypad + and - to zoom in/out.\n"
                "Press A to accelerate, Z to decelerate, left mouse button is jump.\n"
                "Press M to toggle mouse panning.\n"
                "Press Q to quit.\n"
                "F1 brings up a menu with advanced rendering options.\n";
         else
             text4 =
                "Press Ctrl-C to quit. Mouse panning is enabled by default, the left mouse button is accelerate, "
                "the right mouse button is reverse and the middle mouse button is jump.\n";
        const char *text5;
        if (strcmp(sre_internal_backend->name, "GLES2_X11") == 0 ||
        strcmp(sre_internal_backend->name, "GL_X11") == 0 ||
        strcmp(sre_internal_backend->name, "GL_FREEGLUT") == 0)
            text5 = "Press F to toggle full-screen mode.\n";
        else
            text5 = "";
            ;
#ifdef OPENGL_ES2
        printf("OpenGL-ES 2.0 demo ");
#endif
#ifdef OPENGL
        printf("OpenGL 3.0+ demo ");
#endif
        printf("using %s backend.\nUsage: sre-demo <options> demoname\n%s%s%s%s%s\n",
            sre_internal_backend->name, text1, text2, text3, text4, text5);

        exit(0);
    }
 
    sreApplication *app = new DemoApplication;
    sreInitializeApplication(app, &argc, &argv);

    demo_index = - 1;
    if (argc > 1)
        for (int i = 0; i < NU_DEMOS; i++) {
            if (strcmp(argv[1], demo_table[i].name) == 0) {
                demo_index = i;
                break;
            }
        }
    else {
        sreMessage(SRE_MESSAGE_INFO, "No demo name specified.");
        sreFinalizeApplication(app);
        exit(1);
    }
    if (demo_index < 0 || demo_index >= NU_DEMOS) {
        sreMessage(SRE_MESSAGE_INFO, "Invalid demo name.");
        sreFinalizeApplication(app);
        exit(1);
    }
    
    if (demo_index == 6 || demo_index == 0) {
        app->view->SetViewModeLookAt(Point3D(0, - 60.0, 40.0), Point3D(0, 140.0, 0),
            Vector3D(0, 0, 1.0));
        app->view->SetMovementMode(SRE_MOVEMENT_MODE_NONE);
        app->SetFlags(app->GetFlags() | SRE_APPLICATION_FLAG_NO_PHYSICS);
    }
    else {
        // Set the view used by most demos.
        app->view->SetViewModeFollowObject(0, 40.0, Vector3D(0, 0, 10.0));
        app->view->SetMovementMode(SRE_MOVEMENT_MODE_STANDARD);
    }
    // Normally, app->SetFlags() should be called in the demo-specific CreateScene()
    // function when necessary to set set flags (presence of physics, type of gravity
    // etc). 

    demo_table[demo_index].CreateScene(app->scene, app->view);
    // By convention object 0 is the default user-controlled object (usually a ball/sphere).
    app->control_object = 0;

    sreRunApplication(app);
    sreFinalizeApplication(app);
    exit(0);
}
