#include "app.h"
#include "render.h"
#include <SDL2/SDL.h>

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (!App::Init())
    {
        SDL_Log("App::Init failed");
        return 1;
    }

    Uint32 lastTicks = SDL_GetTicks();
    const float kFixedDt = 1.0f / 60.0f;

    while (!Render::ShouldQuit())
    {
        Uint32 now = SDL_GetTicks();
        float frameDt = (now - lastTicks) / 1000.0f;
        lastTicks = now;
        if (frameDt > 0.25f) frameDt = 0.25f; // clamp huge stalls (e.g. breakpoint/debugger pause)

        // Fixed-step update to match the PS3 build's fixed 60Hz loop.
        static float acc = 0.0f;
        acc += frameDt;
        while (acc >= kFixedDt)
        {
            App::Update(kFixedDt);
            acc -= kFixedDt;
        }

        App::Draw();
    }

    App::Shutdown();
    return 0;
}
