#include "save.h"
#include <stdio.h>
#include <string.h>

#if defined(CRUX_PS3)
#define SAVE_PATH SYS_APP_HOME "/save.dat"
#include <sys/paths.h>
#else
#define SAVE_PATH "save.dat"
#endif

namespace Save
{
    static bool sHas[kMaxLevels];
    static float sBest[kMaxLevels];
    static int sLimbColor[kMaxLimbColors];
    static bool sLoaded = false;

    static void EnsureLoaded()
    {
        if (sLoaded) return;
        sLoaded = true;
        memset(sHas, 0, sizeof(sHas));
        memset(sBest, 0, sizeof(sBest));
        memset(sLimbColor, 0, sizeof(sLimbColor));

        FILE *f = fopen(SAVE_PATH, "r");
        if (!f) return;

        int idx, has;
        float val;
        while (fscanf(f, "%d %d %f", &idx, &has, &val) == 3)
        {
            if (idx >= 0 && idx < kMaxLevels)
            {
                sHas[idx] = has != 0;
                sBest[idx] = val;
            }
        }

        // Optional trailing line, added after the per-level lines. Absent on
        // a save written before customizer colors persisted -- sLimbColor
        // just stays all-zero (the default swatch) in that case, matching
        // the old "resets to default each run" behavior.
        char tag[16];
        if (fscanf(f, "%15s", tag) == 1 && strcmp(tag, "LIMBS") == 0)
        {
            for (int i = 0; i < kMaxLimbColors; i++)
            {
                int v;
                if (fscanf(f, "%d", &v) != 1) break;
                sLimbColor[i] = v;
            }
        }

        fclose(f);
    }

    void Load()
    {
        sLoaded = false;
        EnsureLoaded();
    }

    void Flush()
    {
        EnsureLoaded();
        FILE *f = fopen(SAVE_PATH, "w");
        if (!f) return;
        for (int i = 0; i < kMaxLevels; i++)
            fprintf(f, "%d %d %.6f\n", i, sHas[i] ? 1 : 0, sBest[i]);
        fprintf(f, "LIMBS");
        for (int i = 0; i < kMaxLimbColors; i++)
            fprintf(f, " %d", sLimbColor[i]);
        fprintf(f, "\n");
        fclose(f);
    }

    bool HasBest(int levelIndex)
    {
        EnsureLoaded();
        if (levelIndex < 0 || levelIndex >= kMaxLevels) return false;
        return sHas[levelIndex];
    }

    float GetBest(int levelIndex)
    {
        EnsureLoaded();
        if (levelIndex < 0 || levelIndex >= kMaxLevels) return 0.0f;
        return sBest[levelIndex];
    }

    void SetBest(int levelIndex, float value)
    {
        EnsureLoaded();
        if (levelIndex < 0 || levelIndex >= kMaxLevels) return;
        sHas[levelIndex] = true;
        sBest[levelIndex] = value;
        Flush();
    }

    int GetLimbColor(int limbIndex)
    {
        EnsureLoaded();
        if (limbIndex < 0 || limbIndex >= kMaxLimbColors) return 0;
        return sLimbColor[limbIndex];
    }

    void SetLimbColor(int limbIndex, int colorIndex)
    {
        EnsureLoaded();
        if (limbIndex < 0 || limbIndex >= kMaxLimbColors) return;
        sLimbColor[limbIndex] = colorIndex;
        Flush();
    }
}
