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

    bool Init();
    void Shutdown();
    // Polls the pad and refreshes all the state above. Call once per frame,
    // before any gameplay code reads the fields.
    void Update();
}

#endif
