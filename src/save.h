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
    static const int kMaxLimbColors = 4; // matches App::kLimbGroupCount (3) with headroom

    // Idempotent -- safe to call any time; the first call actually reads the
    // file, later calls are no-ops until something is written again.
    void Load();
    void Flush();

    bool HasBest(int levelIndex);
    float GetBest(int levelIndex);
    // Persists immediately (calls Flush internally).
    void SetBest(int levelIndex, float value);

    // Character customizer color selection, per limb group (App::LimbGroup),
    // as an index into app.cpp's swatch palette. Returns 0 (the default/
    // as-authored swatch) for a limb never explicitly set, including on a
    // save file written before this existed.
    int GetLimbColor(int limbIndex);
    // Persists immediately (calls Flush internally).
    void SetLimbColor(int limbIndex, int colorIndex);
}

#endif
