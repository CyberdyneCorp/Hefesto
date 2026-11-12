# App Review inquiry: sandboxed Tcl on iPadOS (M4)

- **Status:** draft, **not submitted**. Ready for the user to send (design D20 of
  `openspec/changes/m4-scripting`, task 2.2). The agents cannot send it: it needs the Apple
  developer account.
- **Submitted:** _(date, by whom, channel)_
- **Answer:** _(date received, verbatim text or summary, case number)_
- **Consequence:** _(filled in with the answer: which iPadOS mode M14 builds — open PDK
  scripts, or curated packs only; see "What the answer decides")_

## Why we ask now

Decision X-3 (`docs/decisions/cross-cutting.md`) embeds a sandboxed Tcl interpreter on every
platform, iPadOS included, so that the open-source PDKs' own Magic scripts (startup files and
device generators) run unchanged. App Store Review Guideline 2.5.2 restricts apps from
downloading or executing code that introduces or changes features or functionality. Running PDK
scripts that the user imports is a grey area, so X-3 asks for an early answer. M4 does not wait
for it: the curated-pack fallback is already specified (capability `hefesto-scripting` "App Store
fallback for iPadOS", Q-SCRIPT-46, decision D-30) and the answer only picks the M14 mode.

## How to send it

1. Sign in to the Apple Developer site with the account that will publish Hefesto.
2. Use **Contact Us → App Review** and choose the topic for a question about the App Review
   Guidelines (a pre-submission question; no build is needed).
3. Paste the text below. Attach nothing else unless asked; offer a TestFlight build if they want
   to try it.
4. Record the date, the channel and the case number above. When the answer arrives, paste it
   above and tell INTEGRATION, which updates `docs/roadmap.md` (M14) and this file's status.

## Text to send

> **Subject:** Pre-submission question on Guideline 2.5.2: interpreted, sandboxed design-kit
> scripts in a chip-layout editor for iPad
>
> Hello App Review team,
>
> We are preparing Hefesto, a chip-layout (VLSI) editor for iPad, macOS and Linux. It edits
> integrated-circuit layouts and is file-compatible with the open-source editor Magic. We would
> like to confirm, before we build the iPad version, how Guideline 2.5.2 applies to one feature.
>
> **What the feature is.** Chip foundries publish "process design kits" (PDKs): technology data
> plus small scripts that set up the editor for that manufacturing process and draw standard
> devices (for example a transistor of a given width and length). The open-source PDKs we support
> (SkyWater sky130, GlobalFoundries gf180mcu, IHP sg13g2; all Apache-2.0) ship these scripts in the
> Tcl language. Hefesto contains a Tcl 8.6 interpreter, compiled statically into the app. A user
> installs a PDK (downloaded by the app from the PDK's public GitHub releases, or imported from the
> Files app) and opens a design that uses it; the PDK's scripts then run in that interpreter.
>
> **What the scripts can and cannot do.**
> - They are interpreted text only. No native code is ever downloaded or loaded: dynamic loading
>   is compiled out of the interpreter, and the commands `load`, `exec`, `socket`, `open |…`,
>   `exit` and unsafe child interpreters are removed. Any attempt fails with an error.
> - They can only call the editor's own layout commands (paint a rectangle, place a cell, set a
>   label, and similar) and pure computation. They cannot add screens, change the app's purpose,
>   reach the network, start processes or access other apps.
> - File access is limited to the open project's folder, the app's PDK store and the app's
>   temporary area, inside the app container. The environment the scripts see is synthesized by
>   the app.
> - Before a PDK's scripts run for the first time, the app asks the user for consent, naming the
>   PDK and its version; the user can decline (the design then opens read-only) or trust that
>   version.
> - The result is a layout document the user edits, much like a macro or a document template.
>
> **Our questions.**
> 1. Is running such user-installed, interpreted PDK scripts, under the restrictions above,
>    acceptable under Guideline 2.5.2?
> 2. If not, is it acceptable when the scripts are bundled in the app (or in PDK packs we review
>    and ship as in-app content) rather than imported by the user?
> 3. Is there anything you would want to see in the review notes or the app (wording of the
>    consent prompt, a settings switch) to make this clear to reviewers?
>
> If user-imported scripts are not acceptable, the iPad version will run only scripts from PDK
> packs that we curate and bundle, together with built-in native generators, and will refuse PDKs
> without bundled scripts.
>
> Thank you,
> _(name, team, developer account)_

## What the answer decides

| Answer | iPadOS mode in M14 | Spec |
|---|---|---|
| Question 1: yes | user-installed PDKs with consent, as on desktop | `hefesto-scripting` "Embedded sandboxed Tcl on all platforms" |
| Question 1: no, question 2: yes | curated-pack mode: bundled PDK scripts plus native generators; other PDKs refused | `hefesto-scripting` "App Store fallback for iPadOS"; `scripting-pdk-integration` "PDKs without bundled scripts in curated-pack mode" (`script::CuratedPackPolicy`, already implemented and unit-tested in M4) |
| Both no | native generators only; to be decided (Open Question in `docs/decisions/`) | — |

If the Mac app is also meant for the Mac App Store, add "and the Mac version" to the questions:
the same guideline applies there.
