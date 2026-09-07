#include <ppu-lv2.h>
#include <sysutil/video.h>
#include <rsx/rsx.h>
#include <io/pad.h>
#include <stdlib.h>
#include <stdio.h>

// إعداد إطار الرسم
gcmContextData *context;
rsxBuffer buffers[2];
int currentBuffer = 0;

void init_screen(u16 *width, u16 *height) {
    videoState state;
    videoGetState(0, 0, &state);
    videoConfiguration vconfig;
    memset(&vconfig, 0, sizeof(videoConfiguration));
    videoGetResolutionAvailability(state.displayMode.resolution, ...);
    // تهيئة الفيديو الأساسية - سيتم تفصيلها لاحقاً
}

void draw_card(u32 x, u32 y, u32 w, u32 h, u32 color) {
    // رسم مستطيل بسيط يمثل "كرت" من واجهة PS5
    rsxSetColorMask(context, RSX_COLOR_MASK_R | RSX_COLOR_MASK_G | RSX_COLOR_MASK_B);
    rsxClearColor(context, color);
    // منطق رسم المستطيل بالإحداثيات x,y,w,h
}

int main(void) {
    ioPadInit(7);#include <ppu-lv2.h>
#include <sysutil/video.h>
#include <rsx/rsx.h>
#include <io/pad.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define BUFFER_COUNT 2

gcmContextData *context;
rsxBuffer buffers[BUFFER_COUNT];
int currentBuffer = 0;
u16 g_width, g_height;

void waitFlip() {
    while (gcmGetFlipStatus() != 0) usleep(200);
    gcmResetFlipStatus();
}

void flip(s32 buffer) {
    gcmSetFlip(context, buffer);
    rsxFlushBuffer(context);
    gcmSetWaitFlip(context);
}

void makeBuffer(rsxBuffer *buf, u16 width, u16 height, int id) {
    int depth = 4; // 4 bytes per pixel (RGBA)
    buf->ptr = (u32 *)rsxMemAlign(64, height * width * depth);
    buf->width = width;
    buf->height = height;
    buf->id = id;
    buf->pitch = width * depth;

    rsxAddressToOffset(buf->ptr, &buf->offset);
    gcmSetDisplayBuffer(id, buf->offset, buf->pitch, width, height);
}

void init_screen(u16 *width, u16 *height) {
    void *host_addr = memalign(1024 * 1024, 1024 * 1024 * 8); // 8MB command buffer
    context = rsxInit(0x10000, 1024 * 1024 * 8, host_addr);

    videoState state;
    videoGetState(0, 0, &state);

    videoConfiguration vconfig;
    memset(&vconfig, 0, sizeof(videoConfiguration));
    vconfig.resolution = state.displayMode.resolution;
    vconfig.format = VIDEO_BUFFER_FORMAT_XRGB;
    vconfig.pitch = 1920 * 4; // نفترض دقة عريضة، سيتم تعديلها حسب الجهاز
    vconfig.aspect = state.displayMode.aspect;

    videoConfigure(0, &vconfig, NULL, 0);
    videoGetState(0, 0, &state);

    videoResolution res;
    videoGetResolution(state.displayMode.resolution, &res);

    *width = res.width;
    *height = res.height;
    g_width = res.width;
    g_height = res.height;

    gcmSetFlipMode(GCM_FLIP_VSYNC);

    for (int i = 0; i < BUFFER_COUNT; i++) {
        makeBuffer(&buffers[i], res.width, res.height, i);
    }

    gcmResetFlipStatus();
}
    u16 width, height;
    init_screen(&width, &height);

    while (1) {
        draw_card(100, 100, 300, 150, 0xFF0055AA); // كرت أزرق تجريبي

        padInfo padinfo;
        padData paddata;
        ioPadGetInfo(&padinfo);
        for (int i = 0; i < MAX_PADS; i++) {
            if (padinfo.status[i]) {
                ioPadGetData(i, &paddata);
                if (paddata.BTN_CROSS) {
                    // تفاعل عند الضغط على زر X
                }
            }
        }

        rsxSetWaitFlip(context);
        rsxFlipBuffer(context, currentBuffer);
        currentBuffer = !currentBuffer;
    }

    return 0;
}
