#!/usr/bin/env python3
"""
tests/test_agent_context_contract.py
Context-contract for multi-agent infrastructure V2.1.
Verifies:
 - required docs exist
 - AGENTS.md does not hardcode mutable/machine-specific authority, nor full core/release SHA, nor This Task
 - CURRENT structure + HEAD resolve from Git + line cap
 - CONTEXT_MAP has no mutable branch/HEAD
 - legacy install/verify labeled non-canonical
 - DECISION IDs unique + valid status
 - no MONO_48K as current expected profile
 - RELEASE_CONTRACT contains POST_INSTALL_MANUAL_FIXES=0 + golden definitions
 - BRANCH_TAG_POLICY policy-only + immutable release tags
 - README legacy workflow safe
 - opencode agents have valid frontmatter with permissions
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

FULL_CORE_SHA = "46bd84ebb0d1b1be8caec7c76fecbe6fb4baa8e9bbd603b44488bcc929dedec6"
FULL_ZIP_SHA = "C5C77A0212E4784A9D0E6D0EDDC4DE1A8BBE0943B9EBEF8B13A18A82A6B9CB1E"

def check_required():
    for p in REQUIRED:
        if not p.exists():
            errors.append(f"missing required {p.relative_to(REPO)}")

def check_agents_no_hardcode():
    p = REPO / "AGENTS.md"
    if not p.exists():
        return
    t = p.read_text(encoding="utf-8", errors="ignore")
    if "feature/bacon-1.5-fx" in t:
        errors.append("AGENTS.md must not hardcode feature/bacon-1.5-fx as canonical branch")
    if "/home/dafunknoise/lgpt-repo" in t:
        if "Canonical WSL repository" in t or "canonical WSL" in t.lower():
            errors.append("AGENTS.md must not define /home/dafunknoise/lgpt-repo as universal canonical path")
    if "lgpt_r36sx_port_libretro.so" in t:
        if "lgpt_r36sx_port_libretro.so" in t and "canonical" in t.lower() and "lgpt_core.so" not in t:
            errors.append("AGENTS.md uses obsolete lgpt_r36sx_port_libretro.so as current canonical core")
    if re.search(r"Current HEAD|active branch|last.*SHA", t, re.I):
        if re.search(r"HEAD[^\\n]{0,40}[0-9a-f]{7,40}", t, re.I):
            errors.append("AGENTS.md must not hardcode current HEAD hash — that belongs in CURRENT.md/Git")
    if t.count("/mnt/g") > 2 or t.count("/mnt/f") > 2:
        errors.append("AGENTS.md must not hardcode /mnt/g or /mnt/f as universal authority (example max 2)")
    if "MONO_48K" in t:
        lines = [l for l in t.splitlines() if "MONO_48K" in l]
        for line in lines:
            low = line.lower()
            if "mono_48k" in low and "not" not in low and "legacy" not in low and "old" not in low and "must not" not in low:
                errors.append(f"AGENTS.md must not describe MONO_48K as current expected (found: {line.strip()[:90]})")

def check_agents_v21():
    p = REPO / "AGENTS.md"
    if not p.exists():
        return
    t = p.read_text(encoding="utf-8", errors="ignore")
    # V2.1: must NOT hardcode full core/ZIP SHA as permanent identity
    if FULL_CORE_SHA in t:
        errors.append("AGENTS.md must not hardcode full core SHA 46bd84... as permanent identity — resolve via CURRENT.md/manifests")
    if FULL_ZIP_SHA in t:
        errors.append("AGENTS.md must not hardcode full release ZIP SHA C5C77A... as permanent identity — resolve via CURRENT.md/manifests")
    # Must NOT contain This Task heading with temporary immutability
    if "(This Task)" in t:
        errors.append("AGENTS.md must not contain '(This Task)' heading — use generic Golden Protection")
    # If still contains old Section 7 title
    if re.search(r"## 7\. SD / Core / Release Immutability \(This Task\)", t):
        errors.append("AGENTS.md still contains old Section 7 '(This Task)' — replace with generic protection")
    # Check for CORE_CHANGED=NO inside AGENTS (should be gone)
    if "CORE_CHANGED=NO" in t and "This Task" in t:
        errors.append("AGENTS.md must not contain temporary CORE_CHANGED/RUNTIME_CHANGED values tied to This Task")
    # Should contain CURRENT_CORE_IDENTITY resolve wording
    if "CURRENT_CORE_IDENTITY" not in t and "resolve from current Source/Physical/Release" not in t.lower():
        warns.append("AGENTS.md should contain CURRENT_CORE_IDENTITY resolve wording (V2.1)")
    if "CURRENT_RELEASE_IDENTITY" not in t:
        warns.append("AGENTS.md should contain CURRENT_RELEASE_IDENTITY resolve wording")

def check_current():
    p = REPO / "CURRENT.md"
    if not p.exists():
        return
    t = p.read_text(encoding="utf-8", errors="ignore")
    lines = t.splitlines()
    required_sections = ["Authority", "Repository", "Current Product Baseline", "Source Golden", "Physical Golden", "Release Golden", "Current Objective", "Last Relevant Validation", "Known Issues", "Pending Validation", "Next Exact Action", "Stop Conditions"]
    for sec in required_sections:
        if sec.lower() not in t.lower():
            errors.append(f"CURRENT.md missing required section: {sec}")
    if "last-known snapshot" not in t.lower() and "must be verified against direct evidence" not in t.lower():
        errors.append("CURRENT.md must state it is a last-known snapshot and must be verified against direct evidence")
    if len(lines) > 150:
        errors.append(f"CURRENT.md too long: {len(lines)} lines (>150) — must be snapshot not changelog")
    if len(re.findall(r"HEAD", t)) > 20:
        warns.append(f"CURRENT.md has many HEAD mentions ({len(re.findall(r'HEAD', t))}) — ensure not append-only changelog")
    # V2.1: HEAD must be RESOLVE FROM GIT, not hardcoded self-SHA
    if "RESOLVE FROM GIT" not in t:
        errors.append("CURRENT.md must say HEAD is RESOLVE FROM GIT AT SESSION START (not hardcoded self-SHA)")
    # Check for hardcoded 7-char HEAD-like that looks like current HEAD value
    # If CURRENT contains a line like HEAD: `b616a5b...` or `4d86e5f...` as literal current, flag
    # Allow historical Source Golden commit hashes (4429d4e etc) but not a line claiming HEAD is that SHA
    for line in t.splitlines():
        if re.match(r".*HEAD:\s*`?[0-9a-f]{7,40}`?.*\(verify.*git rev-parse HEAD\)", line, re.I):
            # This is the old hardcode pattern we want to avoid if it contains literal SHA before verify
            # If line contains a hex SHA before the verify hint, it's hardcoded
            if re.search(r"[0-9a-f]{7,40}", line.split("verify")[0], re.I):
                if "RESOLVE FROM GIT" not in line:
                    errors.append(f"CURRENT.md must not hardcode HEAD SHA literal: {line.strip()[:80]}")
    # Check objective is current (not still describing V2 modernization as pending)
    if "Modernize multi-agent infrastructure" in t and "Pending Validation" in t and "must pass after infra rewrite" in t:
        errors.append("CURRENT.md still describes V2 modernize as pending — update Current Objective/Next Action to V2.1 baseline")
    # Must not have excessive append pattern
    if "HEAD=b616a5b" in t and "RESOLVE FROM GIT" not in t:
        errors.append("CURRENT.md still hardcodes HEAD=b616a5b — must be RESOLVE FROM GIT")

def check_context_map():
    p = REPO / "CONTEXT_MAP.md"
    if not p.exists():
        return
    t = p.read_text(encoding="utf-8", errors="ignore")
    if re.search(r"Active development branch.*feature/bacon", t):
        errors.append("CONTEXT_MAP.md must not contain active branch state")
    if re.search(r"Current.*HEAD|HEAD.*b616a5b|HEAD.*4429d4e", t):
        errors.append("CONTEXT_MAP.md must not contain current HEAD state")
    if "current commit" in t.lower() and "sha" in t.lower():
        errors.append("CONTEXT_MAP.md must not contain current commit/SHA")
    if re.search(r"Current release.*C5C77A", t):
        errors.append("CONTEXT_MAP.md must not hardcode current release SHA")
    if "LEGACY U2523" not in t or "NOT CANONICAL" not in t:
        errors.append("CONTEXT_MAP.md must label scripts/install.sh and verify.sh as LEGACY U2523 NOT CANONICAL")
    if "MONO_48K" in t:
        for line in [l for l in t.splitlines() if "MONO_48K" in l]:
            if "not" not in line.lower() and "legacy" not in line.lower():
                errors.append(f"CONTEXT_MAP.md must not describe MONO_48K as current (found: {line.strip()[:80]})")
    # V2.1: decision references should be semantically correct
    # Check TreeFrog row not pointing to wrong DEC-23-01
    if "TreeFrog audio" in t and "DEC-2026-08-23-01" in t:
        # TreeFrog row should not point to multi-agent architecture
        if "TreeFrog audio (48k stereo)" in t:
            # extract that line
            for line in t.splitlines():
                if "TreeFrog audio" in line and "DEC-2026-08-23-01" in line:
                    errors.append("CONTEXT_MAP.md TreeFrog audio must not point to DEC-2026-08-23-01 (multi-agent) — use — or correct DEC")
    # Agent infra should point to DEC-23-01, not 06
    for line in t.splitlines():
        if "Agent infrastructure" in line and "DEC-2026-08-23-06" in line:
            errors.append("CONTEXT_MAP.md Agent infrastructure must not point to DEC-2026-08-23-06 (kernel) — should be DEC-2026-08-23-01")
        if "Release packaging" in line and "DEC-2026-08-23-05" in line and "DEC-2026-08-23-02" not in line:
            warns.append("CONTEXT_MAP.md Release packaging should include DEC-2026-08-23-02/03/04 (bootstrap/persistent/download-back)")

def check_decisions():
    p = REPO / "DECISIONS.md"
    if not p.exists():
        return
    t = p.read_text(encoding="utf-8", errors="ignore")
    header_ids = re.findall(r"^##\s+(DEC-\d{4}-\d{2}-\d{2}-\d{2})", t, re.MULTILINE)
    ids = header_ids if header_ids else re.findall(r"DEC-\d{4}-\d{2}-\d{2}-\d{2}", t)
    from collections import Counter
    c = Counter(ids)
    dup = [k for k,v in c.items() if v>1]
    if dup:
        errors.append(f"DECISIONS.md duplicate IDs: {dup}")
    valid = {"ACTIVE","SUPERSEDED","DEPRECATED"}
    statuses = re.findall(r"\*\*Status:\*\*\s*(\w+)", t)
    if not statuses:
        statuses = re.findall(r"Status:\s*(ACTIVE|SUPERSEDED|DEPRECATED)", t)
    for s in statuses:
        if s not in valid:
            errors.append(f"DECISIONS.md invalid status: {s}")
    if len(statuses) == 0:
        errors.append("DECISIONS.md must have Status ACTIVE/SUPERSEDED/DEPRECATED for each decision")
    for field in ["Scope", "Context", "Decision", "Reason", "Consequences", "Evidence", "Related"]:
        if field not in t:
            warns.append(f"DECISIONS.md missing field {field} (optional strictness)")
    push_count = len(re.findall(r"Push.*origin/feature", t))
    if push_count > 2:
        errors.append(f"DECISIONS.md still contains operational push events ({push_count}) — should be removed")
    if "MONO_48K" in t:
        for line in [l for l in t.splitlines() if "MONO_48K" in l]:
            low = line.lower()
            if "mono_48k" in low and "not" not in low and "legacy" not in low:
                if "must not" not in low and "no mono" not in low:
                    errors.append(f"DECISIONS.md must not describe MONO_48K as current ({line.strip()[:80]})")
    # Check DEC-32 note exists for superseded Android clause
    if "DEC-2026-08-21-32" in t:
        if "2026-08-23" not in t or "H38-only" not in t:
            # Check if DEC-32 section has a note about H38
            section = re.search(r"## DEC-2026-08-21-32.*?(?=## DEC-|\Z)", t, re.S)
            if section and "H38" not in section.group(0):
                warns.append("DEC-2026-08-21-32 should clarify Android payload is historical / H38-only authority is DEC-23-02/03")

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
    for cls in ["CLASS A", "CLASS B", "CLASS C", "CLASS D", "CLASS E"]:
        if cls not in t:
            errors.append(f"VALIDATION.md must define {cls}")

def check_branch_tag_policy():
    p = REPO / "docs" / "BRANCH_TAG_POLICY.md"
    if not p.exists():
        warns.append("docs/BRANCH_TAG_POLICY.md missing")
        return
    t = p.read_text(encoding="utf-8", errors="ignore")
    # Must not contain hardcoded mutable mappings
    for bad in ["46bd84", "c778512", "557b26d", "b7b2e46", "beb8a12", "6f944d6", "449041f"]:
        if bad in t:
            errors.append(f"BRANCH_TAG_POLICY.md must not hardcode mutable mapping {bad} — Git is authority")
    # Must state IMMUTABLE for release tags
    if "IMMUTABLE" not in t:
        errors.append("BRANCH_TAG_POLICY.md must state release tags are IMMUTABLE once published")
    if "movable only for hygiene" in t.lower():
        errors.append("BRANCH_TAG_POLICY.md must not describe public release tags as movable for hygiene")
    # Must distinguish main vs Release Golden
    if "CLASS A/B" not in t and "may advance through" not in t.lower():
        warns.append("BRANCH_TAG_POLICY.md should state main may advance through CLASS A/B without changing Release Golden")

def check_readme():
    p = REPO / "README.md"
    if not p.exists():
        return
    t = p.read_text(encoding="utf-8", errors="ignore")
    if "legacy u2523" not in t.lower():
        errors.append("README.md must mention scripts/install.sh and verify.sh are legacy U2523 (see CONTEXT_MAP)")
    if "release_contract" not in t.lower():
        errors.append("README.md must reference docs/ai/RELEASE_CONTRACT.md for canonical Bacon-1.5 install path")
    if "scripts/: current build, install, verification" in t:
        errors.append("README.md still describes scripts/: current build, install, verification — must be corrected to build/audit/release/legacy")

def check_agents_version_consistency():
    agents_text = (REPO / "AGENTS.md").read_text(encoding="utf-8", errors="ignore")
    current_text = (REPO / "CURRENT.md").read_text(encoding="utf-8", errors="ignore")
    m1 = re.search(r"\*\*Version:\*\*\s*([0-9]+\.[0-9]+)", agents_text)
    m2 = re.search(r"Constitution:\s*`AGENTS\.md v([0-9]+\.[0-9]+)`", current_text)
    if not m1:
        errors.append("AGENTS.md missing **Version:** X.Y")
        return
    if not m2:
        errors.append("CURRENT.md missing Constitution: AGENTS.md vX.Y")
        return
    if m1.group(1) != m2.group(1):
        errors.append(f"AGENTS version {m1.group(1)} != CURRENT declared AGENTS version {m2.group(1)} — must be consistent")

def check_branch_policy_class_aware():
    p = REPO / "docs" / "BRANCH_TAG_POLICY.md"
    if not p.exists():
        return
    t = p.read_text(encoding="utf-8", errors="ignore")
    # Must reference VALIDATION routing
    if "VALIDATION.md" not in t:
        errors.append("BRANCH_TAG_POLICY.md must reference docs/ai/VALIDATION.md for class routing")
    if "CLASS A" not in t or "CLASS B" not in t or "CLASS C" not in t:
        errors.append("BRANCH_TAG_POLICY.md must reference change-class gates (CLASS A/B/C...) not universal physical gate")
    # Must not require physical R36SX universally for all classes (old universal list)
    # Old pattern: Implement → Host tests → MIPS build → Physical R36SX validation as mandatory for every feature branch
    if "Physical R36SX validation" in t and "Classify change" not in t:
        errors.append("BRANCH_TAG_POLICY.md must not require physical R36SX universally for all classes — use class-aware gates via VALIDATION.md")
    # Generic hygiene: should not hardcode specific release ZIP as eternal policy, prefer pattern
    if "LGPT_R36SX_Bacon-1.5_SD_ROOT.zip" in t and "LGPT_R36SX_Bacon-X.Y" not in t:
        warns.append("BRANCH_TAG_POLICY.md Hygiene should use generic pattern LGPT_R36SX_Bacon-X.Y_SD_ROOT.zip with resolve note, not just concrete 1.5")

def check_current_objective_idle():
    t = (REPO / "CURRENT.md").read_text(encoding="utf-8", errors="ignore")
    if "Multi-agent V2.1 consistency/enforcement cleanup" in t:
        errors.append("CURRENT.md still presents 'Multi-agent V2.1 consistency/enforcement cleanup' as active objective — should be idle: No active implementation/runtime task")
    if "No active implementation/runtime task" not in t:
        warns.append("CURRENT.md Current Objective should be idle: No active implementation/runtime task")
    if "Await explicit" not in t:
        warns.append("CURRENT.md should say 'Await explicit user-approved objective'")

def check_referenced_exist():
    for p in REFERENCED_MUST_EXIST:
        if not p.exists():
            errors.append(f"referenced file missing: {p.relative_to(REPO)}")

def check_agents_core_path():
    t = (REPO / "AGENTS.md").read_text(encoding="utf-8", errors="ignore")
    if "lgpt_r36sx_port_libretro.so" in t:
        if "obsolete" not in t.lower() and "legacy" not in t.lower() and "historical" not in t.lower():
            if "lgpt_r36sx_port_libretro.so" in t and "canonical" in t.lower():
                idx = t.lower().find("lgpt_r36sx_port_libretro.so")
                snippet = t[max(0,idx-200):idx+200].lower()
                if "canonical" in snippet and "lgpt_core.so" not in snippet:
                    errors.append("AGENTS.md presents obsolete lgpt_r36sx_port_libretro.so as canonical — use lgpt_core.so")

def check_opencode_agents():
    agents_dir = REPO / ".opencode" / "agents"
    if not agents_dir.exists():
        errors.append(".opencode/agents directory missing")
        return
    expected = ["audit.md","implement.md","review.md","release.md"]
    for name in expected:
        p = agents_dir / name
        if not p.exists():
            errors.append(f"missing opencode agent {name}")
            continue
        content = p.read_text(encoding="utf-8", errors="ignore")
        # Must have frontmatter
        if not content.lstrip().startswith("---"):
            errors.append(f"{p.relative_to(REPO)} missing YAML frontmatter (must start with ---)")
            continue
        # Extract frontmatter block
        m = re.search(r"^---\s*\n(.*?)\n---\s*\n", content, re.S)
        if not m:
            errors.append(f"{p.relative_to(REPO)} frontmatter not parseable (missing --- delimiters)")
            continue
        fm = m.group(1)
        # description required
        if "description:" not in fm:
            errors.append(f"{p.relative_to(REPO)} frontmatter missing description")
        # mode valid
        mode_match = re.search(r"mode:\s*(\w+)", fm)
        if not mode_match:
            errors.append(f"{p.relative_to(REPO)} frontmatter missing mode")
        else:
            mode = mode_match.group(1)
            if mode not in ("subagent","primary","all"):
                errors.append(f"{p.relative_to(REPO)} mode must be subagent/primary/all, got {mode}")
        # permission field
        if "permission:" not in fm and "permissions:" not in fm:
            errors.append(f"{p.relative_to(REPO)} frontmatter missing permission(s) field")
        # specific checks
        if name in ("audit.md","review.md"):
            # must have edit: deny in frontmatter, not just prose
            if not re.search(r"edit:\s*deny", fm):
                errors.append(f"{p.relative_to(REPO)} must have frontmatter edit: deny (not just prose)")
        if name == "audit.md":
            # bash should be deny or limited
            if "bash:" not in fm:
                errors.append(f"{p.relative_to(REPO)} should have bash permission configured")
        if name == "review.md":
            if not re.search(r"edit:\s*deny", fm):
                errors.append(f"{p.relative_to(REPO)} review must have edit deny")
        if name in ("implement.md","release.md"):
            # should have destructive protection
            if "bash:" not in fm:
                warns.append(f"{p.relative_to(REPO)} should have bash permission for destructive protection")
            # check for ask/deny for destructive
            if name == "release.md" and "edit:" not in fm:
                warns.append(f"{p.relative_to(REPO)} release should have edit permission configured")
    # discovery count
    found = list(agents_dir.glob("*.md"))
    if len(found) < 4:
        warns.append(f".opencode/agents should have at least 4 agents, found {len(found)}")

def main():
    check_required()
    check_agents_no_hardcode()
    check_agents_v21()
    check_agents_version_consistency()
    check_current()
    check_current_objective_idle()
    check_context_map()
    check_decisions()
    check_release_contract()
    check_validation_doc()
    check_branch_tag_policy()
    check_branch_policy_class_aware()
    check_readme()
    check_referenced_exist()
    check_agents_core_path()
    check_opencode_agents()

    for p in [REPO / "AGENTS.md", REPO / "CONTEXT_MAP.md", REPO / "DECISIONS.md", REPO / "docs" / "ai" / "VALIDATION.md", REPO / "docs" / "ai" / "RELEASE_CONTRACT.md"]:
        if p.exists():
            t = p.read_text(encoding="utf-8", errors="ignore")
            if "MONO_48K" in t:
                for i,line in enumerate(t.splitlines(),1):
                    if "MONO_48K" in line and "must not" not in line.lower() and "no mono" not in line.lower() and "legacy" not in line.lower() and "obsolete" not in line.lower():
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
    print("  Branch policy + README + opencode frontmatter PASS")
    return 0

if __name__ == "__main__":
    sys.exit(main())
