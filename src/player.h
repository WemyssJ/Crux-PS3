#ifndef CRUX_PLAYER_H
#define CRUX_PLAYER_H

#include "vec2.h"
#include "level.h"

// Native port of Assets/Scripts/Player/PlayerController.cs.
//
// Simplification vs. the Unity version: instead of simulating the full bone
// hierarchy (body -> arms -> hands), we rely on the fact that in the original
// rig the two hand grab points (handMidPointLeft/Right) are *rigid, fixed-offset
// children of body* -- nothing ever moves them except body's own transform.
// That makes RotateAround(pivot=hand.position, ...) mathematically pivot-invariant
// (the classic pendulum property), so the grab point never actually needs to be
// simulated as a moving bone: we only need body's position/rotation plus the two
// constant local hand offsets to reproduce identical swing physics. Cosmetic
// arm/head/leg posing is a separate, later concern for the renderer, not physics.
class Player
{
public:
    Player();

    void Configure(Vec2 handOffsetLeft, Vec2 handOffsetRight, float handGripRadius);
    void Reset(Vec2 startPos);

    // Combines what PlayerController.cs split into Update()+FixedUpdate() into a
    // single fixed-timestep step (the PS3 build runs a fixed 60Hz loop, so the
    // Update/FixedUpdate distinction Unity has doesn't apply here).
    void Step(float dt, const LevelData &level);

    Vec2 BodyPos() const { return m_bodyPos; }
    float BodyRotZ() const { return m_bodyRotZ; }
    Vec2 HandWorldLeft() const { return HandWorld(true); }
    Vec2 HandWorldRight() const { return HandWorld(false); }
    float HandGripRadius() const { return m_handGripRadius; }

    // Cosmetic rig state, ported from PlayerController.cs's LateUpdate (UpdateArms
    // is skipped -- see the class comment on why arm angle is static rig geometry,
    // not per-frame state) and Update's hand-color logic.
    enum HandColor { kHandDefault, kHandNextGrip, kHandDelay, kHandFallGrab };
    float HeadRotZ() const { return m_headRotZ; }
    float LegAngleLeft() const { return m_legAngleLeft; }
    float LegAngleRight() const { return m_legAngleRight; }
    HandColor LeftHandColor() const { return m_handColorLeft; }
    HandColor RightHandColor() const { return m_handColorRight; }
    bool IsFlying() const { return m_isFlying; }
    bool IsFlipped() const { return m_isFlipped; }
    bool LeftGrabbing() const { return m_leftGrabbing; }
    bool BothHandsAttached() const { return m_bothHandsAttached; }

    // Stats, for ScoreManager / PlayerStatsTracker equivalents.
    int SwingJumps() const { return m_swingJumps; }
    int LaunchJumps() const { return m_launchJumps; }
    int Jumps() const { return m_swingJumps + m_launchJumps; }
    int LeftSwings() const { return m_leftSwings; }
    int RightSwings() const { return m_rightSwings; }
    int Swings() const { return m_leftSwings + m_rightSwings; }
    int Flips() const { return m_flips; }

private:
    Vec2 HandWorld(bool isLeft) const;
    bool HandOverlapsWall(bool isLeft, const LevelData &level) const;
    void HandleUpArrowAttach(const LevelData &level);
    void HandleSpaceInput(const LevelData &level);
    void HandleSwingPhysics(float dt);
    void HandleFlightPhysics(float dt);
    void SwapHandsPreserveDirection(const LevelData &level);
    void TryFlip();
    void UpdateHandColors(const LevelData &level);
    void UpdateHead(float dt);
    void UpdateLegs(float dt);

    // Rig
    Vec2 m_handOffsetLeft, m_handOffsetRight;
    float m_handGripRadius;

    // Body state
    Vec2 m_bodyPos;
    float m_bodyRotZ;

    // Grab/swing state
    Vec2 m_pivotWorld;
    bool m_leftGrabbing;
    bool m_isFlipped;
    bool m_bothHandsAttached;

    // Velocity state
    bool m_isFlying;
    float m_angularVelocity;
    Vec2 m_flightVelocity;

    // Input/timing state
    float m_spaceCooldown;
    bool m_spaceHeld;
    bool m_fallingAfterMiss;
    float m_flipBufferCounter;
    float m_spinInput, m_steerInput;

    // Stats
    int m_swingJumps, m_launchJumps, m_leftSwings, m_rightSwings, m_flips;

    // Cosmetic rig state
    float m_headRotZ;
    float m_legT, m_legFlapPhase, m_legAngleLeft, m_legAngleRight;
    HandColor m_handColorLeft, m_handColorRight;
    float m_maxHeadRotation;
    float m_maxSpeedForOpenLegs;

    // Tunables (defaults match PlayerController.cs's [SerializeField] defaults)
    float m_grabMomentumBoost;
    float m_playerAngularAcceleration;
    float m_gravityTorque;
    float m_maxAngularVelocity;
    float m_damping;
    float m_maxFlightSpeed;
    float m_flightMultiplier;
    float m_flightDecayY;
    float m_flightDecayX;
    float m_riseGravity;
    float m_fallGravity;
    float m_steerStrength;
    float m_flightSpinMultiplyer;
    float m_spaceCooldownDuration;
    float m_flipBufferTime;
    float m_launchSpeed;
};

#endif
