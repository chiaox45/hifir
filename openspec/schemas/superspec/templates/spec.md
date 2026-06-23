<!--
Delta spec template for a change.

This template demonstrates 4 delta sections — use as needed:
- ADDED / MODIFIED / REMOVED / RENAMED
File location: openspec/changes/<change-name>/specs/<capability>/spec.md
(`<capability>` matches openspec/specs/<capability>/ directory name)

Format hard rules (OpenSpec validates):
- Requirement sentences MUST contain `SHALL` or `MUST`
- Each Requirement MUST have at least one `#### Scenario:` (exactly 4 hashtags)
- Scenario MUST use level-4 (`####`), level-3 or bullets will silent fail

BDD Scenario Format (GIVEN/WHEN/THEN):
- **GIVEN** <precondition / initial state>
- **WHEN** <action / trigger>
- **THEN** <expected outcome / observable result>
-->

## ADDED Requirements

<!-- New behavior. List new Requirements to add to this capability. -->

### Requirement: <!-- requirement name -->
<!-- requirement text — must contain SHALL or MUST -->

#### Scenario: <!-- scenario name -->
- **GIVEN** <!-- precondition -->
- **WHEN** <!-- action / trigger -->
- **THEN** <!-- expected outcome -->

---

## MODIFIED Requirements

<!--
Modified existing Requirement. MUST use the EXACT same normalized header
as openspec/specs/<capability>/spec.md (case-sensitive after trim),
otherwise archive delta apply will fail to find the matching requirement.

MUST include the FULL updated content (not just a diff), because OpenSpec
archive applies MODIFIED via full-text replacement.
-->

### Requirement: <!-- same header as existing spec -->
<!-- full updated requirement text — must contain SHALL or MUST -->

#### Scenario: <!-- scenario name (can add new or modify existing) -->
- **GIVEN** <!-- precondition -->
- **WHEN** <!-- action / trigger -->
- **THEN** <!-- expected outcome -->

---

## REMOVED Requirements

<!--
Remove existing Requirement. MUST include Reason and Migration
so reviewers understand why it's being removed and how dependents should migrate.
-->

### Requirement: <!-- header to remove, exact match with existing spec -->

**Reason**: <!-- why remove -->

**Migration**: <!-- how callers/dependents should adjust -->

---

## RENAMED Requirements

<!--
Rename a Requirement header. Fixed format: FROM/TO with code-fence header.
If name change + content change, list the rename in RENAMED AND provide
full updated content in MODIFIED with the NEW header name.

Archive apply order: RENAMED → REMOVED → MODIFIED → ADDED
-->

- FROM: `### Requirement: <Old Name>`
- TO: `### Requirement: <New Name>`
