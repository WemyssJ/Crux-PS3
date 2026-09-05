#include "render.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
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

    static const uint32_t kMaxFonts = 8;
    static TTF_Font *sFonts[kMaxFonts] = { NULL };
    static uint32_t sFontCount = 0;

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
        TTF_Init();

        return true;
    }

    void Shutdown()
    {
        for (uint32_t i = 0; i < sTextureCount; i++)
            if (sTextures[i]) SDL_DestroyTexture(sTextures[i]);
        for (uint32_t i = 0; i < sFontCount; i++)
            if (sFonts[i]) TTF_CloseFont(sFonts[i]);
        TTF_Quit();
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

    FontHandle LoadFont(const char *path, int pointSize)
    {
        if (sFontCount >= kMaxFonts) return 0;

        char full[512];
        snprintf(full, sizeof(full), "data/fonts/%s", path);

        TTF_Font *font = TTF_OpenFont(full, pointSize);
        if (!font)
        {
            SDL_Log("LoadFont failed for %s: %s", full, TTF_GetError());
            return 0;
        }

        sFonts[sFontCount] = font;
        return ++sFontCount;
    }

    void BeginFrame(Vec2 camPos, float orthoHalfHeight)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            // Escape is no longer a hard quit -- it's Input::backIsPressed
            // now (opens the pause menu / backs out of menus instead), so
            // only the window's own close button/Alt+F4 exits the app.
            if (e.type == SDL_QUIT) sQuit = true;
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

    void DrawTexturedQuad(TextureHandle tex, Vec2 center, Vec2 size, float rotationDeg, unsigned int tintArgb, bool flipX, bool flipY)
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

        int flip = SDL_FLIP_NONE;
        if (flipX) flip |= SDL_FLIP_HORIZONTAL;
        if (flipY) flip |= SDL_FLIP_VERTICAL;
        SDL_RenderCopyEx(sRenderer, t, NULL, &dst, -rotationDeg, NULL, (SDL_RendererFlip)flip);
    }

    void DrawUIText(FontHandle font, float nx, float ny, const char *text, unsigned int colorArgb, float scale, bool centered)
    {
        if (font == 0 || font > sFontCount || !sFonts[font - 1] || !text || !text[0]) return;

        SDL_Color color;
        color.a = (Uint8)((colorArgb >> 24) & 0xFF);
        color.r = (Uint8)((colorArgb >> 16) & 0xFF);
        color.g = (Uint8)((colorArgb >> 8) & 0xFF);
        color.b = (Uint8)(colorArgb & 0xFF);

        SDL_Surface *surf = TTF_RenderText_Blended(sFonts[font - 1], text, color);
        if (!surf) return;

        SDL_Texture *tex = SDL_CreateTextureFromSurface(sRenderer, surf);
        int textW = surf->w, textH = surf->h;
        SDL_FreeSurface(surf);
        if (!tex) return;

        int dw = (int)(textW * scale);
        int dh = (int)(textH * scale);
        SDL_Rect dst;
        dst.x = (int)(nx * kWindowW) - (centered ? dw / 2 : 0);
        dst.y = (int)(ny * kWindowH) - (centered ? dh / 2 : 0);
        dst.w = dw;
        dst.h = dh;
        SDL_RenderCopy(sRenderer, tex, NULL, &dst);
        SDL_DestroyTexture(tex);
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
