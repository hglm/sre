SRE (Shadow Rendering Engine) is an optimized real-time 3D rendering
engine using OpenGL or OpenGL-ES 2.0 with several device-specific
back-ends. It currently runs on different Linux platforms but is
portable to other platforms.

The current version as of July 2014 works correctly with OpenGL on a PC
platform; the OpenGL-ES 2.0 front-end with X11 (EGL) back-end
also works well although shadow mapping is not supported (stencil shadow
volumes are supported). Certain framebuffer OpenGL-ES 2.0 back-ends are
also supported.

An older version was ported to Windows (32-bit) using GLFW, and other
platforms are feasable too given the seperation into front and back-ends
and support for the OpenGL-ES 2.0 standard as well as cross-platform
GUI back-ends such as GLFW.

Highlights include a large amount of geometrical and scissors rendering
optimizations with an unlimited number of lights and optimized stencil
shadow volume and shadow mapping implementations, although shadow
mapping is currently not supported on the OpenGL-ES 2.0 platform. Also
included is support for HDR rendering and integration with the Bullet
physics library.

The lighting shaders are implemented using a single large shader source
that is conditionally compiled to optimize for various attribute
combinations. Other shaders include HDR, halo, stencil shadow/shadow map
generation, and image/text shaders.

Drawbacks and features that are still missing include lack of a full
scene graph that is seperate from the spatial (octree) hierarchy. Being
basically a (highly optimized) traditional multi-pass lighting engine, it
does not (yet) utilize some other modern rendering techniques, but it does
have fairly efficient and flexible GPU vertex buffer handling.

See the file README for further information.
