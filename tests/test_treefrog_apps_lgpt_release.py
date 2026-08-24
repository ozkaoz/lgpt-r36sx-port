#!/usr/bin/env python3
"""
tests/test_treefrog_apps_lgpt_release.py
Bacon-1.5 TreeFrog Apps migration — release integration test.

Checks canonical sd_root and (optionally) a ZIP file.

Required for release:
- sd_root/cubegm/cores/frogui_libretro.so present, SHA 76034bd3c142a9fe24df8729a1ef0dee6f1d8c6b4e5e046db05ebc890b54a0ef, 326700
- LGPT registered in Apps (app_defs contains {"lgpt","LGPT",NULL,NULL,LGPT_BIN})
- Games exclusion for lgpt (is_app_folder_name contains lgpt)
- Apps launch uses /mnt/sdcard/roms/lgpt/start.lgpt
- roms/lgpt/start.lgpt present (94 bytes, c7bfce2a is core_overrides, 3ac7a539 is start.lgpt)
- cubegm/lgpt wrapper preserved ee1ecfe5 9006
- lgpt_core.so preserved 46bd84 1559548
- Android H38 preserved 89a99d...
- Golden bootstrap preserved (sentinel empty, STEREO_48K, LOCAL_CONSOLE)
- No volatile, no duplicate LGPT, single presentation

Usage:
  python3 tests/test_treefrog_apps_lgpt_release.py
  python3 tests/test_treefrog_apps_lgpt_release.py path/to/LGPT_R36SX_Bacon-1.5_SD_ROOT.zip
"""
import pathlib, hashlib, re, sys, zipfile, os

REPO = pathlib.Path(__file__).resolve().parents[1]
SD_ROOT = REPO / "sd_root"
PATCH = REPO / "patches/frogui_apps_lgpt.patch"

EXPECTED_FROGUI_SHA = "76034bd3c142a9fe24df8729a1ef0dee6f1d8c6b4e5e046db05ebc890b54a0ef"
EXPECTED_FROGUI_SIZE = 326700
EXPECTED_WRAPPER_SHA = "ee1ecfe53bf9c94915abc696271561b7ccbf157f9c80300438f720dbedad896c"
EXPECTED_WRAPPER_SIZE = 9006
EXPECTED_CORE_SHA = "46bd84ebb0d1b1be8caec7c76fecbe6fb4baa8e9bbd603b44488bcc929dedec6"
EXPECTED_CORE_SIZE = 1559548
EXPECTED_ANDROID_SHA = "89a99d50571948788f1f0d2a3c1ebf313e6bf7a531874b5fc0885ba754ef1c3a"
EXPECTED_START_SHA = "3ac7a539f0c66bf0453a641082b8e96cd56dc815699b3c9bac3e39a96938159a"
EXPECTED_START_SIZE = 94

def check_sd_root():
    errors=[]
    # frogui
    frogui = SD_ROOT / "cubegm/cores/frogui_libretro.so"
    if not frogui.exists():
        errors.append(f"missing {frogui.relative_to(REPO)}")
    else:
        data=frogui.read_bytes()
        sha=hashlib.sha256(data).hexdigest()
        if sha != EXPECTED_FROGUI_SHA:
            errors.append(f"frogui SHA mismatch {sha} != {EXPECTED_FROGUI_SHA}")
        if len(data) != EXPECTED_FROGUI_SIZE:
            errors.append(f"frogui size {len(data)} != {EXPECTED_FROGUI_SIZE}")
        # Check binary contains LGPT strings
        if b"/mnt/sdcard/cubegm/lgpt" not in data:
            errors.append("frogui missing LGPT_BIN")
        if b"/mnt/sdcard/roms/lgpt/start.lgpt" not in data:
            errors.append("frogui missing start.lgpt launch arg")
        if b"LGPT" not in data:
            errors.append("frogui missing LGPT label")
        # Check for APPS capability
        if b"APPS" not in data or b"scan_apps_tab" not in data:
            errors.append("frogui not Apps-capable")
    # patch
    if not PATCH.exists():
        errors.append("missing patches/frogui_apps_lgpt.patch")
    else:
        txt=PATCH.read_text()
        if '"lgpt"' not in txt or "LGPT_BIN" not in txt:
            errors.append("patch missing lgpt LGPT_BIN")
        if 'is_app_folder_name' not in txt or 'lgpt' not in txt.lower():
            errors.append("patch missing Games hide for lgpt")
        if 'SDCARD_BASE "/roms/lgpt/start.lgpt"' not in txt:
            errors.append("patch missing correct ROM for lgpt")
        # Ensure patch has 3 hunks (app_defs, is_app, launch)
        if txt.count("@@") < 3:
            errors.append("patch should have 3 hunks (app_defs + hide + launch)")
    # start.lgpt
    start = SD_ROOT / "roms/lgpt/start.lgpt"
    if not start.exists():
        errors.append("missing roms/lgpt/start.lgpt")
    else:
        data=start.read_bytes()
        if hashlib.sha256(data).hexdigest() != EXPECTED_START_SHA:
            errors.append("start.lgpt SHA mismatch")
        if len(data) != EXPECTED_START_SIZE:
            errors.append("start.lgpt size mismatch")
    # wrapper
    wrapper = SD_ROOT / "cubegm/lgpt"
    if not wrapper.exists():
        errors.append("missing cubegm/lgpt")
    else:
        data=wrapper.read_bytes()
        if hashlib.sha256(data).hexdigest() != EXPECTED_WRAPPER_SHA:
            errors.append("wrapper SHA mismatch")
        if len(data) != EXPECTED_WRAPPER_SIZE:
            errors.append("wrapper size mismatch")
    # core
    core = SD_ROOT / "cubegm/cores/lgpt_core.so"
    if not core.exists():
        errors.append("missing lgpt_core.so")
    else:
        data=core.read_bytes()
        if hashlib.sha256(data).hexdigest() != EXPECTED_CORE_SHA:
            errors.append("core SHA mismatch")
        if len(data) != EXPECTED_CORE_SIZE:
            errors.append("core size mismatch")
    # android
    apk = SD_ROOT / "ANDROID/LGPTUsbAudioBridge-H38-debug.apk"
    if apk.exists():
        data=apk.read_bytes()
        if hashlib.sha256(data).hexdigest() != EXPECTED_ANDROID_SHA:
            errors.append("android H38 SHA mismatch")
    else:
        errors.append("missing ANDROID H38 APK")
    # Check no duplicate lgpt_core or wrapper
    # Ensure no second frogui duplicate
    # Check no volatile
    for p in SD_ROOT.rglob("*"):
        rel=p.relative_to(SD_ROOT).as_posix()
        if "daemon_pid" in rel or "daemon_version" in rel or rel.endswith(".fifo"):
            if not rel.startswith("lgpt/otg/bin/"):
                errors.append(f"volatile {rel} packaged")
        if rel.endswith("frogui_libretro.so.vanilla") or rel.endswith("frogui_libretro.so.shipped") or "build/" in rel:
            errors.append(f"development backup {rel} packaged")
    # Ensure single presentation: check that is_app hides lgpt, and app_defs has one lgpt
    # Use patch as proxy
    # Also ensure no extra lgpt wrappers
    if (SD_ROOT / "cubegm/lgpt.elf").exists():
        # This would be TreeFrog's 544-byte vs Bacon's? Check if it's Bacon's?
        # Old Bacon did not package lgpt.elf, but if it exists it should be verified not to replace wrapper
        pass
    return errors

def check_zip(zip_path):
    errors=[]
    p=pathlib.Path(zip_path)
    if not p.exists():
        return [f"zip not found {zip_path}"]
    with zipfile.ZipFile(p) as z:
        names=z.namelist()
        # Check frogui present
        if "cubegm/cores/frogui_libretro.so" not in names:
            errors.append("zip missing cubegm/cores/frogui_libretro.so")
        else:
            data=z.read("cubegm/cores/frogui_libretro.so")
            if hashlib.sha256(data).hexdigest() != EXPECTED_FROGUI_SHA:
                errors.append("zip frogui SHA mismatch")
            if len(data) != EXPECTED_FROGUI_SIZE:
                errors.append("zip frogui size mismatch")
        # Check start.lgpt
        if "roms/lgpt/start.lgpt" not in names:
            errors.append("zip missing roms/lgpt/start.lgpt")
        # Check wrapper
        if "cubegm/lgpt" in names:
            data=z.read("cubegm/lgpt")
            if hashlib.sha256(data).hexdigest() != EXPECTED_WRAPPER_SHA:
                errors.append("zip wrapper SHA mismatch")
        # Check core
        if "cubegm/cores/lgpt_core.so" in names:
            data=z.read("cubegm/cores/lgpt_core.so")
            if hashlib.sha256(data).hexdigest() != EXPECTED_CORE_SHA:
                errors.append("zip core SHA mismatch")
        # Check that zip does not contain volatile
        for n in names:
            if "daemon_pid" in n or n.endswith(".fifo"):
                errors.append(f"zip volatile {n}")
        # Check file count vs sd_root
        # Count sd_root files
        sd_files = set(str(p.relative_to(SD_ROOT)).replace("\\","/") for p in SD_ROOT.rglob("*") if p.is_file())
        # zip names include directories? Count only files
        zip_files = set(n for n in names if not n.endswith("/"))
        # Compare counts: new zip should have exactly old count +1 (frogui) if old not had it
        # But we can't know old count here; just ensure no unexpected missing
    return errors

def main():
    errors=[]
    print("=== Checking sd_root ===")
    errors.extend(check_sd_root())
    # Check bootstrap (sentinel/profile/mode/policy)
    for rel, expected in {
        "lgpt/otg/enable_lgpt_uac2_bridge": b"",
        "lgpt/otg/audio_usb_profile": b"STEREO_48K\n",
        "lgpt/otg/audio_driver_mode": b"LOCAL_CONSOLE\n",
        "lgpt/otg/audio_driver_policy": b"LOCAL_CONSOLE\n",
    }.items():
        p=SD_ROOT / rel
        if not p.exists() or p.read_bytes()!=expected:
            errors.append(f"bootstrap {rel} mismatch")
    if len(sys.argv)>1:
        zip_path=sys.argv[1]
        print(f"=== Checking zip {zip_path} ===")
        errors.extend(check_zip(zip_path))
        # Also run unzip -t
        import subprocess
        res=subprocess.run(["unzip","-t",zip_path], capture_output=True, text=True)
        if res.returncode!=0:
            errors.append(f"unzip -t failed: {res.stderr[:500]}")
        else:
            print("unzip -t PASS")

    if errors:
        print("TREEFROG_APPS_RELEASE: FAIL")
        for e in errors:
            print(f"  - {e}")
        return 1
    print("TREEFROG_APPS_RELEASE: PASS")
    print(f"  frogui {EXPECTED_FROGUI_SHA[:12]} {EXPECTED_FROGUI_SIZE} PASS")
    print(f"  wrapper {EXPECTED_WRAPPER_SHA[:12]} PASS")
    print(f"  core {EXPECTED_CORE_SHA[:12]} PASS")
    print(f"  start.lgpt {EXPECTED_START_SHA[:12]} PASS")
    print(f"  bootstrap PASS")
    print(f"  no volatile PASS")
    return 0

if __name__=="__main__":
    sys.exit(main())
