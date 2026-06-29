# Verification Report

> This file is produced by the verify artifact AFTER apply phase completes.
> Failed checks require returning to the corresponding artifact for correction
> before re-running verify.

**Change**: `<change-name>`
**Verified at**: `YYYY-MM-DD HH:mm`
**Verifier**: `<who / which agent>`

---

## PRECHECK

Before proceeding, confirm evidence exists:

1. Commit evidence (must return > 0):
   ```bash
   git log --oneline $(git merge-base HEAD origin/main 2>/dev/null || git merge-base HEAD origin/master 2>/dev/null)..HEAD | wc -l
   ```

2. Task progress (must return > 0):
   ```bash
   grep -c '^- \[x\]' tasks.md
   ```

If either returns 0, STOP — apply has not completed yet.

---

## 1. Structural Validation (`openspec validate --all --json`)

- [ ] All items `"valid": true`

**Results**:

```text
<paste openspec validate --all output summary>
```

If any items failed, list id + issues:

| Item | Type | Issues |
|---|---|---|
| — | — | — |

---

## 2. Task Completion (`tasks.md`)

- [ ] All `- [ ]` changed to `- [x]`

**Incomplete tasks** (if any):

| Task | Reason incomplete | Blocks archive? |
|---|---|---|
| — | — | — |

---

## 3. Delta Spec Sync State

For each capability directory under `openspec/changes/<name>/specs/`,
compare against `openspec/specs/<capability>/spec.md`:

| Capability | Sync Status | Notes |
|---|---|---|
| — | ✓ Synced / ✗ Needs sync / N/A | — |

---

## 4. Design / Specs Coherence Spot Check

Sample-check that design.md decisions are reflected in specs/*.md
Requirements and Scenarios:

| Sample | design.md description | specs.md correspondence | Gap |
|---|---|---|---|
| — | — | — | — |

**Drift warnings** (non-blocking):

- <list if any; otherwise "none">

---

## 5. Implementation Signal

- [ ] No unstaged files in worktree
- [ ] All relevant commits pushed

**Commit range** (if known): `<from-sha>..<to-sha>`

---

## 6. Front-Door Routing Leak Detector (warning, non-blocking)

Design output should not land in `docs/superpowers/specs/` — the
discover artifact's output redirection routes it to
`openspec/changes/<name>/discover.md`.

Detection:

```bash
ls docs/superpowers/specs/*.md 2>/dev/null
```

- [ ] No files, or existing files are legitimate pre-schema-install artifacts

**Leak list** (if any):

| File | Content captured in change dir? | Recommended action |
|---|---|---|
| — | — | — |

> Non-blocking. New schema-installed cycle leaks should be moved to
> `openspec/changes/<name>/discover.md` or `design.md` and deleted.

---

## 7. TDD Compliance

Verify test files exist and were written before implementation:

```bash
git log --diff-filter=A --name-only -- '*.test.*' '*.spec.*'
```

- [ ] Test files exist for implemented features
- [ ] Test files were added in earlier commits than corresponding implementation files

**TDD violations** (if any):

| Implementation file | Test file | Test added before impl? |
|---|---|---|
| — | — | ✓ / ✗ |

---

## 8. Deferred Dogfood vs Automated-Test Equivalence

For plan.md tasks marked `[~]` deferred (manual smoke / dogfood / live-environment
checks not run this cycle), list equivalent automated test coverage.
If no equivalent automated test exists, that row is a real coverage gap —
record in retrospective Misses with a follow-up plan.

| Deferred dogfood (plan §) | Equivalent automated test | Coverage assessment | Real gap? |
|---|---|---|---|
| — | — | — | — |

> **Reading rules**:
> - "Equivalent" = automated test assertion set is superset of manual dogfood assertions
> - "Coverage assessment" = list layers touched (context / DB schema / wiring / HTTP path / etc.)
> - Any "Real gap = ✓" row: Overall Decision can still PASS, but must have follow-up in retrospective

> **When can this section be empty**: When plan.md has no `[~]` marked rows,
> this section doesn't need content (empty = PASS).
> If plan.md has any `[~]`, this section must list each one, otherwise Overall Decision = FAIL.

---

## Overall Decision

- [ ] ✅ PASS — proceed to finishing-a-development-branch and archive
- [ ] ⚠️ PASS WITH WARNINGS — proceed but note: `<explanation>`
- [ ] ❌ FAIL — return to failed artifact, correct, and re-run verify

**Next step**:

<describe next action>
