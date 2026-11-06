# Hefesto Collaboration and Diff

## Purpose
Implements decision P-8. Hefesto provides:
- structural and visual layout diff
- a workflow that works with git: deterministic text formats and a merge helper
- document sync through iCloud and the Files app

Determinism comes from capability `hefesto-output-determinism`. Text formats come from capabilities `cell-file-format` and `hefesto-settings`.

## Requirements

### Requirement: Structural layout diff
Hefesto SHALL compare two versions of a cell hierarchy and report the differences in these categories:
- **Paint:** per layer, the added and removed areas, computed as geometric XOR. Areas SHALL be compared geometrically, not as tile lists.
- **Instances:** added, removed, moved or retransformed instances, and changed array parameters, keyed by instance id.
- **Labels and ports:** added, removed or changed labels and ports.
- **Properties:** changed properties.
- **Cells:** added and removed cells.

The diff SHALL be available as a command that returns structured output, and as a JSON report.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-8)

#### Scenario: Moved instance
- **WHEN** instance `inv_0` is moved by 1 µm between two revisions
- **THEN** the diff SHALL report a transform change for `inv_0` and no paint differences in the parent

### Requirement: Visual diff
The canvas SHALL be able to overlay a diff. The overlay SHALL draw added geometry, removed geometry and changed instances in distinct highlight styles, and SHALL support stepping through the differences. On iPad, a swipe SHALL toggle between the two revisions.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-8)

#### Scenario: Step through changes
- **WHEN** the user presses "next difference"
- **THEN** the view SHALL zoom to the next changed region and highlight it

### Requirement: Git-friendly workflow
Hefesto SHALL meet these requirements so that it works well with git:
- **Deterministic text:** `.mag`, `hefesto.project` and settings files SHALL be deterministic text with stable ordering, so that re-saving unchanged data produces no diff.
- **Diff driver:** Hefesto SHALL ship a `git` diff driver (`hefesto-cli diff`) that produces the structural diff for `.mag` files.
- **Merge helper:** Hefesto SHALL ship a three-way merge helper. It SHALL merge non-overlapping paint, instance and label changes automatically, and SHALL report overlapping edits as conflicts.

On desktop the git integration SHALL use the user's git installation. On iPadOS, git operations are out of scope. Collaboration on iPad uses document sync instead.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-4)

#### Scenario: No-op save
- **WHEN** a cell is opened and saved without edits
- **THEN** `git diff` SHALL show no changes

#### Scenario: Automatic merge
- **GIVEN** two branches that paint metal1 in disjoint regions of the same cell
- **WHEN** the merge helper runs
- **THEN** the merged cell SHALL contain both paints and no conflict

### Requirement: Document sync
Projects and cells SHALL be storable in iCloud Drive or another Files provider, and SHALL sync between devices. Hefesto SHALL coordinate file access during sync. On a concurrent edit conflict, Hefesto SHALL keep both versions and SHALL offer the structural diff to resolve the conflict.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-8)

#### Scenario: Conflict on sync
- **WHEN** the same cell was edited offline on a Mac and on an iPad
- **THEN** after sync, Hefesto SHALL present both versions and open the visual diff between them
