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
- [x] **Pause.** `Input::pauseIsPressed` (Return/Start) existed but was never
      consumed anywhere — wired it into `App::Update` (shared `sPaused` flag,
      edge-triggered toggle, early-returns before any gameplay/physics
      update) and added a "PAUSED" overlay to the PC build's
      `DrawUIOverlay()` (PS3 already gets an equivalent for free from the
      SDK template's own `onDbgfont`, which prints "Crux: PAUSE" via
      `gSampleApp.isPause`). `main_ps3.cpp`'s `onUpdate` simplified to drop
      its own redundant `!gSampleApp.isPause` gate, since that and the
      shared `sPaused` flag are edge-triggered off the same underlying pad
      state in the same frame — only `isSysMenu` (in-game XMB, no PC
      equivalent) still gates there. Both targets rebuilt clean, full
      `build.bat` pass (PC+PS3+textures+`.pkg`) verified green afterward.
      **Not interactively verified** — see "Open questions" below.

## Open questions for the user (don't block on these — keep working, just flag)

- **Flip visual read.** Does the hand/foot-mirror-swap approximation for
  `isFlipped` actually look right in motion? This needs live play (press
  the flip key mid-swing), not a screenshot — genuinely can't verify
  further without a human at the keyboard or a way to script input timing.
  (Tried once: temporarily patched `input_pc.cpp` behind a
  `CRUX_SCRIPTED_TEST` macro to drive `Input::` from a hardcoded frame
  script instead of real keys, sidestepping the input-injection problem
  entirely. Partially useful — it did produce a genuine in-flight/falling
  screenshot showing the rig reads fine while airborne (arms/legs/cap/bag
  all sensible, nothing broken or misscaled) — but SDL/asset-loading takes
  long enough on first launch that several fixed-timestep ticks fire in a
  burst before the window is even visible, so the scripted frame numbers
  (tuned for one tick per screenshot-poll) fired well before intended and
  the both-hands-attach/flip-mid-swing scenarios never cleanly triggered
  (angular velocity stayed ~0 throughout since left/right was never held,
  so flip's rotation-sign effect had nothing to act on). Reverted the
  patch fully rather than sink more time into burst-proofing it — a real
  keypress remains the reliable way to check this specific one.)
- **Pause needs a real keypress test.** Tried scripting this four different
  ways (`SendKeys`, `keybd_event`, and `SendInput` — the last confirmed
  successful via both its return codes and a verified genuine
  `SetForegroundWindow` match) and none of them ever registered as a
  keypress inside the app (a temporary debug counter on
  `Input::pauseIsPressed` stayed at 0 every time, even though screenshots
  and focus checks both succeeded). That points at this environment
  sandboxing/isolating synthetic input delivery from whatever desktop the
  window actually renders to, not a bug in the pause logic itself — the
  toggle is a direct copy of the already-working `Pressed()` edge-detection
  pattern already used for restart/jump/flip/up/down. Please just press
  Enter (PC) / Start (PS3) once during a real play session to confirm it
  pauses/unpauses and the "PAUSED" text shows.
- **Package branding.** `packaging/package.conf`'s `Content_ID` and
  `packaging/ICON0.PNG` (currently a crop of `head.png`) are placeholders.
  Real title art/ID is a user call, not something to guess further.
- **Rig proportions in motion.** Everything's been checked at-rest or in a
  landing pose; mid-swing/both-hands-attached poses still haven't had
  dedicated screenshot passes. One data point now exists for airborne/
  falling: the scripted-input attempt above (before it fell through the
  level's reset trigger) caught a genuine in-flight frame — rig reads fine
  spread out mid-air, nothing broken or misscaled. Not a substitute for
  seeing a real controlled swing→flight arc, but a positive sign.
- **PS3 save-data writability.** PB persistence (`save.cpp`) uses plain
  `fopen`/`fwrite` to `SYS_APP_HOME/save.dat` on PS3, not `cellSaveData`.
  Works in principle (same stdio calls as the PC version, which is
  verified), but whether `SYS_APP_HOME` is actually writable in whatever
  context the `.pkg` runs in is unverified without the devkit. Ask if the
  polished `cellSaveData` XMB-icon experience is wanted eventually, or if
  simple file persistence is fine.
- **PS3 textures on real hardware.** The whole PNG->DDS->GTF->
  `cellGcmUtilLoadTexture`->textured-shader pipeline compiles, links, and
  packages clean, and `dds2gtf` accepts the hand-written DDS with zero
  errors -- but none of it has been seen on an actual screen yet. First
  hardware run should specifically check: sprites appear at all (texture
  unit binding correct), right way up / not mirrored (UV orientation
  guess), and correct colors (vertex-color-modulate byte order, same open
  question as the UI text color already flagged above).

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
3. ~~**More climbing levels.**~~ [Done] `LevelData::LoadPlaceholderLevel(int)`
   now dispatches to 3 distinct layouts (`LevelData::PlaceholderLevelCount()`):
   the original tall shaft, a wide "cavern" with floating platform islands
   (flight-jump-focused), and a narrow "chimney" with alternating nubs every
   row (tight continuous-swing-focused). `app.cpp`'s `LoadLevelByIndex`
   tries `data/levelN.lvl` first (real exported data, once available) before
   falling back to the placeholder for that index. Reaching the end trigger
   now auto-advances to the next level (wrapping after the last) instead of
   just stopping the run in place -- simple linear progression, no
   level-select menu yet. All 3 layouts screenshot-verified individually.
   Fixed a correctness issue found along the way: PB was a single global
   value that would've misleadingly carried over between structurally
   different levels -- added `ScoreTracker::ResetPersonalBest()`, called on
   every level load. Note this means PBs don't persist per-level either
   (same root cause as item 4 below, just a different symptom -- worth
   fixing together).
4. ~~**PB persistence.**~~ [Done, PC-verified] New `src/save.h`/`save.cpp`
   (`Save::` namespace) keeps a small keyed store (level index -> best
   score) in a flat text file (`save.dat`, one line per level:
   `<index> <hasValue 0/1> <value>`). `app.cpp`'s `LoadLevelByIndex` loads
   the saved PB for that level (or resets if none saved); the end-trigger
   handler in `Update()` writes a new PB back immediately via
   `Save::SetBest` right after `ScoreTracker::StopRun` (`ScoreTracker`
   itself still doesn't know about levels -- App:: bridges the two, kept
   deliberately separate). Verified with a standalone round-trip test
   (write two levels' PBs, force a fresh reload simulating a relaunch,
   confirm both values and the untouched slots' `HasBest()` all come back
   correct) -- deleted after confirming, wasn't left in the tree.
   Simplified on PS3: plain `fopen`/`fwrite` via `SYS_APP_HOME`, not (yet)
   the full `cellSaveData` dialog/icon/PARAM.SFO-metadata system, which is
   a much bigger API that can't be verified without hardware anyway. This
   compiles for PS3 and should work identically in principle (same stdio
   calls), but is **not verified on real hardware** — if `SYS_APP_HOME`
   turns out not to be writable in whatever context the `.pkg` actually
   runs in, saves would silently fail there. Worth an on-hardware check
   once the devkit's back on; upgrading to real `cellSaveData` (so saves
   show up properly in the XMB) is a separate, larger follow-up if wanted.
5. ~~**PS3 texture pipeline.**~~ [Done, compiles+links clean] The full
   PNG→DDS→GTF→`cellGcmUtilLoadTexture`→textured-Cg-shader chain now works,
   verified against the real Sony tools at every step:
   - **PNG→DDS**: no DDS encoder exists anywhere on this system (no
     ImageMagick, NVIDIA Texture Tools, texconv), and installing one
     wouldn't guarantee a DDS flavor `dds2gtf` actually accepts, so
     `buildscripts/png_to_dds.ps1` hand-writes an uncompressed 32bpp BGRA
     DDS (classic D3D "A8R8G8B8" layout) directly from a `System.Drawing`
     bitmap — same approach as `make_param_sfo.ps1`. Header verified
     byte-for-byte by hand against the DDS spec.
   - **DDS→GTF**: `dds2gtf.exe` (the real SDK tool) accepts the
     hand-written DDS with **zero errors** — real, tool-validated
     confirmation the format is correct, not just structurally plausible.
   - `buildscripts/make_textures.ps1` batch-converts all of
     `data/sprites/*.png` to `data/gtf/*.gtf` (9 sprites currently); wired
     into `build.bat` as its own stage, before the PS3 build.
   - `render_ps3.cpp`'s `LoadTexture` now calls `cellGcmUtilLoadTexture`
     for real (derives `data/gtf/<name>.gtf` from the PNG filename passed
     in); `DrawTexturedQuad` builds a UV-carrying vertex buffer, binds the
     texture via `cellGcmUtilSetTextureUnit`, and draws with a new shader
     pair (see below) — falls back to `DrawQuad`'s flat color only if a
     texture handle is invalid, not unconditionally like before.
   - New `vs_quad_tex.cg`/`fs_quad_tex.cg` (modeled on the SDK's own
     `samples/common/gcmutil/samples/dice` textured-cube shaders): pass
     through a UV coordinate, sample via `tex2D`, modulate by vertex color
     (reuses the existing tint mechanism, e.g. hand-color states). Compiled
     clean through the real `sce-cgc` Cg compiler for both `sce_vp_rsx` and
     `sce_fp_rsx` profiles.
   - Full chain compiles and links clean end-to-end (`crux.ppu.elf` links
     fine against the stub libs already in `PPU_LDLIBS`), and the complete
     `build.bat` run (PC → textures → PS3 → package) succeeds with all 4
     stages green, `.pkg` correctly includes `data/gtf/*.gtf` and both
     shader pairs.
   - **Not visually verified** (needs the devkit) — the *shader logic* and
     *tool acceptance* are real and verified; whether the on-screen result
     actually looks like the PC build (correct UV orientation, correct
     color-mod byte order, no texture-unit state bugs) can't be confirmed
     without hardware. Flagged in "Open questions" below.
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
