#ifndef CRUX_SAVE_H
#define CRUX_SAVE_H

// Per-level personal-best persistence.
//
// Simplified: plain stdio file I/O, not (yet) PS3's full cellSaveData
// dialog/icon/PARAM.SFO-metadata system -- that's a much bigger, multi-step
// API (save-data dialogs, icon assets, etc.) that can't be verified without
// real hardware anyway. This gets real persistence working and testable on
// PC now; see TODO.md for the follow-up if the polished PS3 save-icon UX is
// wanted later. Not tested on real PS3 hardware either way (untestable
// without the devkit).
namespace Save
{
    static const int kMaxLevels = 16;

    // Idempotent -- safe to call any time; the first call actually reads the
    // file, later calls are no-ops until something is written again.
    void Load();
    void Flush();

    bool HasBest(int levelIndex);
    float GetBest(int levelIndex);
    // Persists immediately (calls Flush internally).
    void SetBest(int levelIndex, float value);
}

#endif
