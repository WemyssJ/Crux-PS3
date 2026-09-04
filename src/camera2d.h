#ifndef CRUX_CAMERA2D_H
#define CRUX_CAMERA2D_H

#include "vec2.h"
#include "player.h"

// Native port of Assets/Scripts/CameraController.cs. The scripted intro pan
// (PanSequenceToPlayer, triggered by a debug 'C' keypress) is not ported --
// it's a one-off cosmetic sequence, not core gameplay.
class Camera2D
{
public:
    Camera2D();

    void Reset(Vec2 startPos);
    void Step(float dt, const Player &player);
    // Instant re-center, matching CameraController.UpdatePivot() -- call after
    // a re-grab so the look-ahead doesn't jump.
    void SnapPivot(Vec2 pos) { m_pivotPosition = pos; }

    Vec2 Position() const { return m_pos; }
    float OrthoSize() const { return m_orthoSize; }

private:
    Vec2 m_offset;
    float m_smoothTime;
    float m_pivotCatchUpSpeed;
    float m_lookAheadFactor;
    float m_lookAheadSmooth;
    float m_minZoom, m_maxZoom, m_zoomSpeedFactor;
    float m_flightActivationDelay;

    Vec2 m_pos;
    Vec2 m_velocity;
    Vec2 m_currentLookAhead;
    Vec2 m_lookAheadVelocity;
    Vec2 m_pivotPosition;
    float m_flightTimer;
    bool m_flightCamActive;
    float m_orthoSize;
};

#endif
