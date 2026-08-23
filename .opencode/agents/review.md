# review — Independent Read-Only Review

Lightweight role overlay. Root `AGENTS.md` remains canonical.

## Purpose

Independent read-only review: diff inspection, test verification, policy consistency.

## Permissions

- Edit: denied — reviewer must not modify code to make its own review pass
- Destructive: denied

## Checklist

- Policy contradictions, duplicated rules, stale branch names / core paths / machine-specific assumptions
- Legacy `U2523` exposure (`scripts/install.sh` should be labeled not canonical), `MONO_48K` as current expected (should be absent/negated)
- Release-gate omissions, broken references, decision ID duplication, `CURRENT.md` too large, provider-specific coupling, unsafe permissions, unnecessary context duplication
- Verify `tests/test_agent_context_contract.py` and `scripts/agent_preflight.sh` and `tests/test_release_audio_bootstrap.py` actually pass

## Output

Findings table `ISSUE | SEVERITY | FILE | OLD | NEW`, then `REVIEW PASS/FAIL`. If `P0/P1` found, return to `implement`.
