# Do not edit normally. Configuration settings are in Makefile.conf.


TARGET_MACHINE := $(shell gcc -dumpmachine)

include Makefile.conf

VERSION = 0.4.4
VERSION_MAJOR = 0

CCPLUSPLUS = g++
LINKER_SELECTION_FLAGS = -fuse-ld=gold

DEMO_PROGRAM = sre-demo
ALL_DEMO_PROGRAMS = $(DEMO_PROGRAM) game

# Autodetect platform based on TARGET_MACHINE
ifneq (,$(findstring x86,$(TARGET_MACHINE)))
DETECTED_CPU=X86
CPU_DESCRIPTION="Detected CPU platform: x86"
else
ifneq (,$(findstring arm,$(TARGET_MACHINE)))
DETECTED_CPU=ARM
CPU_DESCRIPTION="Detected CPU platform: ARM"
else
endif
endif

# Detect the back-end when required.
ifeq (,$(DEFAULT_BACKEND))
ifeq ($(DETECTED_CPU), X86)
DEFAULT_BACKEND=GL_X11
BACKEND_DESCRIPTION="Automatically selected back-end: GL_X11 (x86 platform)" 
else ifeq ($(DETECTED_CPU), ARM)
RPI_DETECT := $(shell if [ -d /opt/vc/lib ]; then echo YES; fi)
ifeq ($(RPI_DETECT),YES)
DEFAULT_BACKEND=GLES2_RPI_FB
BACKEND_DESCRIPTION="Automatically selected back-end: GLES2_RPI_FB (Raspberry Pi)"
else
DEFAULT_BACKEND=GLES2_X11
BACKEND_DESCRIPTION="Automatically selected back-end: GLES2_X11 (ARM platform)"
endif
else
DEFAULT_BACKEND=GL_X11
endif
else
BACKEND_DESCRIPTION=Selected back-end $(DEFAULT_BACKEND)
endif
# When SUPPORTED_BACKENDS is empty, assign the default backend.
ifeq (,$(SUPPORTED_BACKENDS))
SUPPORTED_BACKENDS=$(DEFAULT_BACKEND)
endif

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
ifeq ($(LIBRARY_CONFIGURATION), DEBUG)
LIBRARY_LINK_DEFINES =
endif

# CFLAGS with optional tuning for CPU
ifeq ($(LIBRARY_CONFIGURATION), DEBUG)
OPTCFLAGS = -ggdb
else
OPTCFLAGS = -Ofast -ffast-math
endif
ifeq ($(TARGET_CPU), CORTEX_A7)
OPTCFLAGS += -mcpu=cortex-a7 -mfpu=vfpv4
endif
ifeq ($(TARGET_CPU), CORTEX_A8)
OPTCFLAGS += -mcpu=cortex-a8
endif
ifeq ($(TARGET_CPU), CORTEX_A9)
OPTCFLAGS += -mcpu=cortex-a9
endif

# SIMD configuration (SSE on x86).
ifeq ($(TARGET_SIMD), X86_SSE3)
OPTCFLAGS += -msse3 -DUSE_SSE2 -DUSE_SSE3
else
ifeq ($(TARGET_SIMD), X86_SSE2)
OPTCFLAGS += -msse2 -DUSE_SSE2
endif
endif
ifeq ($(TARGET_SIMD), ARM_NEON)
OPTCFLAGS += -DUSE_ARM_NEON -mneon
endif
ifeq ($(TARGET_SIMD), NONE)
OPTCFLAGS += -DNO_SIMD
endif

CFLAGS = -Wall -pipe $(OPTCFLAGS)

# Select GLFW platform
ifeq ($(DEFAULT_BACKEND), GL_GLFW)
GLFW_PLATFORM = PURE_GLFW
else
ifeq ($(DEFAULT_BACKEND), GL_GLFW_X11)
GLFW_PLATFORM = X11
WINDOW_SYSTEM = X11
else

ifeq ($(DEFAULT_BACKEND), GL_X11)
WINDOW_SYSTEM = X11
else

ifeq ($(DEFAULT_BACKEND), GL_GLUT)
GLUT_PLATFORM = GLUT
else
ifeq ($(DEFAULT_BACKEND), GL_FREEGLUT)
GLUT_PLATFORM = FREEGLUT
else

# Select OPENGL_ES2 platform if applicable
ifeq ($(DEFAULT_BACKEND), GLES2_X11)
OPENGL_ES2 = X11
WINDOW_SYSTEM = X11
else
ifeq ($(DEFAULT_BACKEND), GLES2_ALLWINNER_MALI_FB)
OPENGL_ES2 = ALLWINNER_MALI_FB
else
ifeq ($(DEFAULT_BACKEND), GLES2_RPI_FB)
OPENGL_ES2 = RPI_FB
OPENGL_ES2_PLATFORM = RPI
else
ifeq ($(DEFAULT_BACKEND), GLES2_RPI_FB_WITH_X11)
OPENGL_ES2 = RPI_FB_WITH_X11
OPENGL_ES2_PLATFORM = RPI
WINDOW_SYSTEM = X11
else
$(error Invalid value for DEFAULT_BACKEND)
endif
endif
endif
endif
endif
endif
endif
endif
endif

ifeq ($(GLES2_PLATFORM), RPI)
OPENGL_ES2_PLATFORM = RPI
endif

FRAMEBUFFER_COMMON_MODULE_OBJECTS = CriticalSection.o MouseEventQueue.o linux-fb-ui.o

# Variable for things like extra device-specific include directories etc.
EXTRA_CFLAGS_LIB =
EXTRA_CFLAGS_BACKEND =
LFLAGS_LIBRARY = -lpng
PKG_CONFIG_REQUIREMENTS = datasetturbo
PKG_CONFIG_CFLAGS_LIB = `pkg-config --cflags datasetturbo`
PKG_CONFIG_LIBS_LIB =

ifdef OPENGL_ES2
# OPENGL ES2 platform configuration.
PLATFORM_MODULE_OBJECTS = egl-common.o
DEFINES_GLES2_FEATURES =

ifeq ($(OPENGL_ES2), X11)
# X11 EGL window
DEFINES_OPENGL_ES2 = -DOPENGL_ES2_X11
PLATFORM_MODULE_OBJECTS += egl-x11.o
endif

ifeq ($(OPENGL_ES2), RPI_FB)
# Raspberry Pi with Broadcom VideoCore, console.
OPENGL_ES2_PLATFORM = RPI
PLATFORM_MODULE_OBJECTS += egl-rpi-fb.o
endif
ifeq ($(OPENGL_ES2), RPI_FB_WITH_X11)
# Raspberry Pi with Broadcom VideoCore, with X11 input support.
OPENGL_ES2_PLATFORM = RPI
PLATFORM_MODULE_OBJECTS += egl-rpi-fb-with-x11.o
endif
ifeq ($(OPENGL_ES2_PLATFORM), RPI)
# Raspberry Pi (console or X11)
DEFINES_OPENGL_ES2 = -DOPENGL_ES2_PLATFORM_RPI
DEFINES_GLES2_FEATURES += -DGLES2_GLSL_NO_ARRAY_INDEXING \
-DFLOATING_POINT_TEXT_STRING -DGLES2_GLSL_LIMITED_UNIFORM_INT_PRECISION
endif

ifeq ($(OPENGL_ES2), ALLWINNER_MALI_FB)
# Allwinner A1x/A20 platform with ARM Mali-400, framebuffer.
DEFINES_OPENGL_ES2 = -DOPENGL_ES2_MALI -DOPENGL_ES2_A10 # -DOPENGL_ES2_A10_SCALE
PLATFORM_MODULE_OBJECTS += egl-allwinner-fb.o
endif

DEFINES_LIB = -DOPENGL_ES2 $(DEFINES_OPENGL_ES2) $(DEFINES_GLES2_FEATURES) \
-DNO_HDR -DNO_DEPTH_CLAMP -DNO_SRGB -DNO_DEPTH_BOUNDS \
-DNO_PRIMITIVE_RESTART
DEFINES_DEMO = $(DEFINES_OPENGL_ES2) -DOPENGL_ES2
LFLAGS_DEMO =

# Need console mouse support when not using X11
ifneq ($(OPENGL_ES2), X11)
PLATFORM_MODULE_OBJECTS += $(FRAMEBUFFER_COMMON_MODULE_OBJECTS)
endif

ifeq ($(OPENGL_ES2_PLATFORM), RPI)
# RPi as of 2013 requires specific include paths and libraries for
# console (no pkgconfig configuration available).
EXTRA_CFLAGS_LIB += -I/opt/vc/include/ \
-I/opt/vc/include/interface/vcos/pthreads \
-I/opt/vc/include/interface/vmcs_host/linux

EXTRA_CFLAGS_BACKEND = $(EXTRA_CFLAGS_LIB) 
LFLAGS_DEMO += -L/opt/vc/lib/ -lbcm_host -lvcos -lvchiq_arm
LFLAGS_LIBRARY += -lbcm_host -lvcos -lvchiq_arm
endif

LFLAGS_DEMO += -lGLESv2 -lEGL

else # !defined(OPENGL_ES2)

EXTRA_PKG_CONFIG_DEMO = gl glew
EXTRA_PKG_CONFIG_LIB = gl glew
PKG_CONFIG_REQUIREMENTS += gl glew
DEFINES_LIB = -DOPENGL
DEFINES_LIB += -DUSE_SHADOW_SAMPLER #-DCUBE_MAP_STORES_DISTANCE
#DNO_SHADOW_MAP -DNO_HDR -DNO_SRGB -DNO_DEPTH_CLAMP -DNO_MULTI_SAMPLE
DEFINES_DEMO = -DOPENGL #-DNO_LARGE_TEXTURES
LFLAGS_DEMO =
PLATFORM_MODULE_OBJECTS =
LFLAGS_GLUT =

# Raw X11 platform with OpenGL 3+
ifneq (,$(findstring GL_X11,$(SUPPORTED_BACKENDS)))
LFLAGS_GLUT = -lglut
LFLAGS_LIBRARY += -lX11
PLATFORM_MODULE_OBJECTS += opengl-x11.o
WINDOW_SYSTEM = X11
endif

# GLFW platform configuration.
ifneq (,$(findstring GL_GLFW,$(SUPPORTED_BACKENDS)))
LFLAGS_DEMO += -lglfw
LFLAGS_LIBRARY += -lglfw
PKG_CONFIG_REQUIREMENTS += libglfw
PLATFORM_MODULE_OBJECTS += glfw.o
endif

# GLUT platform configuration.
ifneq (,$(findstring GL_GLUT,$(SUPPORTED_BACKENDS)))
LFLAGS_GLUT = -lglut
PLATFORM_MODULE_OBJECTS += glut.o
endif

# Freeglut platform configuration.
ifneq (,$(findstring GL_FREEGLUT,$(SUPPORTED_BACKENDS)))
LFLAGS_GLUT = -lglut
PLATFORM_MODULE_OBJECTS += glut.o
DEFINES_DEMO += -DOPENGL_FREEGLUT
endif

LFLAGS_DEMO += $(LFLAGS_GLUT)
LFLAGS_LIBRARY += $(FLAGS_GLUT)

endif # !defined(OPENGL_ES2)

DEFINES_BACKEND = -DDEFAULT_BACKEND=SRE_BACKEND_$(DEFAULT_BACKEND)
DEFINES_BACKEND += $(patsubst %,-DINCLUDE_BACKEND_%,$(SUPPORTED_BACKENDS))

# X11 is needed when using X11 (OPENGL_ES2_X11 or GL_X11)
ifeq ($(WINDOW_SYSTEM), X11)
LFLAGS_DEMO += -lX11
PLATFORM_MODULE_OBJECTS += x11-common.o
DEFINES_DEMO += -DX11
endif

# End of device-specific configuration.

# Extra (optional) library source modules that should be included (except
# those that are derived from auto-generated source files such as built-in
# shaders).
EXTRA_LIBRARY_MODULE_OBJECTS = 
# Extra library source modules from auto-generated source file.
EXTRA_GENERATED_SOURCE_LIBRARY_MODULE_OBJECTS =
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
SHADER_SOURCES = gl3_billboard.frag gl3_billboard.vert gl3_halo.frag gl3_HDR_average_lum.frag \
gl3_HDR_average_lum.vert gl3_HDR_log_lum.frag gl3_HDR_log_lum.vert gl3_HDR_lum_history_comparison.frag \
gl3_HDR_lum_history_comparison.vert gl3_HDR_lum_history_storage.frag gl3_HDR_lum_history_storage.vert \
gl3_HDR_tone.frag gl3_HDR_tone.vert gl3_image.frag gl3_image.vert gl3_lighting_pass.frag \
gl3_lighting_pass.vert gl3_shadow_map.frag gl3_shadow_map.vert gl3_shadow_volume.frag \
gl3_shadow_volume.vert gl3_text2.frag gl3_text.frag gl3_text.vert
ifneq ($(SHADER_PATH), NONE)
DEFINES_LIB += -DSHADER_PATH='$(SHADER_PATH)'
endif
ifeq ($(SHADERS_BUILTIN), YES)
DEFINES_LIB += -DSHADERS_BUILTIN
EXTRA_GENERATED_LIBRARY_MODULE_OBJECTS += shaders_builtin.o
endif

ifeq ($(DEBUG_OPENGL), YES)
DEFINES_LIB += -DDEBUG_OPENGL
endif
ifeq ($(DEBUG_RENDER_LOG), YES)
DEFINES_LIB += -DDEBUG_RENDER_LOG
endif

ifeq ($(ASSIMP_SUPPORT), YES)
DEFINES_LIB += -DASSIMP_SUPPORT
EXTRA_LIBRARY_MODULE_OBJECTS += assimp.o
LFLAGS_LIBRARY += -lassimp
LFLAGS_DEMO += -lassimp
PKG_CONFIG_REQUIREMENTS += assimp
endif

ifeq ($(COMPRESS_COLOR_ATTRIBUTE), YES)
DEFINES_LIB += -DCOMPRESS_COLOR_ATTRIBUTE
endif

ifeq ($(USE_REFLECTION_VECTOR_GLES2), YES)
DEFINES_LIB += -DUSE_REFLECTION_VECTOR_GLES2
endif

ifeq ($(ENABLE_DIRECTIONAL_LIGHT_SPILL_OVER_FACTOR), YES)
DEFINES_LIB += -DENABLE_DIRECTIONAL_LIGHT_SPILL_OVER_FACTOR \
	-DDEFAULT_DIRECTIONAL_LIGHT_SPILL_OVER_FACTOR=$(DEFAULT_DIRECTIONAL_LIGHT_SPILL_OVER_FACTOR)
endif

ifneq ($(MULTI_SAMPLE_AA), 4)
DEFINES_DEMO += -DNO_MULTI_SAMPLE
endif

ifeq ($(STENCIL_BUFFER), NO)
DEFINES_DEMO += -DNO_STENCIL_BUFFER
endif

DEFINES_DEMO += $(DEMO_STARTUP_DEFINES)

DEFINES_DEMO += -DWINDOW_WIDTH=$(WINDOW_WIDTH) -DWINDOW_HEIGHT=$(WINDOW_HEIGHT)

# Bullet is supported on all platforms.
ifeq ($(BULLET_PHYSICS), YES)
DEFINES_DEMO += -DUSE_BULLET
PLATFORM_MODULE_OBJECTS += bullet.o
PKG_CONFIG_REQUIREMENTS += bullet
EXTRA_PKG_CONFIG_DEMO += bullet
endif

LFLAGS_DEMO += -ldatasetturbo -lpthread
LIBRARY_FLAGS_DEMO =

# Set final CFLAGS.
ifeq ($(LIBRARY_CONFIGURATION), SHARED)
CFLAGS_LIB = -fPIC -fvisibility=hidden
LFLAGS_DEMO += -lpng
LIBRARY_LFLAGS_DEMO += libsrebackend.a -lsre
else
CFLAGS_LIB =
# libpng dependency is not automatically recognized with static library.
LFLAGS_DEMO += -lpng
# The static/debug library is linked from the build directory.
ifeq ($(LIBRARY_CONFIGURATION), DEBUG)
LIBRARY_LFLAGS_DEMO += libsrebackend_dbg.a libsre_dbg.a 
else
LIBRARY_LFLAGS_DEMO += libsrebackend.a libsre.a
endif
endif
CFLAGS_LIB += $(EXTRA_CFLAGS_LIB) $(CFLAGS) $(DEFINES_LIB) -I.
CFLAGS_DEMO = $(EXTRA_CFLAGS_BACKEND) $(CFLAGS) $(DEFINES_DEMO) $(DEFINES_BACKEND) -I.

# Set pkg-config definitions.
PKG_CONFIG_CFLAGS_DEMO = `pkg-config --cflags datasetturbo $(EXTRA_PKG_CONFIG_DEMO)`
PKG_CONFIG_LIBS_DEMO = `pkg-config --libs datasetturbo $(EXTRA_PKG_CONFIG_DEMO)`

ALL_LFLAGS_DEMO=$(LIBRARY_LFLAGS_DEMO) $(LFLAGS_DEMO) $(PKG_CONFIG_LIBS_DEMO)

CORE_LIBRARY_MODULE_OBJECTS = sre.o draw.o geometry.o read_model_file.o binary_model_file.o \
texture.o shadow.o shadow_bounds.o intersection.o preprocess.o mipmap.o \
frustum.o bounds.o octree.o fluid.o standard_objects.o text.o scene.o lights.o shadowmap.o \
bounding_volume.o shader_matrix.o shader_loading.o vertex_buffer.o \
shader_uniform.o draw_object.o
DEMO_MODULE_OBJECTS = demo_main.o demo1.o demo2.o demo4.o demo4b.o \
demo5.o demo7.o demo8.o demo9.o demo10.o demo11.o demo12.o demo13.o demo14.o textdemo.o
ALL_DEMO_MODULE_OBJECTS = $(DEMO_MODULE_OBJECTS) game.o
ALL_BACKEND_MODULE_OBJECTS = sre_backend.o gui-common.o bullet.o glfw.o opengl-x11.o \
x11-common.o glut.o egl-x11.o egl-common.o egl-allwinner-fb.o egl-rpi-fb.o \
egl-rpi-fb-with-x11.o $(FRAMEBUFFER_COMMON_MODULE_OBJECTS)
ORIGINAL_LIBRARY_MODULE_OBJECTS = $(CORE_LIBRARY_MODULE_OBJECTS) $(EXTRA_LIBRARY_MODULE_OBJECTS)
LIBRARY_MODULE_OBJECTS = $(ORIGINAL_LIBRARY_MODULE_OBJECTS) $(EXTRA_GENERATED_LIBRARY_MODULE_OBJECTS)
BACKEND_MODULE_OBJECTS = sre_backend.o gui-common.o $(PLATFORM_MODULE_OBJECTS)

LIBRARY_HEADER_FILES = sre.h sreBoundingVolume.h sreBackend.h

LIBRARY_PKG_CONFIG_FILE = sre.pc

ifeq ($(LIBRARY_CONFIGURATION), DEBUG)
BACKEND_OBJECT = libsrebackend_dbg.a
else
BACKEND_OBJECT = libsrebackend.a
endif
ifeq ($(LIBRARY_CONFIGURATION), SHARED)
LIBRARY_OBJECT = libsre.so.$(VERSION)
INSTALL_TARGET = install_shared install_backend
LIBRARY_DEPENDENCY = $(BACKEND_OBJECT)
else
ifeq ($(LIBRARY_CONFIGURATION), DEBUG)
LIBRARY_OBJECT = libsre_dbg.a
else
LIBRARY_OBJECT = libsre.a
endif
# install_static also works for debugging library
INSTALL_TARGET = install_static install_backend
LIBRARY_DEPENDENCY = $(LIBRARY_OBJECT) $(BACKEND_OBJECT)
endif

default : library backend

library : echo_config $(LIBRARY_OBJECT)

echo_config :
	@echo $(CPU_DESCRIPTION)
	@echo $(BACKEND_DESCRIPTION)

libsre.so.$(VERSION) : $(LIBRARY_MODULE_OBJECTS)
	$(CCPLUSPLUS) $(LINKER_SELECTION_FLAGS) -shared -Wl,-soname,libsre.so.$(VERSION_MAJOR) -fPIC -o libsre.so.$(VERSION) $(LIBRARY_MODULE_OBJECTS) $(LFLAGS_LIBRARY) -lm -lc
	@echo Run '(sudo) make install to install.'

libsre.a : $(LIBRARY_MODULE_OBJECTS)
	ar r libsre.a $(LIBRARY_MODULE_OBJECTS)
	ranlib libsre.a
	@echo 'Run (sudo) make install to install or make demo to make the'
	@echo 'demo program without installing.'

libsre_dbg.a : $(LIBRARY_MODULE_OBJECTS)
	ar r libsre_dbg.a $(LIBRARY_MODULE_OBJECTS)
	ranlib libsre_dbg.a
	@echo 'Make demo to make the demo program without installing.'
	@echo 'Both library and demo are compiled with debugging enabled.'

backend : $(BACKEND_OBJECT)

libsrebackend.a : $(BACKEND_MODULE_OBJECTS)
	ar r libsrebackend.a $(BACKEND_MODULE_OBJECTS)
	ranlib libsrebackend.a
	@echo 'Run (sudo) make install to install or make demo to make the'
	@echo 'demo program without installing.'

libsrebackend_dbg.a : $(BACKEND_MODULE_OBJECTS)
	ar r libsrebackend_dbg.a $(BACKEND_MODULE_OBJECTS)
	ranlib libsrebackend_dbg.a
	@echo 'Make demo to make the demo program without installing.'
	@echo 'Both library and backend are compiled with debugging enabled.'

install : $(INSTALL_TARGET) install_headers install_pkgconfig

install_headers : $(LIBRARY_HEADER_FILES)
	@for x in $(LIBRARY_HEADER_FILES); do \
	echo Installing $$x.; \
	install -m 0644 $$x $(HEADER_FILE_INSTALL_DIR)/$$x; done

install_shared : $(LIBRARY_OBJECT)
	install -m 0644 $(LIBRARY_OBJECT) $(SHARED_LIB_INSTALL_DIR)/$(LIBRARY_OBJECT)
	ln -sf $(SHARED_LIB_INSTALL_DIR)/$(LIBRARY_OBJECT) $(SHARED_LIB_INSTALL_DIR)/libsre.so
	ln -sf $(SHARED_LIB_INSTALL_DIR)/$(LIBRARY_OBJECT) $(SHARED_LIB_INSTALL_DIR)/libsre.so.$(VERSION_MAJOR)
	@echo Run make demo to make the demo program.

install_static : $(LIBRARY_OBJECT)
	install -m 0644 $(LIBRARY_OBJECT) $(STATIC_LIB_INSTALL_DIR)/$(LIBRARY_OBJECT)

install_pkgconfig : $(LIBRARY_PKG_CONFIG_FILE)
	install -m 0644 sre.pc $(PKG_CONFIG_INSTALL_DIR)/pkgconfig/$(LIBRARY_PKG_CONFIG_FILE) 

install_backend : $(BACKEND_OBJECT)
	install -m 0644 $(BACKEND_OBJECT) $(STATIC_LIB_INSTALL_DIR)/$(BACKEND_OBJECT)
	@echo Run make demo to make the demo program.

demo : $(ALL_DEMO_PROGRAMS)

$(DEMO_PROGRAM) : $(LIBRARY_DEPENDENCY) $(BACKEND_OBJECT) $(DEMO_MODULE_OBJECTS)
	$(CCPLUSPLUS) $(LINKER_SELECTION_FLAGS) $(DEMO_MODULE_OBJECTS) -o $(DEMO_PROGRAM) $(ALL_LFLAGS_DEMO)

game : $(LIBRARY_DEPENDENCY) $(BACKEND_OBJECT) game.o
	$(CCPLUSPLUS) $(LINKER_SELECTION_FLAGS) game.o -o game $(ALL_LFLAGS_DEMO)

$(LIBRARY_PKG_CONFIG_FILE) : Makefile.conf Makefile
	@echo Generating sre.pc.
	@echo Name: sre > sre.pc
	@echo Description: SRE real-time rendering engine >> sre.pc
	@echo Requires: $(PKG_CONFIG_REQUIREMENTS) >> sre.pc
	@echo Version: $(VERSION) >> sre.pc
	@echo Libs:  -lsrebackend -lsre $(LFLAGS_DEMO) $(PKG_CONFIG_LIBS_DEMO) >> sre.pc
	@echo Cflags: >> sre.pc

shaders_builtin.cpp : $(SHADER_SOURCES)
	@echo Generating shaders_builtin.cpp
	@echo // libsre v$(VERSION) shaders, `date --rfc-3339='date'` > shaders_builtin.cpp
	@echo // Automatically generated.\\n >> shaders_builtin.cpp
	@echo '#include <math.h>' >> shaders_builtin.cpp
	@echo '#include "sre.h"\n#include "shader.h"\n' >> shaders_builtin.cpp
	@echo -n 'int sre_nu_builtin_shader_sources = ' >> shaders_builtin.cpp
	@echo $(SHADER_SOURCES) | wc -w | awk '{ print($$0 ";") }' >> shaders_builtin.cpp
	@echo '\n'const sreBuiltinShaderTable sre_builtin_shader_table[] = { >> shaders_builtin.cpp
	@for x in $(SHADER_SOURCES); do \
	echo { '"'$$x'"', >> shaders_builtin.cpp; \
	cat $$x | sed ':l1 s/\"/@R@/; tl1; :l2 s/@R@/\\\"/; tl2' | \
	awk '{ print("\"" $$0 "\\n\"") }' >> shaders_builtin.cpp; \
	echo '},' >> shaders_builtin.cpp; done; echo '};' >> shaders_builtin.cpp

clean :
	rm -f $(LIBRARY_MODULE_OBJECTS)
	rm -f $(DEMO_MODULE_OBJECTS) $(ALL_BACKEND_MODULE_OBJECTS)
	rm -f libsre.so.$(VERSION)
	rm -f libsre.a
	rm -f libsre_dbg.a
	rm -f libsrebackend.a
	rm -f sre.pc
	rm -f $(DEMO_PROGRAM)
	rm -f game

cleanall : clean
	rm -f .rules
	rm -f .depend

rules :
	rm -f .rules
	make .rules

.rules : Makefile.conf Makefile
	@echo Generating .rules.
	@rm -f .rules
	@# Create rules to compile library modules.
	@for x in $(LIBRARY_MODULE_OBJECTS); do \
	echo $$x : >> .rules; \
	SOURCEFILE=`echo $$x | sed s/\\\.o/\.cpp/`; \
	echo \\t$(CCPLUSPLUS) -c '$$(CFLAGS_LIB)' "$$SOURCEFILE" \
	-o $$x '$$(PKG_CONFIG_CFLAGS_LIB)' >> .rules; \
	done
	@# Create rules to compile demo/back-end modules.
	@for x in $(ALL_DEMO_MODULE_OBJECTS) $(ALL_BACKEND_MODULE_OBJECTS); do \
	echo $$x : >> .rules; \
	SOURCEFILE=`echo $$x | sed s/\\\.o/\.cpp/`; \
	echo \\t$(CCPLUSPLUS) -c '$$(CFLAGS_DEMO)' "$$SOURCEFILE" \
	-o $$x '$$(PKG_CONFIG_CFLAGS_DEMO)' >> .rules; \
	done

dep :
	rm -f .depend
	make .depend

.depend: Makefile.conf Makefile
	@echo Generating .depend.
	@# Do not include shaders_builtin.cpp yet because creates dependency problems.
	@$(CCPLUSPLUS) -MM $(patsubst %.o,%.cpp,$(ORIGINAL_LIBRARY_MODULE_OBJECTS)) $(PKG_CONFIG_CFLAGS_LIB) >> .depend
        # Make sure Makefile.conf is a dependency for all modules.
	@for x in $(ORIGINAL_LIBRARY_MODULE_OBJECTS); do \
	echo $$x : Makefile.conf >> .depend; done
	@echo '# Module dependencies' >> .depend
	@$(CCPLUSPLUS) -MM $(patsubst %.o,%.cpp,$(ALL_DEMO_MODULE_OBJECTS) \
	$(BACKEND_MODULE_OBJECTS)) $(PKG_CONFIG_CFLAGS_DEMO) >> .depend
	@# Make sure Makefile.conf is a dependency for all modules.
	@for x in $(ALL_DEMO_MODULE_OBJECTS) $(BACKEND_MODULE_OBJECTS); do \
	echo $$x : Makefile.conf >> .depend; done
	@# Add dependencies for builtin shaders.
	@echo shaders_builtin.cpp : $(SHADER_SOURCES) >> .depend
	@echo shaders_builtin.o : shaders_builtin.cpp sre.h shader.h >> .depend

.autodetect :
	@echo Autodetecting platform
	@echo "TARGET_MACHINE="`gcc -dumpmachine` >.autodetect

include .rules
include .depend
