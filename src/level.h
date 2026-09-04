#ifndef CRUX_LEVEL_H
#define CRUX_LEVEL_H

#include "vec2.h"

// AABB trigger volume, matches the Collider2D.bounds center/size Unity gave us
// for LevelEndTrigger / LevelResetTrigger.
struct LevelTrigger
{
    Vec2 center;
    Vec2 size;
};

// Native counterpart of the wall Tilemap + trigger colliders exported per level
// by Assets/Editor/PS3LevelExporter.cs into data/levelN.lvl (see that file for
// the exact text format).
class LevelData
{
public:
    LevelData();
    ~LevelData();

    // path is relative to SYS_APP_HOME, e.g. "/data/level1.lvl".
    bool Load(const char *path);
    void Unload();

    // Builds a small hand-authored test room (floor + side walls + a scattering
    // of grip ledges) in place of real exported data. Used as a fallback until
    // Assets/Editor/PS3LevelExporter.cs has actually been run in the Unity
    // Editor (its output isn't checked in yet -- see the project plan).
    void LoadPlaceholderTestRoom();

    // Cell (cx, cy) is solid ground/wall.
    bool HasWallAtCell(int cx, int cy) const;

    // Matches PlayerController.HandGripColliderOverlapsWall: true if a circle at
    // `center` with `radius` touches any solid tile.
    bool CircleOverlapsWall(Vec2 center, float radius) const;

    bool PointInTrigger(Vec2 p, const LevelTrigger &t) const;

    // Returns the end trigger touched by an AABB centered at `pos` with
    // `halfExtent` half-size, or -1 if none. Same for reset triggers.
    int TouchesEndTrigger(Vec2 pos, float radius) const;
    int TouchesResetTrigger(Vec2 pos, float radius) const;

    Vec2 CellSize() const { return m_cellSize; }
    Vec2 Origin() const { return m_origin; }
    int Width() const { return m_width; }
    int Height() const { return m_height; }
    int BoundsMinX() const { return m_boundsMinX; }
    int BoundsMinY() const { return m_boundsMinY; }
    bool CellOccupied(int localX, int localY) const; // 0-based grid index, for rendering
    Vec2 PlayerStart() const { return m_playerStart; }

private:
    Vec2 m_cellSize;
    Vec2 m_origin;
    int m_boundsMinX, m_boundsMinY, m_boundsMaxX, m_boundsMaxY;
    int m_width, m_height;
    bool *m_cells; // m_width * m_height, row-major (x + y*m_width)

    Vec2 m_playerStart;

    static const int MAX_TRIGGERS = 16;
    LevelTrigger m_endTriggers[MAX_TRIGGERS];
    int m_endTriggerCount;
    LevelTrigger m_resetTriggers[MAX_TRIGGERS];
    int m_resetTriggerCount;
};

#endif
