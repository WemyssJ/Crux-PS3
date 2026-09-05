#include "input.h"
#include "padutil.h"

// The SampleBasic template (template/sampleBasic.cpp) owns the pad's
// init/shutdown/per-frame-update lifecycle (cellPadUtilPadInit/PadEnd/Update
// are called around onInit/onFinish/onUpdate). This backend only reads the
// already-updated button state each frame -- it must NOT also call
// cellPadUtilPadInit/PadEnd/Update, or the "pressed once" edge detection
// (which relies on padutil's internal old/current double-buffering) breaks.
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
    bool backIsPressed = false; // PC-only convenience (Escape key), no PS3 pad equivalent

    static const uint8_t PAD = 0;

    bool Init()
    {
        return true;
    }

    void Shutdown()
    {
    }

    void Update()
    {
        if (!cellPadUtilIsConnected(PAD))
        {
            jumpWasPressed = jumpIsHeld = jumpWasReleased = false;
            leftIsHeld = leftIsPressed = rightIsHeld = rightIsPressed = false;
            flipIsPressed = upIsPressed = downIsPressed = false;
            restartIsPressed = pauseIsPressed = false;
            return;
        }

        jumpIsHeld = cellPadUtilButtonPressed(PAD, CELL_UTIL_BUTTON_CROSS);
        jumpWasPressed = cellPadUtilButtonPressedOnce(PAD, CELL_UTIL_BUTTON_CROSS);
        jumpWasReleased = cellPadUtilButtonReleasedOnce(PAD, CELL_UTIL_BUTTON_CROSS);

        leftIsHeld = cellPadUtilButtonPressed(PAD, CELL_UTIL_BUTTON_LEFT);
        leftIsPressed = cellPadUtilButtonPressedOnce(PAD, CELL_UTIL_BUTTON_LEFT);
        rightIsHeld = cellPadUtilButtonPressed(PAD, CELL_UTIL_BUTTON_RIGHT);
        rightIsPressed = cellPadUtilButtonPressedOnce(PAD, CELL_UTIL_BUTTON_RIGHT);

        flipIsPressed = cellPadUtilButtonPressedOnce(PAD, CELL_UTIL_BUTTON_SQUARE);
        // InputManager.cs uses IsPressed() (continuous hold) for these two,
        // not WasPressedThisFrame() like every other action -- so holding
        // Up/Down and waiting for the grip condition to become true works
        // in the source; matching that here (held, not edge-triggered).
        upIsPressed = cellPadUtilButtonPressed(PAD, CELL_UTIL_BUTTON_TRIANGLE);
        downIsPressed = cellPadUtilButtonPressed(PAD, CELL_UTIL_BUTTON_CIRCLE);

        restartIsPressed = cellPadUtilButtonPressedOnce(PAD, CELL_UTIL_BUTTON_SELECT);
        pauseIsPressed = cellPadUtilButtonPressedOnce(PAD, CELL_UTIL_BUTTON_START);
    }

    bool ControllerConnected()
    {
        return true; // a pad is inherent to the platform
    }
}
