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
typedef unsigned int FontHandle;    // 0 == invalid/unloaded; PS3 ignores the handle (gcmutil has one built-in debug font)

namespace Render
{
    bool Init();
    void Shutdown();

    // path is relative to the data/sprites directory. Returns 0 on failure.
    // Safe to call after Init(), before the first BeginFrame.
    TextureHandle LoadTexture(const char *path);

    // path is relative to data/fonts. Ignored on PS3 (gcmutil's debug font is
    // fixed) -- call it anyway for source parity, just don't rely on the
    // returned handle there.
    FontHandle LoadFont(const char *path, int pointSize);

    // orthoHalfHeight = world-space half-height visible on screen (matches
    // Unity's Camera.orthographicSize).
    void BeginFrame(Vec2 camPos, float orthoHalfHeight);
    void DrawQuad(Vec2 center, Vec2 size, float rotationDeg, unsigned int argb);
    // tintArgb 0xFFFFFFFF = no tint (draw the texture as-is). flipX/flipY
    // mirror the source texture horizontally/vertically before rotating
    // (for paired limbs that share one sprite authored facing one direction
    // -- flipX for left/right pairs like hands/feet, flipY for the leg pair,
    // which needs a top/bottom mirror instead).
    void DrawTexturedQuad(TextureHandle tex, Vec2 center, Vec2 size, float rotationDeg, unsigned int tintArgb, bool flipX = false, bool flipY = false);

    // UI text overlay, in normalized screen fractions (0,0 = top-left, 1,1 =
    // bottom-right) -- not world space, always reads flat regardless of
    // camera. scale is a size multiplier around a reasonable default per
    // platform (they don't share a physical font size concept).
    void DrawUIText(FontHandle font, float nx, float ny, const char *text, unsigned int colorArgb, float scale = 1.0f);

    void EndFrame();

    // True once the app should exit (PC: window close / ESC; PS3: sysutil quit).
    bool ShouldQuit();
}

#endif
