<!--
Issue-first performance campaign template. Composes the GitHub parent Issue body — never a local file. The parent Issue is the campaign identity; its body holds the frozen measurement Contract only — objective, scenarios, reference baseline, correctness acceptance, measurement protocol, boundaries, stop criteria. No experimental model, hypothesis, or implementation plan is frozen here. The freeze record, round records, model projection, experiment events (when no direction child exists), and the verification comment live in Issue comments. The frozen body is canonical: every later SHA check runs against it.
-->

# <campaign title> — Performance Contract

## Objective

<!-- Primary metric, target, and optimization scope. -->

## Primary Scenarios

<!-- Workloads whose performance must improve; each with its baseline anchor and target. -->

## Guardrail Scenarios

<!-- Workloads that need not improve but MUST stay within declared regression budgets. Select for workload importance and performance-regime coverage, not combinatorial input coverage. -->

## Reference Baseline

<!-- Per Primary scenario: baseline value, run-to-run spread, noise floor. Guardrail baselines. The code revision the baseline was measured on — the anchor for restart and re-baselining. -->

## Correctness Acceptance

<!-- Required Fast/Local/Final oracles; what must pass before KEEP and before delivery. -->

## Measurement Protocol

<!-- Protocol source: PERF.md @ <git-revision>. Resolved exact build / benchmark / aggregation commands. Environment and isolation requirements. Warmup/repetition rules. Noise-aware acceptance rule. -->

## Boundaries

<!-- Allowed change surface, engineering budget, risk constraints. -->

## Acceptance / Stop

<!-- Success criteria and permitted terminal verdicts: TARGET-REACHED, HEADROOM-EXHAUSTED, BELOW-NOISE, ROI-STOP, REGRESSION-LIMIT, ARCHITECTURE-ESCALATION. -->
