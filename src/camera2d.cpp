#include "camera2d.h"

static const float INF_SPEED = 1e9f;

Camera2D::Camera2D()
    : m_offset(0.0f, 0.0f), m_smoothTime(0.2f), m_pivotCatchUpSpeed(5.0f),
      m_lookAheadFactor(2.0f), m_lookAheadSmooth(0.3f),
      m_minZoom(6.0f), m_maxZoom(10.0f), m_zoomSpeedFactor(0.05f), m_flightActivationDelay(1.0f),
      m_pos(0.0f, 0.0f), m_velocity(0.0f, 0.0f), m_currentLookAhead(0.0f, 0.0f), m_lookAheadVelocity(0.0f, 0.0f),
      m_pivotPosition(0.0f, 0.0f), m_flightTimer(0.0f), m_flightCamActive(false), m_orthoSize(6.0f)
{
}

void Camera2D::Reset(Vec2 startPos)
{
    m_pivotPosition = startPos;
    m_pos = startPos + m_offset;
    m_velocity = Vec2(0.0f, 0.0f);
    m_currentLookAhead = Vec2(0.0f, 0.0f);
    m_lookAheadVelocity = Vec2(0.0f, 0.0f);
    m_flightTimer = 0.0f;
    m_flightCamActive = false;
    m_orthoSize = m_minZoom;
}

void Camera2D::Step(float dt, const Player &player)
{
    if (dt <= 0.0f) return;

    if (player.IsFlying())
    {
        m_flightTimer += dt;
        if (m_flightTimer >= m_flightActivationDelay) m_flightCamActive = true;
    }
    else
    {
        m_flightTimer = 0.0f;
        m_flightCamActive = false;
    }

    if (m_flightCamActive)
        m_pivotPosition = Vec2(Lerp(m_pivotPosition.x, player.BodyPos().x, m_pivotCatchUpSpeed * dt),
                                Lerp(m_pivotPosition.y, player.BodyPos().y, m_pivotCatchUpSpeed * dt));

    Vec2 targetPos;
    if (m_flightCamActive)
    {
        Vec2 moveDelta = player.BodyPos() - m_pivotPosition;
        Vec2 targetLookAhead = moveDelta.normalized() * m_lookAheadFactor;
        m_currentLookAhead = SmoothDamp(m_currentLookAhead, targetLookAhead, m_lookAheadVelocity, m_lookAheadSmooth, INF_SPEED, dt);

        targetPos = m_pivotPosition + m_currentLookAhead + m_offset;

        float speed = moveDelta.magnitude() / dt;
        float targetZoom = Lerp(m_minZoom, m_maxZoom, speed * m_zoomSpeedFactor);
        m_orthoSize = Lerp(m_orthoSize, targetZoom, dt * 2.0f);
    }
    else
    {
        m_currentLookAhead = Vec2(0.0f, 0.0f);
        targetPos = m_pivotPosition + m_offset;
        m_orthoSize = Lerp(m_orthoSize, m_minZoom, dt * 2.0f);
    }

    m_pos = SmoothDamp(m_pos, targetPos, m_velocity, m_smoothTime, INF_SPEED, dt);
}
