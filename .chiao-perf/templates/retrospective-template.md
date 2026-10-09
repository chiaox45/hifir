<!--
Issue-first perf verification-and-retrospective template. Shapes the single marked verification comment upserted on the parent at final verification — one per campaign, never a local file. Self-contained, links over pastes; no secrets, tokens, personal data, or raw benchmark dumps. A zero-code closure (no accepted experiments, no PR) uses the same shape with the delivery lines marked "none — evidence-only closure".
-->

## Verification

- **Campaign:** <parent Issue reference and title>
- **Contract:** freeze record revision <n>, body SHA `<sha>` re-verified; protocol source `PERF.md @ <rev>`
- **Verified campaign-branch head:** `<sha>` — the final PR delivers exactly this tree (or: zero-code closure, no PR)
- **Reference baseline → final:** <per Primary scenario: frozen baseline vs final measured result, with run conditions>
- **Accepted experiments:** <E### list with commit references and measurement evidence links>
- **Correctness:** <Final oracle results>
- **Guardrails:** <per Guardrail scenario: budget vs measured>
- **Comparability:** <environment/protocol continuity across the recorded history; re-baselining references if any>

## Retrospective

- **Final causal model:** <the bottleneck structure the evidence converged on>
- **Stop verdict:** <terminal verdict + supporting evidence links>
- **Falsified / saturated directions:** <D### references — retained negative knowledge>
- **Remaining limitations:** <what is still unknown or unexplored>
- **Promote candidates:** <each reusable finding with its resolution: landed in PERF.md/repository docs on the campaign branch, filed as a perf:promote follow-up Issue, or declined with reason>
