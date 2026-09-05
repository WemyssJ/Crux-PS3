#include "score.h"
#include <stdio.h>
#include <math.h>

ScoreTracker::ScoreTracker()
    : m_type(kScoreTime), m_runActive(false), m_elapsed(0.0f), m_personalBest(0.0f), m_hasPB(false)
{
}

void ScoreTracker::Configure(ScoreType type, MedalThresholds thresholds)
{
    m_type = type;
    m_thresholds = thresholds;
}

void ScoreTracker::StartRun()
{
    m_runActive = true;
    m_elapsed = 0.0f;
}

void ScoreTracker::Tick(float dt)
{
    if (m_runActive) m_elapsed += dt;
}

float ScoreTracker::CurrentScore(const Player &player) const
{
    switch (m_type)
    {
        case kScoreTime: return m_elapsed;
        case kScoreJumps: return (float)player.Jumps();
        case kScoreFlips: return (float)player.Flips();
        case kScoreSwings: return (float)player.Swings();
    }
    return 0.0f;
}

bool ScoreTracker::IsBetter(float current, float best) const
{
    switch (m_type)
    {
        case kScoreTime:
        case kScoreJumps:
        case kScoreSwings:
            return current < best;
        case kScoreFlips:
            return current > best;
    }
    return false;
}

void ScoreTracker::StopRun(const Player &player)
{
    if (!m_runActive) return;
    m_runActive = false;

    float current = CurrentScore(player);
    if (!m_hasPB || IsBetter(current, m_personalBest))
    {
        m_personalBest = current;
        m_hasPB = true;
        // Actual disk persistence lives one layer up: ScoreTracker doesn't
        // know about levels, so App:: (which does) calls Save::SetBest with
        // PersonalBest() right after this returns -- see app.cpp's Update().
    }
}

Medal ScoreTracker::MedalFor(float score) const
{
    if (score <= m_thresholds.platinum) return kMedalPlatinum;
    if (score <= m_thresholds.gold) return kMedalGold;
    if (score <= m_thresholds.silver) return kMedalSilver;
    return kMedalBronze;
}

void ScoreTracker::FormatScore(float score, char *buf, int bufSize) const
{
    if (m_type == kScoreTime)
    {
        int minutes = (int)floorf(score / 60.0f);
        int seconds = (int)floorf(fmodf(score, 60.0f));
        int millis = (int)floorf(fmodf(score * 1000.0f, 1000.0f));
        snprintf(buf, bufSize, "%02d:%02d:%03d", minutes, seconds, millis);
    }
    else
    {
        snprintf(buf, bufSize, "%d", (int)(score + (score >= 0.0f ? 0.5f : -0.5f)));
    }
}
