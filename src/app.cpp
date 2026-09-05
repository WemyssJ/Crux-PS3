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

    // No C# equivalent to port here -- the real game has no main menu/level
    // select/customizer/pause-menu scripts among Assets/Scripts (SceneController
    // just jumps straight into whichever scene is loaded), so this whole
    // flow is new, not a port. Kept deliberately simple: text/quad menus
    // using the same Render:: primitives already used for the UI overlay,
    // navigated with Left/Right (already edge-triggered) + Jump (confirm) +
    // Restart (back) -- no new input actions needed.
    enum GameState { kStateMainMenu, kStateLevelSelect, kStateCustomizer, kStatePlaying, kStatePauseMenu };
    static GameState sGameState = kStateMainMenu;
    static int sMenuCursor = 0;

    // Character customizer -- per-limb colour picking: choose a limb group,
    // then a replacement colour for just that group, via a swatch-grid
    // picker standing in for a desktop "Windows style" colour dialog (there's
    // no OS colour picker available on PS3, and this project shares one
    // input model/UI across both platforms, so this rolls its own grid
    // instead of shelling out to one). Applied to the clothing surfaces only
    // (Body+Shorts as "Torso", Arm+Shoulder as "Arms", Leg as "Legs"), not
    // skin/hair/accessories (Head/Hand/Feet/Bag), which keep their own
    // authored colors and grip-state tinting.
    enum LimbGroup { kLimbTorso, kLimbArms, kLimbLegs, kLimbGroupCount };
    static const char *kLimbGroupNames[kLimbGroupCount] = { "Torso", "Arms", "Legs" };

    static const unsigned int kColorSwatches[] =
    {
        0xFFFFFFFF, 0xFFFF9090, 0xFF90E090, 0xFF90C0FF,
        0xFFFFE060, 0xFFE090FF, 0xFFB0B0B0, 0xFF404040,
        0xFFFF4040, 0xFF40C040, 0xFF4080FF, 0xFFFFA030,
    };
    static const int kColorSwatchCount = sizeof(kColorSwatches) / sizeof(kColorSwatches[0]);
    static const int kColorGridCols = 4;

    static int sLimbColorIndex[kLimbGroupCount] = { 0, 0, 0 };
    static int sCustomizerStep = 0;   // 0 = choose limb, 1 = pick a colour for it
    static int sCustomizerLimb = 0;   // limb chosen in step 0
    static int sColorGridCursor = 0;  // swatch cursor in step 1
    static bool sPrevMenuUp = false, sPrevMenuDown = false; // local edge-detect for held Up/Down (see UpdateCustomizer)

    // Fixed camera/zoom every non-gameplay menu screen renders at (matches
    // BeginMenuFrame) -- also needed by DrawMenuList to place button boxes
    // under the SAME screen positions DrawUIText's nx/ny use, since the pause
    // menu overlay reuses DrawMenuList but renders under the live gameplay
    // camera instead.
    static const Vec2 kMenuCamPos(0.0f, 0.0f);
    static const float kMenuOrthoHalfHeight = 6.0f;

    // Level Select's grid layout (see DrawLevelSelect/UpdateLevelSelect) --
    // declared up here since UpdateLevelSelect (which comes first in the
    // file) needs kLevelGridCols for its Up/Down row-jump math.
    static const int kLevelGridCols = 3;
    static const int kLevelGridRows = 2;

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
    // shoulders), this derived value's wider/more-horizontal reach was
    // the better match direction-wise, not the earlier guess's narrower/
    // more-vertical one -- re-applied.
    // Iteration 6 (live side-by-side, user-corrected): the DIRECTION was
    // right but the MAGNITUDE was too long -- scaled the derived vector
    // down ~15% (keeping the same angle) rather than re-deriving; this
    // also sets the swing pivot radius (see player.h's pivot-invariance
    // note), not just the visual.
    static const Vec2 kHandOffsetLeft(-1.17f, 1.03f);
    static const Vec2 kHandOffsetRight(1.17f, 1.03f);
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
    // Iteration 4: switched to the real Player.prefab-derived value instead
    // of continuing to eyeball-adjust. Bag is a child of "Shorts" (local
    // (0,-0.9) rel. Body), itself at local (0.18,0.289) rel. Shorts --
    // Bag_rel_Body = (0.18,-0.611) Unity units, x0.6 project scale factor =
    // (0.108,-0.367). Note it's NOT centered on the body (x != 0) in the
    // real rig -- previous iterations assumed symmetric placement.
    // Iteration 6 (tried, reconsidered): repositioned above the belt's top
    // edge instead of inside its vertical span, to stop a "doubled up"
    // overlap -- but Bag is genuinely a CHILD of Shorts in the real rig (a
    // pouch clipped onto the belt), so some overlap is actually correct,
    // not a bug. Iteration 7: back to the real derived position; the
    // belt's own size (below) is what actually needed tuning.
    static const Vec2 kBagLocalOffset(0.11f, -0.37f);
    // Real size from Bag.png's actual crop (94x102 px @ 256 px/unit import,
    // see Bag.png.meta) x0.6 scale = (0.220, 0.239).
    static const Vec2 kBagSize(0.22f, 0.24f);
    // Shorts.png -- the waist BELT band (previously missing entirely; the
    // small pouch above is Bag.png, a separate sprite/layer worn near it).
    // Real Player.prefab data: Shorts is a direct child of Body at local
    // (0,-0.9), same 256x512 @ 256px/unit texture as Body.png itself, so it
    // shares kBodySize's exact scale (0.6x1.2) before any further
    // adjustment. Iterations 6-10 kept shrinking this trying to make the
    // visible belt band thinner -- wrong approach. Pixel-measured the
    // rendered result at 0.08: the whole quad reads as solid olive, no
    // distinct blue-olive-blue banding, because squashing the source this
    // far makes texture filtering blend the surrounding blue into the
    // band, AND disconnects the garment from the torso/legs it should
    // bridge (user-reported: "make shorts on top of torso/belt"). The
    // band's thinness is a fixed proportion of the source texture, not
    // something the container size controls -- reverted to the real
    // derived natural size so it renders as a proper garment layer (belt
    // band included, at its true proportion) instead of a disconnected
    // small olive blob.
    static const Vec2 kShortsLocalOffset(0.0f, -0.54f);
    static const Vec2 kShortsSize(0.6f, 1.2f);
    // User-corrected: move out (further from centerline) and up.
    // Iteration 2 (user-corrected): moved down slightly (paired with
    // shorts moving down the same way).
    static const Vec2 kHipOffsetLeft(-0.22f, -0.48f);
    static const Vec2 kHipOffsetRight(0.22f, -0.48f);
    // Thickness deliberately wider than the raw source PNG ratio (63/512 =
    // 0.123) and leg length scaled 1.5x from 0.6 -- same "~50% too small"
    // correction as the arm reach above.
    static const float kLimbAspect = 0.185f;
    static const float kLegLength = 0.9f;
    // Matched against a reference gameplay screenshot, then nudged back up
    // slightly (user-reported still wrong after the previous pass).
    static const Vec2 kHandSize(0.22f, 0.22f);
    // User-corrected: shoes read as too small -- scaled up 1.5x from 0.20.
    static const Vec2 kFeetSize(0.30f, 0.30f);

    static TextureHandle sTexBody, sTexHead, sTexArm, sTexLeg, sTexHand, sTexFeet, sTexShoulder, sTexBag, sTexShorts, sTexCaveBg;
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
        sTexShorts = Render::LoadTexture("shorts.png");
        sTexCaveBg = Render::LoadTexture("cave_bg.png");
        sFontUI = Render::LoadFont("ui.ttf", 20);

        sPlayer.Configure(kHandOffsetLeft, kHandOffsetRight, kHandGripRadius);

        MedalThresholds thresholds;
        sScore.Configure(kScoreTime, thresholds);

        Save::Load();

        sGameState = kStateMainMenu;

        return true;
    }

    void Shutdown()
    {
        Render::Shutdown();
        Input::Shutdown();
        sLevel.Unload();
    }

    // Left/Right move the cursor (already edge-triggered, safe to reuse
    // directly); Jump confirms; Restart backs out to the Main Menu.
    static void UpdateMainMenu()
    {
        const int kCount = 3; // Play, Select Level, Customize
        if (Input::leftIsPressed) sMenuCursor = (sMenuCursor + kCount - 1) % kCount;
        if (Input::rightIsPressed) sMenuCursor = (sMenuCursor + 1) % kCount;
        if (Input::jumpWasPressed)
        {
            if (sMenuCursor == 0)
            {
                LoadLevelByIndex(sLevelIndex);
                sGameState = kStatePlaying;
            }
            else if (sMenuCursor == 1)
            {
                sMenuCursor = sLevelIndex;
                sGameState = kStateLevelSelect;
            }
            else
            {
                sMenuCursor = sCustomizerLimb;
                sCustomizerStep = 0;
                sGameState = kStateCustomizer;
            }
        }
    }

    // Left/Right move linearly through the flat level list (unchanged --
    // this already pages automatically, see DrawLevelSelect's comment).
    // Up/Down jump a full grid row (+/- kLevelGridCols) for faster movement
    // once there are enough levels to need it; they're HELD signals (see
    // input_pc.cpp), so -- like UpdateCustomizer's color grid -- this needs
    // its own local edge-detection rather than moving a row every frame
    // they're held. Shares sPrevMenuUp/sPrevMenuDown with the customizer;
    // harmless since only one of these states is ever active at a time.
    static void UpdateLevelSelect()
    {
        int count = LevelData::PlaceholderLevelCount();
        bool upEdge = Input::upIsPressed && !sPrevMenuUp;
        bool downEdge = Input::downIsPressed && !sPrevMenuDown;
        sPrevMenuUp = Input::upIsPressed;
        sPrevMenuDown = Input::downIsPressed;

        if (Input::leftIsPressed) sMenuCursor = (sMenuCursor + count - 1) % count;
        if (Input::rightIsPressed) sMenuCursor = (sMenuCursor + 1) % count;
        if (upEdge) sMenuCursor = ((sMenuCursor - kLevelGridCols) % count + count) % count;
        if (downEdge) sMenuCursor = (sMenuCursor + kLevelGridCols) % count;
        if (Input::jumpWasPressed)
        {
            sLevelIndex = sMenuCursor;
            LoadLevelByIndex(sLevelIndex);
            sGameState = kStatePlaying;
        }
        if (Input::restartIsPressed)
        {
            sMenuCursor = 0;
            sGameState = kStateMainMenu;
        }
    }

    // Step 0: Left/Right cycles which limb group is selected (kLimbGroupCount
    // options), Jump drops into that limb's colour grid, Restart backs out
    // to the Main Menu.
    // Step 1: a 2D swatch grid -- Left/Right/Up/Down move the cursor, Jump
    // confirms (commits the swatch to sLimbColorIndex), Restart cancels back
    // to step 0 with no change. Up/Down are HELD signals (Input::upIsPressed/
    // downIsPressed, matching PlayerController.cs's IsPressed-based hand-grip
    // semantics -- see input_pc.cpp), so row movement needs its own local
    // edge-detection here rather than moving one row every single frame
    // they're held.
    static void UpdateCustomizer()
    {
        bool upEdge = Input::upIsPressed && !sPrevMenuUp;
        bool downEdge = Input::downIsPressed && !sPrevMenuDown;
        sPrevMenuUp = Input::upIsPressed;
        sPrevMenuDown = Input::downIsPressed;

        if (sCustomizerStep == 0)
        {
            if (Input::leftIsPressed) sMenuCursor = (sMenuCursor + kLimbGroupCount - 1) % kLimbGroupCount;
            if (Input::rightIsPressed) sMenuCursor = (sMenuCursor + 1) % kLimbGroupCount;
            if (Input::jumpWasPressed)
            {
                sCustomizerLimb = sMenuCursor;
                sColorGridCursor = sLimbColorIndex[sCustomizerLimb];
                sCustomizerStep = 1;
            }
            else if (Input::restartIsPressed)
            {
                sMenuCursor = 0;
                sGameState = kStateMainMenu;
            }
        }
        else
        {
            int rows = (kColorSwatchCount + kColorGridCols - 1) / kColorGridCols;
            int row = sColorGridCursor / kColorGridCols;
            int col = sColorGridCursor % kColorGridCols;
            if (Input::leftIsPressed) col = (col + kColorGridCols - 1) % kColorGridCols;
            if (Input::rightIsPressed) col = (col + 1) % kColorGridCols;
            if (upEdge) row = (row + rows - 1) % rows;
            if (downEdge) row = (row + 1) % rows;
            sColorGridCursor = row * kColorGridCols + col;
            if (sColorGridCursor >= kColorSwatchCount) sColorGridCursor = kColorSwatchCount - 1;

            if (Input::jumpWasPressed)
            {
                sLimbColorIndex[sCustomizerLimb] = sColorGridCursor;
                sCustomizerStep = 0;
            }
            else if (Input::restartIsPressed)
            {
                sCustomizerStep = 0; // cancel, no change
            }
        }
    }

    static void UpdatePauseMenu()
    {
        const int kCount = 3; // Resume, Reset Level, Main Menu
        if (Input::leftIsPressed) sMenuCursor = (sMenuCursor + kCount - 1) % kCount;
        if (Input::rightIsPressed) sMenuCursor = (sMenuCursor + 1) % kCount;
        // Pause toggles back to gameplay too, same edge-triggered pad state
        // in the same frame as the pause menu's own Resume option.
        if (Input::pauseIsPressed) { sGameState = kStatePlaying; return; }
        if (Input::jumpWasPressed)
        {
            if (sMenuCursor == 0)
            {
                sGameState = kStatePlaying;
            }
            else if (sMenuCursor == 1)
            {
                sPlayer.Reset(sLevel.PlayerStart());
                sCamera.Reset(sLevel.PlayerStart());
                sScore.StartRun();
                sGameState = kStatePlaying;
            }
            else
            {
                sMenuCursor = 0;
                sGameState = kStateMainMenu;
            }
        }
    }

    void Update(float dt)
    {
        Input::Update();

        switch (sGameState)
        {
            case kStateMainMenu: UpdateMainMenu(); return;
            case kStateLevelSelect: UpdateLevelSelect(); return;
            case kStateCustomizer: UpdateCustomizer(); return;
            case kStatePauseMenu: UpdatePauseMenu(); return;
            case kStatePlaying: break;
        }

        // On PS3, gSampleApp.isPause (toggled by the SDK's own SampleBasic
        // template on Start, independent of this) and this state stay in
        // lockstep automatically: both are edge-triggered off the same
        // underlying pad state in the same frame (main_ps3.cpp calls
        // Update() every frame regardless of gSampleApp.isPause -- only
        // isSysMenu gates it there, since that's an OS-level concern with no
        // PC equivalent).
        if (Input::pauseIsPressed)
        {
            sMenuCursor = 0;
            sGameState = kStatePauseMenu;
            return;
        }

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
                // Now that a real level-select menu exists, return to it
                // instead of auto-advancing -- the auto-advance was only
                // ever a stand-in for the missing menu (see TODO.md), and
                // the real source's LevelEndTrigger doesn't auto-advance
                // either (it just stops the timer; level choice is
                // separate, number-key-driven in the source).
                sMenuCursor = sLevelIndex;
                sGameState = kStateLevelSelect;
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
        // "PAUSED" itself is now the pause menu's own title (DrawPauseMenuOverlay,
        // called separately from Draw()'s dispatcher) rather than a flag checked
        // here -- the old sPaused bool is gone, replaced by GameState.
    }

    // Everything that used to be the whole of Draw() before menu states
    // were added -- BeginFrame/EndFrame now live in the dispatcher below,
    // since menu screens need a frame too but don't have a real sCamera/
    // sLevel to read from yet (nothing's loaded until Play is picked).
    static void DrawGameplayWorld()
    {
        Vec2 camPos = sCamera.Position();
        float orthoSize = sCamera.OrthoSize();

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

        // Customizer colors -- per-limb group, applied to clothing surfaces
        // only (Body/Shorts as Torso, Arm/Shoulder as Arms, Leg as Legs),
        // not skin/hair/accessories.
        unsigned int torsoColor = kColorSwatches[sLimbColorIndex[kLimbTorso]];
        unsigned int armsColor = kColorSwatches[sLimbColorIndex[kLimbArms]];
        unsigned int legsColor = kColorSwatches[sLimbColorIndex[kLimbLegs]];

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
        DrawLimb(sTexLeg, hipL, hipL + legDirL, kLimbAspect, legsColor, 180.0f);
        DrawLimb(sTexLeg, hipR, hipR + legDirR, kLimbAspect, legsColor, 0.0f, false, true);
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
        // User-corrected: feet pointed inward (toward the body's centerline)
        // instead of outward -- swapped the offset's X sign for both sides.
        Vec2 footOffsetL = RotateAround(Vec2(-kFeetSize.x * 0.28f, kFeetSize.y * 0.18f), Vec2(0, 0), legAngleL);
        Vec2 footOffsetR = RotateAround(Vec2(kFeetSize.x * 0.28f, kFeetSize.y * 0.18f), Vec2(0, 0), legAngleR);
        // User-corrected: both feet needed the same mirror, then both
        // flipped outright, then left specifically needed mirroring back
        // relative to right -- feet ARE a true asymmetric L/R pair after
        // all (like hands), not a "both the same" case.
        Render::DrawTexturedQuad(sTexFeet, hipL + legDirL + footOffsetL, kFeetSize, legAngleL, 0xFFFFFFFF, !flip);
        Render::DrawTexturedQuad(sTexFeet, hipR + legDirR + footOffsetR, kFeetSize, legAngleR, 0xFFFFFFFF, flip);

        // Body + shorts (waist belt band) + bag (the small pouch worn near
        // it -- a separate sprite from the belt itself) + head (head keeps
        // its own slightly-lagging rotation, ported from PlayerController.
        // UpdateHead, while still translating with the body). Draw order
        // matches the real rig hierarchy: Body -> Shorts -> Bag.
        Render::DrawTexturedQuad(sTexBody, bodyPos, kBodySize, bodyRotZ, torsoColor);

        Vec2 shortsPos = bodyPos + RotateAround(kShortsLocalOffset, Vec2(0, 0), bodyRotZ);
        Render::DrawTexturedQuad(sTexShorts, shortsPos, kShortsSize, bodyRotZ, torsoColor);

        Vec2 bagPos = bodyPos + RotateAround(kBagLocalOffset, Vec2(0, 0), bodyRotZ);
        Render::DrawTexturedQuad(sTexBag, bagPos, kBagSize, bodyRotZ, 0xFFFFFFFF);

        Vec2 headPos = bodyPos + RotateAround(kHeadLocalOffset, Vec2(0, 0), bodyRotZ);
        Render::DrawTexturedQuad(sTexHead, headPos, kHeadSize, sPlayer.HeadRotZ(), 0xFFFFFFFF);

        // Arms: geometrically aimed from the shoulder anchor straight at the
        // actual grip point (Player::HandWorldLeft/Right), so they're always
        // visually correct regardless of rig proportion guesses.
        // User-corrected: right arm's baked-in shading was on the wrong
        // side (top instead of bottom) -- mirrored.
        DrawLimb(sTexArm, shoulderL, sPlayer.HandWorldLeft(), kLimbAspect, armsColor);
        DrawLimb(sTexArm, shoulderR, sPlayer.HandWorldRight(), kLimbAspect, armsColor, 0.0f, true);

        // Shoulder caps drawn after the arms, on top, to cover the arm/body
        // seam. Shoulder.png is a dome (rounded top, flat bottom as authored)
        // -- user-corrected orientation: rounded edge should face the body,
        // flat edge should sit along the arm, so rotate its "up" (rounded
        // side) to point from hand back toward the shoulder, same AngleAlong
        // convention DrawLimb uses. Shading isn't symmetric -- user-corrected:
        // both sides' shading was on the wrong side (top instead of bottom),
        // swapped which one mirrors (right now mirrors, left no longer does).
        float shoulderCapAngleL = AngleAlong(shoulderL - sPlayer.HandWorldLeft());
        float shoulderCapAngleR = AngleAlong(shoulderR - sPlayer.HandWorldRight());
        Render::DrawTexturedQuad(sTexShoulder, shoulderL, kShoulderCapSize, shoulderCapAngleL, armsColor);
        Render::DrawTexturedQuad(sTexShoulder, shoulderR, kShoulderCapSize, shoulderCapAngleR, armsColor, true);

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
        Vec2 worldArmDirL = RotateAround(armDirLocalL, Vec2(0, 0), bodyRotZ);
        Vec2 worldArmDirR = RotateAround(armDirLocalR, Vec2(0, 0), bodyRotZ);
        Vec2 handOffsetL = worldArmDirL * (kHandSize.y * 0.5f);
        Vec2 handOffsetR = worldArmDirR * (kHandSize.y * 0.5f);
        // User-corrected: hands were joining the arm at the thumb instead
        // of the base/wrist -- they were rotating with bodyRotZ alone,
        // ignoring the arm's own fixed local angle (the ~55deg baked into
        // the shoulder/hand offsets), so the hand's wrist axis didn't
        // align with the arm's actual direction. Use the same AngleAlong
        // convention as the arm/shoulder-cap sprites instead, so the
        // hand's local "up" (wrist-to-fingers) points along the real arm
        // direction.
        float handAngleL = AngleAlong(worldArmDirL);
        float handAngleR = AngleAlong(worldArmDirR);
        // Hands ARE a true asymmetric L/R pair (unlike feet, see below) --
        // reverted the "both hands same mirror" change: left needs the
        // mirror, right doesn't (Hand.png reads correctly as-authored for
        // one side only, same as the original design before that edit).
        // Both then turned out to be facing the wrong way outright, so the
        // assignment swapped (left<->right) from that first fix.
        Render::DrawTexturedQuad(sTexHand, sPlayer.HandWorldLeft() + handOffsetL, kHandSize, handAngleL, TintForHand(sPlayer.LeftHandColor()), flip);
        Render::DrawTexturedQuad(sTexHand, sPlayer.HandWorldRight() + handOffsetR, kHandSize, handAngleR, TintForHand(sPlayer.RightHandColor()), !flip);

        DrawUIOverlay();
    }

    // Simple centered menu list: title + N selectable lines, cursor shown
    // as "> " in front of the selected one. Same fixed camera/zoom for
    // every menu screen (no sCamera/sLevel to read from yet).
    static void BeginMenuFrame()
    {
        Render::BeginFrame(kMenuCamPos, kMenuOrthoHalfHeight);
    }

    // Reuses cave_bg.png (already rocky/jagged, see TODO.md) as a full-
    // screen backdrop for the non-gameplay menu screens, tinted dusk-blue so
    // it reads as a distant mountain range rather than the tiled cave-wall
    // DrawGameplayWorld uses the same texture for. One big quad, sized to
    // exactly cover kMenuOrthoHalfHeight's visible area -- the menu camera
    // never moves, so no tiling is needed.
    static void DrawMenuBackground()
    {
        Render::DrawTexturedQuad(sTexCaveBg, kMenuCamPos, Vec2(16.0f, 12.0f), 0.0f, 0xFF465A78);
    }

    // Converts a DrawUIText-style normalized screen fraction (nx,ny; 0,0 =
    // top-left) into a world position under the given camera/zoom, using the
    // same math Render::BeginFrame/WorldToScreenRect apply internally (1024x768
    // window). Lets DrawMenuList draw world-space button boxes that line up
    // with its text under BOTH the fixed menu camera and the pause overlay's
    // live gameplay camera (which can be positioned/zoomed anywhere).
    static Vec2 MenuWorldPos(float nx, float ny, Vec2 camPos, float orthoHalfHeight)
    {
        const float kAspectScale = 2.66667f; // (1024/768)*2
        return Vec2(camPos.x + (nx - 0.5f) * orthoHalfHeight * kAspectScale,
                    camPos.y - (ny - 0.5f) * orthoHalfHeight * 2.0f);
    }

    // Carved-stone banner for the main menu's "CRUX" title -- a stone-tinted
    // slab with a darker bezel behind it (for a chiseled inset edge) and the
    // title text given a hard dark drop-shadow + a light highlight offset
    // the other way, so it reads as engraved into the slab rather than
    // floating text over the mountain background.
    static void DrawCruxBanner()
    {
        Vec2 center = MenuWorldPos(0.5f, 0.15f, kMenuCamPos, kMenuOrthoHalfHeight);
        Vec2 slabSize(6.2f, 1.5f);

        Render::DrawQuad(center, slabSize + Vec2(0.16f, 0.16f), 0.0f, 0xFF1A1410); // bezel edge
        Render::DrawQuad(center, slabSize, 0.0f, 0xFF7A6A56);                      // stone slab face
        Render::DrawQuad(center, slabSize - Vec2(0.3f, 0.3f), 0.0f, 0xFF6C5D4C);   // inset panel

        const float kOx = 0.006f, kOy = 0.006f; // shadow/highlight offset, nx/ny fractions
        Render::DrawUIText(sFontUI, 0.5f + kOx, 0.15f + kOy, "CRUX", 0xFF2A2018, 2.0f, true); // shadow
        Render::DrawUIText(sFontUI, 0.5f - kOx, 0.15f - kOy, "CRUX", 0xFFE8DCC0, 2.0f, true); // highlight
        Render::DrawUIText(sFontUI, 0.5f, 0.15f, "CRUX", 0xFFF0E8D8, 2.0f, true);             // face
    }

    // title may be NULL/empty to skip the plain-text title -- used by the
    // Main Menu, which draws its own carved banner (DrawCruxBanner) instead.
    static void DrawMenuList(const char *title, const char **items, int count, int cursor,
                             Vec2 camPos = kMenuCamPos, float orthoHalfHeight = kMenuOrthoHalfHeight)
    {
        if (title && title[0])
            Render::DrawUIText(sFontUI, 0.5f, 0.15f, title, 0xFFFFFFFF, 1.4f, true);

        const float kNxCenter = 0.5f;
        const float kBoxW = 3.6f, kBoxH = 0.62f;
        for (int i = 0; i < count; i++)
        {
            float ny = 0.32f + i * 0.07f;
            bool selected = (i == cursor);
            Vec2 boxCenter = MenuWorldPos(kNxCenter, ny, camPos, orthoHalfHeight);

            if (selected)
                Render::DrawQuad(boxCenter, Vec2(kBoxW + 0.1f, kBoxH + 0.1f), 0.0f, 0xFF40E0E0);
            Render::DrawQuad(boxCenter, Vec2(kBoxW, kBoxH), 0.0f, selected ? 0xFF204848 : 0xD0202028);

            char line[64];
            snprintf(line, sizeof(line), "%s%s", selected ? "> " : "", items[i]);
            unsigned int color = selected ? 0xFF40E0E0 : 0xFFE0E0E0;
            Render::DrawUIText(sFontUI, kNxCenter, ny, line, color, 1.1f, true);
        }
        Render::DrawUIText(sFontUI, 0.5f, 0.85f, "Left/Right: move   Jump: select   Restart: back", 0xFFA0A0A0, 0.8f, true);
    }

    static void DrawMainMenu()
    {
        BeginMenuFrame();
        DrawMenuBackground();
        DrawCruxBanner();
        static const char *kItems[] = { "Play", "Select Level", "Customize" };
        DrawMenuList(NULL, kItems, 3, sMenuCursor);
        Render::EndFrame();
    }

    // Each level as a tile: name, a thumbnail (cave_bg reused with a
    // per-tile hue so tiles are visually distinct -- there's no real
    // per-level art yet, see TODO.md), the saved PB (Save::, spans all
    // levels regardless of which one is currently loaded), and the gold-
    // medal time as "score to beat" (ScoreTracker's medal thresholds are the
    // same for every level right now -- see score.h -- so this is the same
    // sScore instance any level would use; FormatScore doesn't depend on
    // which level is active).
    //
    // Laid out as a paginated kLevelGridCols x kLevelGridRows grid (not one
    // long row) so this scales to a much larger level count -- built with an
    // eye toward ~20 levels, not just the current 3 placeholders. The
    // active page is DERIVED from sMenuCursor (page = cursor / tilesPerPage)
    // rather than tracked separately, so Left/Right's existing linear
    // cursor wrap (unchanged from before) pages automatically as the cursor
    // crosses a page boundary; Up/Down jump a full row (see
    // UpdateLevelSelect) for faster 2D movement within/across pages.
    static void DrawLevelSelect()
    {
        BeginMenuFrame();
        DrawMenuBackground();

        int count = LevelData::PlaceholderLevelCount();
        const int kTilesPerPage = kLevelGridCols * kLevelGridRows;
        int pageCount = (count + kTilesPerPage - 1) / kTilesPerPage;
        int page = sMenuCursor / kTilesPerPage;
        int pageStart = page * kTilesPerPage;

        char title[40];
        if (pageCount > 1)
            snprintf(title, sizeof(title), "SELECT LEVEL  (page %d/%d)", page + 1, pageCount);
        else
            snprintf(title, sizeof(title), "SELECT LEVEL");
        Render::DrawUIText(sFontUI, 0.5f, 0.06f, title, 0xFFFFFFFF, 1.4f, true);

        static const unsigned int kTileHues[] =
        {
            0xFF6090B0, 0xFF80A868, 0xFFB08858, 0xFFA07098, 0xFF70A0A0, 0xFFB0A060
        };
        const float kTileW = 4.2f, kTileH = 4.0f;
        const float kColSpacing = 5.0f, kRowSpacing = 4.6f;
        const float kGridTop = 2.4f;
        float startX = -kColSpacing * (kLevelGridCols - 1) * 0.5f;

        for (int local = 0; local < kTilesPerPage; local++)
        {
            int i = pageStart + local;
            if (i >= count) break;
            int col = local % kLevelGridCols;
            int row = local / kLevelGridCols;
            Vec2 tileCenter(startX + col * kColSpacing, kGridTop - row * kRowSpacing);
            bool selected = (i == sMenuCursor);

            if (selected)
                Render::DrawQuad(tileCenter, Vec2(kTileW + 0.25f, kTileH + 0.25f), 0.0f, 0xFF40E0E0);
            Render::DrawQuad(tileCenter, Vec2(kTileW, kTileH), 0.0f, 0xE0202028);

            Vec2 thumbCenter = tileCenter + Vec2(0.0f, 0.9f);
            unsigned int hue = kTileHues[i % (sizeof(kTileHues) / sizeof(kTileHues[0]))];
            Render::DrawTexturedQuad(sTexCaveBg, thumbCenter, Vec2(kTileW - 0.4f, 1.3f), 0.0f, hue);

            float nx = 0.5f + tileCenter.x / 16.0f;
            char line[48];

            snprintf(line, sizeof(line), "Level %d", i + 1);
            float ny = 0.5f - (tileCenter.y - 0.35f) / 12.0f;
            Render::DrawUIText(sFontUI, nx, ny, line, selected ? 0xFF40E0E0 : 0xFFE0E0E0, 0.85f, true);

            if (Save::HasBest(i))
            {
                char pb[16];
                sScore.FormatScore(Save::GetBest(i), pb, sizeof(pb));
                snprintf(line, sizeof(line), "PB: %s", pb);
            }
            else
            {
                snprintf(line, sizeof(line), "PB: --");
            }
            ny = 0.5f - (tileCenter.y - 1.0f) / 12.0f;
            Render::DrawUIText(sFontUI, nx, ny, line, 0xFFC0C0C0, 0.7f, true);

            char goal[16];
            sScore.FormatScore(sScore.Thresholds().gold, goal, sizeof(goal));
            snprintf(line, sizeof(line), "Beat: %s", goal);
            ny = 0.5f - (tileCenter.y - 1.6f) / 12.0f;
            Render::DrawUIText(sFontUI, nx, ny, line, 0xFFD4AF37, 0.7f, true);
        }

        Render::DrawUIText(sFontUI, 0.5f, 0.92f, "Left/Right: move   Up/Down: row   Jump: select   Restart: back", 0xFFA0A0A0, 0.75f, true);
        Render::EndFrame();
    }

    // Draws the rig with each limb group's OWN committed colour
    // (sLimbColorIndex), except the limb currently being edited in step 1's
    // grid, which live-previews the swatch under the cursor instead of the
    // committed value -- so you see the change before confirming with Jump.
    static void DrawCustomizerRig(Vec2 bodyPos, int previewLimb, int previewSwatch)
    {
        float bodyRotZ = 0.0f;
        unsigned int torsoColor = (previewLimb == kLimbTorso) ? kColorSwatches[previewSwatch] : kColorSwatches[sLimbColorIndex[kLimbTorso]];
        unsigned int armsColor = (previewLimb == kLimbArms) ? kColorSwatches[previewSwatch] : kColorSwatches[sLimbColorIndex[kLimbArms]];
        unsigned int legsColor = (previewLimb == kLimbLegs) ? kColorSwatches[previewSwatch] : kColorSwatches[sLimbColorIndex[kLimbLegs]];

        Vec2 shoulderL = bodyPos + kShoulderOffsetLeft;
        Vec2 shoulderR = bodyPos + kShoulderOffsetRight;
        Vec2 hipL = bodyPos + kHipOffsetLeft;
        Vec2 hipR = bodyPos + kHipOffsetRight;
        Vec2 handL = bodyPos + kHandOffsetLeft;
        Vec2 handR = bodyPos + kHandOffsetRight;
        Vec2 legDir(0.0f, -kLegLength);

        DrawLimb(sTexLeg, hipL, hipL + legDir, kLimbAspect, legsColor, 180.0f);
        DrawLimb(sTexLeg, hipR, hipR + legDir, kLimbAspect, legsColor, 0.0f, false, true);
        Render::DrawTexturedQuad(sTexFeet, hipL + legDir, kFeetSize, 0.0f, 0xFFFFFFFF, true);
        Render::DrawTexturedQuad(sTexFeet, hipR + legDir, kFeetSize, 0.0f, 0xFFFFFFFF, false);

        Render::DrawTexturedQuad(sTexBody, bodyPos, kBodySize, bodyRotZ, torsoColor);
        Vec2 shortsPos = bodyPos + kShortsLocalOffset;
        Render::DrawTexturedQuad(sTexShorts, shortsPos, kShortsSize, bodyRotZ, torsoColor);
        Vec2 bagPos = bodyPos + kBagLocalOffset;
        Render::DrawTexturedQuad(sTexBag, bagPos, kBagSize, bodyRotZ, 0xFFFFFFFF);
        Vec2 headPos = bodyPos + kHeadLocalOffset;
        Render::DrawTexturedQuad(sTexHead, headPos, kHeadSize, bodyRotZ, 0xFFFFFFFF);

        DrawLimb(sTexArm, shoulderL, handL, kLimbAspect, armsColor);
        DrawLimb(sTexArm, shoulderR, handR, kLimbAspect, armsColor, 0.0f, true);
        float capAngleL = AngleAlong(shoulderL - handL);
        float capAngleR = AngleAlong(shoulderR - handR);
        Render::DrawTexturedQuad(sTexShoulder, shoulderL, kShoulderCapSize, capAngleL, armsColor);
        Render::DrawTexturedQuad(sTexShoulder, shoulderR, kShoulderCapSize, capAngleR, armsColor, true);
        Render::DrawTexturedQuad(sTexHand, handL, kHandSize, 0.0f, 0xFFFFFFFF, true);
        Render::DrawTexturedQuad(sTexHand, handR, kHandSize, 0.0f, 0xFFFFFFFF, false);
    }

    static void DrawColorGrid(int limb, int cursor)
    {
        char title[32];
        snprintf(title, sizeof(title), "%s COLOUR", kLimbGroupNames[limb]);
        Render::DrawUIText(sFontUI, 0.56f, 0.16f, title, 0xFFFFFFFF, 1.2f);

        int rows = (kColorSwatchCount + kColorGridCols - 1) / kColorGridCols;
        const float kCell = 0.9f, kGap = 0.18f;
        float gridH = rows * kCell + (rows - 1) * kGap;
        Vec2 gridOrigin(2.1f, 1.2f + gridH * 0.5f); // top-left swatch center, world space

        for (int i = 0; i < kColorSwatchCount; i++)
        {
            int r = i / kColorGridCols;
            int c = i % kColorGridCols;
            Vec2 pos(gridOrigin.x + c * (kCell + kGap), gridOrigin.y - r * (kCell + kGap));
            if (i == cursor)
                Render::DrawQuad(pos, Vec2(kCell + 0.2f, kCell + 0.2f), 0.0f, 0xFF40E0E0);
            Render::DrawQuad(pos, Vec2(kCell, kCell), 0.0f, kColorSwatches[i]);
        }

        Render::DrawUIText(sFontUI, 0.42f, 0.78f, "Arrows: move   Jump: confirm   Restart: cancel", 0xFFA0A0A0, 0.8f);
    }

    // Live rest-pose preview so the color choice is visible while picking --
    // draws the rig directly with fixed constants rather than through
    // sPlayer, since sPlayer may not be Reset() yet if Play hasn't been
    // picked (its default-constructed state isn't a real rest pose). Rig is
    // pushed to the left half of the screen so it doesn't overlap the
    // limb-list/color-grid UI on the right.
    static void DrawCustomizer()
    {
        BeginMenuFrame();
        DrawMenuBackground();

        Render::DrawUIText(sFontUI, 0.10f, 0.06f, "CUSTOMIZE", 0xFFFFFFFF, 1.4f);

        Vec2 bodyPos(-5.3f, -0.3f);
        int previewLimb = (sCustomizerStep == 1) ? sCustomizerLimb : -1;
        DrawCustomizerRig(bodyPos, previewLimb, sColorGridCursor);

        if (sCustomizerStep == 0)
        {
            static const char *kItems[] = { "Torso", "Arms", "Legs" };
            DrawMenuList("PICK A LIMB", kItems, kLimbGroupCount, sMenuCursor);
        }
        else
        {
            DrawColorGrid(sCustomizerLimb, sColorGridCursor);
        }

        Render::EndFrame();
    }

    static void DrawPauseMenuOverlay()
    {
        // Dim the gameplay world behind the pause list -- oversized quad so
        // it covers the visible area regardless of the camera's current zoom
        // (CameraController.cs dynamically zooms out during flight).
        Render::DrawQuad(sCamera.Position(), Vec2(100.0f, 100.0f), 0.0f, 0x90000000);
        static const char *kItems[] = { "Resume", "Reset Level", "Main Menu" };
        DrawMenuList("PAUSED", kItems, 3, sMenuCursor, sCamera.Position(), sCamera.OrthoSize());
    }

    void Draw()
    {
        switch (sGameState)
        {
            case kStateMainMenu: DrawMainMenu(); return;
            case kStateLevelSelect: DrawLevelSelect(); return;
            case kStateCustomizer: DrawCustomizer(); return;
            case kStatePlaying:
            case kStatePauseMenu:
                break;
        }

        Render::BeginFrame(sCamera.Position(), sCamera.OrthoSize());
        DrawGameplayWorld();
        if (sGameState == kStatePauseMenu) DrawPauseMenuOverlay();
        Render::EndFrame();
    }
}
