#ifndef CRUX_INPUT_H
#define CRUX_INPUT_H

// Native port of Assets/Scripts/InputManager.cs. Same static-field shape so
// player.cpp reads identically to PlayerController.cs's use of InputManager.
//
// Pad mapping (pad 0 only, matching the single-player game):
//   D-pad Left / Right -> leftIsHeld / rightIsHeld   (swing direction)
//   Cross               -> jump (swing-release jump / launch jump)
//   Square              -> flip
//   Triangle            -> up   (attach both hands)
//   Circle              -> down (detach secondary hand)
//   Select              -> restart
//   Start               -> pause
namespace Input
{
    extern bool jumpWasPressed;
    extern bool jumpIsHeld;
    extern bool jumpWasReleased;
    extern bool leftIsHeld;
    extern bool leftIsPressed;
    extern bool rightIsHeld;
    extern bool rightIsPressed;
    extern bool flipIsPressed;
    extern bool upIsPressed;
    extern bool downIsPressed;
    extern bool restartIsPressed;
    extern bool pauseIsPressed;
    // PC-only convenience (Escape key) -- no PS3 pad equivalent, always
    // false there. App::Update() treats it as "open pause" during gameplay
    // and "back/cancel" in every menu screen (including resuming from the
    // pause menu itself), context-dependent since those are two different
    // existing meanings (pauseIsPressed vs. restartIsPressed) that can't
    // just be OR'd into both blindly -- restartIsPressed means "reset
    // level" during gameplay, which Escape must NOT also trigger.
    extern bool backIsPressed;

    bool Init();
    void Shutdown();
    // Polls the pad and refreshes all the state above. Call once per frame,
    // before any gameplay code reads the fields.
    void Update();

    // True if input is currently coming from a gamepad rather than the
    // keyboard -- lets menu UI show PS3 button names (Cross/Square/etc.)
    // instead of keyboard hints. Always true on PS3 (a pad is inherent to
    // the platform); on PC, true once an SDL_GameController is connected.
    bool ControllerConnected();
}

#endif
