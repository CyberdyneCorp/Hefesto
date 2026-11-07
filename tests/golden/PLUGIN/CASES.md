# Golden cases — PLUGIN area

The PLUGIN capabilities are `native`: Magic has no plugin API, so there is no Magic reference output. The expected results come from:
- the spec text
- the SDK sample plugins (capability `plugin-sdk`)
- `hefesto-plugin-test`, run against fixtures

Where a case touches layout, the layout output is compared with `.mag` files that headless Magic `fc5daa9e` produces from the same command sequence without the plugin, using built-in commands only.

Abbreviations:
- **min tech**: the minimal test technology used by other areas
- **viafix**: a test plugin (`org.example.viafix`, prefix `viafix`) with a mutating command `stitch`, a check and a panel
- **hello-c / hello-cpp / panel-demo / rect-csv-io / min-area-check / density-overlay / em-rules-adapter / posted-mutation**: the SDK samples
- **H(x.y)**: a test host built at plugin API version x.y

| ID | capability | input description (tech, layout, commands) | what to capture | requirement(s) covered |
|---|---|---|---|---|
| PLUGIN-ABI-01 | plugin-api | `hello-c` built as C11, loaded by H(1.0) on linux-x86_64 and macos-arm64 | exported symbol list (only `hf_plugin_entry`); command result of `hello::greet -name ada` | Stable C ABI; Plugin commands in the command API |
| PLUGIN-ABI-02 | plugin-api | Plugin built against 1.0 whose `hf_check_desc.struct_size` = 48, loaded by H(1.3) | the field values the host reads; `max_markers` defaults to 0 | Stable C ABI |
| PLUGIN-ABI-03 | plugin-api | Two plugins: GCC 13 + libstdc++ and Clang 18 + libc++, both API 1.2, in one Linux host | both load and run; no symbol clashes (`nm -D`) | Header-only C++20 wrapper |
| PLUGIN-ABI-04 | plugin-api | `hello-cpp` command that throws `std::runtime_error("boom")` | status `HF_E_INTERNAL`; diagnostic text `boom`; host alive | Stable C ABI; Header-only C++20 wrapper |
| PLUGIN-VER-01 | plugin-api | Manifest `api` {major 2, minorMin 5}, H(2.3) | refusal `PLUGIN-LOAD-001` and its exact text | API versioning |
| PLUGIN-VER-02 | plugin-api | Manifest `api` {major 2, minorMin 4}, host at 3.0 that supports 2.7 | loads through the major-2 tables; command works | API versioning |
| PLUGIN-VER-03 | plugin-api | Plugin calling deprecated `hf_layout_get_paint` 10 000 times | count of `PLUGIN-DEPR-001` diagnostics (= 1) | Deprecation policy |
| PLUGIN-LIFE-01 | plugin-api | Plugins A and B (B depends on A); A's `init` returns `HF_E_INTERNAL` | states A=`failed`, B skipped with `PLUGIN-LOAD-004`; main window opens | Plugin lifecycle |
| PLUGIN-LIFE-02 | plugin-api | viafix active; disable at runtime; run `viafix::stitch` | "unknown command" result; panels closed; library still mapped (`/proc/self/maps` or `vmmap`) | Plugin lifecycle |
| PLUGIN-LIFE-03 | plugin-api | Start with `--safe-mode` and viafix installed | no third-party plugin loaded | Plugin lifecycle |
| PLUGIN-CMD-01 | plugin-api | min tech, cell `inv`; `viafix::stitch` issues 40 `paint` commands; then `undo` once | `.mag` after undo equals the `.mag` before stitch | Plugin commands in the command API |
| PLUGIN-CMD-02 | plugin-api | `viafix::stitch` paints 3 rectangles, then fails | `.mag` byte-identical to before; script error with the plugin's diagnostic | Plugin commands in the command API |
| PLUGIN-CMD-03 | plugin-api | `viafix::stitch -pitch abc` (pitch declared `double`) | `PLUGIN-CMD-002`; handler call counter = 0 | Plugin commands in the command API |
| PLUGIN-CMD-04 | plugin-api | Plugin with prefix `magic` or `box` | refused at install; diagnostic naming the prefix | Plugin commands in the command API |
| PLUGIN-DB-01 | plugin-api | Background check holds a snapshot of `inv`; paint metal1 on the command thread | the check's rectangle list equals the pre-edit paint | Layout access |
| PLUGIN-DB-02 | plugin-api; plugin-packaging-and-distribution | Plugin sets property `viafix:pitch 0.34` on `inv`; save; open in a host without viafix; edit; save | `string viafix:pitch 0.34` line present in both saves | Layout access; Preservation of plugin data |
| PLUGIN-UI-01 | plugin-api | `panel-demo` on macOS and in the iPadOS simulator; press the button | command-log line identical on both | Panels and views |
| PLUGIN-IO-01 | plugin-api | `rect-csv-io` import of a 3-row CSV into min tech, export back | `.mag` equals Magic's result for the same three `paint` commands; exported CSV equals the input | Importers and exporters |
| PLUGIN-IO-02 | plugin-api | Plugin exporter claiming `.gds`; `save top.gds` | built-in GDS writer used (output equals the built-in golden) | Importers and exporters |
| PLUGIN-IO-03 | plugin-sdk | `hefesto-plugin-test --fuzz-importer example.rect-csv` for 10 min | no crash or sanitizer report; corpus statistics | Test harness; Importers and exporters |
| PLUGIN-CHK-01 | plugin-api | `min-area-check` on a layout with 3 small metal1 islands, run twice | diagnostics JSONL, identical both times, sorted by (cell, y, x, code) | Checks |
| PLUGIN-CHK-02 | plugin-api | Edit during a running check | cancellation observed; partial results discarded; restart on a new snapshot | Checks |
| PLUGIN-OVL-01 | plugin-api | `density-overlay` on `top`; save `top` | `.mag` byte-identical with the overlay on and off | Overlays |
| PLUGIN-PDK-01 | plugin-api | `em-rules-adapter` supplies EM rules for sky130A (whose bundled file is `unavailable`); run `emcheck` | rule source `plugin:<id>@<ver>` in `run.json` | PDK adapters |
| PLUGIN-THR-01 | plugin-api | `posted-mutation`: a worker calls `run_command`, then `post_command` | `HF_E_WRONG_THREAD` for the first; the second runs as its own undo group and is logged | Threading rules |
| PLUGIN-THR-02 | plugin-api | Command handler sleeping 3 s on the command thread | `PLUGIN-PERF-001` warning | Threading rules |
| PLUGIN-THR-03 | plugin-api | A plugin command recursing through `run_command` 17 levels deep | `HF_E_RECURSION` at depth 17 | Threading rules |
| PLUGIN-FLT-01 | plugin-api | Native plugin dereferencing NULL in `viafix::stitch`; relaunch | prompt naming `org.example.viafix` 1.2.0; crash report contains plugin id and version | Fault handling |
| PLUGIN-FLT-02 | plugin-api | Plugin returning `HF_E_INTERNAL` from 5 consecutive callbacks | plugin disabled; `PLUGIN-FAULT-001` | Fault handling |
| PLUGIN-SCR-01 | plugin-api | Script plugin `hello` with `register_command greet` on Linux, macOS and iPadOS simulator | result `hi ada` on all three | Script plugins |
| PLUGIN-CLI-01 | plugin-api | `hefesto-cli --plugin org.example.viafix -T min -c "load top; viafix::stitch; save top; quit"` | exit code 0; `top.mag` | Plugins in headless runs |
| PLUGIN-PKG-01 | plugin-packaging-and-distribution | `.hfplugin` with entry `../../.zshrc` | `PLUGIN-PKG-002`; no file outside staging | Package format |
| PLUGIN-PKG-02 | plugin-packaging-and-distribution | Manifests with bad `prefix`, missing `license`, `schema` 2, an unknown key | `PLUGIN-MAN-002` (key named), `PLUGIN-MAN-002`, `PLUGIN-MAN-001`, a warning | Manifest format |
| PLUGIN-SIG-01 | plugin-packaging-and-distribution | Signed package; flip one byte of the `.so` after install; relaunch | not loaded; `PLUGIN-SIG-003` naming the file | Package signatures |
| PLUGIN-SIG-02 | plugin-packaging-and-distribution | `SIGNATURE` canonical form: fixed test key and files | the exact canonical byte string and the signature bytes (test vector) | Package signatures |
| PLUGIN-DEP-01 | plugin-packaging-and-distribution | geomkit 2.0.0/2.3.1/3.0.0 installed; A `^2.0.0`, B `>=2.1.0 <3.0.0` | active geomkit = 2.3.1; load order | Dependency and version compatibility |
| PLUGIN-DEP-02 | plugin-packaging-and-distribution | A `^2.0.0`, B `^3.0.0` on geomkit; cycle A→B→A | `PLUGIN-DEP-001`; `PLUGIN-DEP-003` with the cycle | Dependency and version compatibility |
| PLUGIN-DISC-01 | plugin-packaging-and-distribution | Project folder containing `plugins/org.evil.x/1.0.0/plugin.json` | not discovered | Discovery locations |
| PLUGIN-INST-01 | plugin-packaging-and-distribution | Install, decline consent | user root has no `org.example.viafix` directory | Installation |
| PLUGIN-CAT-01 | plugin-packaging-and-distribution | Local catalog `index.json` + signature; entry with a wrong sha256 | `PLUGIN-CAT-002`; nothing installed | Plugin catalog |
| PLUGIN-UPD-01 | plugin-packaging-and-distribution | Auto-update on; 1.3.0 adds `network:api.example.org` | not installed until consent | Updates |
| PLUGIN-UPD-02 | plugin-packaging-and-distribution | 1.3.0 whose `init` fails | rollback to 1.2.0; user message | Updates |
| PLUGIN-PRJ-01 | plugin-packaging-and-distribution | Project requiring `org.example.viafix ^1.0.0`, not installed | warning offering installation; layout editable | Project plugin requirements |
| PLUGIN-CLI-02 | plugin-packaging-and-distribution | `hefesto-cli plugin install viafix.hfplugin` without `--yes-permissions` | non-zero exit; `PLUGIN-SEC-004` listing permissions | Command-line management |
| PLUGIN-SEC-01 | plugin-security-and-platforms | Script plugin with only `layout.read` runs `run_command "paint m1"` | Tcl error; `PLUGIN-SEC-001` naming `layout.write`; layout unchanged | Permissions |
| PLUGIN-SEC-02 | plugin-security-and-platforms | `http_request` to an ungranted host; plain `http://` to a granted host | `HF_E_PERMISSION` in both; no socket opened (checked with a local proxy) | Permissions; Plugin network and privacy rules |
| PLUGIN-SEC-03 | plugin-security-and-platforms | Update signed by a new key, same permissions | consent sheet shown again with "signing key changed" | Consent UI |
| PLUGIN-SEC-04 | plugin-security-and-platforms | Script plugin `while 1 {}` | fails after 10 s with `plugin time limit exceeded`; UI responsive | Script plugin sandbox |
| PLUGIN-SEC-05 | plugin-security-and-platforms | Unsigned native package with developer mode off, then on | `PLUGIN-SIG-001`; then installs with an `unsigned` consent sheet and a status-bar indicator | Native plugin hardening on desktop; Developer mode |
| PLUGIN-SEC-06 | plugin-security-and-platforms | Native plugin loaded with `LD_LIBRARY_PATH` / `DYLD_LIBRARY_PATH` pointing at a planted `libgeom.so` | the planted library is not loaded | Native plugin hardening on desktop |
| PLUGIN-PLAT-01 | plugin-security-and-platforms | iPadOS simulator: open a native `.hfplugin` from Files | `PLUGIN-PLAT-001`; nothing under `bin/` extracted | No runtime code loading on iPadOS |
| PLUGIN-PLAT-02 | plugin-security-and-platforms | iPadOS simulator: project requiring a native plugin | "not available on iPad (native code)"; plugin data preserved after save | Platform matrix |
| PLUGIN-PLAT-03 | plugin-security-and-platforms | iPadOS curated-mode build: import a script `.hfplugin` | refused with the "bundled plugins only" message | Script plugins on iPadOS under App Review |
| PLUGIN-PLAT-04 | plugin-security-and-platforms | Two built-in plugins statically linked in the iPadOS build | both registered through the static table; no clash between the renamed entry symbols | No runtime code loading on iPadOS |
| PLUGIN-SDK-01 | plugin-sdk | Out-of-tree CMake project using only the SDK | builds; `nm -D` shows only `hf_plugin_entry` | SDK contents and license |
| PLUGIN-SDK-02 | plugin-sdk | `plugin.h` compiled as C11 and C++20 with `-Wall -Wextra -pedantic` on GCC, Clang and Apple Clang | zero warnings | Header guarantees |
| PLUGIN-SDK-03 | plugin-sdk | Header change inserting a field mid-struct | ABI check fails, naming the struct and offset | Header guarantees |
| PLUGIN-SDK-04 | plugin-sdk | `hefesto-plugin pack` twice on the same directory | byte-identical outputs | Packaging tool |
| PLUGIN-SDK-05 | plugin-sdk | Conformance suite on a plugin declaring `layout.read` that paints | item 3 fails, naming `layout.write` | Conformance suite |
| PLUGIN-SDK-06 | plugin-sdk | SDK 1.0 `hello-cpp` built with SDK 1.4 | builds with at most deprecation warnings; tests pass on H(1.4) | SDK and host version compatibility |
| PLUGIN-SDK-07 | plugin-sdk | All samples on Linux and macOS; script samples on the iPadOS simulator | harness JUnit results | Sample plugins |
