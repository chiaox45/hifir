<!--
Debugging analysis report: captures systematic investigation of a bug/issue.
Use superpowers:systematic-debugging skill to fill this template.

This file is the RAW INPUT for tasks.md.
Do NOT pre-fill tasks.md during analysis. It is a separate artifact
that transforms this content into an implementation checklist.

Output location: openspec/changes/<change-name>/analyze.md
-->

# Bug Analysis: <change-name>

> Written: <YYYY-MM-DD>
> Severity: <critical / high / medium / low>
> Component: <affected module or file>

---

## Symptoms

<!--
What is the observable behavior? What should happen instead?
Include:
- Error messages (exact text)
- Unexpected behavior description
- Expected behavior description
- Reproduction steps (if known)
-->

## Evidence

<!--
Collected data supporting the investigation:
- Log excerpts (with timestamps)
- Stack traces
- Screenshots or recordings
- Test output (if applicable)
- Performance metrics (if applicable)

Paste raw evidence here. Do not interpret yet.
-->

## Hypotheses

<!--
Ranked list of potential root causes (most likely first):

1. **<Hypothesis 1>** (likelihood: high / medium / low)
   - Evidence supporting: ...
   - Evidence against: ...
   - How to verify: ...

2. **<Hypothesis 2>** (likelihood: high / medium / low)
   - Evidence supporting: ...
   - Evidence against: ...
   - How to verify: ...

Continue until you have 2-4 hypotheses.
-->

## Root Cause

<!--
The confirmed root cause after hypothesis elimination.
Explain:
- What is the actual bug?
- Why does it happen?
- What code/config/data is responsible?
- Is this a regression? If yes, what changed?
-->

## Impact Assessment

<!--
Scope and severity of the bug:
- **Affected users**: <estimate or "all" / "none">
- **Affected functionality**: <list features or workflows>
- **Data impact**: <corruption / loss / none>
- **Security impact**: <yes / no, explain if yes>
- **Performance impact**: <degradation / none>
- **Workaround available**: <yes / no, describe if yes>
-->

## Agent Consultation Notes

<!--
Findings from OmO agent consultations (if used):

### Librarian
- Documentation references found: ...
- Known issues or similar bugs: ...
- External resources: ...

### Explore
- Related code locations: ...
- Pattern violations: ...
- Dependencies affected: ...

### Oracle
- Complex reasoning insights: ...
- Architectural implications: ...
- Recommended approach: ...

If an agent was unavailable, note: "<agent> unavailable, proceeded without consultation"
-->

## Recommended Fix Strategy

<!--
High-level approach to fixing the bug:
- What needs to change?
- What tests should be added?
- What risks exist with the fix?
- Estimated effort: <trivial / small / medium / large>
-->
