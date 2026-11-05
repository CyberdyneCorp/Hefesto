# Build and Packaging

## Purpose
Covers how Magic is configured, built, installed and distributed:
- configuration with autoconf (`./configure` wraps `scripts/configure`) and make, with in-tree or out-of-tree builds
- the Tcl build (the default) and the non-Tcl "standard" build
- packaging for Linux, macOS, AppImage (EL7–EL10) and WebAssembly/npm
- the GitHub Actions jobs that check all of this

## Requirements

### Requirement: Configure options
`./configure` SHALL run `scripts/configure --srcdir=<abs>`, with `CFLAGS` defaulting to `-g`. It SHALL support the options below (src: configure:16-17, scripts/configure.in):

| Option | Effect |
|---|---|
| `--with-tcl[=DIR\|no]`, `--with-tk` | Tcl/Tk build; Tcl without Tk sets `MAGIC_NO_TK` |
| `--with-interpreter=tcl\|scheme\|no` | choose the command interpreter |
| `--with-opengl`, `--with-cairo` | graphics backends; both are disabled when X11 is absent |
| `--disable-calma/cif/plot/lef/route` | drop the GDS, CIF, plot, LEF/DEF or routing modules |
| `--disable-compression` | no zlib (no compressed GDS or .mag) |
| `--disable-readline`, `--disable-threads`, `--disable-rsim` | these are forced off when Tcl is enabled |
| `--disable-locking` | no file locking |
| `--enable-modular` | ext2spice/ext2sim become Tcl auto-load modules |
| `--with-openaccess=DIR` | OpenAccess support |
| `--enable-debug`, `--enable-assertions`, `--enable-memdebug` | debugging aids |
| `--disable-ccache` | do not use ccache |
| `--disable-magic-builddate` | omit the build date |
| `--with-distdir=DIR` | install into a staging directory |

#### Scenario: Default configure
- **GIVEN** Tcl/Tk development files are installed
- **WHEN** `./configure && make prepare && make` is run
- **THEN** the Tcl build SHALL be produced (`ALL_TARGET=tcl`, `-DMAGIC_WRAPPER`) (src: scripts/configure.in:1523-1538)

#### Scenario: Standard build
- **WHEN** `./configure --without-tcl` is run
- **THEN** a native `magic/magic` binary SHALL be built, using system GNU readline unless `--disable-readline` is given

### Requirement: Build products
The Tcl build SHALL produce (src: magic/Makefile.in:77-87, tcltk/Makefile.in:46,69-96):
- `magic/tclmagic<SHDLIB_EXT>` (`.so`, or `.dylib` on macOS)
- `tcltk/magicexec`
- `tcltk/magicdnull`
- the scripts `magic.tcl`, `magic.sh`, `ext2spice.sh` and `ext2sim.sh`
- `proto.magicrc`

The per-module Tcl libraries (`magiclef`, `tclplot`, `tclroute`, `exttospice`, `exttosim`) SHALL be built only by an explicit `make -C <module> tcl-main`.

#### Scenario: Optional module library
- **WHEN** `make -C ext2spice tcl-main` is run after a Tcl build
- **THEN** `ext2spice/exttospice<SHDLIB_EXT>` SHALL be built

### Requirement: Version stamping
The version SHALL come from `VERSION`: major.minor is the first two fields and the revision is the third. Version, revision, git commit and build date (which honors `SOURCE_DATE_EPOCH`) SHALL be compiled only into `utils/buildinfo.o`, so that rebuilds stay incremental. (src: scripts/defs.mak.in:126-160,191-194, utils/Makefile.in:27-31)

#### Scenario: version command
- **GIVEN** `VERSION` contains `8.3.694`
- **WHEN** the `version` command is executed
- **THEN** the system SHALL print "Version 8.3 revision 694" followed by the compile date (src: windows/windCmdSZ.c:761-773)

### Requirement: Install layout
`make install` SHALL install the following (src: scripts/defs.mak.in:65-79, scmos/Makefile.in:25-64):
- into `BINDIR`: `magic`, `ext2spice`, `ext2sim`, `spice2sim` and `net2ir`
- into `${libdir}/magic/sys`: the system `.magicrc`/`.initrc`, technology files (`minimum`, `gdsquery`, `scmos`, `scmos-tm`, `scmos-sub`, `scmosWR`, `nmos`), display styles, color maps, glyphs and fonts
- into `${libdir}/magic/tcl`: the Tcl library and scripts

`DESTDIR` SHALL be honored.

#### Scenario: Staged install
- **WHEN** `make install DESTDIR=/tmp/stage` is run
- **THEN** all files SHALL be placed under `/tmp/stage` with the configured prefix

### Requirement: Build-tree launchers
`configure` SHALL generate `run_magic.sh`, `run_magicnull.sh`, `run_magic_gdb.sh` and `run_magic_valgrind.sh`. These SHALL run the uninstalled build against a symlinked `CAD_ROOT` tree at `$builddir/CAD_DIR`. (src: scripts/magic_run_common.sh.in:55-156)

#### Scenario: Run without installing
- **WHEN** `./run_magicnull.sh` is run after `make`
- **THEN** `magicdnull -dnull -nowrapper` SHALL start using the build-tree system files

### Requirement: WebAssembly package
`npm/build.sh --variant=tcl|notcl|both` SHALL build Magic with emscripten. The build SHALL have no X11, Cairo, OpenGL, Tk, readline or compression, and SHALL embed `scmos` and the glyphs at `/magic/sys/current`.

The npm package `magic-vlsi-wasm` SHALL export `.` (notcl), `./tcl` and `./notcl`, and SHALL require Node >= 18. At initialization it SHALL use the arguments `-d null -T minimum`, and SHALL expose `magic_wasm_run_command`, `magic_wasm_source_file` and `magic_wasm_update`. (src: npm/build.sh:115-211, npm/package.json, magic/magicWasm.c:137-280)

#### Scenario: Run a command in WASM
- **WHEN** a JS client calls the run-command entry point with `version`
- **THEN** the version text SHALL be returned without a display

### Requirement: AppImage distribution
The workflows `appimage7.yml` through `appimage10.yml` SHALL build `Magic-<ver>~<date>~<sha>-x86_64-ELn.AppImage` for EL7, EL8, EL9 and EL10 in Docker. EL7 and EL8 SHALL bundle Tcl 8.6.18; EL9 and EL10 SHALL bundle Tcl 9.0.4. The images SHALL be validated with `make validate`, and SHALL be uploaded as release assets on tags. `AppRun` SHALL set `CAD_ROOT` to the bundled `lib` directory. (src: .github/workflows/appimage*.yml, appimage/rsc/AppRun)

#### Scenario: Tag push
- **WHEN** a version tag is pushed
- **THEN** four AppImages SHALL be attached to the GitHub release

### Requirement: Continuous integration
The repository SHALL run the workflows below:

| Workflow | Triggers | What it checks |
|---|---|---|
| `main.yml` | push and PR | Linux Tcl build, the Tcl module libraries, and a non-Tcl build |
| `main-aarch64.yml` | push and PR | ARM Linux build |
| `main-macos.yml` | push and PR | macOS Intel and ARM builds, install, smoke run |
| `main-wasm.yml` | push and PR | WASM build, tests, and publishing on version tags |
| `canary-matrix.yml` | tags | compiler (gcc-10..16, clang-14..22) × dependency-set matrix |
| `appimage*.yml` | tags | AppImage builds |
| `regression.yml` | PR | the external `RTimothyEdwards/magic_regression_tests` workflow |
| `openspec-validate.yaml` | push and PR | `openspec validate --all --strict` |

#### Scenario: Pull request checks
- **WHEN** a pull request is opened
- **THEN** the build workflows, the regression workflow and OpenSpec validation SHALL run as checks
