# CHOPPER_RENDER_DIFF_BACON14_9A326FE.md

| COMPONENT | BACON 1.4 (d2a897a / 48318a0) | 9a326fe (core 91f2a457) | NEW FIX (core d86c0272) | RATIONALE |
|---|---|---|---|---|
| View::ClearRect | `GUIRect rect(x,y,x+w,y+h)` no clipping | Hardened `if(w<=0) return; long long x1=x+w clamp 0..40/0..30` correct | Same hardened (preserved) | Memory safety + correct x,y,w,h |
| AppWindow::ClearRect | direct `_charScreen+x+40*y` no clipping | Hardened same long long clamp | Same hardened | Last barrier |
| Chopper outer border | `ChopperView::DrawFrame` invert at 1,22,0,39 blue solid border | Same (border present) | Removed for TreeFrog: `drawFrame` early return `#if PLATFORM_TREEFROG return;` | Task 22: eliminate azul exterior, keep waveform border `TF_WAVE_X` |
| Pitch panel geometry | `tf_rect(0,60,320,116)` y=60..175 h116 frames 59/176 + char `MakeCenteredMenuLayout 7,11` | Same geometry 60,116 but **only char** `DrawString` + `tf_rect` before Flush (no direct post-flush) → cache prevents raster → blank on hardware | **Direct limited** `tf_rect(0,60,320,116)` + frames 59/176 in `TreeFrogChopperOverlayDraw` post-flush + `tf_text` for title/header/6 values/hints at 64,80,96+ i*8,152,160 etc; `publishOverlayState` publishes `g_chopperPitch*` | 9a326fe removed direct renderer that was added to bypass char cache; restoring limited direct solves blank while not fullscreen |
| Pitch text renderer | Char `DrawString` (Flush raster) | Char only (removed direct) → FAIL blank | Direct `tf_text` post-flush limited 60..176; char still present for non-TreeFrog `#else` | Direct post-flush bypasses `_charScreen/_preScreen` cache, reliable on R36SX stopped playback |
| Operation panel geometry | `tf_rect(0,64,320,112)` y=64..176 h112 + `ClearRect(0,8,40,14)` + char `OPERATION` | Same but char only → progress not visible on hardware (same cache) | **Direct limited** `tf_rect(0,64,320,112)` + frames 63/176 + `tf_text` for `OPERATION`/Status/percent/hints; `g_chopperOperationMessage/Combo` published via `showOperationProgress` | Operation also needs post-flush direct to be visible during DSP; limited not fullscreen |
| Operation progress | `showOperationProgress: operationActive, clamp, ComposeOperationStatus, setStatus, DrawView, publishOverlayState, w_.Flush, ForceRefresh, Sleep(90) if <100, isDirty` | Same preserved | Same + also `snprintf(g_chopperOperationMessage/Combo)` for direct | Preserves golden pacing Sleep(90) |
| Pipeline order | `AppWindow::Flush: raster char → TreeFrogChopperOverlayDraw (waveform) → PostFlushDraw → GUIWindow::Flush` | Same | Same but `TreeFrogChopperOverlayDraw` now draws `operation → pitch → waveform` in that order post-char | Ensures direct panels sit on top of raster, before present |
| STATE vs FRAMEBUFFER | golden STATE correct, FRAMEBUFFER via direct fullscreen 320x240 (tapaba status) | STATE correct, FRAMEBUFFER empty (char cache) | STATE correct, FRAMEBUFFER via limited direct 60,116/64,112 → visible | Fixes STATE correct but FRAMEBUFFER empty |
| Cache | N/A (direct fullscreen bypassed cache) | Char cache `if(*cur!=*prev||*prop!=*prevProp)` prevents re-raster after `tf_rect` direct clear before Flush | Direct post-flush does not rely on cache; draws every `Flush` regardless | Confirms hypothesis task 7/8 |
| Flush | `w_.Flush` + `ForceRefresh` in `showOperationProgress` | Same | Same + direct overlay in `TreeFrogChopperOverlayDraw` already post-flush | Flush alone not sufficient; need post-flush direct |
| Outer border generation | `ChopperView::DrawFrame` invert blue | Same | Removed via `drawFrame` guard | No overdraw, just remove draw call |

## Root Cause (36)

- Did 9a326fe remove direct Pitch renderer? **YES** (`g_chopperPitchActive`, `tf_text`, pitch fullscreen block in `TreeFrogChopperOverlayDraw` + publish)
- Was direct renderer originally added because char-screen refresh was unreliable? **YES** (comment U2.52: con proyecto detenido el char screen no se refresca de forma fiable, por eso se pintaba en `TreeFrogChopperOverlayDraw` cada frame)
- Does char cache prevent re-rasterization after direct framebuffer clear? **YES** (`AppWindow::Flush` only rasterizes if `*current != *previous || prop` ; after `tf_rect` clear before Flush, if char cells unchanged, they are not re-rasterized → panel queda vacío)
- Is Pitch/Env logical state correct while framebuffer is empty? **YES** (`pitchMode`, `operationActive`, DSP, `publishOverlayState` STATE correcto, FRAMEBUFFER vacío)
- Was ClearRect the remaining primary bug? **NO** (View/AppWindow hardening already correct x,y,w,h 40x14=560; AppWindow overflow fixed)
- Was missing Flush the remaining primary bug? **NO** (`showOperationProgress` already does `w_.Flush + ForceRefresh + Sleep(90)`; pitch entry does `publishOverlayState` but Flush is in `AppWindow::onUpdate` Redraw→Flush, still cache prevents)
- What exact renderer will now own Pitch/Env on TreeFrog? **TreeFrogChopperOverlayDraw direct limited** `tf_rect(0,60,320,116)` + `tf_text` post-char-flush, state `g_chopperPitch*` published via `publishOverlayState` (`!suspended && !operationActive && pitchMode`)
- What exact renderer will own operation progress? **Same direct** `tf_rect(0,64,320,112)` + `tf_text` for `OPERATION`/Status/percent/hints, state `g_chopperOperation*` published via `showOperationProgress/clearOperationProgress`
- What generates the outer blue border? **SampleChopperModal::drawFrame → ChopperView::DrawFrame** `invert 1,22,0,39 CHOP_COLOR_BORDER` (char grid)
- How was that border removed? **Guard** `#if defined(PLATFORM_TREEFROG) return; #else DrawFrame #endif` in `drawFrame`, no overdraw, waveform border `TF_WAVE_X` preserved

## Build
- Old core 91f2a4570747a2cc 1.4M 2026-08-21 21:23 (9a326fe)
- New core d86c0272ce7f230c 1.4M 2026-08-21 22:05
- Host tests: clearrect_semantics PASS, bacon14_contract PASS, chopper_char_cache_invalidation PASS, no_outer_blue_border PASS, pitch_entry_render PASS, operation_render PASS, chopper_draw/view PASS, host_syntax PASS, diff --check PASS
- SD: G:\cubegm\cores\lgpt_r36sx_port_libretro.so d86c0272 MATCH YES

## Visuals
- Pitch only limited 60..175 (60,116) not fullscreen 0,0,320,240; status/hints 23-25 intact
- Operation only 64..176 (64,112) not fullscreen
- No outer blue square, waveform border kept
