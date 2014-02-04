# Do not edit normally. Configuration settings are in Makefile.conf.

include Makefile.conf

VERSION = 0.1
VERSION_MAJOR = 0

DEMO_PROGRAM = sre-demo

ifeq ($(SHADOWS), SHADOW_VOLUMES)
DEMO_STARTUP_DEFINES = -DSHADOW_VOLUMES
endif
ifeq ($(SHADOWS), SHADOW_MAPPING)
DEMO_STARTUP_DEFINES = -DSHADOW_MAPPING
endif
ifeq ($(SHADOWS), NONE)
DEMO_STARTUP_DEFINES = -DNO_SHADOWS
endif

ifeq ($(MULTIPLE_LIGHTS), YES)
DEMO_STARTUP_DEFINES += -DMULTIPLE_LIGHTS_ENABLED
endif
ifeq ($(MULTIPLE_LIGHTS), NO)
DEMO_STARTUP_DEFINES += -DMULTIPLE_LIGHTS_DISABLED
endif

# Definitions for static or shared (dynamic) library.
ifeq ($(LIBRARY_CONFIGURATION), STATIC)
LIBRARY_LINK_DEFINES =
endif
ifeq ($(LIBRARY_CONFIGURATION), SHARED)
LIBRARY_LINK_DEFINES = -DSRE_DLL -DSRE_DLL_EXPORTS
endif
ifeq ($(LIBRARY_CONFIGYURATION), DEBUG)
LIBRARY_LINK_DEFINES =
endif

# CFLAGS with optional tuning for CPU
OPTCFLAGS = -Ofast -ffast-math
ifeq ($(TARGET_CPU), CORTEX_A7)
OPTCFLAGS += -mcpu=cortex-a7 -mfpu=vfpv4
endif
ifeq ($(TARGET_CPU), CORTEX_A8)
OPTCFLAGS += -mcpu=cortex-a8
endif
ifeq ($(TARGET_CPU), CORTEX_A9)
OPTCFLAGS += -mcpu=cortex-a9
endif
ifeq ($(LIBRARY_CONFIGURATION), DEBUG)
OPTCFLAGS = -ggdb
endif
CFLAGS = -Wall -pipe $(OPTCFLAGS)

# Select GLFW platform
ifeq ($(TARGET), GL_GLFW)
GLFW_PLATFORM = PURE_GLFW
endif
ifeq ($(TARGET), GL_GLFW_X11)
GLFW_PLATFORM = X11
WINDOW_SYSTEM = X11
endif

ifeq ($(TARGET), GL_X11)
WINDOW_SYSTEM = X11
endif

ifeq ($(TARGET), GL_GLUT)
GLUT_PLATFORM = GLUT
endif
ifeq ($(TARGET), GL_FREEGLUT)
GLUT_PLATFORM = FREEGLUT
endif

# Select OPENGL_ES2 platform if applicable
ifeq ($(TARGET), GLES2_X11)
OPENGL_ES2 = X11
WINDOW_SYSTEM = X11
endif
ifeq ($(TARGET), GLES2_ALLWINNER_MALI_FB)
OPENGL_ES2 = ALLWINNER_MALI_FB
endif
ifeq ($(TARGET) ,GLES2_RPI_FB)
OPENGL_ES2 = RPI_FB
endif

FRAMEBUFFER_COMMON_MODULE_OBJECTS = CriticalSection.o MouseEventQueue.o linux-fb-ui.o

# Raw X11 platform with OpenGL 3+
ifeq ($(TARGET), GL_X11)
DEFINES_LIB = -DOPENGL #-DNO_SHADOW_MAP -DNO_HDR -DNO_SRGB -DNO_DEPTH_CLAMP -DNO_MULTI_SAMPLE
DEFINES_DEMO = -DOPENGL -DOPENGL_X11 #-DNO_LARGE_TEXTURES
EXTRA_PKG_CONFIG_DEMO = gl glew
EXTRA_PKG_CONFIG_LIB = gl glew
LFLAGS_DEMO = -lglut
PLATFORM_MODULE_OBJECTS = opengl-x11.o
endif

ifdef OPENGL_ES2
# OPENGL ES2 platform configuration.
PLATFORM_MODULE_OBJECTS = egl-common.o

ifeq ($(OPENGL_ES2), X11)
# X11 EGL window
DEFINES_OPENGL_ES2 = -DOPENGL_ES2_X11
PLATFORM_MODULE_OBJECTS += egl-x11.o
endif
ifeq ($(OPENGL_ES2), RPI_FB)
# Raspberry Pi with Broadcom VideoCore.
DEFINES_OPENGL_ES2 = -DOPENGL_ES2_RPI
PLATFORM_MODULE_OBJECTS += egl-rpi-fb.o
endif
ifeq ($(OPENGL_ES2), ALLWINNER_MALI_FB)
# Allwinner A1x/A20 platform with ARM Mali-400, framebuffer.
DEFINES_OPENGL_ES2 = -DOPENGL_ES2_MALI -DOPENGL_ES2_A10 # -DOPENGL_ES2_A10_SCALE
PLATFORM_MODULE_OBJECTS += egl-allwinner-fb.o
endif

DEFINES_LIB = -DOPENGL_ES2 -DNO_SHADOW_MAP -DNO_HDR -DNO_DEPTH_CLAMP -DNO_SRGB -DNO_DEPTH_BOUNDS
DEFINES_DEMO = $(DEFINES_OPENGL_ES2) -DOPENGL_ES2
LFLAGS_DEMO = -lGLESv2 -lEGL
PKG_CONFIG_CFLAGS_LIB =
PKG_CONFIG_LIBS_LIB =

# Need console mouse support when not using X11
ifneq ($(OPENGL_ES2), X11)
PLATFORM_MODULE_OBJECTS += $(FRAMEBUFFER_COMMON_MODULE_OBJECTS)
endif

endif # defined(OPENGL_ES2)

# GLFW platform configuration.
ifdef GLFW_PLATFORM
DEFINES_LIB = -DOPENGL #-DNO_SHADOW_MAP -DNO_HDR -DNO_SRGB -DNO_DEPTH_CLAMP -DNO_MULTI_SAMPLE
DEFINES_DEMO = -DOPENGL -DOPENGL_GLFW #-DNO_LARGE_TEXTURES
EXTRA_PKG_CONFIG_DEMO = gl glew
EXTRA_PKG_CONFIG_LIB = gl glew
LFLAGS_DEMO = -lglfw
PLATFORM_MODULE_OBJECTS = glfw.o
endif

# GLUT platform configuration.
ifdef GLUT_PLATFORM
DEFINES_LIB = -DOPENGL #-DNO_SHADOW_MAP -DNO_HDR -DNO_SRGB -DNO_DEPTH_CLAMP -DNO_MULTI_SAMPLE
DEFINES_DEMO = -DOPENGL #-DNO_LARGE_TEXTURES
ifeq ($(GLUT_PLATFORM), GLUT)
DEFINES_DEMO += -DOPENGL_GLUT
else
DEFINES_DEMO += -DOPENGL_FREEGLUT
endif
EXTRA_PKG_CONFIG_DEMO = gl glew
EXTRA_PKG_CONFIG_LIB = gl glew
LFLAGS_DEMO = -lglut
PLATFORM_MODULE_OBJECTS = glut.o
endif

# X11 is needed when using X11 (OPENGL_ES2_X11, GL_GLFW_X11, or GL_X11)
ifeq ($(WINDOW_SYSTEM), X11)
LFLAGS_DEMO += -lX11
PLATFORM_MODULE_OBJECTS += x11-common.o
DEFINES_DEMO += -DX11
endif

# End of device-specific configuration.

EXTRA_LIBRARY_MODULE_OBJECTS =
DEFINES_LIB += $(LIBRARY_LINK_DEFINES)

ifeq ($(SPLASH_SCREEN), LOGO)
DEFINES_LIB += -DSPLASH_SCREEN_LOGO
endif
ifeq ($(SPLASH_SCREEN), BLACK)
DEFINES_LIB += -DSPLASH_SCREEN_BLACK
endif
ifeq ($(SPLASH_SCREEN), NONE)
DEFINES_LIB += -DSPLASH_SCREEN_NONE
endif

ifdef SHADER_LOADING_MASK
DEFINES_LIB += -DSHADER_LOADING_MASK=$(SHADER_LOADING_MASK)
endif

# Shader source configuration.
ifneq ($(SHADER_PATH), NONE)
DEFINES_LIB += -DSHADER_PATH='$(SHADER_PATH)'
endif
ifeq ($(SHADERS_BUILTIN), YES)
DEFINES_LIB += -DSHADERS_BUILTIN
EXTRA_LIBRARY_MODULE_OBJECTS += shaders_builtin.o
SHADERS_BUILTIN_SCRIPT = `ls -1 *.vert *.frag`
endif

ifeq ($(DEBUG_OPENGL), YES)
DEFINES_LIB += -DDEBUG_OPENGL
endif
ifeq ($(DEBUG_RENDER_LOG), YES)
DEFINES_LIB += -DDEBUG_RENDER_LOG
endif

ifneq ($(MULTI_SAMPLE_AA), 4)
DEFINES_DEMO += -DNO_MULTI_SAMPLE
endif

DEFINES_DEMO += $(DEMO_STARTUP_DEFINES)

DEFINES_DEMO += -DWINDOW_WIDTH=$(WINDOW_WIDTH) \
-DWINDOW_HEIGHT=$(WINDOW_HEIGHT)

# Bullet is supported on all platforms.
ifeq ($(BULLET_PHYSICS), YES)
DEFINES_DEMO += -DUSE_BULLET
PLATFORM_MODULE_OBJECTS += bullet.o
endif

LFLAGS_DEMO += -lpthread

# Set final CFLAGS.
ifeq ($(LIBRARY_CONFIGURATION), SHARED)
CFLAGS_LIB = -fPIC -fvisibility=hidden
LFLAGS_DEMO += -lsre
else
CFLAGS_LIB =
# libpng dependency is not automatically recognized with static library.
LFLAGS_DEMO += -lpng
ifeq ($(LIBRARY_CONFIGURATION), DEBUG)
LFLAGS_DEMO += libsre_dbg.a
else
LFLAGS_DEMO += libsre.a
endif
endif
CFLAGS_LIB += $(CFLAGS) $(DEFINES_LIB) -I.
CFLAGS_DEMO = $(CFLAGS) $(DEFINES_DEMO) -I.

# Set pkg-config definitions.
PKG_CONFIG_CFLAGS_DEMO = `pkg-config --cflags bullet $(EXTRA_PKG_CONFIG_DEMO)`
PKG_CONFIG_LIBS_DEMO = `pkg-config --libs bullet $(EXTRA_PKG_CONFIG_DEMO)`

CORE_LIBRARY_MODULE_OBJECTS = sre.o draw.o geometry.o readobjfile.o texture.o shadow.o MatrixClasses.o \
frustum.o bounds.o octree.o fluid.o standard_objects.o text.o scene.o lights.o shadowmap.o \
bounding_volume.o random.o shader_matrix.o shader_loading.o vertex_buffer.o shader_uniform.o \
draw_object.o
CORE_DEMO_MODULE_OBJECTS = gui-common.o main.o demo1.o demo2.o demo4.o \
demo5.o demo7.o demo8.o game.o texture_test.o demo10.o demo11.o textdemo.o
ALL_PLATFORM_MODULE_OBJECTS = bullet.o glfw.o opengl-x11.o x11-common.o glut.o egl-x11.o egl-common.o \
egl-allwinner-fb.o egl-rpi-fb.o $(FRAMEBUFFER_COMMON_MODULE_OBJECTS)
LIBRARY_MODULE_OBJECTS = $(CORE_LIBRARY_MODULE_OBJECTS) $(EXTRA_LIBRARY_MODULE_OBJECTS)
DEMO_MODULE_OBJECTS = $(PLATFORM_MODULE_OBJECTS) $(CORE_DEMO_MODULE_OBJECTS)

ifeq ($(LIBRARY_CONFIGURATION), SHARED)
LIBRARY_OBJECT = libsre.so.$(VERSION)
INSTALL_TARGET = install_shared
LIBRARY_DEPENDENCY = $(SHARED_LIB_DIR)/$(LIBRARY_OBJECT)
else
ifeq ($(LIBRARY_CONFIGURATION), DEBUG)
LIBRARY_OBJECT = libsre_dbg.a
else
LIBRARY_OBJECT = libsre.a
endif
# install_static also works for debugging library
INSTALL_TARGET = install_static
LIBRARY_DEPENDENCY = $(LIBRARY_OBJECT)
endif

default : $(LIBRARY_OBJECT)

library : $(LIBRARY_OBJECT)

libsre.so.$(VERSION) : $(LIBRARY_MODULE_OBJECTS)
	g++ -shared -fPIC -o libsre.so.$(VERSION) $(LIBRARY_MODULE_OBJECTS) \
-lc -lm -lpng $(PKG_CONFIG_LIBS_LIB)
	@echo Run '(sudo) make install to install.'

libsre.a : $(LIBRARY_MODULE_OBJECTS)
	ar r libsre.a $(LIBRARY_MODULE_OBJECTS)
	@echo 'Run (sudo) make install to install or make demo to make the'
	@echo 'demo program without installing.'

libsre_dbg.a : $(LIBRARY_MODULE_OBJECTS)
	ar r libsre_dbg.a $(LIBRARY_MODULE_OBJECTS)
	@echo 'Make demo to make the demo program without installing.'
	@echo 'Both library and demo are compiled with debugging enabled.'

install : $(INSTALL_TARGET) install_header

install_header : sre.h
	install -m 0644 sre.h $(INCLUDE_DIR)/sre.h

install_shared : $(LIBRARY_OBJECT)
	install -m 0644 $(LIBRARY_OBJECT) $(SHARED_LIB_DIR)/$(LIBRARY_OBJECT)
	ln -sf $(SHARED_LIB_DIR)/$(LIBRARY_OBJECT) $(SHARED_LIB_DIR)/libsre.so
	ln -sf $(SHARED_LIB_DIR)/$(LIBRARY_OBJECT) $(SHARED_LIB_DIR)/libsre.so.$(VERSION_MAJOR)
	@echo Run make demo to make the demo program.

install_static : $(LIBRARY_OBJECT) sre.h
	install -m 0644 $(LIBRARY_OBJECT) $(STATIC_LIB_DIR)/$(LIBRARY_OBJECT)
	@echo Run make demo to make the demo program.

demo : $(DEMO_PROGRAM)

$(DEMO_PROGRAM) :  $(LIBRARY_DEPENDENCY) $(DEMO_MODULE_OBJECTS)
	g++ $(DEMO_MODULE_OBJECTS) -o $(DEMO_PROGRAM) $(PKG_CONFIG_LIBS_DEMO) $(LFLAGS_DEMO)

shaders_builtin.cpp :
	echo // libsre v$(VERSION) shaders, `date --rfc-3339='date'` > shaders_builtin.cpp
	echo // Automatically generated.\\n >> shaders_builtin.cpp
	echo '#include <math.h>' >> shaders_builtin.cpp
	echo '#include "sre.h"\n#include "shader.h"\n' >> shaders_builtin.cpp
	echo -n 'int sre_nu_builtin_shader_sources = ' >> shaders_builtin.cpp
	echo $(SHADERS_BUILTIN_SCRIPT) | wc -w | awk '{ print($$0 ";") }' >> shaders_builtin.cpp
	echo '\n'const sreBuiltinShaderTable sre_builtin_shader_table[] = { >> shaders_builtin.cpp
	for x in $(SHADERS_BUILTIN_SCRIPT); do \
	echo { '"'$$x'"', >> shaders_builtin.cpp; \
	cat $$x | sed ':l1 s/\"/@R@/; tl1; :l2 s/@R@/\\\"/; tl2' | \
	awk '{ print("\"" $$0 "\\n\"") }' >> shaders_builtin.cpp; \
	echo '},' >> shaders_builtin.cpp; done; echo '};' >> shaders_builtin.cpp

clean :
	rm -f $(LIBRARY_MODULE_OBJECTS)
	rm -f $(CORE_DEMO_MODULE_OBJECTS) $(ALL_PLATFORM_MODULE_OBJECTS)
	rm -f libsre.so.$(VERSION)
	rm -f libsre.a
	rm -f libsre_dbg.a
	rm -f $(DEMO_PROGRAM)

cleanall : clean
	rm -f .rules
	rm -f .depend

rules :
	rm -f .rules
	make .rules

.rules :
	# Create rules to compile library modules.
	for x in $(LIBRARY_MODULE_OBJECTS); do \
	echo $$x : >> .rules; \
	SOURCEFILE=`echo $$x | sed s/\\\.o/\.cpp/`; \
	echo \\tg++ -c '$$(CFLAGS_LIB)' "$$SOURCEFILE" \
	-o $$x '$$(PKG_CONFIG_CFLAGS_LIB)' >> .rules; \
	done
	# Create rules to compile demo/back-end modules.
	for x in $(CORE_DEMO_MODULE_OBJECTS) $(ALL_PLATFORM_MODULE_OBJECTS); do \
	echo $$x : >> .rules; \
	SOURCEFILE=`echo $$x | sed s/\\\.o/\.cpp/`; \
	echo \\tg++ -c '$$(CFLAGS_DEMO)' "$$SOURCEFILE" \
	-o $$x '$$(PKG_CONFIG_CFLAGS_DEMO)' >> .rules; \
	done

dep :
	rm -f .depend
	make .depend

.depend:
	g++ -MM $(patsubst %.o,%.cpp,$(LIBRARY_MODULE_OBJECTS)) >> .depend
        # Make sure Makefile.conf is a dependency for all modules.
	for x in $(LIBRARY_MODULE_OBJECTS); do \
	echo $$x : Makefile.conf >> .depend; done
	echo '# Module dependencies' >> .depend
	g++ -MM $(patsubst %.o,%.cpp,$(DEMO_MODULE_OBJECTS)) >> .depend
        # Make sure Makefile.conf is a dependency for all modules.
	for x in $(DEMO_MODULE_OBJECTS); do \
	echo $$x : Makefile.conf >> .depend; done
	# Add dependencies for builtin shaders.
	echo shaders_builtin.cpp : $(SHADERS_BUILTIN_SCRIPT) >> .depend

include .rules
include .depend
