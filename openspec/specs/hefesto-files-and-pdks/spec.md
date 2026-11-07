# Hefesto Files and PDKs

## Purpose
Defines how Hefesto provides Magic's file-system behavior (cell search paths, PDK discovery, locking, crash backups) on all platforms. This includes the iPadOS app sandbox, which has no global file system, no environment variables and no `/tmp` shared between processes.

## Requirements

### Requirement: PDK store
Hefesto SHALL keep a PDK store of installed technologies on every platform. Each entry SHALL hold:
- tech files
- display styles
- startup data
- cell libraries

On Linux and macOS, the store SHALL also find PDKs by the same search rules as Magic: `PDK_ROOT`, `PDKPATH` and the installation paths (capabilities `cell-library-resolution` and `tech-loading-and-scaling`). On iPadOS, PDKs SHALL be installed into the app sandbox only through the PDK manager (capability `hefesto-pdk-manager`), by download or by importing an offline PDK pack, and SHALL then be resolved by name.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-4, docs/decisions/product.md#P-14)

#### Scenario: PDK auto-discovery on iPad
- **GIVEN** sky130A has been installed into the PDK store on iPad through the PDK manager
- **WHEN** a `.mag` file with `tech sky130A` is opened
- **THEN** the technology SHALL load automatically, as Magic would load it from `PDK_ROOT`

### Requirement: Document and library access
Cell files SHALL be opened and saved through each platform's document model. On iPadOS this means security-scoped access through the Files app, including access to whole folders, so that subcell paths resolve. Path resolution SHALL follow capability `cell-library-resolution`, applied within the folders the user has granted access to.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-4)

#### Scenario: Subcell outside granted folder
- **WHEN** a parent cell references a child outside the folders the user has granted access to
- **THEN** the system SHALL report the missing child, as Magic does for an unreadable file, and SHALL offer to grant access to that folder

### Requirement: Locking and recovery replacements
Where `fcntl` locks are unavailable or meaningless, as in the iPadOS sandbox, Hefesto SHALL use platform file coordination to prevent conflicting edits.

Hefesto SHALL replace Magic's `/tmp` crash backups with an autosave service on every platform. Autosave is the platform service; the file it writes is the crash backup. Autosave SHALL:
- trigger crash backups: on the periodic timer of capability `ui-gui-managers`, requirement "Crash backup timer"; on a termination request; and on iPadOS also when the app moves to the background;
- store them in the app-private crash backup directory, with a per-session lock file, as specified by capability `cell-io-safety`, requirement "Crash backup location and session lock" (Application Support on macOS and iPadOS, `$XDG_STATE_HOME/hefesto` on Linux; never a temporary or caches directory);
- keep the crash backup file format readable by Magic (capability `cell-io-safety`, requirement "Crash backup file format");
- keep the recovery semantics of capability `cell-io-safety`: modified cells only, an offer to recover on the next launch, and removal of the crash backup after recovery;
- on Linux and macOS, also offer Magic's `$TMPDIR/MAG*` crash backups for import (capability `cell-io-safety`, requirement "Import of Magic crash backups").

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-4, docs/decisions/audit/DECISIONS.md#D-8)

#### Scenario: Recovery after termination
- **GIVEN** the app was terminated with unsaved changes
- **WHEN** Hefesto is launched again
- **THEN** it SHALL offer to recover the modified cells

#### Scenario: Backup when the iPad app goes to the background
- **GIVEN** Hefesto on iPadOS with a modified editable cell
- **WHEN** the app moves to the background
- **THEN** autosave SHALL write a crash backup to the app's Application Support crash backup directory

#### Scenario: Magic backup offered on Linux
- **GIVEN** Hefesto on Linux and an abandoned Magic crash backup `/tmp/MAG1234.abcdef` owned by the user
- **WHEN** Hefesto is launched
- **THEN** it SHALL offer to recover the cells in `/tmp/MAG1234.abcdef`
