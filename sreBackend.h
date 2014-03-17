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

class sreBackend {
public :
    int index;
    const char *name;
    virtual void Initialize(int *argc, char ***argv, int requested_width, int requested_height,
        int& actual_width, int& actual_height) = 0;
    virtual void Finalize() = 0;
    virtual void GLSwapBuffers() = 0;
    virtual void GLSync() = 0;
    virtual double GetCurrentTime() = 0;
    virtual void ProcessGUIEvents() = 0;
    virtual void ToggleFullScreenMode(int& width, int& height, bool pan_with_mouse) = 0;
    virtual void HideCursor() = 0;
    virtual void RestoreCursor() = 0;
    virtual void WarpCursor(int x, int y) = 0;
};

enum {
    SRE_BACKEND_GL_X11 = 0,
    SRE_BACKEND_GL_FREEGLUT,
    SRE_BACKEND_GL_GLUT,
    SRE_BACKEND_GL_GLFW,
    SRE_BACKEND_GLES2_X11 = 0x100,
    SRE_BACKEND_GLES2_ALLWINNER_MALI_FB = 0x101,
    SRE_BACKEND_GLES2_RPI_FB = 0x102,
    SRE_BACKEND_DEFAULT = 0x1000
};

enum {
    SRE_APPLICATION_FLAG_DISPLAY_FPS = 0x1,
    SRE_APPLICATION_FLAG_NO_GROUND_PLANE = 0x2,
    SRE_APPLICATION_FLAG_NO_GRAVITY = 0x4,
    SRE_APPLICATION_FLAG_DYNAMIC_GRAVITY = 0x8,
    SRE_APPLICATION_FLAG_JUMP_ALLOWED = 0x10,
    // Whether mouse panning is locked.
    SRE_APPLICATION_FLAG_LOCK_PANNING = 0x20,
    // Whether mouse panning is enabled (usually in a windowing environment).
    SRE_APPLICATION_FLAG_PAN_WITH_MOUSE = 0x40,
    SRE_APPLICATION_FLAG_NO_PHYSICS = 0x80
};

class sreApplication {
public :
    unsigned int flags;
    int window_width, window_height;
    Vector2D mouse_sensitivity;
    sreScene *scene;
    sreView *view;
    Point3D gravity_position;
    double start_time;
    bool stop_signalled;
    // Control object.
    int control_object;
    float input_acceleration;
    float horizontal_acceleration;
    float max_horizontal_velocity;
    float hovering_height;
    float hovering_height_acceleration;
    bool jump_requested;
    // Standard text overlay.
    double text_message_time, text_message_timeout;
    int nu_text_message_lines;
    char *text_message[24];

    sreApplication();
    unsigned int GetFlags();
    void SetFlags(unsigned int flags);
    void ApplyControlObjectInputs(double dt);
    virtual void StepBeforeRender(double demo_time) = 0;
    virtual void StepBeforePhysics(double demo_time) = 0;
    virtual void InitializePhysics() = 0;
    virtual void DoPhysics(double previous_time, double current_time) = 0;
    virtual void DestroyPhysics() = 0;
};

class sreNoPhysicsApplication : public sreApplication {
public : 
    virtual void InitializePhysics();
    virtual void DoPhysics(double previous_time, double current_time);
    virtual void StepBeforePhysics(double demo_time);
};

class sreGenericPhysicsApplication : public sreApplication {
public :
    virtual void InitializePhysics();
    virtual void DoPhysics(double previous_time, double current_time);
};

class sreBulletPhysicsApplication : public sreApplication {
public :
    virtual void InitializePhysics();
    virtual void DoPhysics(double previous_time, double current_time);
    virtual void DestroyPhysics();
};

extern sreBackend *sre_internal_backend;
extern sreApplication *sre_internal_application;

SRE_API void sreSelectBackend(int backend);
// Initialize the back-end and create an empty app->scene and default app->view.
SRE_API void sreInitializeApplication(sreApplication *app, int *argc, char ***argv);
// Run the application using the selected back-end.
SRE_API void sreRunApplication(sreApplication *app);
// Run the main loop without destroying the device-specific window.
SRE_API void sreMainLoop(sreApplication *app, unsigned int prepare_flags);
SRE_API void sreBackendGLSwapBuffers();
SRE_API void sreBackendStandardTextOverlay();

