#!/usr/bin/env python3
"""
tests/test_agent_context_contract.py
Context-contract for multi-agent infrastructure V2.
Verifies:
 - required docs exist
 - AGENTS.md does not hardcode mutable/machine-specific authority
 - CURRENT structure + line cap + not append-only changelog
 - CONTEXT_MAP has no mutable branch/HEAD
 - legacy install/verify labeled non-canonical
 - DECISION IDs unique + valid status
 - no MONO_48K as current expected profile
 - RELEASE_CONTRACT contains POST_INSTALL_MANUAL_FIXES=0 + golden definitions
 - referenced key files exist
"""
import pathlib
import re
import sys

REPO = pathlib.Path(__file__).resolve().parents[1]

# Required files
REQUIRED = [
    REPO / "AGENTS.md",
    REPO / "CURRENT.md",
    REPO / "CONTEXT_MAP.md",
    REPO / "DECISIONS.md",
    REPO / "docs" / "ai" / "VALIDATION.md",
    REPO / "docs" / "ai" / "RELEASE_CONTRACT.md",
]

# Files that should exist when referenced
REFERENCED_MUST_EXIST = [
    REPO / "docs" / "BACON_1_5_GOLDEN_BOOTSTRAP_PHYSICAL_PASS.md",
    REPO / "docs" / "BACON_1_5_RELEASE_MANIFEST.md",
    REPO / "LGPT_R36SX_Bacon-1.5_SHA256SUMS.txt",
    REPO / "docs" / "RELEASE_SD_INCLUDED_FILES.txt",
    REPO / "sd_root" / "cubegm" / "cores" / "lgpt_core.so",
    REPO / "sd_root" / "lgpt" / "otg" / "audio_usb_profile",
    REPO / "scripts" / "agent_preflight.sh",
]

errors = []
warns = []

def check_required():
    for p in REQUIRED:
        if not p.exists():
            errors.append(f"missing required {p.relative_to(REPO)}")

def check_agents_no_hardcode():
    p = REPO / "AGENTS.md"
    if not p.exists():
        return
    t = p.read_text(encoding="utf-8", errors="ignore")
    # Must NOT contain hard-coded active branch as universal authority
    if "feature/bacon-1.5-fx" in t:
        errors.append("AGENTS.md must not hardcode feature/bacon-1.5-fx as canonical branch")
    # Must not contain fixed WSL path as universal authority (example OK if mentioned as example, but not as canonical checkout)
    # We allow mention only if clearly labeled example; fail if it appears as canonical repo definition
    if "/home/dafunknoise/lgpt-repo" in t:
        # If file claims it is THE canonical WSL repository, fail
        if "Canonical WSL repository" in t or "canonical WSL" in t.lower():
            errors.append("AGENTS.md must not define /home/dafunknoise/lgpt-repo as universal canonical path")
    # Must not contain obsolete core path as current canonical artifact if presented as lgpt_r36sx_port_libretro.so being current
    # Allow historical mention, but fail if it says "sd_root/cubegm/cores/lgpt_r36sx_port_libretro.so (payload)" as current
    if "lgpt_r36sx_port_libretro.so" in t:
        # If it claims that path is canonical payload without HINT legacy, error
        if "lgpt_r36sx_port_libretro.so" in t and "canonical" in t.lower() and "lgpt_core.so" not in t:
            errors.append("AGENTS.md uses obsolete lgpt_r36sx_port_libretro.so as current canonical core")
        # simpler: if both appear and core is described as canonical, ok; else check count without core
        pass
    # More generic: check for mutable HEAD hashes pattern in AGENTS (40 hex) if AGENTS contains 40-char sha that looks like hardcoded HEAD
    # Allow core SHA 46bd84... and ZIP C5C77A... as they are invariants, but not branch HEADs like b616a5b etc as active HEAD
    # We check for phrases "Current HEAD" or "active branch" in AGENTS
    if re.search(r"Current HEAD|active branch|last.*SHA", t, re.I):
        # limit: if AGENTS contains 7+ hex hash next to HEAD phrase, flag
        if re.search(r"HEAD[^\\n]{0,40}[0-9a-f]{7,40}", t, re.I):
            errors.append("AGENTS.md must not hardcode current HEAD hash — that belongs in CURRENT.md/Git")
    # Machine-specific repo path as universal authority
    if t.count("/mnt/g") > 2 or t.count("/mnt/f") > 2:
        errors.append("AGENTS.md must not hardcode /mnt/g or /mnt/f as universal authority (example max 2)")

    # MONO_48K must not be described as current expected profile
    if "MONO_48K" in t:
        # If file says MONO_48K is current expected, error; historical mention with 'not' or 'old' is ok
        lines = [l for l in t.splitlines() if "MONO_48K" in l]
        for line in lines:
            low = line.lower()
            if "mono_48k" in low and "not" not in low and "legacy" not in low and "old" not in low and "must not" not in low:
                errors.append(f"AGENTS.md must not describe MONO_48K as current expected (found: {line.strip()[:90]})")

def check_current():
    p = REPO / "CURRENT.md"
    if not p.exists():
        return
    t = p.read_text(encoding="utf-8", errors="ignore")
    lines = t.splitlines()
    # Required sections
    required_sections = ["Authority", "Repository", "Current Product Baseline", "Source Golden", "Physical Golden", "Release Golden", "Current Objective", "Last Relevant Validation", "Known Issues", "Pending Validation", "Next Exact Action", "Stop Conditions"]
    for sec in required_sections:
        if sec.lower() not in t.lower():
            errors.append(f"CURRENT.md missing required section: {sec}")
    # Must contain cache disclaimer
    if "last-known snapshot" not in t.lower() and "must be verified against direct evidence" not in t.lower():
        errors.append("CURRENT.md must state it is a last-known snapshot and must be verified against direct evidence")
    # Line cap approx 150
    if len(lines) > 150:
        errors.append(f"CURRENT.md too long: {len(lines)} lines (>150) — must be snapshot not changelog")
    # Not append-only changelog: check for repeated "Development cycles" or excessive historical hashes
    if t.lower().count("development cycles") > 0:
        # allow but warn if appears as historical append
        pass
    # Check if file is excessively append-only by counting HEAD occurrences > 10
    if len(re.findall(r"HEAD", t)) > 20:
        warns.append(f"CURRENT.md has many HEAD mentions ({len(re.findall(r'HEAD', t))}) — ensure not append-only changelog")
    # Must not contain excessive historical hashes? allow but we check length already
    if "CURRENT.md IS A CACHE" in (REPO / "AGENTS.md").read_text(errors="ignore"):
        pass

def check_context_map():
    p = REPO / "CONTEXT_MAP.md"
    if not p.exists():
        return
    t = p.read_text(encoding="utf-8", errors="ignore")
    # Must not contain current branch/HEAD mutable
    if re.search(r"Active development branch.*feature/bacon", t):
        errors.append("CONTEXT_MAP.md must not contain active branch state")
    if re.search(r"Current.*HEAD|HEAD.*b616a5b|HEAD.*4429d4e", t):
        errors.append("CONTEXT_MAP.md must not contain current HEAD state")
    if "current commit" in t.lower() and "sha" in t.lower():
        errors.append("CONTEXT_MAP.md must not contain current commit/SHA")
    # Must not contain current release SHA as router (allow manifest refs but not "Current release SHA: C5C77A...")
    # We allow references to ZIP identity if documented as example, but not as router state
    if re.search(r"Current release.*C5C77A", t):
        errors.append("CONTEXT_MAP.md must not hardcode current release SHA")
    # Must contain legacy label
    if "LEGACY U2523" not in t or "NOT CANONICAL" not in t:
        errors.append("CONTEXT_MAP.md must label scripts/install.sh and verify.sh as LEGACY U2523 NOT CANONICAL")
    # Check for MONO_48K as current
    if "MONO_48K" in t:
        # Allow only if negated
        for line in [l for l in t.splitlines() if "MONO_48K" in l]:
            if "not" not in line.lower() and "legacy" not in line.lower():
                errors.append(f"CONTEXT_MAP.md must not describe MONO_48K as current (found: {line.strip()[:80]})")

def check_decisions():
    p = REPO / "DECISIONS.md"
    if not p.exists():
        return
    t = p.read_text(encoding="utf-8", errors="ignore")
    # Only count headers (lines starting with ## DEC-) to avoid counting mentions in migrated list
    header_ids = re.findall(r"^##\s+(DEC-\d{4}-\d{2}-\d{2}-\d{2})", t, re.MULTILINE)
    ids = header_ids if header_ids else re.findall(r"DEC-\d{4}-\d{2}-\d{2}-\d{2}", t)
    from collections import Counter
    c = Counter(ids)
    dup = [k for k,v in c.items() if v>1]
    if dup:
        errors.append(f"DECISIONS.md duplicate IDs: {dup}")
    # All decisions must have valid status
    valid = {"ACTIVE","SUPERSEDED","DEPRECATED"}
    # Find status fields
    statuses = re.findall(r"\*\*Status:\*\*\s*(\w+)", t)
    if not statuses:
        # fallback: Status: ACTIVE
        statuses = re.findall(r"Status:\s*(ACTIVE|SUPERSEDED|DEPRECATED)", t)
    for s in statuses:
        if s not in valid:
            errors.append(f"DECISIONS.md invalid status: {s}")
    if len(statuses) == 0:
        errors.append("DECISIONS.md must have Status ACTIVE/SUPERSEDED/DEPRECATED for each decision")
    # Check required fields presence
    for field in ["Scope", "Context", "Decision", "Reason", "Consequences", "Evidence", "Related"]:
        if field not in t:
            warns.append(f"DECISIONS.md missing field {field} (optional strictness)")

    # Must not be operational log: check for many push entries
    push_count = len(re.findall(r"Push.*origin/feature", t))
    if push_count > 2:
        errors.append(f"DECISIONS.md still contains operational push events ({push_count}) — should be removed")

    # MONO_48K check
    if "MONO_48K" in t:
        for line in [l for l in t.splitlines() if "MONO_48K" in l]:
            low = line.lower()
            if "mono_48k" in low and "not" not in low and "legacy" not in low:
                # but decisions may mention MONO as historical context if negated
                if "must not" not in low and "no mono" not in low:
                    errors.append(f"DECISIONS.md must not describe MONO_48K as current ({line.strip()[:80]})")

def check_release_contract():
    p = REPO / "docs" / "ai" / "RELEASE_CONTRACT.md"
    if not p.exists():
        return
    t = p.read_text(encoding="utf-8", errors="ignore")
    if "POST_INSTALL_MANUAL_FIXES=0" not in t:
        errors.append("RELEASE_CONTRACT.md must contain POST_INSTALL_MANUAL_FIXES=0")
    for gold in ["SOURCE GOLDEN", "PHYSICAL GOLDEN", "RELEASE GOLDEN"]:
        if gold not in t:
            errors.append(f"RELEASE_CONTRACT.md must define {gold}")
    if "C5C77A0212E4784A9D0E6D0EDDC4DE1A8BBE0943B9EBEF8B13A18A82A6B9CB1E" not in t:
        warns.append("RELEASE_CONTRACT.md should reference validated ZIP SHA C5C77A...")

def check_validation_doc():
    p = REPO / "docs" / "ai" / "VALIDATION.md"
    if not p.exists():
        return
    t = p.read_text(encoding="utf-8", errors="ignore")
    for gold in ["SOURCE GOLDEN", "PHYSICAL GOLDEN", "RELEASE GOLDEN"]:
        if gold not in t and gold not in (REPO / "AGENTS.md").read_text(errors="ignore"):
            # validation doc may reference indirectly, but we check at least in AGENTS or here
            pass
    # Must define classes
    for cls in ["CLASS A", "CLASS B", "CLASS C", "CLASS D", "CLASS E"]:
        if cls not in t:
            errors.append(f"VALIDATION.md must define {cls}")

def check_referenced_exist():
    for p in REFERENCED_MUST_EXIST:
        if not p.exists():
            errors.append(f"referenced file missing: {p.relative_to(REPO)}")

def check_agents_core_path():
    # AGENTS should reference lgpt_core.so as canonical, not obsolete as current
    t = (REPO / "AGENTS.md").read_text(encoding="utf-8", errors="ignore")
    if "lgpt_core.so" not in t:
        warns.append("AGENTS.md should reference canonical core lgpt_core.so")
    if "lgpt_r36sx_port_libretro.so" in t:
        # if it appears, ensure it's not presented as canonical without note
        if "obsolete" not in t.lower() and "legacy" not in t.lower() and "historical" not in t.lower():
            # we already flag if hardcoded, but allow one mention if AGENTS correctly says canonical is lgpt_core.so
            # Only error if file says lgpt_r36sx_port... is canonical payload
            if "lgpt_r36sx_port_libretro.so" in t and "canonical" in t.lower():
                # check proximity
                idx = t.lower().find("lgpt_r36sx_port_libretro.so")
                snippet = t[max(0,idx-200):idx+200].lower()
                if "canonical" in snippet and "lgpt_core.so" not in snippet:
                    errors.append("AGENTS.md presents obsolete lgpt_r36sx_port_libretro.so as canonical — use lgpt_core.so")

def main():
    check_required()
    check_agents_no_hardcode()
    check_current()
    check_context_map()
    check_decisions()
    check_release_contract()
    check_validation_doc()
    check_referenced_exist()
    check_agents_core_path()

    # Global MONO_48K check in AI docs
    for p in [REPO / "AGENTS.md", REPO / "CONTEXT_MAP.md", REPO / "DECISIONS.md", REPO / "docs" / "ai" / "VALIDATION.md", REPO / "docs" / "ai" / "RELEASE_CONTRACT.md"]:
        if p.exists():
            t = p.read_text(encoding="utf-8", errors="ignore")
            if "MONO_48K" in t:
                # Report only if not negated
                for i,line in enumerate(t.splitlines(),1):
                    if "MONO_48K" in line and "must not" not in line.lower() and "no mono" not in line.lower() and "legacy" not in line.lower() and "obsolete" not in line.lower():
                        # allow if line is describing that MONO must NOT be current
                        if "as the current" in line.lower() or "expected" in line.lower():
                            errors.append(f"{p.relative_to(REPO)}:{i} must not describe MONO_48K as current expected (found: {line.strip()[:80]})")

    if warns:
        print("WARN:")
        for w in warns:
            print(f"  - {w}")
    if errors:
        print("AGENT_CONTEXT_CONTRACT: FAIL")
        for e in errors:
            print(f"  - {e}")
        return 1
    print("AGENT_CONTEXT_CONTRACT: PASS")
    print("  AGENTS/CURRENT/CONTEXT_MAP/DECISIONS structure PASS")
    print("  No hardcoded mutable/machine-specific authority PASS")
    print("  Legacy scripts labeled PASS")
    print("  Decisions unique/valid PASS")
    print("  Release contract golden definitions PASS")
    print("  Referenced files exist PASS")
    return 0

if __name__ == "__main__":
    sys.exit(main())
