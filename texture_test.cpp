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
#include <ctype.h>
#include <math.h>

#include "sre.h"
#include "demo.h"

void TextureMemoryTest(bool compressed) {
    printf("Warning - this test fills (video) memory up with textures to test how many fit.\n");
    printf("System may become slow or unstable.\n");
    printf("Enter y or Y to continue.\n");
    if (toupper(getchar()) != 'Y')
        return;
    int count = 0;
    for (;;) {
        sreTexture *tex;
        if (compressed)
            tex = new sreTexture("volcanic8", TEXTURE_TYPE_NORMAL);
        else
            tex = new sreTexture("volcanic8", TEXTURE_TYPE_USE_RAW_TEXTURE);
        count++;
        printf("%d mipmapped 1024x1024 textures succesfully loaded.\n", count);
    }
}

// Texture performance test, use 10 large different textures.

static int player_object_id;

void TextureTestCreateScene(bool compressed) {
    sreModel *globe_model = sreCreateSphereModel(scene, 0);
    // Add player sphere as scene object 0.
    scene->SetFlags(SRE_OBJECT_DYNAMIC_POSITION | SRE_OBJECT_NO_PHYSICS);
    Color c;
    c.r = 0.00;
    c.g = 0.75;
    c.b = 1.0;
    scene->SetColor(c);
    player_object_id = scene->AddObject(globe_model, 0, 0, 3.0, 0, 0, 0, 3.0);
    sreModel *block_model = sreCreateUnitBlockModel(scene);
    sreTexture *tex[10];
    for (int i = 0; i < 10; i++) {
        if (compressed) {
            if ((i & 1) == 0)
                tex[i] = new sreTexture("water1", TEXTURE_TYPE_NORMAL);
            else
                tex[i] = new sreTexture("volcanic8", TEXTURE_TYPE_NORMAL);
        }
        else {
            if ((i & 1) == 0)
                tex[i] = new sreTexture("water1", TEXTURE_TYPE_USE_RAW_TEXTURE);
            else
                tex[i] = new sreTexture("volcanic8", TEXTURE_TYPE_USE_RAW_TEXTURE);
        }
    }
    scene->SetFlags(SRE_OBJECT_USE_TEXTURE | SRE_OBJECT_NO_PHYSICS);
    for (int y = 0; y < 10; y++)
        for (int x = 0; x < 10; x++) {
            scene->SetTexture(tex[(x + y) % 10]);
            scene->AddObject(block_model, - 50 + x * 10, 5 + y * 10, 0, 0, 0, 0, 9.0);
        }
    scene->AddDirectionalLight(0, Vector3D(0.1, - 0.5, 1.0), Color(1.0, 1.0, 1.0));
    // View mode already set in main.cpp.
//    view->SetViewModeFollowObject(oid, 40.0, Vector3D(0, 0, 10.0));
}

void TextureTestRender() {
    scene->Render(view);
}

static int count = 0;

void TextureTestTimeIteration(double previous_time, double current_time) {
    scene->ChangePosition(player_object_id, scene->object[player_object_id]->position.x,
        (double)(count % 256) * 0.2,
        scene->object[player_object_id]->position.z);
    count++;
}
