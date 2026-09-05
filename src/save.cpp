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
    static bool sLoaded = false;

    static void EnsureLoaded()
    {
        if (sLoaded) return;
        sLoaded = true;
        memset(sHas, 0, sizeof(sHas));
        memset(sBest, 0, sizeof(sBest));

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
}
