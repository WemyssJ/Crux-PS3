# Crux PS3 port — working TODO

Goal: recreate Crux (the Unity game at `D:\Unity Projects\Crux\Crux`) natively
on the DECHJ00A devkit, as close to the original as possible — or better —
using the real Sony PS3 SDK at `D:\PS3`. See also the original plan at
`C:\Users\JoeWe\.claude\plans\fancy-mapping-seal.md` for the phased approach
and rationale (why this is a native rewrite, not a Unity export).

Build both targets any time with `build.bat` (stages to `Build\PC\` and
`Build\PS3\`, including a runnable installable `.pkg` for PS3 — see below).
Nothing but source should live outside `Build\`/`buildscripts\pkg_stage\`
after a build — if you see a stray `.exe`/`.self`/`.dll`/`.pkg` sitting in
the project root, that's a bug, not intended.

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
- [x] Full rig wired up with real sprite art (`data/sprites/*.png`): body,
      head, arms, legs, hands, feet, shoulder caps, waist bag. Iterated
      against multiple user-provided reference screenshots — proportions,
      V-shaped arm reach, hand/foot mirroring, wrist/heel anchor points,
      shoulder cap orientation, neck join, and bag placement all
      screenshot-verified and corrected at least once each. See git log
      for the individual correction commits if a specific constant's
      history matters. PS3 side compiles but still falls back to flat
      colored quads for all of this (see "PS3 texture pipeline" below).
- [x] **Flip mirroring wired up** — `Player::IsFlipped()` existed but
      `app.cpp` never used it. Best-effort interpretation (documented
      inline in `app.cpp` and below under "Open questions"): the
      original's `FlipAroundCurrentHandPivot` does a 3D rotate-around-the-
      arm-axis that has no direct 2D equivalent and, under our fixed-
      offset pivot model, collapses to a physics no-op (see the comment in
      `player.h`/`player.cpp`'s `TryFlip`); its only concretely-documented
      visual effect (`FlipHands()`) mirrors the two hand sprites, so that's
      what's implemented — `isFlipped` swaps which hand/foot gets the
      horizontal-mirror flag. **Not yet interactively playtested** (needs
      a live keypress at the right moment mid-swing, not just a screenshot)
      — flag for the user to confirm this reads right.
- [x] **PS3 `.pkg` packaging pipeline** — builds a genuine installable
      package via the real Sony tools (`make_fself_npdrm`,
      `make_package_npdrm`), not just a debug `.self`:
      - `buildscripts/make_param_sfo.ps1` generates PARAM.SFO by hand (the
        SDK ships no scriptable tool for this — `ps3sys.exe` is a GUI-only
        submission tool).
      - `buildscripts/make_pkg.ps1` stages `USRDIR/` (signed `EBOOT.BIN` +
        game data), generates `PARAM.SFO`/copies `ICON0.PNG`, and calls
        `make_package_npdrm`.
      - `build.bat`'s PS3 stage now runs this automatically, producing
        `Build\PS3\UP0001-CRUX00001_00-CRUXPS3PORT00001.pkg`.
      - `packaging/package.conf` and `packaging/ICON0.PNG` are placeholder
        homebrew values (not a real Sony-registered title) — see "Open
        questions" below on whether the user wants different branding.
      - **Not yet install-tested** (needs the devkit powered on).
- [x] Fixed a real bug: a `build\` scripts folder and the `Build\` output
      folder are the *same directory* on a case-insensitive filesystem.
      Scripts now live in `buildscripts\` instead.
- [x] Replaced the small flat placeholder test room with a tall vertical
      climbing shaft (`LevelData::LoadPlaceholderTestRoom` in `level.cpp`,
      ~48 units tall) with staggered ledges whose gaps widen with height,
      so both swing-only and flight-jump traversal get exercised, and so
      the camera's flight-follow/zoom path (which only activates after
      >1s of sustained flight) actually has room to trigger.
- [x] `build.bat` builds all targets and stages runnable copies into
      `Build\PC\` and `Build\PS3\`, with zero leftover artifacts in the
      project root (verified — `move`, not `copy`, for every build output).
      A launcher copy also lives at `Build\build.bat` for discoverability.

## Open questions for the user (don't block on these — keep working, just flag)

- **Flip visual read.** Does the hand/foot-mirror-swap approximation for
  `isFlipped` actually look right in motion? This needs live play (press
  the flip key mid-swing), not a screenshot — genuinely can't verify
  further without a human at the keyboard or a way to script input timing.
- **Package branding.** `packaging/package.conf`'s `Content_ID` and
  `packaging/ICON0.PNG` (currently a crop of `head.png`) are placeholders.
  Real title art/ID is a user call, not something to guess further.
- **Rig proportions in motion.** Everything's been checked at-rest or in a
  landing pose; mid-swing/flying/both-hands-attached poses haven't had
  dedicated screenshot passes.

## Next — can make real progress on these without the user

Ordered roughly by priority. Rebuild via `build.bat` and screenshot-verify
(PC) after each meaningful change; keep the PS3 build compiling even where
it can't be tested on hardware yet.

1. ~~**Background rendering.**~~ [Done] `cave_bg.png` (copied from
   `Assets/Art/Background/Stylised/Cave/Cave - BigRocks1.png`) tiled behind
   the level, following the camera, dark-tinted for depth. Screenshot-
   verified — reads clearly as a cave, close to reference. Note:
   `cave_bg.png` is actually a whole Unity sprite atlas (multiple rock
   formations meant to be sliced via .asset metadata this project doesn't
   have easy access to outside the Editor), so it's drawn as one big
   repeating tile rather than properly sliced -- looks fine at this
   distance/scale but has visible repetition up close. Real per-sprite
   slicing is a nice-to-have follow-up, not blocking. PS3 side still falls
   back to a flat tinted quad (texture pipeline stub, see below).
2. ~~**On-screen UI text.**~~ [Done] Added `Render::LoadFont`/`DrawUIText`
   (normalized-screen-space text overlay) to the render interface: PC via
   SDL2_ttf (font: `data/fonts/ui.ttf`, copied from Unity's own bundled
   `TextMesh Pro/Fonts/LiberationSans.ttf` -- open-source, safe to
   redistribute), PS3 via gcmutil's built-in `cellGcmUtilPrintf`/debug font
   (genuinely functional, not a stub, though color byte-order vs. our ARGB
   convention isn't verified against real hardware -- flagged inline in
   `render_ps3.cpp`). `ScoreTracker::FormatScore` added (ported from
   `ScoreManager.cs`'s time mm:ss:ms / count formatting). `app.cpp`'s new
   `DrawUIOverlay()` shows Current/PB + a Platinum/Gold/Silver/Bronze
   trophy panel in medal colors, ported from `HighscoreManager.cs`/
   `ScoreManager.cs` minus the PlayFab-backed WR (no online leaderboard --
   just not shown, rather than faked). Screenshot-verified, closely matches
   the reference screenshots' layout.
3. **More climbing levels.** Only one placeholder level exists (the tall
   shaft above). Build a few more distinct ones (varied layout/difficulty)
   so the level-select/progression structure exists end-to-end, and wire a
   `LevelReset.cs`/`LevelEnd.cs`-equivalent flow between them. Swap for
   real exported data once available (see "Blocked on the user").
4. **PB persistence.** `ScoreTracker` currently keeps personal-bests in
   memory only (resets on relaunch) — see the TODO comment in `score.cpp`.
   Wire real persistence: a simple local file on PC (fully testable now);
   `cellSaveData` on PS3 (compiles, but can't be verified without hardware).
5. **PS3 texture pipeline.** `Render::LoadTexture`/`DrawTexturedQuad` in
   `render_ps3.cpp` are stubs (see the TODO comment there) — falls back to
   flat-colored quads. Needs: PNG→DDS conversion (check for a usable
   converter; `D:\PS3\host-win32\bin\dds2gtf.exe` takes DDS, not PNG, so
   this is the missing link), `dds2gtf` → GTF, `cellGcmUtilLoadTexture`,
   and a textured variant of `vs_quad.cg`/`fs_quad.cg` (UV attribute +
   texture sampler — current shaders are vertex-color-only). This is the
   biggest remaining engineering chunk and the main thing standing between
   the PS3 build and looking like the PC build.
6. Re-run `build.bat` after each step above; keep both targets green.

## Blocked on the user — can't proceed without you

- **Real Level 1–7 layout + player rig transform data.** Needs one click of
  the `PS3 > Export Level + Rig Data` Unity Editor menu item
  (`Assets/Editor/PS3LevelExporter.cs`) — blocked from running headless
  because Unity's batch-mode license check needs an activated `.ulf` this
  machine doesn't have (it's a normal Hub-login license). Once exported
  (`data/level1.lvl` … `level7.lvl`, `data/player_rig.txt`), swap the
  placeholder level/rig constants in `app.cpp` for the real thing — this
  would also resolve a lot of the remaining rig-proportion guesswork.
- **On-hardware testing.** The DECHJ00A is powered off. `Build\PS3\` is
  staged and ready (see `PS3_DEPLOY_README.txt`) for whenever it's back on
  — either via Target Manager (target "PS3 Test", 10.1.1.2), copying the
  folder over directly, or installing the `.pkg` (untested either way).
- **Package branding** (icon art, title metadata) — see "Open questions."

## Backlog / explicitly out of scope

- Online leaderboard (PlayFab) — dropped per project scope, local-only scoring.
- Anything requiring the official Sony NP network services.
