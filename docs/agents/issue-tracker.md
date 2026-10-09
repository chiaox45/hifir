# Issue Tracker Configuration

## Tracker

- Backend: GitHub (selected explicitly; the `gitcode` remote is a mirror, `origin`/`hifirworks` upstream is read-only for this credential)
- Repository: `chiaox45/hifir` (fork carrying the campaign; local remote name: `github`)
- Default branch: `main`

## Project

- Project: `hifir PERF` (owner `chiaox45`, project number 9 — resolve live IDs from GitHub at operation time)
- Phase field: `Phase` (single-select)
- Semantic phase mapping:
  - `draft` → Phase option "draft"
  - `review` → Phase option "review"
  - `frozen` → Phase option "frozen"
  - `implementing` → Phase option "implementing" (covers optimization work)
  - `verified` → Phase option "verified"

## Labels

- `workflow:perf` — campaign provenance (parent + direction children); provenance, not authority
- `perf:promote` — knowledge-promotion follow-up Issues

## Trust

- Trust mode: `process-authority` — the agent operates under the human's GitHub credential (`chiaox45`), so record authorship is audit provenance only; acceptance references point at the interactive cold stop, which carries the human authority.
- Authorized human actors: `chiaox45` (repository owner). An agent account is never an authorized actor.
- Human acceptance is required for: contract freeze, material re-baselining, parent closure.

## Assets

- `.chiao-perf/templates/` — campaign, direction, retrospective (user-owned, skip-if-exists)
- `.chiao-perf/scripts/` — `idd-hash`, `parse-records`, `upsert-comment` (framework-owned, checksum manifest)
