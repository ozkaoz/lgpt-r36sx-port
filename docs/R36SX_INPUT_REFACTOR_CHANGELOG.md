
## Step 01 - X/Y symbols completed after partial patch

Scope:
- sources/UIFramework/BasicDatas/GUIEvent.h
- sources/UIFramework/BasicDatas/GUIEvent.cpp
- sources/Application/Views/BaseClasses/View.h
- sources/Adapters/TREEFROG/GUI/TreeFrogEventManager.cpp
- sources/Adapters/TREEFROG/Main/TreeFrogLibretro.cpp

Intent:
- Add EPBT_X / EPBT_Y.
- Add EPBM_X / EPBM_Y.
- Prepare future dedicated X/Y actions.
- Keep current behavior unchanged:
  X duplicates A.
  Y duplicates B.

No user-visible behavior change expected.

## Step 02 - Activate dedicated Y input

Scope:
- sources/Adapters/TREEFROG/Main/TreeFrogLibretro.cpp

Intent:
- Stop mapping physical Y to LGPT_B.
- Map physical Y to EPBT_Y.
- Keep X as A duplicate for now.
- Reserve Y for a future Project Tools menu.

Expected behavior:
- A unchanged.
- B unchanged.
- X still duplicates A.
- Y no longer duplicates B.
- No project/filesystem feature added yet.

## Step 03 - Activate reserved L2/R2 inputs, keep Select inactive

Scope:
- sources/UIFramework/BasicDatas/GUIEvent.h
- sources/UIFramework/BasicDatas/GUIEvent.cpp
- sources/Application/Views/BaseClasses/View.h
- sources/Adapters/TREEFROG/GUI/TreeFrogEventManager.cpp
- sources/Adapters/TREEFROG/Main/TreeFrogLibretro.cpp

Intent:
- Add EPBT_L2 / EPBT_R2.
- Add EPBM_L2 / EPBM_R2.
- Stop mapping physical L2/R2 as duplicates of L/R when those mappings exist.
- Keep Select implemented but inactive through TREEFROG_ENABLE_SELECT=0.
- No feature assigned to L2/R2/Y yet.

Expected behavior:
- L1/R1 keep existing behavior.
- L2/R2 should not trigger visible actions yet.
- Y remains dedicated and inactive.
- Select remains inactive.
- X still duplicates A.

## Step 04 - Activate dedicated X input

Scope:
- sources/Adapters/TREEFROG/Main/TreeFrogLibretro.cpp

Intent:
- Stop mapping physical X to LGPT_A.
- Map physical X to EPBT_X.
- Keep X reserved/inactive for future actions.

Expected behavior:
- A unchanged.
- B unchanged.
- X no longer duplicates A.
- Y remains dedicated/inactive.
- L2/R2 remain dedicated/inactive.
- Select remains inactive.

## Step 05 - Song A+B clear cell / Y+X cut compact

Scope:
- sources/Application/Views/SongView.h
- sources/Application/Views/SongView.cpp

Intent:
- Change Song normal mode only:
  A+B clears the current Song cell and leaves --.
  Y+X performs the previous cut/compact behavior.
- Preserve Chain/Phrase/Table behavior unchanged.
- Preserve Song selection mode unchanged for now.

Expected behavior:
- Song normal mode:
  B then A over a chain cell -> cell becomes --, rows do not move.
  Y+X over a chain cell -> old cut/compact behavior, rows move up.
- START, Listen, Import, Exit unchanged.
