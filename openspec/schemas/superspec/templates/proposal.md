## Why

<!--
Explain the motivation for this change. What problem does this solve? Why now?

Hard constraint: 50 <= chars <= 1000 (OpenSpec validates this).
- Too short: "Why section must be at least 50 characters"
- Too long: "Why section should not exceed 1000 characters"

Suggested structure: current pain, why now, expected benefit (1-2 sentences each)
-->

## What Changes

<!--
Describe what will change. Be specific about new capabilities, modifications, or removals.

For behavior changes with clear before/after, use From/To format:

**<Section or Behavior Name>**
- From: <current state / requirement>
- To: <future state / requirement>
- Reason: <why this change is needed>
- Impact: <breaking / non-breaking, who's affected>

Multiple changes: repeat this block. Pure additions or removals can use simple lists.
-->

## Capabilities

### New Capabilities
<!--
Capabilities being introduced. Replace <name> with kebab-case identifier.
Naming convention: use compound nouns (at least 2 words),
e.g. `user-auth`, `data-export`, `api-rate-limiting`.
Each creates specs/<name>/spec.md
-->
- `<name>`: <brief description of what this capability covers>

### Modified Capabilities
<!--
Existing capabilities whose REQUIREMENTS are changing (not just implementation).
Only list here if spec-level behavior changes. Each needs a delta spec file.
Use existing spec names from openspec/specs/. Leave empty if no requirement changes.
-->
- `<existing-name>`: <what requirement is changing>

## Impact

<!-- Affected code, APIs, dependencies, systems -->
