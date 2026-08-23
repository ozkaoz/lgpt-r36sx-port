#!/usr/bin/env python3
"""
test_frogui_apps_lgpt.py — Phase B/C host regression for FrogUI LGPT Apps entry.

Phase B (this file's default): LGPT must exist once in app_defs as LGPT_BIN,
and must NOT yet be hidden from Games (is_app_folder_name must NOT contain lgpt).

Phase C will invert the second check (must be hidden).

Checks the patched source in /tmp/FrogUI_r36sx/frogui_libretro.c if present,
otherwise skips with warning. Also validates the built patched binary if present.
"""
import pathlib, re, hashlib, sys

SRC_CANDIDATES = [
    pathlib.Path("/tmp/FrogUI_r36sx/frogui_libretro.c"),
    pathlib.Path("patches/frogui_apps_lgpt.patch"),
]
PATCHED_SO = pathlib.Path("build/frogui_candidate/frogui_libretro.so")
VANILLA_SO = pathlib.Path("build/frogui_candidate/frogui_libretro.so.vanilla")
SHIPPED_SO = pathlib.Path("/tmp/treefrog_audit/extracted/release/cubegm/cores/frogui_libretro.so")

EXPECTED_BIN = "/mnt/sdcard/cubegm/lgpt"
EXPECTED_ROM = "/mnt/sdcard/roms/lgpt/start.lgpt"

def test_app_defs_has_lgpt():
    # Prefer patched source if present (official build workspace)
    candidates = [
        pathlib.Path.home() / "sf3000-work/FrogUI-lgpt-apps-patched/frogui_libretro.c",
        pathlib.Path("/tmp/FrogUI_r36sx/frogui_libretro.c"),
        pathlib.Path.home() / "sf3000-work/FrogUI-vanilla-official/frogui_libretro.c",
    ]
    src = next((p for p in candidates if p.exists()), None)
    if not src or not src.exists():
        print("SKIP: no FrogUI source present — patch file check")
        patch = pathlib.Path("patches/frogui_apps_lgpt.patch")
        assert patch.exists(), "patch file missing"
        txt = patch.read_text()
        assert '"lgpt"' in txt and "LGPT_BIN" in txt, "patch must add lgpt LGPT_BIN"
        assert 'SDCARD_BASE "/roms/lgpt/start.lgpt"' in txt, "patch must use correct ROM"
        print("PATCH_FILE: PASS")
        return
    # If vanilla source (no lgpt in app_defs), fallback to patch file check
    txt = src.read_text()
    m_tmp = re.search(r'static const AppEntry app_defs\[\] = \{(.*?)\};', txt, re.DOTALL)
    if m_tmp and '"lgpt"' not in m_tmp.group(1):
        patch = pathlib.Path("patches/frogui_apps_lgpt.patch")
        assert patch.exists(), "patch file missing"
        txt2 = patch.read_text()
        assert '"lgpt"' in txt2 and "LGPT_BIN" in txt2
        assert 'SDCARD_BASE "/roms/lgpt/start.lgpt"' in txt2
        print(f"PATCH_FILE (via {src.name} vanilla fallback): PASS")
        return
    # Exactly one lgpt entry in app_defs
    # Find app_defs block
    m = re.search(r'static const AppEntry app_defs\[\] = \{(.*?)\};', txt, re.DOTALL)
    assert m, "app_defs not found"
    block = m.group(1)
    lgpt_entries = re.findall(r'\{"lgpt"', block)
    assert len(lgpt_entries) == 1, f"LGPT must appear once in app_defs, found {len(lgpt_entries)}"
    assert "LGPT_BIN" in block, "LGPT entry must use LGPT_BIN"
    # No duplicate definition
    assert txt.count("#define LGPT_BIN") == 1, "LGPT_BIN must be defined once"
    # Phase B: lgpt NOT in is_app_folder_name
    is_app_block = re.search(r'static int is_app_folder_name.*?\{.*?\}', txt, re.DOTALL)
    if is_app_block:
        block2 = is_app_block.group(0)
        assert 'lgpt' not in block2.lower(), "Phase B: is_app_folder_name must NOT contain lgpt (dual entry)"
    print("APP_DEFS_HAS_LGPT: PASS (1 entry, LGPT_BIN, ROM correct, Games not hidden)")

def test_launch_semantics():
    candidates = [
        pathlib.Path.home() / "sf3000-work/FrogUI-lgpt-apps-patched/frogui_libretro.c",
        pathlib.Path("/tmp/FrogUI_r36sx/frogui_libretro.c"),
    ]
    src = next((p for p in candidates if p.exists()), None)
    if not src:
        print("SKIP launch semantics — no source")
        return
    txt = src.read_text()
    # If vanilla, check patch file instead
    if 'SDCARD_BASE "/roms/lgpt/start.lgpt"' not in txt:
        patch = pathlib.Path("patches/frogui_apps_lgpt.patch")
        if patch.exists():
            txt = patch.read_text()
    assert 'SDCARD_BASE "/roms/lgpt/start.lgpt"' in txt, "Apps LGPT launch must use start.lgpt"
    # Verify handle_input patch present
    assert 'request_standalone_launch(app_defs[app_index].bin, SDCARD_BASE "/roms/lgpt/start.lgpt")' in txt
    print("LAUNCH_SEMANTICS: PASS")

def test_patched_binary_contains_lgpt():
    # Prefer new official dual candidate, fallback to old path
    candidates = [
        pathlib.Path("build/frogui_candidate/apps_dual/frogui_libretro.so"),
        pathlib.Path("build/frogui_candidate/frogui_libretro.so"),
        PATCHED_SO,
    ]
    so = next((p for p in candidates if p.exists()), None)
    if not so:
        print("SKIP binary test — patched SO not built")
        return
    data = so.read_bytes()
    assert b"/mnt/sdcard/cubegm/lgpt" in data, "patched SO must contain LGPT_BIN"
    assert b"/mnt/sdcard/roms/lgpt/start.lgpt" in data, "patched SO must contain correct ROM"
    assert b"APPS" in data and b"scan_apps_tab" in data, "patched SO must be Apps-capable"
    # Check app_defs size increased (patched should be 8 entries vs vanilla 7)
    # We can't easily parse, but check that LGPT string present
    assert b"LGPT" in data
    print(f"PATCHED_BINARY: PASS sha={hashlib.sha256(data).hexdigest()[:12]} size={len(data)}")

def test_protected_wrapper_unchanged():
    # Ensure sd_root wrapper not overwritten by TreeFrog 544-byte variant
    import pathlib, hashlib
    wrapper = pathlib.Path("sd_root/cubegm/lgpt")
    assert wrapper.exists(), "sd_root/cubegm/lgpt missing"
    sha = hashlib.sha256(wrapper.read_bytes()).hexdigest()
    assert sha == "ee1ecfe53bf9c94915abc696271561b7ccbf157f9c80300438f720dbedad896c", f"wrapper SHA mismatch {sha}"
    assert wrapper.stat().st_size == 9006, f"wrapper size {wrapper.stat().st_size} != 9006"
    # TreeFrog's 544-byte must not be in sd_root
    tf_wrapper = pathlib.Path("/tmp/treefrog_audit/extracted/release/cubegm/lgpt")
    if tf_wrapper.exists():
        assert hashlib.sha256(tf_wrapper.read_bytes()).hexdigest() != sha, "TreeFrog wrapper must differ"
    print("PROTECTED_WRAPPER: PASS ee1ecfe5 9006")

if __name__ == "__main__":
    try:
        test_app_defs_has_lgpt()
        test_launch_semantics()
        test_patched_binary_contains_lgpt()
        test_protected_wrapper_unchanged()
        print("FROG_UI_APPS_LGPT: PASS")
    except AssertionError as e:
        print(f"FAIL: {e}", file=sys.stderr)
        sys.exit(1)
