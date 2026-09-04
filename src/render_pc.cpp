#include "render.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdio.h>
#include <string.h>

namespace Render
{
    static const int kWindowW = 1024;
    static const int kWindowH = 768;

    static SDL_Window *sWindow = NULL;
    static SDL_Renderer *sRenderer = NULL;
    static SDL_Texture *sWhite = NULL;
    static bool sQuit = false;

    static Vec2 sCamPos;
    static float sPixelsPerUnit = 64.0f;

    static const uint32_t kMaxTextures = 64;
    static SDL_Texture *sTextures[kMaxTextures] = { NULL };
    static uint32_t sTextureCount = 0;

    bool Init()
    {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) return false;

        sWindow = SDL_CreateWindow("Crux (PC preview)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                    kWindowW, kWindowH, SDL_WINDOW_SHOWN);
        if (!sWindow) return false;

        sRenderer = SDL_CreateRenderer(sWindow, -1, SDL_RENDERER_ACCELERATED);
        if (!sRenderer) return false;

        Uint32 pixel = 0xFFFFFFFF;
        sWhite = SDL_CreateTexture(sRenderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STATIC, 1, 1);
        SDL_UpdateTexture(sWhite, NULL, &pixel, sizeof(Uint32));
        SDL_SetTextureBlendMode(sWhite, SDL_BLENDMODE_BLEND);

        IMG_Init(IMG_INIT_PNG);

        return true;
    }

    void Shutdown()
    {
        for (uint32_t i = 0; i < sTextureCount; i++)
            if (sTextures[i]) SDL_DestroyTexture(sTextures[i]);
        IMG_Quit();
        if (sWhite) SDL_DestroyTexture(sWhite);
        if (sRenderer) SDL_DestroyRenderer(sRenderer);
        if (sWindow) SDL_DestroyWindow(sWindow);
        SDL_Quit();
    }

    TextureHandle LoadTexture(const char *path)
    {
        if (sTextureCount >= kMaxTextures) return 0;

        char full[512];
        snprintf(full, sizeof(full), "data/sprites/%s", path);

        SDL_Texture *tex = IMG_LoadTexture(sRenderer, full);
        if (!tex)
        {
            SDL_Log("LoadTexture failed for %s: %s", full, IMG_GetError());
            return 0;
        }
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

        sTextures[sTextureCount] = tex;
        return ++sTextureCount; // 1-based; 0 stays "invalid"
    }

    void BeginFrame(Vec2 camPos, float orthoHalfHeight)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT) sQuit = true;
            if (e.type == SDL_KEYDOWN && e.key.keysym.scancode == SDL_SCANCODE_ESCAPE) sQuit = true;
        }

        sCamPos = camPos;
        sPixelsPerUnit = (kWindowH * 0.5f) / (orthoHalfHeight > 0.01f ? orthoHalfHeight : 0.01f);

        SDL_SetRenderDrawColor(sRenderer, 0x20, 0x20, 0x28, 0xFF);
        SDL_RenderClear(sRenderer);
    }

    static SDL_Rect WorldToScreenRect(Vec2 center, Vec2 size)
    {
        float sx = kWindowW * 0.5f + (center.x - sCamPos.x) * sPixelsPerUnit;
        float sy = kWindowH * 0.5f - (center.y - sCamPos.y) * sPixelsPerUnit;
        float w = size.x * sPixelsPerUnit;
        float h = size.y * sPixelsPerUnit;

        SDL_Rect dst;
        dst.x = (int)(sx - w * 0.5f);
        dst.y = (int)(sy - h * 0.5f);
        dst.w = (int)w;
        dst.h = (int)h;
        return dst;
    }

    void DrawQuad(Vec2 center, Vec2 size, float rotationDeg, unsigned int argb)
    {
        SDL_Rect dst = WorldToScreenRect(center, size);

        Uint8 a = (Uint8)((argb >> 24) & 0xFF);
        Uint8 r = (Uint8)((argb >> 16) & 0xFF);
        Uint8 g = (Uint8)((argb >> 8) & 0xFF);
        Uint8 b = (Uint8)(argb & 0xFF);
        SDL_SetTextureColorMod(sWhite, r, g, b);
        SDL_SetTextureAlphaMod(sWhite, a);

        SDL_RenderCopyEx(sRenderer, sWhite, NULL, &dst, -rotationDeg, NULL, SDL_FLIP_NONE);
    }

    void DrawTexturedQuad(TextureHandle tex, Vec2 center, Vec2 size, float rotationDeg, unsigned int tintArgb, bool flipX)
    {
        if (tex == 0 || tex > sTextureCount || !sTextures[tex - 1])
        {
            DrawQuad(center, size, rotationDeg, 0xFFFF00FF); // magenta = missing texture, loud on purpose
            return;
        }

        SDL_Texture *t = sTextures[tex - 1];
        SDL_Rect dst = WorldToScreenRect(center, size);

        Uint8 a = (Uint8)((tintArgb >> 24) & 0xFF);
        Uint8 r = (Uint8)((tintArgb >> 16) & 0xFF);
        Uint8 g = (Uint8)((tintArgb >> 8) & 0xFF);
        Uint8 b = (Uint8)(tintArgb & 0xFF);
        SDL_SetTextureColorMod(t, r, g, b);
        SDL_SetTextureAlphaMod(t, a);

        SDL_RenderCopyEx(sRenderer, t, NULL, &dst, -rotationDeg, NULL, flipX ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    }

    void EndFrame()
    {
        SDL_RenderPresent(sRenderer);
    }

    bool ShouldQuit()
    {
        return sQuit;
    }
}
