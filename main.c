/*
 * PS5-style Dashboard UI for PS3 CFW 4.92 (Homebrew, PSL1GHT)
 * Consolidated single-file version combining:
 *   - Screen/RSX initialization
 *   - Texture loading
 *   - Shader program loading
 *   - Card drawing (textured quads)
 *   - Card animation & D-pad navigation
 *
 * Build requirements (kept separate, see accompanying files):
 *   - shaders/vertex.cg, shaders/fragment.cg  -> compiled with cgcomp into .vpo/.fpo
 *   - Makefile                                -> PSL1GHT build rules
 *
 * This is a homebrew UI/dashboard project. It does not include and will not
 * include any code for loading pirated PKG game licenses or DRM bypass tools.
 */

#include <ppu-lv2.h>
#include <sysutil/video.h>
#include <rsx/rsx.h>
#include <io/pad.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Types                                                               */
/* ------------------------------------------------------------------ */

#define BUFFER_COUNT 2
#define NUM_CARDS    4
#define LERP_SPEED   0.15f

typedef struct {
    u32 offset;
    u16 width;
    u16 height;
} Texture;

typedef struct {
    u32 vertex_offset;
    u32 fragment_offset;
    void *ucode;
} ShaderProgram;

typedef struct {
    float current_x, current_y;
    float target_x, target_y;
    float scale;
    float target_scale;
    int   selected;
} CardAnimState;

/* ------------------------------------------------------------------ */
/* Globals                                                             */
/* ------------------------------------------------------------------ */

static gcmContextData *context;
static rsxBuffer buffers[BUFFER_COUNT];
static int currentBuffer = 0;
static u16 g_width, g_height;

/* Linker symbols for precompiled shader binaries (produced by cgcomp,
 * embedded via the Makefile's binary-to-object rule). */
extern u8 _binary_vertex_vpo_start[];
extern u8 _binary_fragment_fpo_start[];

/* ------------------------------------------------------------------ */
/* Screen / RSX setup                                                  */
/* ------------------------------------------------------------------ */

static void waitFlip(void) {
    while (gcmGetFlipStatus() != 0) usleep(200);
    gcmResetFlipStatus();
}

static void flip(s32 buffer) {
    gcmSetFlip(context, buffer);
    rsxFlushBuffer(context);
    gcmSetWaitFlip(context);
}

static void makeBuffer(rsxBuffer *buf, u16 width, u16 height, int id) {
    int depth = 4; /* RGBA */
    buf->ptr = (u32 *)rsxMemAlign(64, height * width * depth);
    buf->width = width;
    buf->height = height;
    buf->id = id;
    buf->pitch = width * depth;

    rsxAddressToOffset(buf->ptr, &buf->offset);
    gcmSetDisplayBuffer(id, buf->offset, buf->pitch, width, height);
}

static void init_screen(u16 *width, u16 *height) {
    void *host_addr = memalign(1024 * 1024, 1024 * 1024 * 8); /* 8MB cmd buffer */
    context = rsxInit(0x10000, 1024 * 1024 * 8, host_addr);

    videoState state;
    videoGetState(0, 0, &state);

    videoConfiguration vconfig;
    memset(&vconfig, 0, sizeof(videoConfiguration));
    vconfig.resolution = state.displayMode.resolution;
    vconfig.format = VIDEO_BUFFER_FORMAT_XRGB;
    vconfig.aspect = state.displayMode.aspect;

    videoConfigure(0, &vconfig, NULL, 0);
    videoGetState(0, 0, &state);

    videoResolution res;
    videoGetResolution(state.displayMode.resolution, &res);

    *width  = res.width;
    *height = res.height;
    g_width  = res.width;
    g_height = res.height;

    vconfig.pitch = res.width * 4;
    videoConfigure(0, &vconfig, NULL, 0);

    gcmSetFlipMode(GCM_FLIP_VSYNC);

    for (int i = 0; i < BUFFER_COUNT; i++) {
        makeBuffer(&buffers[i], res.width, res.height, i);
    }

    gcmResetFlipStatus();
}

/* ------------------------------------------------------------------ */
/* Shaders                                                             */
/* ------------------------------------------------------------------ */

static void init_shaders(ShaderProgram *prog) {
    rsxVertexProgram   *vpo = (rsxVertexProgram *)_binary_vertex_vpo_start;
    rsxFragmentProgram *fpo = (rsxFragmentProgram *)_binary_fragment_fpo_start;

    u32 fragment_size;
    void *fragment_ucode;
    rsxFragmentProgramGetUCode(fpo, &fragment_ucode, &fragment_size);

    prog->ucode = rsxMemAlign(64, fragment_size);
    memcpy(prog->ucode, fragment_ucode, fragment_size);
    rsxAddressToOffset(prog->ucode, &prog->fragment_offset);

    rsxLoadVertexProgram(context, vpo);
}

/* ------------------------------------------------------------------ */
/* Textures                                                             */
/* ------------------------------------------------------------------ */

/* NOTE: loads raw RGBA pixel dumps. For PNG/JPG assets you need a
 * decoding step (e.g. libpng) before this call - not included here. */
static int load_texture_from_file(const char *path, Texture *tex) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    void *data = rsxMemAlign(128, size);
    fread(data, 1, size, f);
    fclose(f);

    rsxAddressToOffset(data, &tex->offset);
    return 0;
}

static void draw_textured_card(ShaderProgram *prog, Texture *tex,
                                u32 x, u32 y, u32 w, u32 h,
                                u16 screen_w, u16 screen_h) {
    rsxTexture texObj;
    memset(&texObj, 0, sizeof(rsxTexture));
    texObj.format  = GCM_TEXTURE_FORMAT_A8R8G8B8 | GCM_TEXTURE_FORMAT_LIN;
    texObj.width   = tex->width;
    texObj.height  = tex->height;
    texObj.offset  = tex->offset;
    texObj.mipmap  = 1;
    texObj.pitch   = tex->width * 4;

    rsxLoadTexture(context, 0, &texObj);
    rsxTextureControl(context, 0, GCM_TRUE, 0, 12, GCM_TEXTURE_NEAREST);

    float x0 = (2.0f * x / screen_w) - 1.0f;
    float y0 = 1.0f - (2.0f * y / screen_h);
    float x1 = (2.0f * (x + w) / screen_w) - 1.0f;
    float y1 = 1.0f - (2.0f * (y + h) / screen_h);

    float vertices[] = {
        x0, y0, 0.0f,   0.0f, 0.0f,
        x1, y0, 0.0f,   1.0f, 0.0f,
        x1, y1, 0.0f,   1.0f, 1.0f,
        x0, y1, 0.0f,   0.0f, 1.0f,
    };

    void *vbuf = rsxMemAlign(64, sizeof(vertices));
    memcpy(vbuf, vertices, sizeof(vertices));

    u32 voffset;
    rsxAddressToOffset(vbuf, &voffset);

    rsxBindVertexArrayAttrib(context, 0, 0, voffset,
                              5 * sizeof(float), 3, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);
    rsxBindVertexArrayAttrib(context, 8, 0, voffset + 3 * sizeof(float),
                              5 * sizeof(float), 2, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);

    rsxDrawVertexArray(context, GCM_TYPE_QUADS, 0, 4);
}

/* ------------------------------------------------------------------ */
/* Animation                                                            */
/* ------------------------------------------------------------------ */

static void update_card_animation(CardAnimState *anim) {
    anim->current_x += (anim->target_x - anim->current_x) * LERP_SPEED;
    anim->current_y += (anim->target_y - anim->current_y) * LERP_SPEED;
    anim->scale      += (anim->target_scale - anim->scale) * LERP_SPEED;
}

static void select_card(CardAnimState *anim, int is_selected) {
    anim->selected = is_selected;
    anim->target_scale = is_selected ? 1.15f : 1.0f;
}

/* ------------------------------------------------------------------ */
/* Main                                                                 */
/* ------------------------------------------------------------------ */

int main(void) {
    ioPadInit(7);

    u16 width, height;
    init_screen(&width, &height);

    ShaderProgram prog;
    init_shaders(&prog);

    Texture bgTexture;
    load_texture_from_file(
        "/dev_hdd0/game/PS5DASH/USRDIR/assets/backgrounds/bg1.raw", &bgTexture);

    Texture cardTextures[NUM_CARDS];
    CardAnimState cardAnims[NUM_CARDS];

    for (int i = 0; i < NUM_CARDS; i++) {
        char path[128];
        sprintf(path, "/dev_hdd0/game/PS5DASH/USRDIR/assets/icons/card%d.raw", i + 1);
        load_texture_from_file(path, &cardTextures[i]);

        cardAnims[i].current_x = 100 + i * 220;
        cardAnims[i].target_x  = 100 + i * 220;
        cardAnims[i].current_y = 200;
        cardAnims[i].target_y  = 200;
        cardAnims[i].scale        = 1.0f;
        cardAnims[i].target_scale = 1.0f;
        cardAnims[i].selected      = 0;
    }

    int selectedIndex = 0;
    select_card(&cardAnims[selectedIndex], 1);

    while (1) {
        draw_textured_card(&prog, &bgTexture, 0, 0, width, height, width, height);

        padInfo padinfo;
        padData paddata;
        ioPadGetInfo(&padinfo);
        for (int i = 0; i < MAX_PADS; i++) {
            if (padinfo.status[i]) {
                ioPadGetData(i, &paddata);

                if (paddata.BTN_RIGHT && selectedIndex < NUM_CARDS - 1) {
                    select_card(&cardAnims[selectedIndex], 0);
                    selectedIndex++;
                    select_card(&cardAnims[selectedIndex], 1);
                }
                if (paddata.BTN_LEFT && selectedIndex > 0) {
                    select_card(&cardAnims[selectedIndex], 0);
                    selectedIndex--;
                    select_card(&cardAnims[selectedIndex], 1);
                }
            }
        }

        for (int i = 0; i < NUM_CARDS; i++) {
            update_card_animation(&cardAnims[i]);
            u32 w = (u32)(200 * cardAnims[i].scale);
            u32 h = (u32)(120 * cardAnims[i].scale);
            draw_textured_card(&prog, &cardTextures[i],
                                (u32)cardAnims[i].current_x, (u32)cardAnims[i].current_y,
                                w, h, width, height);
        }

        flip(currentBuffer);
        currentBuffer = !currentBuffer;
    }

    return 0;
}

