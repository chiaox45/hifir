# Retrospective: <change-name>

> Written: <YYYY-MM-DD> (after verify passed)
> Commit range: `<base-sha>..<head-sha>`
> Worktree: <path or "merged to main">

---

## §0 Evidence

> Quantitative front-matter — downstream Wins/Misses bullets reference this section
> instead of inlining [evidence: ...] per line.
> Cold-write scenario (retro written some time after cycle ends), only `git log` +
> `tasks.md` + commit messages should suffice to reconstruct this section.

- **Commit range**: `<base-sha>..<head-sha>` (<n> commits)
- **Diff size**: <+X / -Y lines across N files>
- **Tasks done**: <x>/<y> (`grep -cE '^\s*- \[x\]' tasks.md` → x)
- **Active hours**: <estimate>
- **Subagent dispatches**: <count or "n/a">
- **Model usage breakdown**: <which OmO categories used for which tasks>
- **New external dependencies**: <list, with license + version, or "none">
- **Bugs encountered post-merge**: <count, one-line each, or "none">
- **OpenSpec validate state at archive**: <pass / fail / not-run>
- **Test coverage signal**: <e.g. jacoco %, pytest count, vitest count, or "n/a">

Commit chain (chronological):

```
<base-sha> <one-line summary>
...
<head-sha> <archive commit one-line>
```

---

## §1 Wins

- [evidence: <commit/file/test>] <description>

## §2 Misses

- 🔴 [blocking | evidence: ...] <description>
- 🟡 [painful  | evidence: ...] <description>
- 📌 [nit      | evidence: ...] <description>

## §3 Plan Deviations

| Plan task | What changed | Why |
|-----------|--------------|-----|
| 1.2       | ...          | ... |

## §4 Skill / Workflow Compliance

| Skill                                            | Used |
|--------------------------------------------------|------|
| superpowers:brainstorming                        |      |
| superpowers:writing-plans                        |      |
| superpowers:using-git-worktrees                  |      |
| superpowers:subagent-driven-development          |      |
| (transitive) superpowers:test-driven-development |      |
| (transitive) superpowers:requesting-code-review  |      |
| superpowers:finishing-a-development-branch       |      |

> **Default expectation**: all ✓. Each skill is part of the schema design;
> skipping is an abnormal situation. Any ✗ must be explained in the
> `### Deliberately Skipped Skills` subsection below.

### Deliberately Skipped Skills

> Skipping a skill is a design escape hatch, not the normal path.
> Each ✗ must answer three questions below; the entire section being
> empty (all green) is the expected state.

- **`<skill name>`**
  - **What was skipped**: <specific skill or sub-step that was skipped>
  - **Why this cycle**: <specific cycle condition — do NOT write vague reasons like "not needed" / "too small" / "no time" / "blocked by external dep" / "skill output looked off"; name the actual trigger (specific commit / log line / observed behavior)>
  - **How to prevent recurrence**: How does the next cycle in the same situation avoid skipping? Choose one:
    - `schema graph fix` — specify which part of schema.yaml to change
    - `skill description tightening` — specify which skill's frontmatter / instruction to change
    - `CLAUDE.md trigger` — specify which reading rule to add to adopter CLAUDE.md.fragment
    - `scope-judgment rule` — specify how the cycle's scope should be judged
    - `one-off — schema boundary case, no prevention possible` — must state why it's a boundary case

> **Relationship with §6 Promote candidates**: Multiple cycles skipping the same
> skill with the same "How to prevent" answer → that pattern should be promoted
> to §6, directly triggering a schema / skill PR. Must not accumulate as "normal".

## §5 Surprises

- <assumption that turned out wrong>

## §6 Promote Candidates → long-term learning

Each candidate uses a `- [ ]` checklist:

- Title: severity emoji (🔴/🟡/📌) + one-sentence learning
- `→ **Promote to** <destination>` (memory / CLAUDE.md / schema / skill / one-off)
- Two body lines (matching feedback memory body schema):
  - `> **Why**: <reason; often a past incident or strong preference>`
  - `> **How to apply**: <when/where this guidance kicks in>`

Unchecked `- [ ]` items represent candidates not yet promoted — carry forward
to next cycle's retro for re-evaluation, or keep as cross-cycle observation points.

> **Carry-forward mechanism**: When writing next cycle's retro,
> `grep -A 5 '^- \[ \]' openspec/changes/archive/*/retrospective.md` to retrieve
> previous unchecked candidates. Judge each: carry-forward to this cycle's §6,
> promote in-place, or mark stale and stop tracking.

Example:

- [ ] 🔴 **<short rule>** → **Promote to memory** (type: feedback)
  > **Why**: <past incident or strong preference that motivated this rule>
  > **How to apply**: <which file / cycle phase / decision moment this kicks in>

- [ ] 🟡 **<another candidate>** → **Promote to project CLAUDE.md** (`<path/to/CLAUDE.md>` section)
  > **Why**: ...
  > **How to apply**: ...

- [ ] 📌 **<third candidate>** → **One-off** (record only, don't promote)
  > **Why**: <why it doesn't generalize>

## §7 Documentation Updates

> Record what was updated based on §6 promote candidates.
> Each entry: bullet-point brief, precise about what changed and where.

### openspec/config.yaml

- <learning added to `context` or `rules.<artifact-id>`>

### AGENTS.md / CLAUDE.md

- <learning added, with target file and section>
