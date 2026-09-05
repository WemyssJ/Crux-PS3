#include "player.h"
#include "input.h"
#include <math.h>

static const float DEG2RAD = 3.14159265358979f / 180.0f;
static const float RAD2DEG = 180.0f / 3.14159265358979f;

Player::Player()
    : m_handOffsetLeft(-0.3f, 0.4f), m_handOffsetRight(0.3f, 0.4f),
      m_shoulderOffsetLeft(-0.3f, 0.2f), m_shoulderOffsetRight(0.3f, 0.2f), m_handGripRadius(0.15f),
      m_bodyPos(0.0f, 0.0f), m_bodyRotZ(0.0f),
      m_pivotWorld(0.0f, 0.0f), m_leftGrabbing(true), m_isFlipped(false), m_bothHandsAttached(false),
      m_pivotJustChanged(false),
      m_isFlying(false), m_angularVelocity(0.0f), m_flightVelocity(0.0f, 0.0f),
      m_spaceCooldown(0.0f), m_spaceHeld(false), m_fallingAfterMiss(false), m_flipBufferCounter(0.0f),
      m_spinInput(0.0f), m_steerInput(0.0f),
      m_swingJumps(0), m_launchJumps(0), m_leftSwings(0), m_rightSwings(0), m_flips(0),
      m_headRotZ(0.0f), m_legT(0.0f), m_legFlapPhase(0.0f), m_legAngleLeft(5.0f), m_legAngleRight(-5.0f),
      m_handColorLeft(kHandDefault), m_handColorRight(kHandDefault),
      m_maxHeadRotation(10.0f), m_maxSpeedForOpenLegs(150.0f),
      m_grabMomentumBoost(1.2f), m_playerAngularAcceleration(600.0f), m_gravityTorque(1200.0f),
      m_maxAngularVelocity(600.0f), m_damping(0.99f), m_maxFlightSpeed(300.0f), m_flightMultiplier(1.0f),
      m_flightDecayY(2.0f), m_flightDecayX(0.2f), m_riseGravity(15.0f), m_fallGravity(50.0f),
      m_steerStrength(10.0f), m_flightSpinMultiplyer(0.5f), m_spaceCooldownDuration(0.33f),
      m_flipBufferTime(0.15f), m_launchSpeed(15.0f)
{
}

void Player::Configure(Vec2 handOffsetLeft, Vec2 handOffsetRight, float handGripRadius)
{
    m_handOffsetLeft = handOffsetLeft;
    m_handOffsetRight = handOffsetRight;
    m_handGripRadius = handGripRadius;
}

void Player::ConfigureShoulders(Vec2 shoulderOffsetLeft, Vec2 shoulderOffsetRight)
{
    m_shoulderOffsetLeft = shoulderOffsetLeft;
    m_shoulderOffsetRight = shoulderOffsetRight;
}

void Player::Reset(Vec2 startPos)
{
    m_bodyPos = startPos;
    m_bodyRotZ = 0.0f;
    m_leftGrabbing = true;
    m_pivotWorld = HandWorld(true);
    m_isFlipped = false;
    m_bothHandsAttached = false;
    m_pivotJustChanged = false;
    m_isFlying = false;
    m_angularVelocity = 0.0f;
    m_flightVelocity = Vec2(0.0f, 0.0f);
    m_spaceCooldown = 0.0f;
    m_spaceHeld = false;
    m_fallingAfterMiss = false;
    m_flipBufferCounter = 0.0f;
    m_swingJumps = m_launchJumps = m_leftSwings = m_rightSwings = m_flips = 0;
    m_headRotZ = m_bodyRotZ;
    m_legT = 0.0f;
    m_legFlapPhase = 0.0f;
    m_legAngleLeft = 5.0f;
    m_legAngleRight = -5.0f;
    m_handColorLeft = m_handColorRight = kHandDefault;
}

Vec2 Player::HandWorld(bool isLeft) const
{
    Vec2 offset = isLeft ? m_handOffsetLeft : m_handOffsetRight;
    return m_bodyPos + RotateAround(offset, Vec2(0.0f, 0.0f), m_bodyRotZ);
}

bool Player::HandOverlapsWall(bool isLeft, const LevelData &level) const
{
    return level.CircleOverlapsWall(HandWorld(isLeft), m_handGripRadius);
}

void Player::HandleUpArrowAttach(const LevelData &level)
{
    if (Input::upIsPressed)
    {
        if (HandOverlapsWall(true, level) && HandOverlapsWall(false, level))
        {
            m_bothHandsAttached = true;
            m_isFlying = false;
            m_angularVelocity = 0.0f;
            m_flightVelocity = Vec2(0.0f, 0.0f);
            m_pivotJustChanged = true;
        }
    }

    if (m_bothHandsAttached && Input::jumpWasPressed)
    {
        if (!m_isFlying) m_launchJumps++;
        m_bothHandsAttached = false;
        m_isFlying = true;

        Vec2 launchDir = RotateAround(Vec2(0.0f, 1.0f), Vec2(0.0f, 0.0f), m_bodyRotZ);
        m_flightVelocity = launchDir.normalized() * m_launchSpeed;
        m_angularVelocity = 0.0f;
    }

    if (m_bothHandsAttached && Input::downIsPressed)
    {
        m_bothHandsAttached = false;
        SwapHandsPreserveDirection(level);

        Vec2 pivotToBody = m_bodyPos - m_pivotWorld;
        float radius = pivotToBody.magnitude();
        if (radius > 1e-6f)
        {
            Vec2 tangentCCW = Vec2(-pivotToBody.y, pivotToBody.x).normalized();
            Vec2 tangentCW = tangentCCW * -1.0f;

            float accelCCW = Dot(Vec2(0.0f, -1.0f) * m_fallGravity, tangentCCW);
            float accelCW = Dot(Vec2(0.0f, -1.0f) * m_fallGravity, tangentCW);
            Vec2 chosenTangent = accelCCW > accelCW ? tangentCCW : tangentCW;

            float tangentialSpeed = Dot(m_flightVelocity, chosenTangent);
            m_angularVelocity = tangentialSpeed / radius * RAD2DEG;
        }

        m_isFlying = false;
        // Unconditional in the source (PlayerController.cs's down-detach
        // branch calls cameraController?.UpdatePivot() regardless of whether
        // SwapHandsPreserveDirection above actually found a new grip).
        m_pivotJustChanged = true;
    }
}

void Player::HandleSpaceInput(const LevelData &level)
{
    if (m_spaceCooldown > 0.0f) return;
    if (m_bothHandsAttached) return;

    if (Input::jumpWasPressed)
    {
        if (!m_isFlying) m_swingJumps++;
        m_spaceHeld = true;
    }

    if (Input::jumpWasReleased && m_spaceHeld)
    {
        if (HandOverlapsWall(true, level) || HandOverlapsWall(false, level))
        {
            m_fallingAfterMiss = false;
            if (m_isFlying) m_isFlying = false;
            SwapHandsPreserveDirection(level);
            m_spaceCooldown = m_spaceCooldownDuration;
        }
        else if (m_fallingAfterMiss)
        {
            bool leftCanGrab = HandOverlapsWall(true, level);
            bool rightCanGrab = HandOverlapsWall(false, level);

            if (leftCanGrab || rightCanGrab)
            {
                m_leftGrabbing = leftCanGrab;
                m_pivotWorld = HandWorld(m_leftGrabbing);
                m_isFlying = false;
                m_fallingAfterMiss = false;

                Vec2 pivotToBody = m_bodyPos - m_pivotWorld;
                float radius = pivotToBody.magnitude();
                if (radius > 1e-6f)
                {
                    float speed = fabsf(m_angularVelocity) * DEG2RAD * radius;
                    m_angularVelocity = Sign(m_angularVelocity) * RAD2DEG * speed / radius;
                }
                m_pivotJustChanged = true;
                m_spaceCooldown = m_spaceCooldownDuration;
            }
            else
            {
                m_isFlying = true;
                m_fallingAfterMiss = true;
            }
        }
        else
        {
            m_isFlying = true;
            m_fallingAfterMiss = true;
        }

        m_spaceHeld = false;
    }

    if (Input::jumpIsHeld && !m_isFlying && m_spaceHeld)
    {
        if (HandOverlapsWall(m_leftGrabbing, level))
        {
            // StartFlight, ported inline (the standalone method above only
            // guards the case that can't happen given this call site).
            Vec2 pivotToBody = m_bodyPos - m_pivotWorld;
            float radius = pivotToBody.magnitude();
            Vec2 tangent = Vec2(-pivotToBody.y, pivotToBody.x).normalized();
            float speed = fabsf(m_angularVelocity) * DEG2RAD * radius * m_flightMultiplier;
            if (m_angularVelocity < 0.0f) tangent = tangent * -1.0f;

            m_isFlying = true;
            m_flightVelocity = ClampMagnitude(tangent * speed, m_maxFlightSpeed);
            m_pivotJustChanged = true;
        }
        else
        {
            m_isFlying = true;
            m_fallingAfterMiss = true;
        }
    }
}

void Player::HandleSwingPhysics(float dt)
{
    float speedFactor = 1.0f + fabsf(m_angularVelocity) / m_maxAngularVelocity;
    m_angularVelocity += m_spinInput * m_playerAngularAcceleration * dt * speedFactor;

    Vec2 pivotToBody = m_bodyPos - m_pivotWorld;
    float angleFromDown = SignedAngle(Vec2(0.0f, -1.0f), pivotToBody.normalized());
    m_angularVelocity -= sinf(DEG2RAD * angleFromDown) * m_gravityTorque * dt * speedFactor;

    m_angularVelocity *= m_damping;
    m_angularVelocity = Clamp(m_angularVelocity, -m_maxAngularVelocity, m_maxAngularVelocity);

    float deltaDeg = m_angularVelocity * dt;
    m_bodyPos = RotateAround(m_bodyPos, m_pivotWorld, deltaDeg);
    m_bodyRotZ += deltaDeg;
}

void Player::HandleFlightPhysics(float dt)
{
    m_bodyPos += m_flightVelocity * dt;
    m_flightVelocity.x += m_steerInput * m_steerStrength * dt;
    m_flightVelocity.y += (m_flightVelocity.y > 0.0f ? -m_riseGravity : -m_fallGravity) * dt;
    m_flightVelocity.y /= 1.0f + m_flightDecayY * dt;
    m_flightVelocity.x *= powf(m_flightDecayX, dt);
    m_flightVelocity = ClampMagnitude(m_flightVelocity, m_maxFlightSpeed);

    m_angularVelocity += m_spinInput * m_flightSpinMultiplyer * m_playerAngularAcceleration * dt;
    m_angularVelocity = Clamp(m_angularVelocity, -m_maxAngularVelocity * 0.25f, m_maxAngularVelocity * 0.25f);

    m_bodyRotZ += m_isFlipped ? -m_angularVelocity * dt : m_angularVelocity * dt;
}

void Player::SwapHandsPreserveDirection(const LevelData &level)
{
    bool candidateIsLeft = !m_leftGrabbing;
    if (!HandOverlapsWall(candidateIsLeft, level)) candidateIsLeft = m_leftGrabbing;
    if (!HandOverlapsWall(candidateIsLeft, level)) return;

    Vec2 newPivot = HandWorld(candidateIsLeft);
    m_leftGrabbing = candidateIsLeft;

    Vec2 pivotToBody = m_bodyPos - newPivot;
    float radius = pivotToBody.magnitude();
    if (radius > 1e-6f)
    {
        Vec2 tangent = Vec2(-pivotToBody.y, pivotToBody.x).normalized();
        float projectedVelocity = Dot(m_flightVelocity, tangent);
        projectedVelocity = projectedVelocity * 0.5f + projectedVelocity * m_grabMomentumBoost * 0.5f;

        m_angularVelocity = projectedVelocity / radius * RAD2DEG;
        m_flightVelocity = tangent * projectedVelocity;
    }

    m_pivotWorld = newPivot;
    m_pivotJustChanged = true;
}

void Player::UpdateHandColors(const LevelData &level)
{
    if (m_spaceCooldown > 0.0f)
    {
        m_handColorLeft = m_handColorRight = kHandDelay;
        return;
    }
    if (m_bothHandsAttached)
    {
        m_handColorLeft = m_handColorRight = kHandNextGrip;
        return;
    }
    if (!m_isFlying)
    {
        m_handColorLeft = m_handColorRight = kHandDefault;
        return;
    }

    bool leftCanGrip = HandOverlapsWall(true, level);
    bool rightCanGrip = HandOverlapsWall(false, level);
    bool nextIsLeft = !m_leftGrabbing;
    bool nextCanGrip = nextIsLeft ? leftCanGrip : rightCanGrip;
    bool otherCanGrip = nextIsLeft ? rightCanGrip : leftCanGrip;

    HandColor next, other, fallback;
    if (m_fallingAfterMiss)
    {
        next = kHandNextGrip;
        other = kHandFallGrab;
        fallback = kHandFallGrab;
    }
    else
    {
        next = kHandNextGrip;
        other = kHandDefault;
        fallback = kHandDefault;
    }

    HandColor leftColor, rightColor;
    if (nextCanGrip)
    {
        leftColor = nextIsLeft ? next : other;
        rightColor = nextIsLeft ? other : next;
    }
    else if (otherCanGrip)
    {
        leftColor = nextIsLeft ? other : next;
        rightColor = nextIsLeft ? next : other;
    }
    else
    {
        leftColor = rightColor = fallback;
    }

    m_handColorLeft = leftColor;
    m_handColorRight = rightColor;
}

void Player::UpdateHead(float dt)
{
    float currentRelativeZ = DeltaAngle(m_bodyRotZ, m_headRotZ);
    float clampedRelativeZ = Clamp(currentRelativeZ, -m_maxHeadRotation, m_maxHeadRotation);
    float newRelativeZ = MoveTowardsAngle(currentRelativeZ, clampedRelativeZ, 180.0f * dt);
    m_headRotZ = m_bodyRotZ + newRelativeZ;
}

void Player::UpdateLegs(float dt)
{
    float speed = m_isFlying ? m_flightVelocity.magnitude() : fabsf(m_angularVelocity);
    float targetT = Clamp01(speed / m_maxSpeedForOpenLegs);
    m_legT = Lerp(m_legT, targetT, dt * 7.0f);

    float baseLeft = Lerp(5.0f, -50.0f, m_legT);
    float baseRight = Lerp(-5.0f, 50.0f, m_legT);

    if (m_isFlying)
    {
        m_legFlapPhase += dt * (5.0f + speed * 0.05f);
        float flapAmplitude = 15.0f + speed * 0.02f;
        float flapOffset = sinf(m_legFlapPhase) * flapAmplitude;
        baseLeft += flapOffset;
        baseRight -= flapOffset;
    }

    m_legAngleLeft = baseLeft;
    m_legAngleRight = baseRight;
}

void Player::TryFlip()
{
    // Earlier version of this comment argued the physical flip "reduces to
    // a no-op" since our pivot is tracked explicitly -- WRONG, and flagged
    // by the user after playtesting (pressing flip did nothing to the
    // actual swing). Re-read PlayerController.cs's FlipAroundCurrentHandPivot
    // precisely: it reflects the body 180deg around an axis running from the
    // gripping arm's shoulder through the current hand pivot (keeping the
    // pivot itself fixed), then recomputes angularVelocity's SIGN so the
    // real-world tangential swing direction is preserved -- a reflection
    // doesn't change speed, only (possibly) which way "positive" points
    // relative to the new position. That's the actual "change which way
    // you're swinging" effect the flip button is for.
    //
    // CanFlip() in the source requires !isFlying && currentVisualPivot !=
    // null; currentVisualPivot is explicitly nulled while both-hands-
    // attached, so both-hands-attached implicitly can't flip either --
    // this project has no null-pivot state, so check it explicitly instead.
    if (m_isFlying || m_bothHandsAttached) return;

    m_isFlipped = !m_isFlipped;
    m_flips++;

    Vec2 handOffset = m_leftGrabbing ? m_handOffsetLeft : m_handOffsetRight;
    Vec2 shoulderOffset = m_leftGrabbing ? m_shoulderOffsetLeft : m_shoulderOffsetRight;

    // Flip axis direction, world space: shoulder->pivot. Both shoulder and
    // hand-grip are fixed offsets on the same rigid body-local rotation, so
    // their world-space difference is just that same fixed local difference
    // rotated by bodyRotZ (rotation distributes linearly over vector
    // subtraction when rotating around a shared origin).
    Vec2 axisDir = RotateAround(handOffset - shoulderOffset, Vec2(0.0f, 0.0f), m_bodyRotZ).normalized();

    // d = pivot->body (matches the source's pivotToBodyBefore).
    Vec2 d = m_bodyPos - m_pivotWorld;
    float radius = d.magnitude();
    if (radius < 1e-4f) return; // degenerate; leave position/velocity alone

    // Reflect d across the line through the origin in direction axisDir.
    Vec2 dReflected = axisDir * (2.0f * Dot(d, axisDir)) - d;
    m_bodyPos = m_pivotWorld + dReflected;

    // Recompute angularVelocity's sign to preserve the real-world tangential
    // velocity direction (magnitude is unchanged by a reflection).
    Vec2 tangentBefore = Vec2(-d.y, d.x) * (1.0f / radius);
    float tangentialSpeed = m_angularVelocity * DEG2RAD * radius;
    Vec2 velocityWorld = tangentBefore * tangentialSpeed;

    Vec2 tangentAfter = Vec2(-dReflected.y, dReflected.x) * (1.0f / radius);
    float sign = Dot(velocityWorld, tangentAfter) >= 0.0f ? 1.0f : -1.0f;
    m_angularVelocity = sign * fabsf(tangentialSpeed) / radius * RAD2DEG;

    // Solve bodyRotZ directly (rather than accumulating a delta) so
    // HandWorld(handOffset) lands back exactly on the unchanged pivot --
    // i.e. RotateAround(handOffset, bodyRotZ) == -dReflected.
    float targetAngle = atan2f(-dReflected.y, -dReflected.x) * RAD2DEG;
    float handOffsetAngle = atan2f(handOffset.y, handOffset.x) * RAD2DEG;
    m_bodyRotZ = targetAngle - handOffsetAngle;
}

void Player::Step(float dt, const LevelData &level)
{
    m_pivotJustChanged = false;
    if (m_spaceCooldown > 0.0f) m_spaceCooldown -= dt;

    m_spinInput = 0.0f;
    m_steerInput = 0.0f;
    if (Input::leftIsHeld)
    {
        m_spinInput -= 1.0f;
        m_steerInput -= 1.0f;
        if (Input::leftIsPressed && !m_isFlying) m_leftSwings++;
    }
    if (Input::rightIsHeld)
    {
        m_spinInput += 1.0f;
        m_steerInput += 1.0f;
        if (Input::rightIsPressed && !m_isFlying) m_rightSwings++;
    }

    HandleUpArrowAttach(level);
    HandleSpaceInput(level);
    UpdateHandColors(level);

    if (Input::flipIsPressed) m_flipBufferCounter = m_flipBufferTime;
    if (m_flipBufferCounter > 0.0f)
    {
        m_flipBufferCounter -= dt;
        if (!m_isFlying)
        {
            TryFlip();
            m_flipBufferCounter = 0.0f;
        }
    }

    if (m_isFlying) HandleFlightPhysics(dt);
    else if (!m_bothHandsAttached) HandleSwingPhysics(dt);

    UpdateHead(dt);
    UpdateLegs(dt);
}
