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
#include <math.h>
#include <string.h>

#include "sre.h"
#include "sreBackend.h"
#include "gui-common.h"

static bool accelerate_pressed = false;
static bool decelerate_pressed = false;
static bool ascend_pressed = false;
static bool descend_pressed = false;
static bool jump_input_detected = false;
static int menu_mode = 0;
static char short_engine_settings_text[128];
static int text_filtering_mode = SRE_TEXTURE_FILTER_LINEAR;
static float anisotropy = 1.0;
static char anisotropy_text[64];
static int visualized_shadow_map = - 1;

void GUIMouseButtonCallback(int button, int state) {
    if (state == SRE_PRESS) {
        if (sre_internal_application->flags & SRE_APPLICATION_FLAG_NO_GRAVITY) {
            if (button == SRE_MOUSE_BUTTON_LEFT)
                ascend_pressed = true;
            else
            if (button == SRE_MOUSE_BUTTON_RIGHT)
                descend_pressed = true;
        }
        else
        if (button == SRE_MOUSE_BUTTON_LEFT)
            jump_input_detected = true;
    }
    else if (state == SRE_RELEASE) {
        if (sre_internal_application->flags & SRE_APPLICATION_FLAG_NO_GRAVITY) {
            if (button == SRE_MOUSE_BUTTON_LEFT)
                ascend_pressed = false;
            else
            if (button == SRE_MOUSE_BUTTON_RIGHT)
                descend_pressed = false;
        }
    }
}

// In non-keyboard mode, use mouse buttons to accelerate/decelerate.

void GUIMouseButtonCallbackNoKeyboard(int button, int state) {
    if (state == SRE_PRESS && button == SRE_MOUSE_BUTTON_LEFT)
        accelerate_pressed = true;
    if (state == SRE_RELEASE && button == SRE_MOUSE_BUTTON_LEFT)
        accelerate_pressed = false;
    if (state == SRE_PRESS && button == SRE_MOUSE_BUTTON_RIGHT)
        decelerate_pressed = true;
    if (state == SRE_RELEASE && button == SRE_MOUSE_BUTTON_RIGHT)
        decelerate_pressed = false;
    if (state == SRE_PRESS && button == SRE_MOUSE_BUTTON_MIDDLE)
        jump_input_detected = true;
}

static void SetShortEngineSettingsText() {
    sreEngineSettingsInfo *info = sreGetEngineSettingsInfo();
    sprintf(short_engine_settings_text,
        "Current: %s, %s (Press I for more)", info->shadows_description,
        info->scissors_description);
    delete info;
}

static bool scene_info_text_initialized = false;
static char *scene_info_text_line[22];

static const char *no_yes_str[2] = { "No", "Yes" };
static const char *disabled_enabled_str[2] = { "Disabled", "Enabled" };
static const char *opengl_str[2] = { "OpenGL 3.0+ (core)", "OpenGL-ES 2.0" }; 
static const char *visibility_test_str[4] = {
    "None", "Standard", "Darkcap only", "Full (Darkcap)" };

static char *strend(char *s) {
    return s + strlen(s);
}

static void SetSceneInfo(sreScene *scene, sreEngineSettingsInfo *settings_info) {
    sprintf(scene_info_text_line[13],
        "Number of objects: %d (capacity %d), models: %d (capacity %d)",
        scene->nu_objects, scene->max_objects, scene->nu_models, scene->max_models);
    sprintf(scene_info_text_line[14],
        "Visible number of objects:  %d (capacity %d), final pass: %d (%d)",
        scene->nu_visible_objects, scene->max_visible_objects,
        scene->nu_final_pass_objects, scene->max_final_pass_objects);
    sprintf(scene_info_text_line[15],
        "Total number of lights: %d (capacity %d)", scene->nu_lights, scene->max_scene_lights);
    char active_lights_str[16];
    if (settings_info->max_visible_active_lights == SRE_MAX_ACTIVE_LIGHTS_UNLIMITED)
        sprintf(active_lights_str, "Unlimited");
    else
        sprintf(active_lights_str, "%d", settings_info->max_visible_active_lights);
    sprintf(scene_info_text_line[16],
        "Visible lights (frustum): %d (capacity %d), max visible: %s",
        scene->nu_visible_lights, scene->max_visible_lights, active_lights_str);
    if (settings_info->shadows_method == SRE_SHADOWS_SHADOW_VOLUMES) {
        sreShadowRenderingInfo *info = sreGetShadowRenderingInfo();
        sprintf(scene_info_text_line[17], "Shadow volumes rendered: %d, silhouettes calculated: %d",
            info->shadow_volume_count, info->silhouette_count);
        sprintf(scene_info_text_line[18],
            "Shadow object cache hits/misses %d/%d (entries used %d/%d, %d vertices)",
            info->object_cache_hits, info->object_cache_misses,
            info->object_cache_entries_used, info->object_cache_total_entries,
            info->object_cache_total_vertex_count);
        sprintf(scene_info_text_line[19],
            "Shadow model cache hits/misses %d/%d (entries used %d/%d, %d vertices)",
            info->model_cache_hits, info->model_cache_misses,
            info->model_cache_entries_used, info->model_cache_total_entries,
            info->model_cache_total_vertex_count);
        sprintf(scene_info_text_line[20], "Shadow cache: %s, Use strips: %s, Use fans: %s"
            ", Test vis.: %s",
            disabled_enabled_str[
                (settings_info->rendering_flags & SRE_RENDERING_FLAG_SHADOW_CACHE_ENABLED) != 0],
            no_yes_str[
                (settings_info->rendering_flags & SRE_RENDERING_FLAG_USE_TRIANGLE_STRIPS_FOR_SHADOW_VOLUMES) != 0],
            no_yes_str[
                (settings_info->rendering_flags & SRE_RENDERING_FLAG_USE_TRIANGLE_FANS_FOR_SHADOW_VOLUMES) != 0],
            visibility_test_str[
                ((settings_info->rendering_flags & SRE_RENDERING_FLAG_SHADOW_VOLUME_VISIBILITY_TEST) != 0) + 2 *
                ((settings_info->rendering_flags & SRE_RENDERING_FLAG_SHADOW_VOLUME_DARKCAP_VISIBILITY_TEST) != 0)]
            );

        delete info;
    }
    else {
        sprintf(scene_info_text_line[17], "");
        sprintf(scene_info_text_line[18], "");
        sprintf(scene_info_text_line[19], "");
        sprintf(scene_info_text_line[20], "");
    }
    sprintf(scene_info_text_line[21], "");
}

static void SetEngineSettingsInfo(sreEngineSettingsInfo *info) {
    sprintf(scene_info_text_line[0], "SRE v0.2, %s, back-end: %s", opengl_str[info->opengl_version],
        sre_internal_backend->name);
    sprintf(scene_info_text_line[1], "");
    sprintf(scene_info_text_line[2], "Resolution: %dx%d", info->window_width, info->window_height);
    sprintf(scene_info_text_line[3],
        "Multi-pass rendering: %s (press 6 or 7 to change)", no_yes_str[info->multi_pass_rendering]);
    sprintf(scene_info_text_line[4], "Reflection model: %s (4 5)", info->reflection_model_description);
    sprintf(scene_info_text_line[5], "Shadows setting: %s (1 2 3)", info->shadows_description);
    if (info->shadows_method == SRE_SHADOWS_SHADOW_VOLUMES)
        sprintf(strend(scene_info_text_line[5]), ", Shadow cache: %s",
            disabled_enabled_str[(info->rendering_flags & SRE_RENDERING_FLAG_SHADOW_CACHE_ENABLED) != 0]);
    const char *scissors_cache_description;
    if (info->scissors_method == SRE_SCISSORS_GEOMETRY) {
        if (info->rendering_flags & SRE_RENDERING_FLAG_GEOMETRY_SCISSORS_CACHE_ENABLED)
            scissors_cache_description = " Scissors cache enabled";
        else
            scissors_cache_description = " Scissors cache disabled";
    }
    else
        scissors_cache_description = "";
    sprintf(scene_info_text_line[6], "Scissors optimization mode: %s (D S G)%s",
        info->scissors_description, scissors_cache_description);
    sprintf(scene_info_text_line[7], "");
    sprintf(scene_info_text_line[8], "Max texture filtering anisotropy level: %.1f", info->max_anisotropy);
    sprintf(scene_info_text_line[9], "");
    sprintf(scene_info_text_line[10], "HDR rendering: %s (F2 F3)", disabled_enabled_str[info->HDR_enabled]);
    if (info->HDR_enabled)
        sprintf(strend(scene_info_text_line[10]), ", Tone-mapping shader: %s (F4)",
            sreGetToneMappingShaderName(info->HDR_tone_mapping_shader));
    sprintf(scene_info_text_line[11], "");
    sprintf(scene_info_text_line[12], "");
    sprintf(scene_info_text_line[13], "");
}

static sreEngineSettingsInfo *settings_info;

static void SetInfoScreen() {
    if (!scene_info_text_initialized) {
        for (int i = 0; i < 22; i++) {
            scene_info_text_line[i] = new char[128];
            scene_info_text_line[i][0] = '\0';
        }
        scene_info_text_initialized = true;
    }

    settings_info = sreGetEngineSettingsInfo();
    SetEngineSettingsInfo(settings_info);
    SetSceneInfo(sre_internal_application->scene, settings_info);
    delete settings_info;

    for (int i = 0; i < 22; i++)
        sre_internal_application->text_message[i] = scene_info_text_line[i];
    sre_internal_application->text_message[22] = "";
    sre_internal_application->nu_text_message_lines = 23;
}

void GUIProcessMouseMotion(int x, int y) {
    if (!(sre_internal_application->flags & SRE_APPLICATION_FLAG_PAN_WITH_MOUSE))
        return;
    if (sre_internal_application->flags & SRE_APPLICATION_FLAG_LOCK_PANNING) {
        sre_internal_backend->WarpCursor(sre_internal_application->window_width / 2, sre_internal_application->window_height / 2);
        return;
    }
    Vector3D angles;
    sre_internal_application->view->GetViewAngles(angles);
    float dx = (x - sre_internal_application->window_width / 2) * sre_internal_application->mouse_sensitivity.x;
    float dy = (y - sre_internal_application->window_height / 2) * sre_internal_application->mouse_sensitivity.y;
    angles.z -= dx * 360.0f * 0.5 / sre_internal_application->window_width;
    angles.x -= dy * 360.0f * 0.5 / sre_internal_application->window_width;
    // The horizontal field of view wraps around.
    if (angles.z < - 180)
        angles.z += 360;
    if (angles.z >= 180)
        angles.z -= 360;
    // Restrict the vertical field of view.
    if (angles.x < - 80)
        angles.x = - 80;
    if (angles.x > 10)
        angles.x = 10;
    sre_internal_application->view->SetViewAngles(angles);
    sre_internal_backend->WarpCursor(sre_internal_application->window_width / 2, sre_internal_application->window_height / 2);
}

void GUITextMessageTimeoutCallback() {
    // Avoid the callback from called unless another message is posted.
    sre_internal_application->text_message_timeout = 1000000.0;
    if (menu_mode > 0) {
        // Keep the menu.
        return;
    }
    // No menu, remove the text message.
    sre_internal_application->nu_text_message_lines = 2;
    sre_internal_application->text_message[0] = "";
    sre_internal_application->text_message[1] = "";
}

void GUIKeyPressCallback(unsigned int key) {
    switch (key) {
    case 'Q' :
        sre_internal_application->stop_signal = SRE_APPLICATION_STOP_SIGNAL_QUIT;
//        sre_internal_backend->Finalize();
//        exit(0);
        break;
    case 'F' :
        sre_internal_backend->GLSync();
        sre_internal_backend->ToggleFullScreenMode(
            sre_internal_application->window_width, sre_internal_application->window_height,
            (sre_internal_application->flags & SRE_APPLICATION_FLAG_PAN_WITH_MOUSE) != 0);
        break;
    case 'M' :
        if (sre_internal_application->flags & SRE_APPLICATION_FLAG_PAN_WITH_MOUSE) {
            sre_internal_backend->RestoreCursor();
            sre_internal_application->SetFlags(sre_internal_application->GetFlags() & (~SRE_APPLICATION_FLAG_PAN_WITH_MOUSE));
        }
        else {
            sre_internal_backend->WarpCursor(sre_internal_application->window_width / 2,
               sre_internal_application->window_height / 2);
            sre_internal_backend->HideCursor();
            sre_internal_application->SetFlags(sre_internal_application->GetFlags() | SRE_APPLICATION_FLAG_PAN_WITH_MOUSE);
        }
        break;
    case '+' :
        sre_internal_application->view->SetZoom(sre_internal_application->view->GetZoom() * 1.0f / 1.1f);
        break;
    case '-' :
        sre_internal_application->view->SetZoom(sre_internal_application->view->GetZoom() * 1.1f);
        break;
    case 'A' :
        accelerate_pressed = true;
        break;
    case 'Z' :
        decelerate_pressed = true;
        break;
    case '/' :
        jump_input_detected = true;
        break;
    case ' ' :
        // Used to toggle gravity and start hovering mode.
        sre_internal_application->SetFlags(sre_internal_application->GetFlags()
            ^ SRE_APPLICATION_FLAG_NO_GRAVITY);
        if (SRE_APPLICATION_FLAG_NO_GRAVITY) {
            if (sre_internal_application->view->GetMovementMode() == SRE_MOVEMENT_MODE_USE_FORWARD_AND_ASCEND_VECTOR)
                 sre_internal_application->hovering_height = Magnitude(ProjectOnto(
                     sre_internal_application->scene->object[sre_internal_application->control_object]->position,
                     sre_internal_application->view->GetAscendVector()));
            else
                sre_internal_application->hovering_height = sre_internal_application->scene->object[0]->position.z;
        }
        break;
    }
    if (!(sre_internal_application->flags & SRE_APPLICATION_FLAG_LOCK_PANNING))
    switch (key) {
    case ',' :
        // Rotate view direction 5 degrees along the z axis.
        sre_internal_application->view->RotateViewDirection(Vector3D(0, 0, 5.0f));
        break;
    case '.' :
        sre_internal_application->view->RotateViewDirection(Vector3D(0, 0, - 5.0f));
        break;
    case 'N' :
        sre_internal_application->view->RotateViewDirection(Vector3D(5.0f, 0, 0));
        break;
    case 'H' :
        sre_internal_application->view->RotateViewDirection(Vector3D(-5.0f, 0, 0));
        break;
    }
    switch (key) {
    case SRE_KEY_F5 :
        sreSetShaderSelection(SRE_SHADER_SELECTION_ALL);
        sre_internal_application->text_message[0] = "All optimized shaders enabled";
        sre_internal_application->text_message_time = sre_internal_backend->GetCurrentTime();
        break;
    case SRE_KEY_F6 :
        sreSetShaderSelection(SRE_SHADER_SELECTION_UNOPTIMIZED);
        sre_internal_application->text_message[0] = "All optimized shaders disabled";
        sre_internal_application->text_message_time = sre_internal_backend->GetCurrentTime();
        break;
    case '[' :
        // Cycle viewpoint to previous object.
        if (sre_internal_application->view->GetFollowedObject() > 0) {
            float distance;
            Vector3D offset_vector;
            sre_internal_application->view->GetFollowedObjectParameters(distance, offset_vector);
            sre_internal_application->view->SetViewModeFollowObject(sre_internal_application->view->GetFollowedObject() - 1,
                distance, offset_vector);
        }
        break;
    case ']' :
        // Cycle viewpoint to next object.
        if (sre_internal_application->view->GetFollowedObject() < sre_internal_application->scene->nu_objects - 1) {
            float distance;
            Vector3D offset_vector;
            sre_internal_application->view->GetFollowedObjectParameters(distance, offset_vector);
            sre_internal_application->view->SetViewModeFollowObject(sre_internal_application->view->GetFollowedObject() + 1,
                distance, offset_vector);
        }
        break;
    case '\\' :
        // Bird's eye view toward (0, 0, 0).
        sre_internal_application->view->SetViewModeLookAt(Point3D(0, 0, 200.0), Point3D(0, 0, 0), Vector3D(0, 1.0, 0));           
        break;
    case 'U' :
        // Cycle visualized shadow map.
        visualized_shadow_map++;
        // - 1 is disable.
        if (visualized_shadow_map >= sre_internal_application->scene->nu_lights)
            visualized_shadow_map = - 1;
        sreSetVisualizedShadowMap(visualized_shadow_map);
        break;
    case 'T' : {
        // Cycle GL filter for character set texture.
        text_filtering_mode++;
        if (text_filtering_mode > SRE_TEXTURE_FILTER_LINEAR)
            text_filtering_mode = 0;
        sreFont *font = sreSetFont(NULL);
        font->tex->ChangeParameters(SRE_TEXTURE_FLAG_SET_FILTER, text_filtering_mode, 1.0);
        break;
        }
    }

    if (menu_mode != 1 && key == SRE_KEY_F1) {
        menu_mode = 1;
        sre_internal_application->text_message[0] = "Rendering engine settings:";
        sre_internal_application->text_message[1] = "";
        sre_internal_application->text_message[2] = "1 -- No shadows";
        sre_internal_application->text_message[3] = "2 -- Shadow volumes";
        sre_internal_application->text_message[4] = "3 -- Shadow mapping";
        sre_internal_application->text_message[5] = "4 -- Standard reflection model";
        sre_internal_application->text_message[6] = "5 -- Microfacet reflection model";
        sre_internal_application->text_message[7] = "6 -- Single-pass rendering (only one light)";
        sre_internal_application->text_message[8] = "7 -- Multi-pass rendering";
        sre_internal_application->text_message[9] = "Scissors settings: d/s/g -- Disabled/light scissors/geometry scissors";
        sre_internal_application->text_message[10] = "Insert/Delete Enable/disable geometry scissors cache";
        sre_internal_application->text_message[11] = "";
        sre_internal_application->text_message[12] = "Enable/disable shadow volume settings: F9/F10 - strip/fans, F11/F12 - Cache";
        sre_internal_application->text_message[13] = "v/b x/c - visibility tests, =/Backspace -- Force depth-fail stencil rendering";
        sre_internal_application->text_message[14] = "l/k -- Enable/disable light attenuation";
        sre_internal_application->text_message[15] = "8/9 -- Enable/disable light object list rendering";
        sre_internal_application->text_message[16] = "F2/F3 -- Disable/enable HDR rendering  F4 -- Cycle tone mapping shader";
//        sre_internal_application->text_message[16] = "F5/F6 -- Enable/disable certain optimized shaders";
        sre_internal_application->text_message[17] = "F7 -- Cycle texture anisotropy  F8 -- Cycle number of visible lights";
        sre_internal_application->text_message[18] = "";
        SetShortEngineSettingsText();
        sre_internal_application->text_message[19] = short_engine_settings_text;
        sre_internal_application->text_message[20] = "";
        sre_internal_application->text_message[21] = "";
        sre_internal_application->text_message[22] = "";
        sre_internal_application->nu_text_message_lines = 23;
        sre_internal_application->text_message_time = sre_internal_backend->GetCurrentTime() + 1000000.0;
   }
   else if (menu_mode != 2 && key == 'I') {
        menu_mode = 2;
        // Show info screen with engine settings and scene info.
        SetInfoScreen();
        sre_internal_application->text_message_time = sre_internal_backend->GetCurrentTime() + 1000000.0;
   }
   else if ((menu_mode == 1 && key == SRE_KEY_F1) || (menu_mode == 2 && key == 'I')) {
       // Clear menu/info overlay.
       menu_mode = 0;
       sre_internal_application->nu_text_message_lines = 2;
       sre_internal_application->text_message[0] = "";
       sre_internal_application->text_message[1] = "";
   }

   // Make messages appear below the menu when it is active.
   int line_number = 0;
   if (menu_mode > 0)
       line_number = 22;

        // Menu choices that can also be selected when the menu is not active.
        bool menu_message = true;        
        switch (key) {
        case '2' :
            sreSetShadowsMethod(SRE_SHADOWS_SHADOW_VOLUMES);
            sre_internal_application->text_message[line_number] = "Shadow volumes enabled";
            break;
        case '1' :
            sreSetShadowsMethod(SRE_SHADOWS_NONE);
            sre_internal_application->text_message[line_number] = "Shadows disabled";
            break;
        case '7' :
            sreEnableMultiPassRendering();
            sre_internal_application->text_message[line_number] = "Multi-pass rendering enabled";
            break;
        case '6' :
            sreDisableMultiPassRendering();
            sre_internal_application->text_message[line_number] = "Multi-pass rendering disabled";
            break;
        case '4':
            sreSetReflectionModel(SRE_REFLECTION_MODEL_STANDARD);
            sre_internal_application->text_message[line_number] = "Standard reflection model selected";
            break;
        case '5' :
            sreSetReflectionModel(SRE_REFLECTION_MODEL_MICROFACET);
            sre_internal_application->text_message[line_number] = "Microfacet reflection model selected";
            break;
        case '3' :
            sreSetShadowsMethod(SRE_SHADOWS_SHADOW_MAPPING);
            sre_internal_application->text_message[line_number] = "Shadow mapping enabled";
            break;
        case 'L' :
            sreSetLightAttenuation(true);
            sre_internal_application->text_message[line_number] = "Light attenuation enabled";
            break;
        case 'K' :
            sreSetLightAttenuation(false);
            sre_internal_application->text_message[line_number] = "Light attenuation disabled";
            break;
        case 'S' :
            sreSetLightScissors(SRE_SCISSORS_LIGHT);
            sre_internal_application->text_message[line_number] = "Light scissors enabled";
            break;
        case 'G' :
            sreSetLightScissors(SRE_SCISSORS_GEOMETRY);
            sre_internal_application->text_message[line_number] = "Geometry scissors enabled";
            break;
        case 'D' :
            sreSetLightScissors(SRE_SCISSORS_NONE);
            sre_internal_application->text_message[line_number] = "Light/geometry scissors disabled";
            break;
         // Excentric scissors optimization that modifies the transformation
         // matrix, doesn't really work in practice on modern hardware due to
         // precision consistency so is disabled.
//        case 'O' :
//            sreSetLightScissors(SRE_SCISSORS_GEOMETRY_MATRIX);
//            sre_internal_application->text_message[line_number] = "Geometry matrix scissors enabled";
//            break;
        case 'V' :
            sreSetShadowVolumeVisibilityTest(true);
            sre_internal_application->text_message[line_number] = "Shadow volume visibility test enabled";
            break;
        case 'B' :
            sreSetShadowVolumeVisibilityTest(false);
            sre_internal_application->text_message[line_number] = "Shadow volume visibility test disabled";
            break;
        case 'X' :
            sreSetShadowVolumeDarkCapVisibilityTest(true);
            sre_internal_application->text_message[line_number] = "Shadow volume darkcap visibility test enabled";
            break;
        case 'C' :
            sreSetShadowVolumeDarkCapVisibilityTest(false);
            sre_internal_application->text_message[line_number] = "Shadow volume darkcap visibility test disabled";
            break;
        case '8' :
            sreSetLightObjectLists(true);
            sre_internal_application->text_message[line_number] = "Light object list rendering enabled";
            break;
        case '9' :
            sreSetLightObjectLists(false);
            sre_internal_application->text_message[line_number] = "Light object list rendering disabled";
            break;
        case SRE_KEY_F2 :
            sreSetHDRRendering(false);
            sre_internal_application->text_message[line_number] = "HDR rendering disabled";
            break;
        case SRE_KEY_F3 :
            sreSetHDRRendering(true);
            sre_internal_application->text_message[line_number] = "HDR rendering enabled";
            break;
        case SRE_KEY_F4 :
            sreSetHDRToneMappingShader((sreGetCurrentHDRToneMappingShader() + 1) % SRE_NUMBER_OF_TONE_MAPPING_SHADERS);
	    sre_internal_application->text_message[line_number] = "HDR tone mapping shader changed:";
            sre_internal_application->text_message[line_number + 1] = (char *)sreGetToneMappingShaderName(sreGetCurrentHDRToneMappingShader());
            break;
        case SRE_KEY_F7 : {
            float max_anisotropy = sreGetMaxAnisotropyLevel();
            if (max_anisotropy < 1.01)
                sre_internal_application->text_message[line_number] = "Anisotropic filtering not supported";
            else {
                anisotropy = roundf(anisotropy + 1.0);
                if (anisotropy > max_anisotropy + 0.01)
                    anisotropy = 1.0;
                sprintf(anisotropy_text, "Anisotropy level for texture filtering: %.1f %s", anisotropy,
                    anisotropy < 1.01 ? "(disabled)" : "");
                sre_internal_application->text_message[line_number] = anisotropy_text;
                sre_internal_application->text_message[line_number + 1] = "Applying to all suitable textures";
                sre_internal_application->scene->ApplyGlobalTextureParameters(SRE_TEXTURE_FLAG_SET_ANISOTROPY, 0, anisotropy);
            }
            break;
            }
        case SRE_KEY_F8 : {
            sreEngineSettingsInfo *info = sreGetEngineSettingsInfo();
            int n;
            if (info->max_visible_active_lights == SRE_MAX_ACTIVE_LIGHTS_UNLIMITED)
                n = 1;
            else if (info->max_visible_active_lights == 1)
                n = 2;
            else {
                n = info->max_visible_active_lights * 2;
                if (n >= sre_internal_application->scene->nu_lights)
                    n = SRE_MAX_ACTIVE_LIGHTS_UNLIMITED;
            }
            sreSetMultiPassMaxActiveLights(n);
            delete info;
            break;
            }
        case SRE_KEY_F9 : {
            sreSetTriangleStripUseForShadowVolumes(true);
            sreSetTriangleFanUseForShadowVolumes(true);
            sre_internal_application->text_message[line_number] = "Triangle strip/fan use for shadow volumes enabled";
            break;
            }
        case SRE_KEY_F10 : {
            sreSetTriangleStripUseForShadowVolumes(false);
            sreSetTriangleFanUseForShadowVolumes(false);
            sre_internal_application->text_message[line_number] = "Triangle strip/fan use for shadow volumes disabled";
            break;
            }
        case SRE_KEY_F11 : {
            sreSetShadowVolumeCache(true);
            sre_internal_application->text_message[line_number] = "Shadow volume cache enabled";
            break;
            }
        case SRE_KEY_F12 : {
            sreSetShadowVolumeCache(false);
            sre_internal_application->text_message[line_number] = "Shadow volume cache disabled";
            break;
            }
        case '=' : {
            sreSetForceDepthFailRendering(true);
            sre_internal_application->text_message[line_number] = "Force stencil shadow volume depth-fail rendering enabled";
            break;
            }
        case SRE_KEY_BACKSPACE : {
            sreSetForceDepthFailRendering(false);
            sre_internal_application->text_message[line_number] = "Force stencil shadow volume depth-fail rendering disabled";
            break;
            }
        case SRE_KEY_INSERT : {
            sreSetGeometryScissorsCache(true);
            sre_internal_application->text_message[line_number] = "Geometry scissors cache enabled";
            break;
        }
        case SRE_KEY_DELETE : {
            sreSetGeometryScissorsCache(false);
            sre_internal_application->text_message[line_number] = "Geometry scissors cache disabled";
            break;
        }
        default :
            menu_message = false;
            break;
        }
        if (menu_message) {
            // Set the timeout for the text message.
            sre_internal_application->text_message_time = sre_internal_backend->GetCurrentTime();
            sre_internal_application->text_message_timeout = 3.0;
            if (menu_mode == 2) {
                sreEngineSettingsInfo *info = sreGetEngineSettingsInfo();
                SetEngineSettingsInfo(info);
                delete info;
            }
            else if (menu_mode == 1) {
                SetShortEngineSettingsText();
                sre_internal_application->text_message[19] = short_engine_settings_text;
            }
        }
}

void GUIKeyReleaseCallback(unsigned int key) {
    switch (key) {
    case 'A' :
        accelerate_pressed = false;
        break;
    case 'Z' :
        decelerate_pressed = false;
        break;
    }
}

int GUITranslateKeycode(unsigned int platform_keycode, const unsigned int *table) {
    int i = 0;
    for (;;) {
        if (table[i + 1] == SRE_TABLE_END_TOKEN)
            return 0;
        if (table[i + 1] == SRE_KEY_MAPPING_RANGE_TOKEN) {
            unsigned int key0 = table[i] & 0xFFFF;
            unsigned int key1 = table[i] >> 16;
            if (platform_keycode >= key0 && platform_keycode <= key1)
                return platform_keycode;
        }
         if (table[i + 1] & SRE_KEY_MAPPING_RANGE_WITH_OFFSET_MASK) {
            unsigned int key0 = table[i] & 0xFFFF;
            unsigned int key1 = table[i] >> 16;
            if (platform_keycode >= key0 && platform_keycode <= key1) {
                unsigned int new_key0 = table[i + 1] & (SRE_KEY_MAPPING_RANGE_WITH_OFFSET_MASK - 1);
                return platform_keycode - key0 + new_key0;
            }
        }
        if (table[i] == platform_keycode)
            return table[i + 1];
        i += 2;
    }
}

#define ONE_SIXTIETH (1.0f / 60.0f)

#define Accelerate(dt) \
    input_acceleration += horizontal_acceleration * (dt)

#define Decelerate(dt) \
    input_acceleration -= horizontal_acceleration * (dt)

void sreApplication::ApplyControlObjectInputs(double dt) {
    // The move player function is always called by the GUI back-end, even
    // there is no user control. Take the opportunity to update scene info
    // (visible object counts etc) if the info screen is enabled.
    if (menu_mode == 2) {
        settings_info = sreGetEngineSettingsInfo();
        SetSceneInfo(scene, settings_info);
        delete settings_info;
    }

    if (view->GetMovementMode() == SRE_MOVEMENT_MODE_NONE)
        return;

    bool accelerate_pressed_previously = accelerate_pressed;
    bool decelerate_pressed_previously = decelerate_pressed;
    if (accelerate_pressed) {
        if (!accelerate_pressed_previously)
            // If the accelerate key was pressed during the last frame, assume it was held down for 1 / 60th seconds.
            Accelerate(ONE_SIXTIETH);
        else
            // Accelerate key was held down continuously during last frame.
            Accelerate(dt);
    }
    if (decelerate_pressed) {
        if (!decelerate_pressed_previously)
            Decelerate(ONE_SIXTIETH);
        else
            Decelerate(dt);
    }
    if (flags & SRE_APPLICATION_FLAG_NO_GRAVITY)  {
       if (ascend_pressed) {
           sre_internal_application->hovering_height += sre_internal_application->hovering_height_acceleration * dt;
       }
       else
       if (descend_pressed) {
           hovering_height -= hovering_height_acceleration * dt;
           if (hovering_height < 0)
               hovering_height = 0;
       }
    }
    else if (jump_input_detected) {
        jump_requested = true;
        jump_input_detected = false;
    }
}


