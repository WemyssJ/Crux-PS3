#include "level.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

LevelData::LevelData()
    : m_cellSize(1.0f, 1.0f), m_origin(0.0f, 0.0f),
      m_boundsMinX(0), m_boundsMinY(0), m_boundsMaxX(0), m_boundsMaxY(0),
      m_width(0), m_height(0), m_cells(NULL),
      m_playerStart(0.0f, 0.0f), m_endTriggerCount(0), m_resetTriggerCount(0)
{
}

LevelData::~LevelData()
{
    Unload();
}

void LevelData::Unload()
{
    if (m_cells) { free(m_cells); m_cells = NULL; }
    m_width = m_height = 0;
    m_endTriggerCount = m_resetTriggerCount = 0;
}

bool LevelData::Load(const char *path)
{
    Unload();

    FILE *f = fopen(path, "r");
    if (!f) return false;

    char tag[32];
    char line[256];
    while (fgets(line, sizeof(line), f))
    {
        if (sscanf(line, "%31s", tag) != 1) continue;

        if (strcmp(tag, "CELLSIZE") == 0)
        {
            sscanf(line, "%*s %f %f", &m_cellSize.x, &m_cellSize.y);
        }
        else if (strcmp(tag, "ORIGIN") == 0)
        {
            sscanf(line, "%*s %f %f", &m_origin.x, &m_origin.y);
        }
        else if (strcmp(tag, "BOUNDS") == 0)
        {
            sscanf(line, "%*s %d %d %d %d", &m_boundsMinX, &m_boundsMinY, &m_boundsMaxX, &m_boundsMaxY);
            m_width = m_boundsMaxX - m_boundsMinX;
            m_height = m_boundsMaxY - m_boundsMinY;
            if (m_width > 0 && m_height > 0)
            {
                m_cells = (bool *)malloc(sizeof(bool) * m_width * m_height);
                memset(m_cells, 0, sizeof(bool) * m_width * m_height);
            }
        }
        else if (strcmp(tag, "CELLCOUNT") == 0)
        {
            // informational only; cells themselves follow as CELL lines.
        }
        else if (strcmp(tag, "CELL") == 0)
        {
            int cx, cy;
            sscanf(line, "%*s %d %d", &cx, &cy);
            int lx = cx - m_boundsMinX;
            int ly = cy - m_boundsMinY;
            if (m_cells && lx >= 0 && lx < m_width && ly >= 0 && ly < m_height)
                m_cells[lx + ly * m_width] = true;
        }
        else if (strcmp(tag, "PLAYERSTART") == 0)
        {
            sscanf(line, "%*s %f %f", &m_playerStart.x, &m_playerStart.y);
        }
        else if (strcmp(tag, "ENDTRIGGER") == 0)
        {
            if (m_endTriggerCount < MAX_TRIGGERS)
            {
                LevelTrigger &t = m_endTriggers[m_endTriggerCount++];
                sscanf(line, "%*s %f %f %f %f", &t.center.x, &t.center.y, &t.size.x, &t.size.y);
            }
        }
        else if (strcmp(tag, "RESETTRIGGER") == 0)
        {
            if (m_resetTriggerCount < MAX_TRIGGERS)
            {
                LevelTrigger &t = m_resetTriggers[m_resetTriggerCount++];
                sscanf(line, "%*s %f %f %f %f", &t.center.x, &t.center.y, &t.size.x, &t.size.y);
            }
        }
    }

    fclose(f);
    return true;
}

void LevelData::LoadPlaceholderLevel(int index)
{
    int count = PlaceholderLevelCount();
    int wrapped = ((index % count) + count) % count;
    switch (wrapped)
    {
        case 0: LoadPlaceholderShaft(); break;
        case 1: LoadPlaceholderCavern(); break;
        case 2: LoadPlaceholderChimney(); break;
    }
}

void LevelData::LoadPlaceholderShaft()
{
    Unload();

    // A tall vertical climbing shaft (not just a small flat room) so the
    // camera's flight-follow/zoom behavior (CameraController.cs's
    // flightCamActive path, ported in camera2d.cpp) actually gets exercised
    // -- that logic only kicks in after >1s of sustained flight, which a
    // small room never gives room for.
    m_cellSize = Vec2(1.0f, 1.0f);
    m_origin = Vec2(0.0f, 0.0f);
    m_boundsMinX = -8; m_boundsMinY = -3;
    m_boundsMaxX = 8; m_boundsMaxY = 46;
    m_width = m_boundsMaxX - m_boundsMinX;
    m_height = m_boundsMaxY - m_boundsMinY;
    m_cells = (bool *)malloc(sizeof(bool) * m_width * m_height);
    memset(m_cells, 0, sizeof(bool) * m_width * m_height);

    for (int cx = m_boundsMinX; cx < m_boundsMaxX; cx++)
        m_cells[(cx - m_boundsMinX) + (0 - m_boundsMinY) * m_width] = true; // floor

    // Full-height side walls, plus staggered ledges poking inward that
    // alternate left/right as you climb -- gaps between them widen with
    // height so the lower shaft is swing-only and the upper shaft forces
    // flight jumps between ledges (exercising both movement modes and the
    // flight camera).
    for (int cy = 1; cy < m_boundsMaxY - 1; cy++)
    {
        int ly = cy - m_boundsMinY;
        m_cells[(m_boundsMinX - m_boundsMinX) + ly * m_width] = true;
        m_cells[(m_boundsMaxX - 1 - m_boundsMinX) + ly * m_width] = true;

        int rung = cy % 6 < 3 ? cy % 6 : -1; // ledge every 3 rows within each 6-row band
        if (rung == 0 || rung == 3)
        {
            bool fromLeft = ((cy / 6) % 2) == 0;
            int reach = 3 + (cy / 10); // ledges reach further in higher up -> bigger gap to cross
            if (fromLeft)
            {
                for (int i = 1; i <= reach; i++)
                    m_cells[(m_boundsMinX + i - m_boundsMinX) + ly * m_width] = true;
            }
            else
            {
                for (int i = 1; i <= reach; i++)
                    m_cells[(m_boundsMaxX - 1 - i - m_boundsMinX) + ly * m_width] = true;
            }
        }
    }

    m_playerStart = Vec2(0.0f, 2.0f);

    m_endTriggerCount = 1;
    m_endTriggers[0].center = Vec2(0.0f, (float)(m_boundsMaxY - 2));
    m_endTriggers[0].size = Vec2((float)m_width, 1.0f);

    m_resetTriggerCount = 1;
    m_resetTriggers[0].center = Vec2(0.0f, (float)(m_boundsMinY + 0.5f));
    m_resetTriggers[0].size = Vec2((float)m_width, 1.0f);
}

void LevelData::LoadPlaceholderCavern()
{
    Unload();

    // Wide and comparatively short, with scattered floating platforms (not
    // touching either wall) instead of ledges -- forces flight-jump
    // traversal between islands rather than continuous swinging, the
    // opposite emphasis from the shaft level.
    m_cellSize = Vec2(1.0f, 1.0f);
    m_origin = Vec2(0.0f, 0.0f);
    m_boundsMinX = -20; m_boundsMinY = -3;
    m_boundsMaxX = 20; m_boundsMaxY = 22;
    m_width = m_boundsMaxX - m_boundsMinX;
    m_height = m_boundsMaxY - m_boundsMinY;
    m_cells = (bool *)malloc(sizeof(bool) * m_width * m_height);
    memset(m_cells, 0, sizeof(bool) * m_width * m_height);

    for (int cx = m_boundsMinX; cx < m_boundsMaxX; cx++)
        m_cells[(cx - m_boundsMinX) + (0 - m_boundsMinY) * m_width] = true; // floor

    // Floating islands, stepping up and to the right in a loose staircase,
    // each just out of comfortable swing range of the last -- gaps grow
    // with height like the shaft does, for the same reason.
    struct Island { int cx, cy, w; };
    static const Island islands[] = {
        { -14, 5, 4 }, { -6, 8, 3 }, { 2, 6, 4 }, { 9, 10, 3 },
        { -10, 13, 3 }, { -1, 16, 4 }, { 8, 14, 3 }, { 14, 18, 5 },
    };
    for (size_t i = 0; i < sizeof(islands) / sizeof(islands[0]); i++)
    {
        const Island &isl = islands[i];
        int ly = isl.cy - m_boundsMinY;
        if (ly < 0 || ly >= m_height) continue;
        for (int dx = 0; dx < isl.w; dx++)
        {
            int lx = (isl.cx + dx) - m_boundsMinX;
            if (lx >= 0 && lx < m_width) m_cells[lx + ly * m_width] = true;
        }
    }

    // Side walls so a missed jump doesn't sail off into the void sideways.
    for (int cy = 1; cy < m_boundsMaxY - 1; cy++)
    {
        int ly = cy - m_boundsMinY;
        m_cells[(m_boundsMinX - m_boundsMinX) + ly * m_width] = true;
        m_cells[(m_boundsMaxX - 1 - m_boundsMinX) + ly * m_width] = true;
    }

    m_playerStart = Vec2(-17.0f, 2.0f);

    m_endTriggerCount = 1;
    m_endTriggers[0].center = Vec2(16.0f, (float)(m_boundsMaxY - 3));
    m_endTriggers[0].size = Vec2(5.0f, 2.0f);

    m_resetTriggerCount = 1;
    m_resetTriggers[0].center = Vec2(0.0f, (float)(m_boundsMinY + 0.5f));
    m_resetTriggers[0].size = Vec2((float)m_width, 1.0f);
}

void LevelData::LoadPlaceholderChimney()
{
    Unload();

    // Narrow shaft (walls close together) -- forces frequent alternating
    // swings with little room to build flight speed, the opposite emphasis
    // from the cavern level. Ledges alternate almost every row.
    m_cellSize = Vec2(1.0f, 1.0f);
    m_origin = Vec2(0.0f, 0.0f);
    m_boundsMinX = -3; m_boundsMinY = -3;
    m_boundsMaxX = 3; m_boundsMaxY = 40;
    m_width = m_boundsMaxX - m_boundsMinX;
    m_height = m_boundsMaxY - m_boundsMinY;
    m_cells = (bool *)malloc(sizeof(bool) * m_width * m_height);
    memset(m_cells, 0, sizeof(bool) * m_width * m_height);

    for (int cx = m_boundsMinX; cx < m_boundsMaxX; cx++)
        m_cells[(cx - m_boundsMinX) + (0 - m_boundsMinY) * m_width] = true; // floor

    for (int cy = 1; cy < m_boundsMaxY - 1; cy++)
    {
        int ly = cy - m_boundsMinY;
        m_cells[(m_boundsMinX - m_boundsMinX) + ly * m_width] = true;
        m_cells[(m_boundsMaxX - 1 - m_boundsMinX) + ly * m_width] = true;

        if (cy % 2 == 0)
        {
            bool fromLeft = ((cy / 2) % 2) == 0;
            int lx = fromLeft ? (m_boundsMinX + 1 - m_boundsMinX) : (m_boundsMaxX - 2 - m_boundsMinX);
            m_cells[lx + ly * m_width] = true;
        }
    }

    m_playerStart = Vec2(0.0f, 2.0f);

    m_endTriggerCount = 1;
    m_endTriggers[0].center = Vec2(0.0f, (float)(m_boundsMaxY - 2));
    m_endTriggers[0].size = Vec2((float)m_width, 1.0f);

    m_resetTriggerCount = 1;
    m_resetTriggers[0].center = Vec2(0.0f, (float)(m_boundsMinY + 0.5f));
    m_resetTriggers[0].size = Vec2((float)m_width, 1.0f);
}

bool LevelData::HasWallAtCell(int cx, int cy) const
{
    int lx = cx - m_boundsMinX;
    int ly = cy - m_boundsMinY;
    if (!m_cells || lx < 0 || lx >= m_width || ly < 0 || ly >= m_height) return false;
    return m_cells[lx + ly * m_width];
}

bool LevelData::CellOccupied(int localX, int localY) const
{
    if (!m_cells || localX < 0 || localX >= m_width || localY < 0 || localY >= m_height) return false;
    return m_cells[localX + localY * m_width];
}

// A tile at grid cell (cx,cy) occupies world-space [origin + cx*cellSize, origin + (cx+1)*cellSize).
// Overlap test: does the circle come within `radius` of that AABB? We only need
// to check the handful of cells the circle's bounding box could touch.
bool LevelData::CircleOverlapsWall(Vec2 center, float radius) const
{
    if (!m_cells) return false;

    int minCx = (int)floorf((center.x - radius - m_origin.x) / m_cellSize.x);
    int maxCx = (int)floorf((center.x + radius - m_origin.x) / m_cellSize.x);
    int minCy = (int)floorf((center.y - radius - m_origin.y) / m_cellSize.y);
    int maxCy = (int)floorf((center.y + radius - m_origin.y) / m_cellSize.y);

    for (int cy = minCy; cy <= maxCy; cy++)
    {
        for (int cx = minCx; cx <= maxCx; cx++)
        {
            if (!HasWallAtCell(cx, cy)) continue;

            float tileMinX = m_origin.x + cx * m_cellSize.x;
            float tileMinY = m_origin.y + cy * m_cellSize.y;
            float tileMaxX = tileMinX + m_cellSize.x;
            float tileMaxY = tileMinY + m_cellSize.y;

            float closestX = Clamp(center.x, tileMinX, tileMaxX);
            float closestY = Clamp(center.y, tileMinY, tileMaxY);
            float dx = center.x - closestX;
            float dy = center.y - closestY;
            if (dx * dx + dy * dy <= radius * radius) return true;
        }
    }
    return false;
}

bool LevelData::PointInTrigger(Vec2 p, const LevelTrigger &t) const
{
    float hx = t.size.x * 0.5f;
    float hy = t.size.y * 0.5f;
    return p.x >= t.center.x - hx && p.x <= t.center.x + hx &&
           p.y >= t.center.y - hy && p.y <= t.center.y + hy;
}

static int touches(const LevelTrigger *triggers, int count, Vec2 pos, float radius)
{
    for (int i = 0; i < count; i++)
    {
        const LevelTrigger &t = triggers[i];
        float hx = t.size.x * 0.5f + radius;
        float hy = t.size.y * 0.5f + radius;
        if (pos.x >= t.center.x - hx && pos.x <= t.center.x + hx &&
            pos.y >= t.center.y - hy && pos.y <= t.center.y + hy)
            return i;
    }
    return -1;
}

int LevelData::TouchesEndTrigger(Vec2 pos, float radius) const
{
    return touches(m_endTriggers, m_endTriggerCount, pos, radius);
}

int LevelData::TouchesResetTrigger(Vec2 pos, float radius) const
{
    return touches(m_resetTriggers, m_resetTriggerCount, pos, radius);
}
