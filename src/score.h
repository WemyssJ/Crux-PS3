#ifndef CRUX_SCORE_H
#define CRUX_SCORE_H

#include "player.h"

// Native port of ScoreType.cs + ScoreManager.cs + LevelTimer.cs +
// PlayerStatsTracker.cs's scoring pieces (PlayFab online leaderboard is
// intentionally not ported -- offline/local scoring only, per project scope).
enum ScoreType
{
    kScoreTime,
    kScoreJumps,
    kScoreFlips,
    kScoreSwings
};

enum Medal
{
    kMedalPlatinum,
    kMedalGold,
    kMedalSilver,
    kMedalBronze
};

struct MedalThresholds
{
    float platinum, gold, silver;
    MedalThresholds() : platinum(30.0f), gold(60.0f), silver(120.0f) {}
};

class ScoreTracker
{
public:
    ScoreTracker();

    void Configure(ScoreType type, MedalThresholds thresholds);
    // Levels aren't the same shape, so a PB from one is meaningless (or
    // misleading) on another -- call when switching levels. Also means PBs
    // don't persist across a level revisit yet, same as they don't survive
    // a relaunch (see the TODO on StopRun) -- real per-level persistence is
    // still open.
    void ResetPersonalBest() { m_hasPB = false; m_personalBest = 0.0f; }
    void StartRun();
    // Call every frame while a run is active; drives the Time score type.
    void Tick(float dt);
    // Call once when the player reaches the level-end trigger.
    void StopRun(const Player &player);
    bool IsRunActive() const { return m_runActive; }

    float CurrentScore(const Player &player) const;
    bool HasPersonalBest() const { return m_hasPB; }
    float PersonalBest() const { return m_personalBest; }
    Medal MedalFor(float score) const;
    bool IsBetter(float current, float best) const;
    const MedalThresholds &Thresholds() const { return m_thresholds; }
    ScoreType Type() const { return m_type; }

    // Ported from ScoreManager.cs's FormatScore. buf must be at least 16 bytes.
    void FormatScore(float score, char *buf, int bufSize) const;

private:
    ScoreType m_type;
    MedalThresholds m_thresholds;
    bool m_runActive;
    float m_elapsed;
    float m_personalBest;
    bool m_hasPB;
};

#endif
