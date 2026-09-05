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
      history matters. PS3 side now renders the same real sprite textures
      too, not flat colored quads (see "PS3 texture pipeline" below, done
      since this note was originally written).
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
- [x] **Camera pivot wiring bug, found via source comparison.** Read
      `CameraController.cs`/`PlayerController.cs` fresh against
      `camera2d.cpp`/`player.cpp` and found a real gap: the source calls
      `cameraController?.UpdatePivot()` (snap the camera's follow-pivot to
      the body's current position) from five discrete grip-change call
      sites — both-hands attach, the down-detach branch (unconditionally,
      regardless of whether the swap below it found a new grip),
      `HandleSpaceInput`'s regrab-after-falling branch, `StartFlight`, and
      `SwapHandsPreserveDirection` — rather than every frame, so the camera
      stays anchored on the current grip point while continuously swinging
      on it. `Camera2D::SnapPivot()` already existed for exactly this
      (comment cites `UpdatePivot()` by name) but nothing in `app.cpp` ever
      called it — the pivot only ever moved via `Camera2D::Step`'s own
      `flightCamActive` per-frame catch-up, meaning outside of sustained
      flight the camera would never leave the level's start position no
      matter how much regrabbing/swapping happened. Fixed by adding
      `Player::PivotJustChanged()` (set at the same four `player.cpp`
      call sites the source's `UpdatePivot()` calls map to — attach,
      detach, regrab-after-falling, and the inline `StartFlight` block;
      flip does *not* call `UpdatePivot()` in the source, so it's
      correctly left out), cleared at the top of every `Step()`, and
      checked in `App::Update` right after `sPlayer.Step()` to call
      `sCamera.SnapPivot(sPlayer.BodyPos())`. Verified with a standalone
      test that the flag never fires on idle frames and always clears the
      frame after any event it did fire on (deleted after confirming);
      couldn't get the placeholder level's exact start pose to trigger an
      actual attach/regrab event in the test (swing physics don't require
      continuous wall contact at the pivot, so the geometry needed for
      that is level-layout-specific) — relying on the careful 1:1 mapping
      against every source call site for "fires at the right moment"
      confidence. Both targets rebuild clean, full `build.bat` pass green.
      **Not visually confirmed during real regrab/launch/attach events**
      (needs live play, same category as the other input-gated behaviors
      above) — but this is a source-comparison bug fix, not a guess.
- [x] **Score/level-trigger source-comparison pass — no bugs found, but
      confirms correctness.** Read `LevelEnd.cs`/`LevelReset.cs`/
      `LevelTimer.cs`/`SceneController.cs`/`ScoreManager.cs`/
      `HighscoreManager.cs`/`PlayerStatsTracker.cs` fresh against
      `score.cpp`/`app.cpp`'s trigger handling. Everything checked out:
      medal thresholds (30/60/120s — the *actual* values baked into
      `[UI].prefab`, not the C# script's unused 20/40/90 defaults, checked
      by grepping the prefab YAML directly rather than trusting the
      script), the `<=` threshold-comparison operator, `IsBetter`'s
      Time/Jumps/Swings-lower-is-better vs. Flips-higher-is-better split,
      and `FormatScore`'s mm:ss:ms/integer-rounding logic all match the
      source exactly. `HighscoreManager`'s two-clock design (`LevelTimer`
      for the UI display, `PlayerStatsTracker.ElapsedTime` for the actual
      PB comparison) collapses to our single `ScoreTracker::m_elapsed` —
      confirmed behaviorally equivalent since both source clocks start/
      stop together and the PB snapshot happens synchronously at
      `StopRun()` in both versions, not a bug. One clarification is worth
      its own note above (see item 3's "More climbing levels" — the
      auto-advance-to-next-level behavior isn't from the source at all).
- [x] **Up/Down input-semantics bug, found via source comparison.**
      `InputManager.cs`'s `Update()` uses `WasPressedThisFrame()` (edge-
      triggered) for every action *except* `upIsPressed`/`downIsPressed`,
      which use `IsPressed()` (continuous hold) — the only two actions
      with this distinction. Both `input_pc.cpp` and `input_ps3.cpp` had
      them wired as edge-triggered (`Pressed()` / `cellPadUtilButton
      PressedOnce`), matching every *other* action but missing this one
      exception. Concretely: in the source, holding Up while swinging into
      position triggers the both-hands-attach the *moment* both hands
      start overlapping a wall, however many frames after the button was
      first pressed; with edge-triggered semantics, the single press-frame
      is wasted if it lands even one frame too early, and the attach can
      never fire without releasing and re-pressing at exactly the right
      instant — a real, meaningfully stricter (and wrong) input-timing
      requirement. Fixed by switching both backends to held-state
      (`Held()` / `cellPadUtilButtonPressed`) for just these two fields.
      Both targets rebuild clean, full `build.bat` pass green, static
      screenshot confirms no regression at rest (no keys held, so nothing
      should look different — and doesn't). **Not confirmed with a real
      held-Up-then-swing-into-position test** (needs live play) but this
      is a source-comparison bug fix backed by an exact line-for-line
      read of `InputManager.cs`, not a guess.
- [x] **Full source-comparison sweep complete.** Every gameplay C# script
      under `Assets/Scripts` has now been read fresh this session and
      checked against its C++ port: `PlayerController.cs`,
      `CameraController.cs`, `InputManager.cs`, `LevelEnd.cs`,
      `LevelReset.cs`, `LevelTimer.cs`, `SceneController.cs`,
      `ScoreManager.cs`, `HighscoreManager.cs`, `PlayerStatsTracker.cs`,
      `ScoreType.cs` — only `PlayFabNameManager.cs` wasn't (PlayFab is
      explicitly out of scope, see "Backlog" below). Found and fixed two
      real bugs (camera pivot wiring, up/down input semantics) and
      confirmed everything else already matches. This technique
      (comparing actual source against the port, not deriving numeric
      constants from ambiguous transform/pivot data) has been reliably
      safe and productive all session — worth reaching for again first if
      more porting issues turn up later.
- [x] **Placeholder level-geometry review — two real bugs found and
      fixed.** No C# equivalent exists for these (Unity's real levels are
      Tilemap-based, not scripted), so this was pure geometry reasoning
      over `level.cpp`'s three `LoadPlaceholder*` layouts, not source
      comparison. Found:
      - **Shaft**: `int rung = cy % 6 < 3 ? cy % 6 : -1;` can never equal
        3 (anything ≥3 collapses to -1), so `if (rung == 0 || rung == 3)`
        only ever fired at `rung==0` — ledges appeared every 6 rows
        instead of the every-3-rows the comment describes, roughly
        doubling the intended gap size. Fixed by using `cy % 6` directly.
      - **Cavern**: the hardcoded island list had one badly oversized gap
        — `{9,10,3}` (right edge x=12) to `{-10,13,3}` (left edge x=-7)
        is a ~16-unit jump, versus ~4-5 units for every other transition
        in the sequence, breaking the "stepping up and to the right"
        pattern the comment describes and likely impossible to cross, not
        just hard. Fixed by inserting one stepping-stone island
        (`{0,11,3}`) between them, splitting it into two ~6-7 unit gaps
        in line with the rest of the level's difficulty curve.
      Both fixes verified: rebuilt, screenshotted the shaft (a ledge now
      renders near the start, confirming the fix produces ledges at all)
      and the cavern (loads and renders without corruption after the
      array change — verified via a temporary `sLevelIndex` override to
      load index 1, reverted before committing). Full `build.bat` pass
      green, root clean. **Not verified by actually playing the new gap
      sizes** (needs live play to confirm ~6-7 units is comfortably
      crossable, not just "less obviously impossible than 16") — a
      reasonable, well-justified estimate, not a wild guess, but still an
      estimate.
- **Autonomous session status: stopping here.** This has been a long,
  productive overnight session (PS3 texture pipeline, pause feature,
  cave-background sprite crop, camera pivot fix, up/down input-semantics
  fix, two level-geometry bugs, a full source-comparison sweep of every
  gameplay script, and a well-documented reverted rig-offset derivation
  attempt — see git log for the complete history). Checked render_pc.cpp/
  render_ps3.cpp/vec2.h once more with fresh eyes looking for further
  genuine bugs (not style) and found nothing further worth changing
  blind. Everything actionable without the user is done; what's left is
  either genuinely blocked (real rig/level export data, on-hardware
  testing, package branding) or needs live human playtesting to verify
  (pause, flip visual read, the new gap sizes, rig proportions in
  motion) — not something to keep guessing at. Stopping the loop cleanly
  here rather than manufacturing further busywork.
- [x] **Live side-by-side rig corrections (user playtesting both games at
      once).** A batch of concrete, directly-observed fixes:
      - Left leg was upside down (`Leg.png` authored orientation) — added
        a +180° rotation, left-leg-only.
      - Right leg needed a top/bottom mirror, not left/right — added real
        `flipY` support to `Render::DrawTexturedQuad` (both PC via
        `SDL_FLIP_VERTICAL` and PS3 via a V-coordinate swap, alongside the
        existing `flipX`), used for the right leg only.
      - Shoulders/arms read as too close against the torso — widened
        `kShoulderOffsetLeft/Right`'s X magnitude (0.26 -> 0.32).
      - Head needed to sit slightly higher — raised `kHeadLocalOffset.y`
        (0.76 -> 0.82).
      - Hands were anchored overlapping the arm's own end instead of
        continuing past it — `Hand.png`'s real pivot is at its own bottom
        edge (wrist), not center, so the previous pull-back-toward-the-arm
        offset had it backwards; now pushes the quad's center outward from
        the arm tip by half the hand's own size instead, matching that
        pivot convention.
      - Bag/belt sat too low — reduced `kBagLocalOffset.y` magnitude
        (-0.35 -> -0.25).
      All verified via rebuild + screenshot at rest and mid-swing; full
      `build.bat` pass green on both targets, root clean.
      **Round 2 (same live comparison, further correction):**
      - Arms were too long/scaling looked wrong after the "re-applied
        derived hand offset" fix (see above) — the *direction* (wide,
        near-horizontal reach) was right, but the *magnitude* overshot.
        Scaled `kHandOffsetLeft/Right` down ~15% (same angle) rather than
        re-deriving from scratch: `(-1.376,1.209)` -> `(-1.17,1.03)`.
      - The "both hands same mirror" fix from round 1 was itself wrong —
        reverted it. Hands are a true asymmetric L/R pair (`Hand.png`
        reads correctly as-authored for one side only); it was feet that
        needed the "both the same" treatment, not hands. Feet now both
        use `!flip`; hands are back to `!flip`/`flip` (left/right).
      All re-verified via rebuild + screenshot. Full `build.bat` pass
      green, root clean.
      **Round 3 (same live comparison):** both hands and both feet were
      facing the wrong way outright after round 2's fixes — hands: the
      `!flip`/`flip` assignment was swapped left<->right (left now
      `flip`, right now `!flip`); feet: both flipped from `!flip` to
      `flip` (keeping both-the-same from round 2, just inverted).
      Re-verified via rebuild + screenshot, full `build.bat` pass green.
      **Round 4 (user sent a screenshot with specifics):**
      - Hands were joining the arm at the thumb instead of the base/
        wrist — they were rotating with `bodyRotZ` alone, ignoring the
        arm's own fixed local angle (the ~55deg baked into the shoulder/
        hand offsets), so the hand's wrist axis didn't align with the
        arm's actual direction. Now uses the same `AngleAlong` convention
        as the arm/shoulder-cap sprites (computed from the real world-
        space arm direction, `worldArmDirL/R`, reused for both the
        rotation and the outward offset).
      - Belt/bag read as too thick/chunky — shrunk `kBagSize` (0.32 ->
        0.24).
      - Feet pointed inward (toward the body's centerline) instead of
        outward — swapped `footOffsetL/R`'s X sign for both sides.
      Re-verified via rebuild + screenshot, full `build.bat` pass green,
      root clean.
      **Round 5 (two more screenshots, "better" but more corrections):**
      - Left shoulder, right shoulder, and right arm had their baked-in
        shading on the wrong side (top instead of bottom) — mirrored all
        three (left shoulder's mirror removed, right shoulder's added,
        right arm's added).
      - Bag/belt size and placement switched to the real Player.prefab-
        derived value instead of continuing to eyeball-adjust: Bag is a
        child of "Shorts" (local (0,-0.9) rel. Body), itself at local
        (0.18,0.289) rel. Shorts, giving (0.108,-0.367) in this project's
        0.6-scaled units — notably NOT centered on the body (previous
        iterations assumed symmetric placement). Size from Bag.png's real
        crop (94x102px @ 256px/unit) scaled the same way: (0.22,0.24).
      - Reviewed leg-into-body and feet-into-leg placement plus shoe size
        against both screenshots — nothing looked clearly broken to me at
        this resolution, so left unchanged rather than guess a change
        with no clear direction. Flagging back to the user rather than
        silently doing nothing.
      Re-verified via rebuild + screenshot, full `build.bat` pass green,
      root clean.
      **Round 6 — a real missing piece, not just a tuning pass.** The
      user asked "are we missing a shorts sprite?" and directly prompted
      me to look at `Shorts.png` itself (not just its `.meta`) — it's the
      waist BELT band (blue overalls above and below, an olive horizontal
      stripe through the middle), a completely separate sprite/layer from
      `Bag.png` (the small pouch) that had never been copied into this
      project or rendered at all. This explains the "belt"/"bag" back-
      and-forth in earlier rounds — every "belt" comment was actually
      landing on the Bag.png pouch rendering, since there was no real
      belt layer to be looking at yet.
      - Added `data/sprites/shorts.png` (copied from the Unity source),
        `sTexShorts`, and a new draw call between Body and Bag, matching
        the real rig hierarchy (Body -> Shorts -> Bag). Real Player.
        prefab data: Shorts is a direct child of Body at local (0,-0.9),
        same 256x512 @ 256px/unit texture as Body.png, so it shares
        `kBodySize`'s exact scale (0.6x1.2) before adjustment.
      - User-corrected: read as too deep (tall) once visible — halved
        `kShortsSize`'s height (0.6x1.2 -> 0.6x0.6), keeping width, to
        read as a thinner belt-line.
      - Bag moved down slightly (`kBagLocalOffset.y`: -0.37 -> -0.42).
      - Shoes read as too small — scaled `kFeetSize` up 1.5x (0.20 ->
        0.30).
      - Hips moved out and up (`kHipOffsetLeft/Right`: (-0.16,-0.5) ->
        (-0.22,-0.42), mirrored for the right side).
      All verified via rebuild + screenshot at rest and mid-swing. Full
      `build.bat` pass green on both targets (including PS3 -- the new
      sprite went through the texture pipeline automatically), root
      clean.
      **Round 7 (one more screenshot, "better"):**
      - Left foot needed mirroring relative to right — feet turned out to
        be a true asymmetric L/R pair after all (like hands), not a
        "both the same" case as round 3 assumed. Left now `!flip`, right
        `flip`.
      - Belt (`Shorts.png`) and bag (`Bag.png`) overlapped enough to read
        as "doubled up" once the belt was actually visible for the first
        time — `kBagLocalOffset` was inside `kShortsSize`'s vertical span
        (belt: y in [-0.84,-0.24] at the time; bag center at -0.42, well
        inside it). Repositioned the bag above the belt's top edge
        instead (`kBagLocalOffset.y`: -0.42 -> -0.30) and shrunk the belt
        further (`kShortsSize`: 0.6x0.6 -> 0.6x0.4) for a cleaner gap
        between the two.
      Verified via rebuild + screenshot at rest and mid-swing. Full
      `build.bat` pass green on both targets, root clean.
      **Round 8 — user is happy with the overall rig now, wants belt/bag/
      shorts polish specifically ("look like the original").** Viewed
      `Bag.png`'s actual pixels for the first time (previously only its
      `.meta`) — a small olive-green satchel with a cream flap and a red
      tag, matching what was already rendering; the art itself wasn't the
      issue. Reconsidered round 6's "separate the bag from the belt"
      fix: in the real rig, Bag is a CHILD of Shorts (a pouch literally
      clipped onto the belt), so some overlap between them is correct,
      not the bug round 6 assumed — reverted `kBagLocalOffset` back to
      the real derived position (0.11,-0.37), keeping the belt's
      shrunk-height approximation from round 6 (our renderer can only
      stretch a whole texture, not crop to just the visible band within
      it, so the shrunk height is a deliberate approximation of "mostly
      just the band," not a precise crop). Verified via rebuild +
      screenshot. Full `build.bat` pass green, root clean.
      **Still flagged, not yet actionable:** "legs are floppy and should
      move with rotation when flying" was reported without enough
      specifics to safely change blind (the leg-angle math already
      includes `bodyRotZ`, so "floppy" likely means something more
      specific than what that suggests) — need a screenshot or more
      detail before touching it.

## Open questions for the user (don't block on these — keep working, just flag)

- **Flip visual read.** Does the hand/foot-mirror-swap approximation for
  `isFlipped` actually look right in motion? Confidence upgraded this
  session: read `PlayerController.cs`'s actual `FlipHands()` (called from
  inside `CanFlip()`, which the flip-input path calls twice per flip event
  — once with the pre-toggle `isFlipped`, once post-toggle via
  `FlipAroundCurrentHandPivot`'s own internal `CanFlip()` call) and it's a
  plain, unconditional `localScale.x` sign-flip on both `handLeft`/
  `handRight` gated only by "not flying and has a pivot" — exactly what
  `app.cpp`'s `isFlipped`-driven mirror-swap already does. This is no
  longer a best-effort guess, it's a confirmed 1:1 match to the source.
  Still needs live play to confirm it *reads* right at a glance (readable
  visual feedback is a UX question the code match can't settle by itself),
  but the underlying logic is now known-correct rather than approximated.
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
- **Rig proportions in motion.** Real progress this session: the user
  shared actual screenshots of the real game running side-by-side with
  this port, both in a similar flying/falling spread pose. Used them to
  re-validate the hand-offset fix above (see "Real Level 1–7..." note).
  Both-hands-attached and mid-swing (not flying) poses still haven't
  been checked against real reference the same way — worth another
  side-by-side pass if more reference screenshots become available.
  Shoulder/Bag/Head/Leg offsets (the ones not re-derived from
  Player.prefab data, see above) are the most likely next candidates if
  anything still reads off in a fresh comparison.
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

1. ~~**Background rendering.**~~ [Done] `cave_bg.png` (originally copied
   whole from `Assets/Art/Background/Stylised/Cave/Cave - BigRocks1.png`)
   tiled behind the level, following the camera, dark-tinted for depth.
   Screenshot-verified — reads clearly as a cave.
   **Follow-up (this session):** the "properly sliced" nice-to-have from
   the original note is done — `Cave - BigRocks1.png.meta` has real
   per-sprite pixel rects for its 10 sub-sprites (Unity texture atlases
   store these directly in the `.meta` YAML, no Editor needed), so
   `cave_bg.png` is now cropped to just sub-sprite `_2`'s rect
   (x=1416,y=466,w=316,h=499, Unity's bottom-left-origin Y converted to
   top-left for the crop) instead of the whole 10-sprite sheet. Reads as
   one coherent rock silhouette repeating, not a jumbled sprite sheet —
   screenshot-verified. Caveat found along the way (spawned a research
   agent to check): this atlas isn't actually Unity's primary level
   background at all — it's used as sparse decorative accent tiles
   (2 placements, Level 1 only) within a much larger multi-atlas Tilemap
   that forms the real wall/background art. Faithfully reproducing *that*
   would need full tile-layout export (same blocker as level/rig data,
   Unity Editor click required) — out of scope for now, documented here
   rather than silently treated as solved. This crop is a better-looking
   placeholder, not the real thing. Now loads as a real GTF on PS3 too
   (texture pipeline below), not a flat tinted quad.
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
   **Clarified this session** (source-comparison pass against
   `LevelEnd.cs`/`SceneController.cs`): the real game doesn't auto-advance
   at all -- `LevelEndTrigger` calls `SceneController.EndRun()`, which only
   stops the timer and saves the PB; advancing to a specific level is a
   *separate*, number-key-triggered `LoadLevel(index)` call that has
   nothing to do with the end trigger. So this auto-advance isn't a
   simplified port of real end-trigger behavior, it's a deliberate stand-in
   for the level-select menu that was never built -- worth knowing if a
   real menu gets added later, since at that point this auto-advance
   should probably go away entirely rather than coexist with it.
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
     `data/sprites/*.png` to `data/gtf/*.gtf` (10 sprites currently); wired
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
  **A shortcut, tried/reverted/re-applied this session:** Unity prefab/
  scene/`.meta` files are plain YAML, so `Assets/Prefab/Player.prefab`'s
  real transform hierarchy (Arm/Leg/Shoulder/Bag/Head local positions and
  rotations, `Arm.png`/`Leg.png`/etc.'s real pivot/pixel-per-unit import
  settings) is readable straight from disk without the Editor — a research
  agent extracted the full hierarchy (see git log for the exact numbers).
  Composed the hand-grip offset from it (Arm's local pos+rotation, plus
  Hand/HandGrip's further local offset, scaled by this project's Unity-
  relative unit factor of 0.6), got `(-1.376, 1.209)`, and *initially*
  reverted it as a regression — but that judgment was only against an
  earlier, never-independently-verified guess, not real footage. Once the
  user shared actual side-by-side screenshots of the real game (both arms
  reaching in a wide, nearly-straight diagonal spanning past both
  shoulders), the derived value's wider/more-horizontal reach matched
  visibly better than the old guess's narrower/more-vertical one — see
  `app.cpp`'s `kHandOffsetLeft` comment. Re-applied, screenshot-verified
  at rest and mid-swing, both look right. Lesson: a "regression" judged
  only against a prior unvalidated guess isn't real evidence — real
  reference footage is what actually settles it.
  Still didn't attempt Shoulder/Bag/Head/Leg from this data — those
  sprites have non-center pivots (e.g. Shoulder/Bag pivot at their own
  bottom-left, not center), so their GameObject position isn't directly
  comparable to this project's center-anchored `DrawTexturedQuad`
  convention without ALSO knowing each sprite's real visible-content
  bounds (pixel inspection, not just the `.meta`). Worth revisiting now
  that real reference screenshots exist to check against — see "Open
  questions" below.
- **On-hardware testing.** The DECHJ00A is powered off. `Build\PS3\` is
  staged and ready (see `PS3_DEPLOY_README.txt`) for whenever it's back on
  — either via Target Manager (target "PS3 Test", 10.1.1.2), copying the
  folder over directly, or installing the `.pkg` (untested either way).
- **Package branding** (icon art, title metadata) — see "Open questions."

## Backlog / explicitly out of scope

- Online leaderboard (PlayFab) — dropped per project scope, local-only scoring.
- Anything requiring the official Sony NP network services.
