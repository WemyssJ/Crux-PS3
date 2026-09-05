#include "app.h"
#include "render.h"
#include "input.h"
#include "level.h"
#include "player.h"
#include "camera2d.h"
#include "score.h"
#include "save.h"
#include <math.h>
#include <stdio.h>

#if defined(CRUX_PS3)
#define LEVEL_PATH_FMT SYS_APP_HOME "/data/level%d.lvl"
#include <sys/paths.h>
#else
#define LEVEL_PATH_FMT "data/level%d.lvl"
#endif

namespace App
{
    static LevelData sLevel;
    static Player sPlayer;
    static Camera2D sCamera;
    static ScoreTracker sScore;
    static bool sUsingPlaceholder = false;
    static int sLevelIndex = 0;

    // Tries data/level<N+1>.lvl (real exported data, once
    // PS3/Export-Level-+-Rig-Data has been run in Unity -- see TODO.md)
    // before falling back to one of LevelData's built-in placeholder
    // layouts. Ported flow: LevelEndTrigger reaching the end (via
    // TouchesEndTrigger in Update()) advances to the next index and wraps,
    // matching a simple linear level-progression loop (no level-select menu
    // yet -- see TODO.md).
    static void LoadLevelByIndex(int index)
    {
        char path[128];
        snprintf(path, sizeof(path), LEVEL_PATH_FMT, index + 1);

        if (!sLevel.Load(path))
        {
            sLevel.LoadPlaceholderLevel(index);
            sUsingPlaceholder = true;
        }
        else
        {
            sUsingPlaceholder = false;
        }

        sPlayer.Reset(sLevel.PlayerStart());
        sCamera.Reset(sLevel.PlayerStart());
        if (Save::HasBest(index))
            sScore.SetPersonalBest(Save::GetBest(index));
        else
            sScore.ResetPersonalBest();
        sScore.StartRun();
    }

    // Derived from Assets/Prefab/Player.prefab's real local transforms:
    // Arm at (-0.45,0.725) rel. Body rotated 55deg, with Hand+HandGrip a
    // further (0,2.25) out in Arm-local space -- gripRelBody =
    // armLocalPos + RotateCCW((0,2.25), 55deg), scaled by this project's
    // 0.6 Unity-relative unit factor (kBodySize 0.6x1.2 vs. Body.png's
    // real 1.0x2.0). Right arm mirrors exactly (Hand's local x is 0, so
    // Arm(R)'s -1 x-scale has no effect on this vector).
    // Iteration 5 history: first tried this, then reverted it after
    // judging it a regression against an EARLIER guess -- but that guess
    // was itself never checked against real footage, only against a
    // vaguer "V-shaped reach" impression. With actual side-by-side
    // reference screenshots of the real game now available (both arms
    // reaching in a wide, nearly-straight diagonal spanning past both
    // shoulders), this derived value's wider/more-horizontal reach is
    // the better match, not the earlier guess's narrower/more-vertical
    // one -- re-applying it. This also sets the swing pivot radius (see
    // player.h's pivot-invariance note), not just the visual.
    static const Vec2 kHandOffsetLeft(-1.376f, 1.209f);
    static const Vec2 kHandOffsetRight(1.376f, 1.209f);
    static const float kHandGripRadius = 0.15f;

    // Rig sprite proportions -- also placeholder-approximate until real
    // per-part transforms are exported (see TODO.md, "Real rig proportions").
    // Aspect ratios (Body 256x512, Head 256x256, Arm/Leg 63x512, Hand 128x128)
    // are the real source PNG dimensions, so sprites at least aren't stretched.
    // Iteration 2 (rig-proportion pass): reference screenshots show a
    // proportionally larger, rounder head and a stubbier torso/legs than the
    // first pass had -- shrunk the body, grew the head, shortened the legs.
    static const Vec2 kBodySize(0.6f, 1.2f);
    static const Vec2 kHeadSize(0.62f, 0.62f);
    // User-corrected: the neck is already part of Body.png's own torso art
    // (visible at its top edge), not a separate missing piece -- pulling the
    // head down to 0.56 buried it too low into the torso instead of sitting
    // on top of the neck that's already there. Raised above the original
    // 0.68 so the head anchors cleanly on top of the torso's neck.
    // User-corrected: head needed to sit slightly higher still.
    static const Vec2 kHeadLocalOffset(0.0f, 0.82f);
    // User-corrected: arms/shoulders read as too close in against the
    // torso -- pushed the anchor out slightly.
    static const Vec2 kShoulderOffsetLeft(-0.32f, 0.5f);
    static const Vec2 kShoulderOffsetRight(0.32f, 0.5f);
    static const Vec2 kShoulderCapSize(0.30f, 0.30f);
    // User-corrected: it's a waist pouch, not a backpack -- moved down from
    // the upper back (0, 0.1) to waist/belt height, and shrunk to match.
    // Iteration 2 (user-corrected): sat too low/deep -- raised back up some.
    static const Vec2 kBagLocalOffset(0.0f, -0.25f);
    static const Vec2 kBagSize(0.32f, 0.32f);
    static const Vec2 kHipOffsetLeft(-0.16f, -0.5f);
    static const Vec2 kHipOffsetRight(0.16f, -0.5f);
    // Thickness deliberately wider than the raw source PNG ratio (63/512 =
    // 0.123) and leg length scaled 1.5x from 0.6 -- same "~50% too small"
    // correction as the arm reach above.
    static const float kLimbAspect = 0.185f;
    static const float kLegLength = 0.9f;
    // Matched against a reference gameplay screenshot, then nudged back up
    // slightly (user-reported still wrong after the previous pass).
    static const Vec2 kHandSize(0.22f, 0.22f);
    static const Vec2 kFeetSize(0.20f, 0.20f);

    static TextureHandle sTexBody, sTexHead, sTexArm, sTexLeg, sTexHand, sTexFeet, sTexShoulder, sTexBag, sTexCaveBg;
    static FontHandle sFontUI;

    // cave_bg.png is a single sprite cropped from Unity's "Cave - BigRocks1"
    // atlas (sub-sprite _2's exact rect, per its .meta) rather than the raw
    // 10-sprite atlas -- reads as one coherent rock silhouette when tiled
    // instead of a jumbled sheet. Note this specific atlas isn't actually
    // Unity's primary level background asset (that's an assembled Tilemap
    // from several other tileset atlases); this is a placeholder chosen for
    // being visually clean when repeated, not a faithful reproduction. Tiled
    // in a loose grid behind the level, each tile much bigger than a level
    // cell since the source image itself is large.
    static const float kBgTileSize = 24.0f;

    static unsigned int TintForHand(Player::HandColor c)
    {
        switch (c)
        {
            case Player::kHandNextGrip: return 0xFF33DD33;
            case Player::kHandDelay:    return 0xFFFFA300;
            case Player::kHandFallGrab: return 0xFFFF3333;
            case Player::kHandDefault:
            default:                    return 0xFFFFFFFF;
        }
    }

    bool Init()
    {
        if (!Input::Init()) return false;
        if (!Render::Init()) return false;

        sTexBody = Render::LoadTexture("body.png");
        sTexHead = Render::LoadTexture("head.png");
        sTexArm = Render::LoadTexture("arm.png");
        sTexLeg = Render::LoadTexture("leg.png");
        sTexHand = Render::LoadTexture("hand.png");
        sTexFeet = Render::LoadTexture("feet.png");
        sTexShoulder = Render::LoadTexture("shoulder.png");
        sTexBag = Render::LoadTexture("bag.png");
        sTexCaveBg = Render::LoadTexture("cave_bg.png");
        sFontUI = Render::LoadFont("ui.ttf", 20);

        sPlayer.Configure(kHandOffsetLeft, kHandOffsetRight, kHandGripRadius);

        MedalThresholds thresholds;
        sScore.Configure(kScoreTime, thresholds);

        Save::Load();

        sLevelIndex = 0;
        LoadLevelByIndex(sLevelIndex);

        return true;
    }

    void Shutdown()
    {
        Render::Shutdown();
        Input::Shutdown();
        sLevel.Unload();
    }

    static bool sPaused = false;

    void Update(float dt)
    {
        Input::Update();

        // On PS3, gSampleApp.isPause (toggled by the SDK's own SampleBasic
        // template on Start, independent of this) and sPaused stay in
        // lockstep automatically: both are edge-triggered off the same
        // underlying pad state in the same frame (main_ps3.cpp calls
        // Update() every frame regardless of gSampleApp.isPause -- only
        // isSysMenu gates it there, since that's an OS-level concern with no
        // PC equivalent). This is also the first real pause implementation
        // PC has had; previously Input::pauseIsPressed was read but never
        // consumed anywhere.
        if (Input::pauseIsPressed) sPaused = !sPaused;
        if (sPaused) return;

        if (Input::restartIsPressed)
        {
            sPlayer.Reset(sLevel.PlayerStart());
            sCamera.Reset(sLevel.PlayerStart());
            sScore.StartRun();
            return;
        }

        sPlayer.Step(dt, sLevel);
        // Ported from CameraController.UpdatePivot() being called from
        // PlayerController.cs on every grip-change event (attach/regrab/
        // swap/launch) instead of tracking the body every frame -- the
        // camera stays anchored on the current grip point while
        // continuously swinging on it. This was previously unwired: the
        // method (Camera2D::SnapPivot) already existed but nothing called
        // it, so the camera pivot never moved past the level's start
        // position outside of flightCamActive's own per-frame catch-up.
        if (sPlayer.PivotJustChanged()) sCamera.SnapPivot(sPlayer.BodyPos());
        sCamera.Step(dt, sPlayer);
        sScore.Tick(dt);

        Vec2 body = sPlayer.BodyPos();
        if (sLevel.TouchesEndTrigger(body, 0.3f) >= 0)
        {
            if (sScore.IsRunActive())
            {
                sScore.StopRun(sPlayer);
                // StopRun only updates ScoreTracker's in-memory PB;
                // ScoreTracker doesn't know which level it's tracking, so
                // the persistence write happens here instead.
                Save::SetBest(sLevelIndex, sScore.PersonalBest());
                // No level-select menu yet (see TODO.md) -- advance straight
                // to the next level, wrapping after the last placeholder.
                // LoadLevelByIndex resets the player away from this trigger,
                // so there's no same-frame re-trigger risk.
                sLevelIndex = (sLevelIndex + 1) % LevelData::PlaceholderLevelCount();
                LoadLevelByIndex(sLevelIndex);
            }
            return;
        }
        if (sLevel.TouchesResetTrigger(body, 0.3f) >= 0)
        {
            sPlayer.Reset(sLevel.PlayerStart());
            sCamera.Reset(sLevel.PlayerStart());
            sScore.StartRun();
        }
    }

    static unsigned int ColorForCell()
    {
        return 0xFF707070;
    }

    // Angle (degrees) to rotate a sprite whose authored "up" points from tip to
    // pivot-end so that "up" instead points along `dir`. Derived from
    // Render::DrawQuad's rotation convention (positive = counter-clockwise);
    // see the derivation note in TODO.md if this ever needs re-deriving.
    static float AngleAlong(Vec2 dir)
    {
        return atan2f(-dir.x, dir.y) * (180.0f / CRUX_PI);
    }

    static void DrawLimb(TextureHandle tex, Vec2 from, Vec2 to, float aspect, unsigned int tint,
                         float extraAngleDeg = 0.0f, bool flipX = false, bool flipY = false)
    {
        Vec2 d = to - from;
        float length = d.magnitude();
        if (length < 1e-4f) return;
        Vec2 center = (from + to) * 0.5f;
        float angle = AngleAlong(d) + extraAngleDeg;
        Render::DrawTexturedQuad(tex, center, Vec2(length * aspect, length), angle, tint, flipX, flipY);
    }

    static const char *ScoreTypeName(ScoreType t)
    {
        switch (t)
        {
            case kScoreTime: return "Time";
            case kScoreJumps: return "Jumps";
            case kScoreFlips: return "Flips";
            case kScoreSwings: return "Swings";
        }
        return "";
    }

    // Ported from HighscoreManager.cs/ScoreManager.cs's UI (Current/PB/WR +
    // trophy thresholds) minus the PlayFab-backed parts -- WR has no meaning
    // without an online leaderboard (dropped per project scope), so it's
    // just not shown rather than displaying a fake value.
    static void DrawUIOverlay()
    {
        char buf[32];
        const unsigned int kUIWhite = 0xFFE0E0E0;

        sScore.FormatScore(sScore.CurrentScore(sPlayer), buf, sizeof(buf));
        char line[64];
        snprintf(line, sizeof(line), "Current: %s", buf);
        Render::DrawUIText(sFontUI, 0.03f, 0.03f, line, kUIWhite);

        if (sScore.HasPersonalBest())
        {
            sScore.FormatScore(sScore.PersonalBest(), buf, sizeof(buf));
            snprintf(line, sizeof(line), "PB: %s", buf);
        }
        else
        {
            snprintf(line, sizeof(line), "PB: --");
        }
        Render::DrawUIText(sFontUI, 0.03f, 0.07f, line, kUIWhite);

        snprintf(line, sizeof(line), "TROPHIES (%s)", ScoreTypeName(sScore.Type()));
        Render::DrawUIText(sFontUI, 0.03f, 0.13f, line, kUIWhite);

        const MedalThresholds &th = sScore.Thresholds();
        sScore.FormatScore(th.platinum, buf, sizeof(buf));
        snprintf(line, sizeof(line), "Platinum: %s", buf);
        Render::DrawUIText(sFontUI, 0.03f, 0.17f, line, 0xFF40E0E0);

        sScore.FormatScore(th.gold, buf, sizeof(buf));
        snprintf(line, sizeof(line), "Gold: %s", buf);
        Render::DrawUIText(sFontUI, 0.03f, 0.21f, line, 0xFFD4AF37);

        sScore.FormatScore(th.silver, buf, sizeof(buf));
        snprintf(line, sizeof(line), "Silver: %s", buf);
        Render::DrawUIText(sFontUI, 0.03f, 0.25f, line, 0xFFC0C0C0);

        Render::DrawUIText(sFontUI, 0.03f, 0.29f, "Bronze: any", 0xFFCD7F32);

        // PS3 already gets a "PAUSE" indicator for free from the SDK
        // template's own onDbgfont (see main_ps3.cpp) -- this is the PC
        // build's equivalent, driven by the same shared sPaused flag.
        if (sPaused)
            Render::DrawUIText(sFontUI, 0.42f, 0.46f, "PAUSED", 0xFFFFFFFF, 1.5f);
    }

    void Draw()
    {
        Vec2 camPos = sCamera.Position();
        float orthoSize = sCamera.OrthoSize();
        Render::BeginFrame(camPos, orthoSize);

        // Cave background, tiled loosely behind everything and following the
        // camera so it always fills the screen -- replaces the flat navy
        // clear color with something that actually reads as a cave.
        float bgRadius = orthoSize * 3.0f + kBgTileSize;
        int bgMinX = (int)floorf((camPos.x - bgRadius) / kBgTileSize);
        int bgMaxX = (int)ceilf((camPos.x + bgRadius) / kBgTileSize);
        int bgMinY = (int)floorf((camPos.y - bgRadius) / kBgTileSize);
        int bgMaxY = (int)ceilf((camPos.y + bgRadius) / kBgTileSize);
        for (int ty = bgMinY; ty <= bgMaxY; ty++)
        {
            for (int tx = bgMinX; tx <= bgMaxX; tx++)
            {
                Vec2 tileCenter((tx + 0.5f) * kBgTileSize, (ty + 0.5f) * kBgTileSize);
                Render::DrawTexturedQuad(sTexCaveBg, tileCenter, Vec2(kBgTileSize, kBgTileSize), 0.0f, 0xFF555560);
            }
        }

        // Tile grid (batched per-cell for now; fine at this level's scale --
        // a real static-mesh batch is a later optimization pass).
        Vec2 cellSize = sLevel.CellSize();
        Vec2 origin = sLevel.Origin();
        for (int ly = 0; ly < sLevel.Height(); ly++)
        {
            for (int lx = 0; lx < sLevel.Width(); lx++)
            {
                if (!sLevel.CellOccupied(lx, ly)) continue;
                float wx = origin.x + (sLevel.BoundsMinX() + lx + 0.5f) * cellSize.x;
                float wy = origin.y + (sLevel.BoundsMinY() + ly + 0.5f) * cellSize.y;
                Render::DrawQuad(Vec2(wx, wy), cellSize, 0.0f, ColorForCell());
            }
        }

        Vec2 bodyPos = sPlayer.BodyPos();
        float bodyRotZ = sPlayer.BodyRotZ();

        // isFlipped (ported from PlayerController's flip mechanic -- see
        // Player::TryFlip's comment on why the original's 3D rotate-around-
        // the-arm-axis math collapses to a no-op physics-wise under our
        // fixed-offset pivot model). The original's only *visible* effect
        // was FlipHands() mirroring the two hand sprites' local scale.x; the
        // rest of the visual "flip" came from the 3D rotation itself, which
        // has no direct 2D equivalent here. Best-effort interpretation:
        // swap which hand/foot gets the mirror. Flagged for the user to
        // confirm this reads right once they're back -- see TODO.md.
        bool flip = sPlayer.IsFlipped();

        Vec2 shoulderL = bodyPos + RotateAround(kShoulderOffsetLeft, Vec2(0, 0), bodyRotZ);
        Vec2 shoulderR = bodyPos + RotateAround(kShoulderOffsetRight, Vec2(0, 0), bodyRotZ);
        Vec2 hipL = bodyPos + RotateAround(kHipOffsetLeft, Vec2(0, 0), bodyRotZ);
        Vec2 hipR = bodyPos + RotateAround(kHipOffsetRight, Vec2(0, 0), bodyRotZ);

        // Legs hang from the hips; LegAngleLeft/Right (from Player, ported from
        // PlayerController.UpdateLegs) swings them relative to straight-down.
        Vec2 legDirL = RotateAround(Vec2(0, -kLegLength), Vec2(0, 0), bodyRotZ + sPlayer.LegAngleLeft());
        Vec2 legDirR = RotateAround(Vec2(0, -kLegLength), Vec2(0, 0), bodyRotZ + sPlayer.LegAngleRight());
        // User-corrected: Leg.png's authored orientation put the left leg
        // upside down (needs +180) and the right leg needed a top/bottom
        // mirror (not left/right -- flipY, not flipX) to read correctly.
        DrawLimb(sTexLeg, hipL, hipL + legDirL, kLimbAspect, 0xFFFFFFFF, 180.0f);
        DrawLimb(sTexLeg, hipR, hipR + legDirR, kLimbAspect, 0xFFFFFFFF, 0.0f, false, true);
        // Hand.png/Feet.png are authored facing one direction (not symmetric),
        // so one side needs a horizontal mirror to match visually.
        //
        // Feet.png's heel sits toward the top-left of the image, toe toward
        // the right -- anchor by the heel (where the leg meets the shoe)
        // rather than the sprite's geometric center. The offset's X sign
        // flips with the mirror so "heel-side" tracks the visually-mirrored
        // image, not the unflipped source.
        float legAngleL = bodyRotZ + sPlayer.LegAngleLeft();
        float legAngleR = bodyRotZ + sPlayer.LegAngleRight();
        Vec2 footOffsetL = RotateAround(Vec2(kFeetSize.x * 0.28f, kFeetSize.y * 0.18f), Vec2(0, 0), legAngleL);
        Vec2 footOffsetR = RotateAround(Vec2(-kFeetSize.x * 0.28f, kFeetSize.y * 0.18f), Vec2(0, 0), legAngleR);
        Render::DrawTexturedQuad(sTexFeet, hipL + legDirL + footOffsetL, kFeetSize, legAngleL, 0xFFFFFFFF, !flip);
        Render::DrawTexturedQuad(sTexFeet, hipR + legDirR + footOffsetR, kFeetSize, legAngleR, 0xFFFFFFFF, flip);

        // Body + bag (bag worn on the back, under the head/arms) + head (head
        // keeps its own slightly-lagging rotation, ported from
        // PlayerController.UpdateHead, while still translating with the body).
        Render::DrawTexturedQuad(sTexBody, bodyPos, kBodySize, bodyRotZ, 0xFFFFFFFF);

        Vec2 bagPos = bodyPos + RotateAround(kBagLocalOffset, Vec2(0, 0), bodyRotZ);
        Render::DrawTexturedQuad(sTexBag, bagPos, kBagSize, bodyRotZ, 0xFFFFFFFF);

        Vec2 headPos = bodyPos + RotateAround(kHeadLocalOffset, Vec2(0, 0), bodyRotZ);
        Render::DrawTexturedQuad(sTexHead, headPos, kHeadSize, sPlayer.HeadRotZ(), 0xFFFFFFFF);

        // Arms: geometrically aimed from the shoulder anchor straight at the
        // actual grip point (Player::HandWorldLeft/Right), so they're always
        // visually correct regardless of rig proportion guesses.
        DrawLimb(sTexArm, shoulderL, sPlayer.HandWorldLeft(), kLimbAspect, 0xFFFFFFFF);
        DrawLimb(sTexArm, shoulderR, sPlayer.HandWorldRight(), kLimbAspect, 0xFFFFFFFF);

        // Shoulder caps drawn after the arms, on top, to cover the arm/body
        // seam. Shoulder.png is a dome (rounded top, flat bottom as authored)
        // -- user-corrected orientation: rounded edge should face the body,
        // flat edge should sit along the arm, so rotate its "up" (rounded
        // side) to point from hand back toward the shoulder, same AngleAlong
        // convention DrawLimb uses. Shading isn't symmetric, so left mirrors.
        float shoulderCapAngleL = AngleAlong(shoulderL - sPlayer.HandWorldLeft());
        float shoulderCapAngleR = AngleAlong(shoulderR - sPlayer.HandWorldRight());
        Render::DrawTexturedQuad(sTexShoulder, shoulderL, kShoulderCapSize, shoulderCapAngleL, 0xFFFFFFFF, true);
        Render::DrawTexturedQuad(sTexShoulder, shoulderR, kShoulderCapSize, shoulderCapAngleR, 0xFFFFFFFF);

        // Hands, tinted by grip state (ported from PlayerController.SetHandsColor).
        // Hand.png's real pivot is at its own bottom edge (wrist), not
        // center -- Hand.png.meta: alignment 7, pivot {0.5, 0}, meaning the
        // sprite's content extends outward from the wrist, not around it.
        // Our DrawTexturedQuad always centers the quad, so to emulate a
        // bottom-pivoted sprite we push the quad's center outward from the
        // arm tip by half the sprite's own height, landing the wrist edge
        // right at the tip instead of overlapping back into the arm.
        // User-corrected: previously pulled back toward the arm instead,
        // which put the hand sprite over the arm's own end rather than
        // continuing past it.
        Vec2 armDirLocalL = (kHandOffsetLeft - kShoulderOffsetLeft).normalized();
        Vec2 armDirLocalR = (kHandOffsetRight - kShoulderOffsetRight).normalized();
        Vec2 handOffsetL = RotateAround(armDirLocalL * (kHandSize.y * 0.5f), Vec2(0, 0), bodyRotZ);
        Vec2 handOffsetR = RotateAround(armDirLocalR * (kHandSize.y * 0.5f), Vec2(0, 0), bodyRotZ);
        // User-corrected: both hands need the same horizontal mirror, not
        // just the left one.
        Render::DrawTexturedQuad(sTexHand, sPlayer.HandWorldLeft() + handOffsetL, kHandSize, bodyRotZ, TintForHand(sPlayer.LeftHandColor()), !flip);
        Render::DrawTexturedQuad(sTexHand, sPlayer.HandWorldRight() + handOffsetR, kHandSize, bodyRotZ, TintForHand(sPlayer.RightHandColor()), !flip);

        DrawUIOverlay();

        Render::EndFrame();
    }
}
