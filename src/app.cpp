#include "app.h"
#include "render.h"
#include "input.h"
#include "level.h"
#include "player.h"
#include "camera2d.h"
#include "score.h"
#include <math.h>

#if defined(CRUX_PS3)
#define LEVEL_PATH SYS_APP_HOME "/data/level1.lvl"
#include <sys/paths.h>
#else
#define LEVEL_PATH "data/level1.lvl"
#endif

namespace App
{
    static LevelData sLevel;
    static Player sPlayer;
    static Camera2D sCamera;
    static ScoreTracker sScore;
    static bool sUsingPlaceholder = false;

    // TODO: replace with real values from data/player_rig.txt once
    // PS3/Export-Level-+-Rig-Data has been run in the Unity Editor (see
    // Assets/Editor/PS3LevelExporter.cs). These are placeholder guesses.
    // Iteration 3: pulled wide/high to read as a V-shaped reach (matches the
    // reference screenshots) -- was nearly on top of the shoulder anchor
    // before, which drew arms as short stubs with no clear angle. Since this
    // point doubles as the swing pivot (see player.h's pivot-invariance
    // note), this also changes swing radius/feel, not just the visual --
    // worth re-checking once real rig data replaces this guess.
    // Iteration 4 (user-reported): arms/legs read as ~50% too small overall
    // (both length and thickness) -- scaled the reach anchor 1.5x from the
    // previous iteration's (-0.62, 0.95).
    static const Vec2 kHandOffsetLeft(-0.93f, 1.43f);
    static const Vec2 kHandOffsetRight(0.93f, 1.43f);
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
    static const Vec2 kHeadLocalOffset(0.0f, 0.68f);
    static const Vec2 kShoulderOffsetLeft(-0.26f, 0.5f);
    static const Vec2 kShoulderOffsetRight(0.26f, 0.5f);
    static const Vec2 kHipOffsetLeft(-0.16f, -0.5f);
    static const Vec2 kHipOffsetRight(0.16f, -0.5f);
    // Thickness deliberately wider than the raw source PNG ratio (63/512 =
    // 0.123) and leg length scaled 1.5x from 0.6 -- same "~50% too small"
    // correction as the arm reach above.
    static const float kLimbAspect = 0.185f;
    static const float kLegLength = 0.9f;
    // 2x per user feedback (were reading as too small relative to the limbs).
    static const Vec2 kHandSize(0.36f, 0.36f);
    static const Vec2 kFeetSize(0.48f, 0.48f);

    static TextureHandle sTexBody, sTexHead, sTexArm, sTexLeg, sTexHand, sTexFeet;

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

        if (!sLevel.Load(LEVEL_PATH))
        {
            sLevel.LoadPlaceholderTestRoom();
            sUsingPlaceholder = true;
        }

        sPlayer.Configure(kHandOffsetLeft, kHandOffsetRight, kHandGripRadius);
        sPlayer.Reset(sLevel.PlayerStart());
        sCamera.Reset(sLevel.PlayerStart());

        MedalThresholds thresholds;
        sScore.Configure(kScoreTime, thresholds);
        sScore.StartRun();

        return true;
    }

    void Shutdown()
    {
        Render::Shutdown();
        Input::Shutdown();
        sLevel.Unload();
    }

    void Update(float dt)
    {
        Input::Update();

        if (Input::restartIsPressed)
        {
            sPlayer.Reset(sLevel.PlayerStart());
            sCamera.Reset(sLevel.PlayerStart());
            sScore.StartRun();
            return;
        }

        sPlayer.Step(dt, sLevel);
        sCamera.Step(dt, sPlayer);
        sScore.Tick(dt);

        Vec2 body = sPlayer.BodyPos();
        if (sLevel.TouchesEndTrigger(body, 0.3f) >= 0)
        {
            sScore.StopRun(sPlayer);
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

    static void DrawLimb(TextureHandle tex, Vec2 from, Vec2 to, float aspect, unsigned int tint)
    {
        Vec2 d = to - from;
        float length = d.magnitude();
        if (length < 1e-4f) return;
        Vec2 center = (from + to) * 0.5f;
        float angle = AngleAlong(d);
        Render::DrawTexturedQuad(tex, center, Vec2(length * aspect, length), angle, tint);
    }

    void Draw()
    {
        Render::BeginFrame(sCamera.Position(), sCamera.OrthoSize());

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

        Vec2 shoulderL = bodyPos + RotateAround(kShoulderOffsetLeft, Vec2(0, 0), bodyRotZ);
        Vec2 shoulderR = bodyPos + RotateAround(kShoulderOffsetRight, Vec2(0, 0), bodyRotZ);
        Vec2 hipL = bodyPos + RotateAround(kHipOffsetLeft, Vec2(0, 0), bodyRotZ);
        Vec2 hipR = bodyPos + RotateAround(kHipOffsetRight, Vec2(0, 0), bodyRotZ);

        // Legs hang from the hips; LegAngleLeft/Right (from Player, ported from
        // PlayerController.UpdateLegs) swings them relative to straight-down.
        Vec2 legDirL = RotateAround(Vec2(0, -kLegLength), Vec2(0, 0), bodyRotZ + sPlayer.LegAngleLeft());
        Vec2 legDirR = RotateAround(Vec2(0, -kLegLength), Vec2(0, 0), bodyRotZ + sPlayer.LegAngleRight());
        DrawLimb(sTexLeg, hipL, hipL + legDirL, kLimbAspect, 0xFFFFFFFF);
        DrawLimb(sTexLeg, hipR, hipR + legDirR, kLimbAspect, 0xFFFFFFFF);
        Render::DrawTexturedQuad(sTexFeet, hipL + legDirL, kFeetSize, bodyRotZ + sPlayer.LegAngleLeft(), 0xFFFFFFFF);
        Render::DrawTexturedQuad(sTexFeet, hipR + legDirR, kFeetSize, bodyRotZ + sPlayer.LegAngleRight(), 0xFFFFFFFF);

        // Body + head (head keeps its own slightly-lagging rotation, ported from
        // PlayerController.UpdateHead, while still translating with the body).
        Render::DrawTexturedQuad(sTexBody, bodyPos, kBodySize, bodyRotZ, 0xFFFFFFFF);

        Vec2 headPos = bodyPos + RotateAround(kHeadLocalOffset, Vec2(0, 0), bodyRotZ);
        Render::DrawTexturedQuad(sTexHead, headPos, kHeadSize, sPlayer.HeadRotZ(), 0xFFFFFFFF);

        // Arms: geometrically aimed from the shoulder anchor straight at the
        // actual grip point (Player::HandWorldLeft/Right), so they're always
        // visually correct regardless of rig proportion guesses.
        DrawLimb(sTexArm, shoulderL, sPlayer.HandWorldLeft(), kLimbAspect, 0xFFFFFFFF);
        DrawLimb(sTexArm, shoulderR, sPlayer.HandWorldRight(), kLimbAspect, 0xFFFFFFFF);

        // Hands, tinted by grip state (ported from PlayerController.SetHandsColor).
        Render::DrawTexturedQuad(sTexHand, sPlayer.HandWorldLeft(), kHandSize, bodyRotZ, TintForHand(sPlayer.LeftHandColor()));
        Render::DrawTexturedQuad(sTexHand, sPlayer.HandWorldRight(), kHandSize, bodyRotZ, TintForHand(sPlayer.RightHandColor()));

        Render::EndFrame();
    }
}
