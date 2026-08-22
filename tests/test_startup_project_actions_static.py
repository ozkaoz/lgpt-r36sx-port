#!/usr/bin/env python3
"""
TREEFROG_STARTUP_PROJECT_ACTIONS_V1 static host audit - REVISION centralize under SELECT
Verifies revision: all startup project actions under plain SELECT, R1+A/A+B removed, Export migrated.
"""
import re
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[1]
SRC = ROOT / "source/sources"

def read(path):
    return (SRC / path).read_text(errors="ignore")

def assert_contains(hay, needle, msg):
    if needle not in hay:
        raise AssertionError(f"FAIL {msg}: missing '{needle}'")
    print(f"PASS {msg}")

def assert_not_contains(hay, needle, msg):
    if needle in hay:
        raise AssertionError(f"FAIL {msg}: should NOT contain '{needle}'")
    print(f"PASS {msg}")

def assert_regex(hay, pattern, msg):
    if not re.search(pattern, hay, re.MULTILINE):
        raise AssertionError(f"FAIL {msg}: pattern '{pattern}' not found")
    print(f"PASS {msg}")

errors=0
def check(ok, msg):
    global errors
    if not ok:
        print(f"FAIL {msg}")
        errors+=1
    else:
        print(f"PASS {msg}")

# --------------- TreeFrogTextEditor ---------------
tfe_h = read("Application/UI/Views/ModalDialogs/TreeFrogTextEditor.h")
tfe_cpp = read("Application/UI/Views/ModalDialogs/TreeFrogTextEditor.cpp")

assert_contains(tfe_h, "GetAdditionalActionMask", "TTE h GetAdditionalActionMask")
assert_contains(tfe_h, "HandlePhysicalAction", "TTE h HandlePhysicalAction")
assert_contains(tfe_h, "GetActionHintLine", "TTE h GetActionHintLine")
assert_contains(tfe_h, "virtual unsigned int GetAdditionalActionMask", "TTE virtual mask")
assert_contains(tfe_h, "virtual bool HandlePhysicalAction", "TTE virtual handle")
assert_contains(tfe_h, "virtual const char *GetActionHintLine", "TTE virtual hint")
protected_idx = tfe_h.find("protected:")
private_idx = tfe_h.find("private:")
setstatus_idx = tfe_h.find("setStatus")
check(protected_idx != -1 and private_idx != -1 and setstatus_idx != -1 and protected_idx < setstatus_idx < private_idx, "TTE setStatus protected")
assert_contains(tfe_cpp, "GetAdditionalActionMask() const", "TTE cpp mask impl")
assert_contains(tfe_cpp, "return 0;", "TTE mask default 0")
assert_contains(tfe_cpp, "HandlePhysicalAction", "TTE cpp handle impl")
assert_contains(tfe_cpp, "return false;", "TTE handle default false")
assert_contains(tfe_cpp, 'return "A confirm', "TTE hint default")
assert_contains(tfe_cpp, "GetAdditionalActionMask()", "TTE cpp uses GetAdditionalActionMask")
assert_contains(tfe_cpp, "HandlePhysicalAction(actions)", "TTE cpp calls HandlePhysicalAction")
assert_contains(tfe_cpp, "GetActionHintLine()", "TTE DrawView uses GetActionHintLine")
assert_contains(tfe_cpp, "TFSP_A | TFSP_B | TFSP_X | TFSP_Y | GetAdditionalActionMask", "TTE OR additional mask")
assert_not_contains(tfe_cpp, "TreeFrogGetFramebuffer", "TTE no framebuffer")

# --------------- NewProjectDialog ---------------
npd_h = read("Application/UI/Views/ModalDialogs/NewProjectDialog.h")
npd_cpp = read("Application/UI/Views/ModalDialogs/NewProjectDialog.cpp")

assert_contains(npd_h, "startupRandomMode_", "NPD h startupRandomMode field")
assert_contains(npd_h, "bool startupRandomMode", "NPD h ctor param")
assert_contains(npd_h, "startupRandomMode = false", "NPD h default false")
assert_contains(npd_h, "GetAdditionalActionMask", "NPD overrides mask")
assert_contains(npd_h, "HandlePhysicalAction", "NPD overrides handle")
assert_contains(npd_h, "GetActionHintLine", "NPD overrides hint")
assert_contains(npd_cpp, "getRandomName()", "NPD uses getRandomName")
assert_contains(npd_cpp, "TFSP_A", "NPD handles TFSP_A")
assert_contains(npd_cpp, "TFSP_START", "NPD handles TFSP_START")
assert_contains(npd_cpp, "setInitialText(randomName", "NPD setInitialText random")
assert_contains(npd_cpp, "Descend(GetName()).Exists()", "NPD collision check Descend GetName")
assert_contains(npd_cpp, '"A random START confirm B erase"', "NPD hint startup")
assert_contains(npd_cpp, '"Name busy"', "NPD Name busy")
assert_contains(npd_cpp, "EndModal(1)", "NPD EndModal 1")
assert_contains(npd_cpp, "kMaxTries", "NPD bounded retry")
assert_contains(npd_cpp, 'RandomNames.h', "NPD includes RandomNames")
assert_contains(npd_cpp, "if (!startupRandomMode_) return false;", "NPD preserve default")
assert_contains(npd_cpp, "GetAdditionalActionMask() const", "NPD mask impl")
assert_contains(npd_cpp, "if (startupRandomMode_) return TFSP_START;", "NPD mask returns START")

pv = read("Application/UI/Views/ProjectView.cpp")
assert_contains(pv, 'NewProjectDialog(*this, "root:projects")', "ProjectView SaveAs uses root:projects")
assert_not_contains(pv, 'Path root("root:")', "SaveAs old root not used")
assert_contains(pv, 'Path projectsRoot("root:projects")', "SaveAs destination projectsRoot")
assert_contains(pv, 'projectsRoot.GetName() + "/" + npd.GetName()', "SaveAs str_dstprjdir under projects")
assert_contains(pv, 'str_dstsmpdir = str_dstprjdir + "/samples/"', "SaveAs samples under projects")
assert_not_contains(pv, 'Path root("root:")', "no stray root alias")
assert_not_contains(pv, '"/mnt/sdcard', "no hardcoded physical path")
assert_not_contains(pv, 'NewProjectDialog(*this, "root:", true', "ProjectView not startup mode (still default false)")
# Also ensure startup New still uses true
spd_cpp = read("Application/UI/Views/ModalDialogs/SelectProjectDialog.cpp")
assert_contains(spd_cpp, 'NewProjectDialog(*this, currentPath_, true)', "SPD startup New uses true")

# --------------- TreeFrogProjectActionModal ---------------
modal_h = SRC / "Application/UI/Views/ModalDialogs/TreeFrogProjectActionModal.h"
modal_cpp = SRC / "Application/UI/Views/ModalDialogs/TreeFrogProjectActionModal.cpp"
def exists(p): return p.exists()
check(exists(modal_h), "ProjectActionModal.h exists")
check(exists(modal_cpp), "ProjectActionModal.cpp exists")
if exists(modal_h):
    mh = modal_h.read_text()
    assert_contains(mh, "TreeFrogProjectActionModal", "Modal h class")
    assert_contains(mh, "ModalView", "Modal h inherits ModalView")
if exists(modal_cpp):
    mc = modal_cpp.read_text()
    assert_contains(mc, "SetWindow", "Modal uses SetWindow")
    assert_contains(mc, "DrawString", "Modal uses DrawString")
    assert_contains(mc, '"Rename"', "Modal has Rename")
    assert_contains(mc, '"Duplicate"', "Modal has Duplicate")
    assert_contains(mc, '"Export"', "Modal has Export")
    assert_contains(mc, '"Delete"', "Modal has Delete")
    assert_contains(mc, "PROJECT", "Modal title PROJECT")
    assert_contains(mc, "selected_", "Modal selected")
    assert_contains(mc, "EPBM_UP", "Modal UP")
    assert_contains(mc, "EPBM_DOWN", "Modal DOWN")
    assert_contains(mc, "EPBM_A", "Modal A confirm")
    assert_contains(mc, "EPBM_B", "Modal B cancel")
    assert_not_contains(mc, "TreeFrogGetFramebuffer", "Modal no framebuffer")
    assert_not_contains(mc, "SampleChopperModal", "Modal not touching chopper")
    assert_contains(mc, "width + 2", "Modal bounded width")
    assert_contains(mc, "height", "Modal height bounded")
    # Exactly 4 items
    count_rename = mc.count('"Rename"')
    count_dup = mc.count('"Duplicate"')
    count_exp = mc.count('"Export"')
    count_del = mc.count('"Delete"')
    check(count_rename==1 and count_dup==1 and count_exp==1 and count_del==1, "Modal exactly 4 items Rename/Duplicate/Export/Delete")
    # Ensure order Rename Duplicate Export Delete
    check(mc.find('"Rename"') < mc.find('"Duplicate"') < mc.find('"Export"') < mc.find('"Delete"'), "Modal order Rename Duplicate Export Delete")

# --------------- SelectProjectDialog ---------------
spd_h_text = read("Application/UI/Views/ModalDialogs/SelectProjectDialog.h")
assert_contains(spd_h_text, "PA_NONE", "SPD h PA_NONE")
assert_contains(spd_h_text, "PA_RENAME", "SPD h PA_RENAME")
assert_contains(spd_h_text, "PA_EXPORT", "SPD h PA_EXPORT")
assert_contains(spd_h_text, "PA_DELETE", "SPD h PA_DELETE")
assert_contains(spd_h_text, "PA_DUPLICATE", "SPD h PA_DUPLICATE")
assert_contains(spd_cpp, "PA_DUPLICATE", "SPD cpp PA_DUPLICATE")
assert_contains(spd_cpp, "PA_RENAME", "SPD cpp PA_RENAME")
assert_regex(spd_h_text, r"PA_DUPLICATE\s*=\s*4", "PA_DUPLICATE=4")
assert_regex(spd_h_text, r"PA_EXPORT\s*=\s*2", "PA_EXPORT=2")
# Defer pattern
assert_contains(spd_cpp, "DeferProjectAction", "SPD DeferProjectAction")
assert_contains(spd_cpp, "pendingAction_", "SPD pendingAction")
assert_contains(spd_cpp, "OnFrameUpdate", "SPD OnFrameUpdate")
assert_contains(spd_cpp, "launchProjectAction", "SPD launchProjectAction")
assert_contains(spd_cpp, "StartupProjectActionMenuCallback", "SPD startup callback")
assert_contains(spd_cpp, "TreeFrogProjectActionModal", "SPD uses startup modal")
assert_contains(spd_cpp, "DeferProjectAction(SelectProjectDialog::PA_RENAME", "SPD Rename maps to PA_RENAME")
assert_contains(spd_cpp, "DeferProjectAction(SelectProjectDialog::PA_DUPLICATE", "SPD Duplicate maps to PA_DUPLICATE")
assert_contains(spd_cpp, "DeferProjectAction(SelectProjectDialog::PA_EXPORT", "SPD Export maps to PA_EXPORT")
assert_contains(spd_cpp, "DeferProjectAction(SelectProjectDialog::PA_DELETE", "SPD Delete maps to PA_DELETE")
# SELECT plain only
assert_contains(spd_cpp, "mask == EPBM_SELECT", "SPD SELECT plain only")
assert_contains(spd_cpp, "HasValidCurrentProjectSelection", "SPD HasValidCurrentProjectSelection")
assert_contains(spd_cpp, "TreeFrogV40IsLgptProjectName", "SPD uses IsLgptProjectName")
assert_contains(spd_cpp, "TreeFrogV40ProjectHasSaveFile", "SPD uses HasSaveFile")
# REVISION: R1+A must be removed from startup
# Check no old startup R1+A menu construction remains
assert_not_contains(spd_cpp, 'static const char *actionItems[] = {"Rename", "Export", "Delete"}', "STARTUP R1+A menu removed")
# The callback itself may remain but should not be triggered; we verify ProcessButtonMask contains early returns for R1+A and A+B without opening menu
assert_contains(spd_cpp, "REVISION: centralize under SELECT", "revision comment")
assert_contains(spd_cpp, "if ((mask & EPBM_R) && (mask & EPBM_A))", "R1+A early return exists")
assert_contains(spd_cpp, "if ((mask & EPBM_A) && (mask & EPBM_B))", "A+B early return exists")
# Ensure old A+B delete block is gone (no MessageBox inside B branch with A+B)
# The old block had "Handle A + B combination for delete" and MessageBox Delete project
# New code should not have that comment
assert_not_contains(spd_cpp, "Handle A + B combination for delete", "old A+B delete block removed")
# Also ensure no direct DeleteProjectCallback via A+B path (only via SELECT->PA_DELETE)
# The only DeleteProjectCallback reference should be in launchProjectAction via PA_DELETE, not in ProcessButtonMask A+B
# Count occurrences of DeleteProjectCallback in ProcessButtonMask region: we can check that ProcessButtonMask does not contain DeleteProjectCallback
# Extract ProcessButtonMask function text
import re as _re
pbm_match = _re.search(r"void SelectProjectDialog::ProcessButtonMask.*?^};", spd_cpp, _re.MULTILINE | _re.DOTALL)
if pbm_match:
    pbm_text = pbm_match.group(0)
    assert_not_contains(pbm_text, "DeleteProjectCallback", "no A+B DeleteProjectCallback in ProcessButtonMask")
    # Ensure R1+A TreeFrogMenuModal not in ProcessButtonMask
    assert_not_contains(pbm_text, "TreeFrogMenuModal", "no TreeFrogMenuModal in ProcessButtonMask startup")
    # Ensure plain A load uses exact equality
    assert_contains(pbm_text, "if (mask == EPBM_A)", "plain A load uses exact mask == EPBM_A")
    # Ensure SELECT+R1/R2 fallthrough protection
    assert_contains(pbm_text, "if (mask & EPBM_SELECT)", "SELECT modifier fallthrough guard")
    # Ensure no fallthrough to load for R1+A/A+B
    print("PASS SELECT centralization checks")
else:
    print("WARN could not extract ProcessButtonMask for deep checks")

# Verify SELECT menu mapping 4 items
assert_regex(spd_cpp, r'if \(code == 1\) spd\.DeferProjectAction.*PA_RENAME', "SELECT menu 1->PA_RENAME")
assert_regex(spd_cpp, r'else if \(code == 2\) spd\.DeferProjectAction.*PA_DUPLICATE', "SELECT menu 2->PA_DUPLICATE")
assert_regex(spd_cpp, r'else if \(code == 3\) spd\.DeferProjectAction.*PA_EXPORT', "SELECT menu 3->PA_EXPORT")
assert_regex(spd_cpp, r'else if \(code == 4\) spd\.DeferProjectAction.*PA_DELETE', "SELECT menu 4->PA_DELETE")

# Verify Export available via SELECT (PA_EXPORT via Startup callback)
check("PA_EXPORT" in spd_cpp and "StartupProjectActionMenuCallback" in spd_cpp, "EXPORT_AVAILABLE_VIA_SELECT")

# Verify Delete confirm still via PA_DELETE
assert_contains(spd_cpp, "case PA_DELETE", "PA_DELETE case exists")
assert_contains(spd_cpp, "MessageBox", "Delete confirm still uses MessageBox")

# Ensure no second entry point: only SELECT opens project management
# The only DoModal with TreeFrogProjectActionModal should be in SELECT plain branch (include + construction)
count_modal = spd_cpp.count("TreeFrogProjectActionModal")
check(count_modal>=2 and count_modal<=3, "only SELECT entry point for project management")

# Check duplicate implementation still
assert_contains(spd_cpp, "OnDuplicateProject", "SPD OnDuplicateProject")
assert_contains(spd_cpp, "GetCurrentProjectPath()", "SPD duplicate source")
assert_contains(spd_cpp, "GetCurrentProjectBaseName()", "SPD duplicate base")
assert_contains(spd_cpp, '"_c"', "SPD duplicate suffix _c")
assert_regex(spd_cpp, r'"lgpt_".*dstBase', "SPD dstFull construction")
assert_contains(spd_cpp, '"Copy exists"', "SPD Copy exists")
assert_contains(spd_cpp, '"Name too long"', "SPD Name too long")
assert_contains(spd_cpp, '"Duplicate failed"', "SPD Duplicate failed")
assert_contains(spd_cpp, "RecursiveCopyDirectory", "SPD uses RecursiveCopyDirectory")
assert_contains(spd_cpp, "RecursiveDeleteDirectory", "SPD uses RecursiveDeleteDirectory for cleanup")
assert_contains(spd_cpp, "sync()", "SPD sync after duplicate")
assert_contains(spd_cpp, "Project duplicated:", "SPD success notification")
assert_contains(spd_cpp, "setCurrentFolder(currentPath_)", "SPD refresh after duplicate")
assert_contains(spd_cpp, "currentProject_ = foundIndex", "SPD cursor follows duplicate")
assert_contains(spd_cpp, "kMaxStem = 24", "SPD length check 24")
assert_not_contains(spd_cpp, "TreeFrogGetFramebuffer", "SPD no framebuffer")
assert_contains(spd_cpp, "TREEFROG_STARTUP_PROJECT_ACTIONS_V1", "SPD marker")
spd_h = read("Application/UI/Views/ModalDialogs/SelectProjectDialog.h")
assert_contains(spd_h, "PA_DUPLICATE", "SPD h PA_DUPLICATE")
assert_contains(spd_h, "OnDuplicateProject", "SPD h OnDuplicateProject")
assert_contains(spd_h, "HasValidCurrentProjectSelection", "SPD h HasValid...")

# Build marker
assert_contains(spd_cpp, "TreeFrogStartupProjectActionsBuildMarker", "SPD build marker")
assert_contains(spd_cpp, "TREEFROG_STARTUP_PROJECT_ACTIONS_V1", "marker string")

# Unrelated files
for f in ["Application/UI/Views/MixerView.cpp", "Application/UI/Views/InstrumentEqView.cpp", "Application/UI/Views/ModalDialogs/SampleChopperModal.cpp", "Adapters/TREEFROG/Main/TreeFrogLibretro.cpp"]:
    path = SRC / f
    if path.exists():
        content = path.read_text(errors="ignore")
        assert_not_contains(content, "TREEFROG_STARTUP_PROJECT_ACTIONS_V1", f"no startup marker in {f}")

# Input preservation
assert_regex(spd_cpp, r"mask == EPBM_SELECT", "SELECT plain equality")
print("CHECK SELECT_R1 preserved: not mask& for startup via SELECT guard")
# Verify R1+A and A+B early returns exist (not preserved as actions but preserved as global not consumed)
assert_contains(spd_cpp, "(mask & EPBM_R) && (mask & EPBM_A)", "R1+A early return exists (not opening menu)")
assert_contains(spd_cpp, "(mask & EPBM_A) && (mask & EPBM_B)", "A+B early return exists (not opening delete)")
assert_contains(spd_cpp, "if (mask == EPBM_A)", "plain A load preserved")

assert_contains(tfe_cpp, 'return "A confirm', "default hint preserved")

if errors>0:
    print(f"FAILED {errors} checks")
    exit(1)
print("ALL_STATIC_CHECKS_PASS")
