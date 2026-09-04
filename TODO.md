# Crux PS3 port — working TODO

Goal: recreate Crux (the Unity game at `D:\Unity Projects\Crux\Crux`) natively
on the DECHJ00A devkit, as close to the original as possible — or better —
using the real Sony PS3 SDK at `D:\PS3`. See also the original plan at
`C:\Users\JoeWe\.claude\plans\fancy-mapping-seal.md` for the phased approach
and rationale (why this is a native rewrite, not a Unity export).

Build both targets any time with `build.bat` (stages to `Build\PC\` and
`Build\PS3\`).

## Done

- [x] Verified the real PS3 SDK toolchain end-to-end (SN GCC, Cg shader
      compiler, `make_fself`) — see plan file for the MSYS2/`make` story.
- [x] Core swing/flight/flip physics ported 1:1 from `PlayerController.cs`
      (`src/player.cpp`) — pivot-invariance trick documented in `player.h`.
- [x] Camera follow/lookahead/zoom ported from `CameraController.cs`
      (`src/camera2d.cpp`).
- [x] Scoring/medals ported from `ScoreManager.cs`/`LevelTimer.cs`
      (`src/score.cpp`) — **local-only**, no PlayFab (per project scope).
- [x] Leg flap, head bob, and hand-color grip-state animation ported from
      `PlayerController.cs`'s `UpdateLegs`/`UpdateHead`/`UpdateHandColors`.
- [x] Real player sprite art wired in (`data/sprites/*.png`, copied from
      `Assets/Art/Player`) — PC-verified by screenshot, looks recognizably
      like Crux. PS3 side compiles but falls back to flat colored quads
      (see "PS3 texture pipeline" below).
- [x] `build.bat` builds both targets and stages runnable copies into
      `Build\PC\` and `Build\PS3\` (with `PS3_DEPLOY_README.txt`).

## Next — can make real progress on these without the user

Ordered roughly by priority. Rebuild via `build.bat` and screenshot-verify
(PC) after each meaningful change; keep the PS3 build compiling even where
it can't be tested on hardware yet.

1. **Rig proportions.** [Iterating] Pass 1: too-small head, over-long torso
   -- fixed (bigger head, more compact body/legs). Pass 2 (user-reported):
   arms weren't V-shaped (shoulder and hand-grip anchors were almost the
   same point, so arms rendered as short stubs) and legs had no feet, just
   a bare trouser sprite -- fixed by pulling the hand-grip anchor
   (`kHandOffsetLeft/Right` in app.cpp) wide and high, and adding
   `feet.png` at each leg's end. Screenshot-verified, now a close match to
   the reference "landing" pose. Note: the hand-grip anchor also doubles
   as the swing physics pivot, so this changed swing radius/feel too, not
   just the visual -- worth re-checking gameplay feel, not just looks.
   Pass 3 (user-reported): arms/legs still ~50% too small overall (length
   and thickness) -- scaled reach anchor and leg length 1.5x, and widened
   the limb aspect ratio beyond the raw source PNG ratio (63/512 -> 0.185)
   for a chunkier look. Screenshot-verified, clear improvement.
   Pass 4 (user-reported): hands/feet also too small -- doubled kHandSize
   and kFeetSize. Screenshot-verified, both now clearly visible/readable.
   Still to check: proportions across more poses (mid-swing at speed,
   flying, both-hands-attached, flipped) -- only checked at-rest so far.
2. **Background rendering.** Load the Cave art
   (`Assets/Art/Background/Stylised/Cave/*.png`) and draw it behind the
   tile grid instead of the flat navy fill. Reference screenshots show
   large background rock formations plus the foreground climbable tiles.
3. **On-screen UI text.** Reference screenshots show `Current` / `PB` / `WR`
   and a `TROPHIES (Jumps)` panel with Platinum/Gold/Silver/Bronze
   thresholds in medal colors, plus a player name/ID line — this is
   `HighscoreManager.cs` + `ScoreManager.cs`'s UI, minus the PlayFab parts
   (WR can just be omitted or show "--" locally). Needs a text-rendering
   path: SDL2_ttf is the easy route on PC; PS3 already has `cellDbgFontDrawGcm`
   wired via gcmutil's `onDbgfont` (see `main_ps3.cpp`) which can host the
   same info cheaply there.
4. **7 levels.** Only `data/level1.lvl` (via the placeholder test room) is
   wired up right now. Build 6 more distinct placeholder rooms (varied
   layout/difficulty) so the level-select/progression structure exists
   end-to-end, and swap in `LevelReset.cs`/`LevelEnd.cs`-equivalent flow
   between them. Swap for real exported data once available (see below).
5. **PB persistence.** `ScoreTracker` currently keeps personal-bests in
   memory only (resets on relaunch) — see the TODO comment in `score.cpp`.
   Wire real persistence: a simple local file on PC (fully testable now);
   `cellSaveData` on PS3 (compiles, but can't be verified without hardware).
6. **PS3 texture pipeline.** `Render::LoadTexture`/`DrawTexturedQuad` in
   `render_ps3.cpp` are stubs (see the TODO comment there) — falls back to
   flat-colored quads. Needs: PNG→DDS conversion (check for a usable
   converter; `D:\PS3\host-win32\bin\dds2gtf.exe` takes DDS, not PNG, so
   this is the missing link), `dds2gtf` → GTF, `cellGcmUtilLoadTexture`,
   and a textured variant of `vs_quad.cg`/`fs_quad.cg` (UV attribute +
   texture sampler — current shaders are vertex-color-only). This is the
   biggest remaining engineering chunk and the main thing standing between
   the PS3 build and looking like the PC build.
7. Re-run `build.bat` after each step above; keep both targets green.

## Blocked on the user — can't proceed without you

- **Real Level 1–7 layout + player rig transform data.** Needs one click of
  the `PS3 > Export Level + Rig Data` Unity Editor menu item
  (`Assets/Editor/PS3LevelExporter.cs`) — blocked from running headless
  because Unity's batch-mode license check needs an activated `.ulf` this
  machine doesn't have (it's a normal Hub-login license). Once exported
  (`data/level1.lvl` … `level7.lvl`, `data/player_rig.txt`), swap the
  placeholder level/rig constants in `app.cpp` for the real thing.
- **On-hardware testing.** The DECHJ00A is powered off. `Build\PS3\` is
  staged and ready (see `PS3_DEPLOY_README.txt`) for whenever it's back on
  — either via Target Manager (target "PS3 Test", 10.1.1.2) or by copying
  the folder over directly.
- **Installable `.pkg` polish** (icon art, title metadata) — can scaffold
  the packaging mechanics, but final call on branding/title is the user's.

## Backlog / explicitly out of scope

- Online leaderboard (PlayFab) — dropped per project scope, local-only scoring.
- Anything requiring the official Sony NP network services.
