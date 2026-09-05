#include "input.h"
#include <SDL2/SDL.h>
#include <string.h>

// PC input backend: keyboard AND a real SDL game controller (if one's
// connected) both drive every action -- either source works, matching the
// PS3-style mapping used throughout:
//   Left/Right arrow, A/D, D-pad, or left stick   -> leftIsHeld / rightIsHeld
//   Space or A/Cross button                        -> jump
//   X (key) or X/Square button                     -> flip
//   Up arrow/W, D-pad up, left stick up, or Y/Triangle   -> up   (attach both hands)
//   Down arrow/S, D-pad down, left stick down, or B/Circle -> down (detach secondary hand)
//   R or Back/Select button                        -> restart
//   Return or Start button                         -> pause
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

    static SDL_GameController *sController = NULL;

    // Previous-frame state of the COMBINED (keyboard OR controller) signal
    // for each edge-triggered action -- edge detection has to happen on
    // the combined signal, not each source separately, or a press held on
    // one source while the other toggles could misfire an extra edge.
    static bool sPrevLeft = false, sPrevRight = false, sPrevJump = false;
    static bool sPrevFlip = false, sPrevRestart = false, sPrevPause = false;

    bool Init()
    {
        SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
        for (int i = 0; i < SDL_NumJoysticks(); i++)
        {
            if (SDL_IsGameController(i))
            {
                sController = SDL_GameControllerOpen(i);
                if (sController) break;
            }
        }
        return true;
    }

    void Shutdown()
    {
        if (sController)
        {
            SDL_GameControllerClose(sController);
            sController = NULL;
        }
    }

    static bool KeyHeld(const Uint8 *state, SDL_Scancode a, SDL_Scancode b)
    {
        return state[a] || state[b];
    }

    void Update()
    {
        SDL_PumpEvents();

        // Hot-plug: keep checking for a controller each frame until one
        // shows up -- cheap at 60Hz, lets a controller plugged in mid-
        // session start working without restarting the game.
        if (!sController)
        {
            for (int i = 0; i < SDL_NumJoysticks(); i++)
            {
                if (SDL_IsGameController(i))
                {
                    sController = SDL_GameControllerOpen(i);
                    if (sController) break;
                }
            }
        }

        const Uint8 *kb = SDL_GetKeyboardState(NULL);

        bool padLeft = false, padRight = false, padUp = false, padDown = false;
        bool padJump = false, padFlip = false, padRestart = false, padPause = false;
        if (sController)
        {
            const Sint16 kDeadzone = 8000;
            Sint16 axisX = SDL_GameControllerGetAxis(sController, SDL_CONTROLLER_AXIS_LEFTX);
            Sint16 axisY = SDL_GameControllerGetAxis(sController, SDL_CONTROLLER_AXIS_LEFTY);
            padLeft = SDL_GameControllerGetButton(sController, SDL_CONTROLLER_BUTTON_DPAD_LEFT) || axisX < -kDeadzone;
            padRight = SDL_GameControllerGetButton(sController, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || axisX > kDeadzone;
            padUp = SDL_GameControllerGetButton(sController, SDL_CONTROLLER_BUTTON_DPAD_UP) || axisY < -kDeadzone
                    || SDL_GameControllerGetButton(sController, SDL_CONTROLLER_BUTTON_Y);
            padDown = SDL_GameControllerGetButton(sController, SDL_CONTROLLER_BUTTON_DPAD_DOWN) || axisY > kDeadzone
                    || SDL_GameControllerGetButton(sController, SDL_CONTROLLER_BUTTON_B);
            padJump = SDL_GameControllerGetButton(sController, SDL_CONTROLLER_BUTTON_A) != 0;
            padFlip = SDL_GameControllerGetButton(sController, SDL_CONTROLLER_BUTTON_X) != 0;
            padRestart = SDL_GameControllerGetButton(sController, SDL_CONTROLLER_BUTTON_BACK) != 0;
            padPause = SDL_GameControllerGetButton(sController, SDL_CONTROLLER_BUTTON_START) != 0;
        }

        bool curLeft = KeyHeld(kb, SDL_SCANCODE_LEFT, SDL_SCANCODE_A) || padLeft;
        bool curRight = KeyHeld(kb, SDL_SCANCODE_RIGHT, SDL_SCANCODE_D) || padRight;
        bool curJump = KeyHeld(kb, SDL_SCANCODE_SPACE, SDL_SCANCODE_SPACE) || padJump;
        bool curFlip = KeyHeld(kb, SDL_SCANCODE_X, SDL_SCANCODE_X) || padFlip;
        // InputManager.cs uses IsPressed() (continuous hold) for these two,
        // not WasPressedThisFrame() like every other action -- so holding
        // Up/Down and waiting for the grip condition to become true works
        // in the source; matching that here (held, not edge-triggered).
        bool curUp = KeyHeld(kb, SDL_SCANCODE_UP, SDL_SCANCODE_W) || padUp;
        bool curDown = KeyHeld(kb, SDL_SCANCODE_DOWN, SDL_SCANCODE_S) || padDown;
        bool curRestart = KeyHeld(kb, SDL_SCANCODE_R, SDL_SCANCODE_R) || padRestart;
        bool curPause = KeyHeld(kb, SDL_SCANCODE_RETURN, SDL_SCANCODE_RETURN) || padPause;

        leftIsHeld = curLeft;
        leftIsPressed = curLeft && !sPrevLeft;
        rightIsHeld = curRight;
        rightIsPressed = curRight && !sPrevRight;

        jumpIsHeld = curJump;
        jumpWasPressed = curJump && !sPrevJump;
        jumpWasReleased = !curJump && sPrevJump;

        flipIsPressed = curFlip && !sPrevFlip;

        upIsPressed = curUp;
        downIsPressed = curDown;

        restartIsPressed = curRestart && !sPrevRestart;
        pauseIsPressed = curPause && !sPrevPause;

        sPrevLeft = curLeft;
        sPrevRight = curRight;
        sPrevJump = curJump;
        sPrevFlip = curFlip;
        sPrevRestart = curRestart;
        sPrevPause = curPause;
    }
}
