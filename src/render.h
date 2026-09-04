#ifndef CRUX_RENDER_H
#define CRUX_RENDER_H

#include "vec2.h"

// Thin, platform-agnostic drawing interface. render_pc.cpp implements this
// with SDL2/SDL2_image (full texture support) for fast iteration on desktop.
// render_ps3.cpp implements the colored-quad half now; DrawTexturedQuad there
// currently falls back to a flat color until the PNG->GTF conversion pipeline
// is wired up (see TODO.md) -- so the PS3 build keeps compiling and running,
// just without real sprites yet.
typedef unsigned int TextureHandle; // 0 == invalid/unloaded

namespace Render
{
    bool Init();
    void Shutdown();

    // path is relative to the data/sprites directory. Returns 0 on failure.
    // Safe to call after Init(), before the first BeginFrame.
    TextureHandle LoadTexture(const char *path);

    // orthoHalfHeight = world-space half-height visible on screen (matches
    // Unity's Camera.orthographicSize).
    void BeginFrame(Vec2 camPos, float orthoHalfHeight);
    void DrawQuad(Vec2 center, Vec2 size, float rotationDeg, unsigned int argb);
    // tintArgb 0xFFFFFFFF = no tint (draw the texture as-is).
    void DrawTexturedQuad(TextureHandle tex, Vec2 center, Vec2 size, float rotationDeg, unsigned int tintArgb);
    void EndFrame();

    // True once the app should exit (PC: window close / ESC; PS3: sysutil quit).
    bool ShouldQuit();
}

#endif
