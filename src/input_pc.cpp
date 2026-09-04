#include "input.h"
#include <SDL2/SDL.h>
#include <string.h>

// PC input backend: keyboard, mirroring the pad mapping used on PS3.
//   Left/Right arrow or A/D -> leftIsHeld / rightIsHeld
//   Space                   -> jump
//   X                       -> flip
//   Up arrow / W            -> up   (attach both hands)
//   Down arrow / S          -> down (detach secondary hand)
//   R                       -> restart
//   Return                  -> pause
namespace Input
{
    bool jumpWasPressed = false;
    bool jumpIsHeld = false;
    bool jumpWasReleased = false;
    bool leftIsHeld = false;
    bool leftIsPressed = false;
    bool rightIsHeld = false;
    bool rightIsPressed = false;
    bool flipIsPressed = false;
    bool upIsPressed = false;
    bool downIsPressed = false;
    bool restartIsPressed = false;
    bool pauseIsPressed = false;

    static Uint8 sPrev[SDL_NUM_SCANCODES];
    static bool sPrevInit = false;

    bool Init()
    {
        memset(sPrev, 0, sizeof(sPrev));
        sPrevInit = true;
        return true;
    }

    void Shutdown()
    {
    }

    static bool Held(const Uint8 *state, SDL_Scancode a, SDL_Scancode b)
    {
        return state[a] || state[b];
    }
    static bool Pressed(const Uint8 *state, SDL_Scancode a, SDL_Scancode b)
    {
        return (state[a] && !sPrev[a]) || (state[b] && !sPrev[b]);
    }
    static bool Released(const Uint8 *state, SDL_Scancode a, SDL_Scancode b)
    {
        return (!state[a] && sPrev[a]) || (!state[b] && sPrev[b]);
    }

    void Update()
    {
        SDL_PumpEvents();
        const Uint8 *state = SDL_GetKeyboardState(NULL);

        leftIsHeld = Held(state, SDL_SCANCODE_LEFT, SDL_SCANCODE_A);
        leftIsPressed = Pressed(state, SDL_SCANCODE_LEFT, SDL_SCANCODE_A);
        rightIsHeld = Held(state, SDL_SCANCODE_RIGHT, SDL_SCANCODE_D);
        rightIsPressed = Pressed(state, SDL_SCANCODE_RIGHT, SDL_SCANCODE_D);

        jumpIsHeld = Held(state, SDL_SCANCODE_SPACE, SDL_SCANCODE_SPACE);
        jumpWasPressed = Pressed(state, SDL_SCANCODE_SPACE, SDL_SCANCODE_SPACE);
        jumpWasReleased = Released(state, SDL_SCANCODE_SPACE, SDL_SCANCODE_SPACE);

        flipIsPressed = Pressed(state, SDL_SCANCODE_X, SDL_SCANCODE_X);
        upIsPressed = Pressed(state, SDL_SCANCODE_UP, SDL_SCANCODE_W);
        downIsPressed = Pressed(state, SDL_SCANCODE_DOWN, SDL_SCANCODE_S);

        restartIsPressed = Pressed(state, SDL_SCANCODE_R, SDL_SCANCODE_R);
        pauseIsPressed = Pressed(state, SDL_SCANCODE_RETURN, SDL_SCANCODE_RETURN);

        memcpy(sPrev, state, sizeof(sPrev));
    }
}
