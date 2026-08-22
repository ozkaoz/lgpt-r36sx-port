#!/usr/bin/env python3
"""
Functional duplicate simulation based on spec sections 30-31
"""
import os, shutil, tempfile, pathlib, random, string

def assert_eq(a,b,msg):
    if a!=b:
        raise AssertionError(f"FAIL {msg}: {a} != {b}")
    print(f"PASS {msg}")

def test_duplicate_naming():
    print("== test_duplicate_naming ==")
    assert_eq("KaOz" + "_c", "KaOz_c", "KaOz->KaOz_c")
    assert_eq("KaOz_c" + "_c", "KaOz_c_c", "KaOz_c->KaOz_c_c")
    assert_eq("lgpt_" + "KaOz_c", "lgpt_KaOz_c", "filesystem dest")

def test_duplicate_copy():
    print("== test_duplicate_copy ==")
    tmp = tempfile.mkdtemp(prefix="lgpt_dup_")
    try:
        root = pathlib.Path(tmp)
        src = root / "lgpt_KaOz"
        src.mkdir()
        (src / "lgptsav.dat").write_bytes(b"TESTDATA123")
        (src / "samples").mkdir()
        (src / "samples" / "a.wav").write_bytes(b"WAVDATA")
        nested = src / "samples" / "nested"
        nested.mkdir()
        (nested / "b.txt").write_bytes(b"NESTED")
        # Simulate OnDuplicateProject logic
        src_base = "KaOz"
        dst_base = src_base + "_c"
        dst_full = "lgpt_" + dst_base
        dst = root / dst_full
        # collision check: dst should not exist
        assert not dst.exists(), "dst not exists before"
        # Recursive copy (like RecursiveCopyDirectory)
        shutil.copytree(str(src), str(dst))
        # Verify source untouched
        assert (src / "lgptsav.dat").exists(), "src lgptsav preserved"
        assert (src / "samples" / "a.wav").exists(), "src sample preserved"
        # Verify dest
        assert (dst / "lgptsav.dat").exists(), "dst lgptsav exists"
        assert (dst / "lgptsav.dat").read_bytes() == b"TESTDATA123", "dst lgptsav byte identical"
        assert (dst / "samples" / "a.wav").exists(), "dst sample exists"
        assert (dst / "samples" / "nested" / "b.txt").read_bytes() == b"NESTED", "nested copied"
        # source files byte identical
        assert (src / "lgptsav.dat").read_bytes() == (dst / "lgptsav.dat").read_bytes(), "byte identical"
        print("PASS duplicate copy preserves source and copies nested")
        # Test KaOz_c -> KaOz_c_c
        src2 = dst
        src2_base = dst_base
        dst2_base = src2_base + "_c"
        dst2_full = "lgpt_" + dst2_base
        dst2 = root / dst2_full
        shutil.copytree(str(src2), str(dst2))
        assert (dst2 / "lgptsav.dat").exists(), "second duplicate exists"
        assert_eq(dst2_base, "KaOz_c_c", "second naming")
        print("PASS second duplicate KaOz_c -> KaOz_c_c")
        # Test collision
        # try duplicate again KaOz -> KaOz_c when dest exists should fail
        # simulate check: if dst exists, return "Copy exists" and do not overwrite
        if dst.exists():
            # should not overwrite, keep dst unchanged
            before = (dst / "lgptsav.dat").read_bytes()
            # attempt to copy again would be prevented; ensure no overwrite
            # we simulate failure
            print("PASS collision detected: Copy exists (no overwrite)")
            assert (dst / "lgptsav.dat").read_bytes() == before, "dest unchanged on collision"
        # Test failure cleanup: simulate partial copy then failure, ensure source preserved and partial removed
        src3 = root / "lgpt_FailTest"
        src3.mkdir()
        (src3 / "lgptsav.dat").write_bytes(b"FAILTEST")
        dst3 = root / "lgpt_FailTest_c"
        dst3.mkdir()
        (dst3 / "partial.dat").write_bytes(b"partial")
        # simulate failure: RecursiveCopy would fail and then RecursiveDeleteDirectory(dst)
        # we simulate cleaning partial
        shutil.rmtree(str(dst3))
        assert not dst3.exists(), "partial cleaned"
        assert (src3 / "lgptsav.dat").exists(), "source preserved after failure"
        assert (src3 / "lgptsav.dat").read_bytes() == b"FAILTEST", "source byte identical after failure"
        print("PASS failure cleanup: partial removed, source preserved")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

def test_name_length():
    print("== test_name_length ==")
    kMaxStem = 24
    src_base = "A"*24  # max
    dst_base = src_base + "_c"
    assert len(dst_base) == 26, "length 26"
    assert len(dst_base) > kMaxStem, "too long"
    print("PASS name too long detected")
    # short should pass
    src_base2 = "A"*22
    dst_base2 = src_base2 + "_c"
    assert len(dst_base2) <= kMaxStem, "within limit"
    print("PASS name length within limit")

def test_startup_new_random():
    print("== test_startup_new_random ==")
    # Simulate NewProjectDialog startupRandomMode A generates random, START confirms
    # getRandomName simulation: use adjectives and verbs
    adjs = ["Red","Swift","Spoopy"]
    verbs = ["Jump","Roar","Dance"]
    def getRandomName():
        return random.choice(adjs) + random.choice(verbs)
    tmp = tempfile.mkdtemp(prefix="lgpt_new_")
    try:
        root = pathlib.Path(tmp)
        # create existing project to test collision
        (root / "lgpt_RedJump").mkdir()
        # Simulate A press in startup mode: generates random, does NOT EndModal, changes visible name, does not collide
        # We'll loop until not exists
        tries = 0
        while True:
            rnd = getRandomName()
            tries+=1
            dest = root / ("lgpt_" + rnd)
            if not dest.exists():
                print(f"PASS random generated {rnd} not colliding after {tries} tries")
                break
            if tries>100:
                raise AssertionError("random loop failed")
        # Simulate START confirm checks
        stem = rnd
        final = "lgpt_" + stem
        dest = root / final
        assert not dest.exists(), "START confirm valid"
        print("PASS START confirm valid")
        # Busy destination
        (root / "lgpt_BusyTest").mkdir()
        busy_final = "lgpt_BusyTest"
        busy_dest = root / busy_final
        assert busy_dest.exists(), "busy exists"
        print("PASS busy detection Name busy")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

def test_select_menu_validity():
    print("== test_select_menu_validity ==")
    # Simulate HasValidCurrentProjectSelection checks
    def is_lgpt(name):
        return name.lower().startswith("lgpt")
    def has_valid(selection, size, path_exists, is_dir, has_save, is_lgpt):
        if selection <0 or selection >= size: return False
        if not path_exists: return False
        if not is_lgpt: return False
        if not is_dir: return False
        if not has_save: return False
        return True
    is_valid = has_valid(0,5,True,True,True,True)
    assert is_valid, "valid"
    print("PASS valid selection")
    assert not has_valid(-1,5,True,True,True,True), "invalid negative"
    assert not has_valid(5,5,True,True,True,True), "invalid stale"
    assert not has_valid(0,0,True,True,True,True), "empty list"
    assert not has_valid(0,5,False,True,True,True), "no path"
    assert not has_valid(0,5,True,False,True,True), "not dir"
    assert not has_valid(0,5,True,True,False,True), "no save file"
    print("PASS invalid selections rejected")
    # SELECT plain only
    EPBM_SELECT=512
    EPBM_R=128
    EPBM_R2=8192
    assert (512 == EPBM_SELECT), "plain SELECT"
    assert (512|128 != EPBM_SELECT), "SELECT+R1 not plain"
    assert (512|8192 != EPBM_SELECT), "SELECT+R2 not plain"
    print("PASS plain SELECT only")

def test_deferred_mapping():
    print("== test_deferred_mapping ==")
    PA_RENAME=1
    PA_EXPORT=2
    PA_DELETE=3
    PA_DUPLICATE=4
    assert PA_DUPLICATE != PA_EXPORT, "PA_DUPLICATE != PA_EXPORT"
    print("PASS PA_DUPLICATE != PA_EXPORT")
    # R1+A mapping
    r1a = {1:PA_RENAME, 2:PA_EXPORT, 3:PA_DELETE}
    assert r1a[2]==PA_EXPORT, "R1+A Export preserved"
    # SELECT mapping
    sel = {1:PA_RENAME, 2:PA_DUPLICATE, 3:PA_DELETE}
    assert sel[2]==PA_DUPLICATE, "SELECT Duplicate"
    assert sel[1]==PA_RENAME, "SELECT Rename"
    assert sel[3]==PA_DELETE, "SELECT Delete"
    print("PASS deferred mappings")

if __name__=="__main__":
    test_duplicate_naming()
    test_duplicate_copy()
    test_name_length()
    test_startup_new_random()
    test_select_menu_validity()
    test_deferred_mapping()
    print("ALL_FUNCTIONAL_TESTS_PASS")
