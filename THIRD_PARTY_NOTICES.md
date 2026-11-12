# Third-party notices

Hefesto's own source code is licensed under the Apache License 2.0 (see `LICENSE`, decision P-9).
Hefesto includes or links the third-party components below. Their copyright and permission
notices are reproduced here, as required by capability `hefesto-licensing`, requirement
"Attribution for permissive sources". This file ships in every package; the in-app
About → Acknowledgements screen shows the same notices.

Pins are in `cmake/Dependencies.cmake`; `scripts/check_third_party_notices.py` checks in CI
that every dependency declared there is listed here with its pin.

## Components in M0

| Component | Version / pin | License | Shipped in | Source |
|---|---|---|---|---|
| zlib | v1.3.1 | Zlib | all packages (core) | https://github.com/madler/zlib |
| nlohmann/json | v3.11.3 | MIT | all packages (core, header-only) | https://github.com/nlohmann/json |
| Raven | v0.2.2, commit `4a05d6f11c79e5a6eae77d7fe26f702f5fea4331` | Apache-2.0 | desktop (Linux, macOS) | https://github.com/oscar-soirey/Raven |
| ↳ stb_truetype 1.26, stb_image 2.30 (in Raven) | as bundled by Raven | MIT or Unlicense (public domain) | desktop | https://github.com/nothings/stb |
| ↳ nanosvg, nanosvgrast (in Raven) | as bundled by Raven | Zlib | desktop | https://github.com/memononen/nanosvg |
| ↳ Roboto Regular, subset (in Raven) | as bundled by Raven | Apache-2.0 | desktop | https://github.com/googlefonts/roboto |
| ↳ Roboto Mono Regular, subset (in Raven) | as bundled by Raven | OFL-1.1 | desktop | https://github.com/googlefonts/RobotoMono |
| ↳ Wayland protocol descriptions (in Raven) | as bundled by Raven | MIT | desktop (Linux) | https://gitlab.freedesktop.org/wayland/wayland-protocols |
| MoltenVK | pinned when the macOS package is built (M14 packaging) | Apache-2.0 | macOS package | https://github.com/KhronosGroup/MoltenVK |
| Catch2 | v3.7.1, commit `fa43b77429ba76c462b1898d6cd2f2d7a9416b14` | BSL-1.0 | **not shipped** (unit tests only) | https://github.com/catchorg/Catch2 |
| Vulkan-Headers | from the Vulkan SDK used to build | Apache-2.0 OR MIT | **not shipped** (build only) | https://github.com/KhronosGroup/Vulkan-Headers |

Raven's Wayland platform opens `libwayland-client`, `libxkbcommon`, `libwayland-cursor` and
`libdecor` (all MIT) from the system at run time; they are not bundled. The Vulkan loader is the
system's on Linux.

## Components in M1

| Component | Version / pin | License | Shipped in | Source |
|---|---|---|---|---|
| Zstandard (decoder only) | v1.5.6, release archive SHA-256 `8c29e06cf42aacc1eafc4077ae2ec6c6fcb96a626157e0593d5e82a34fd403c1` | BSD-3-Clause | all packages (core) | https://github.com/facebook/zstd |
| Magic VLSI (ported logic) | 8.3.694, commit `fc5daa9e2856b89a0a3056f296ded1fd4474e39e` | UC Berkeley permissive notice | all packages (core) | https://github.com/RTimothyEdwards/magic |
| Inter (UI font) | 4.1, archive SHA-256 `9883fdd4a49d4fb66bd8177ba6625ef9a64aa45899767dde3d36aa425756b11e` | OFL-1.1 | desktop | https://github.com/rsms/inter |
| JetBrains Mono (UI font) | 2.304, archive SHA-256 `6f6376c6ed2960ea8a963cd7387ec9d76e3f629125bc33d1fdcd7eb7012f7bbf` | OFL-1.1 | desktop | https://github.com/JetBrains/JetBrainsMono |
| libcurl (HTTPS client) | curl-8_23_0 (8.23.0), release archive SHA-256 `8dadd7d2536a314f982ad05a651b713f426bf7d02cc2c76cffb9796f2218e6c5` | curl (MIT-style) | Linux CLI and desktop | https://github.com/curl/curl |
| OpenSSL 3.x (TLS for libcurl) | the system's (`libssl`), linked dynamically, not bundled | Apache-2.0 | Linux, from the system | https://github.com/openssl/openssl |

Fonts are data files (`assets/fonts/`, unmodified, with their license texts beside them; see
`assets/fonts/README.md`). Magic's FreeSans, FreeSerif and FreeMono `.pt3` outline fonts are not
bundled: they are read at run time from a Magic installation (`$CAD_ROOT/magic/sys`). PDK data
(`.cache/pdks`) and Magic's display-style files used by tests (`build/ref/magic`) are fetched by
`scripts/fetch_pdk.py` and never committed or shipped.

## Components in M4

| Component | Version / pin | License | Shipped in | Source |
|---|---|---|---|---|
| Tcl | 8.6.16, release archive `tcl8.6.16-src.tar.gz`, SHA-256 `91cb8fa61771c63c262efb553059b7c7ad6757afa5857af6265e4b0bdc2a14a5` | Tcl/Tk license (BSD-style, TCL) | all packages (core) | https://downloads.sourceforge.net/project/tcl/Tcl/8.6.16/tcl8.6.16-src.tar.gz (https://www.tcl-lang.org) |
| ↳ LibTomMath (in Tcl) | as bundled by Tcl 8.6.16 | Unlicense (public domain) | all packages (core) | https://github.com/libtom/libtommath |

## libcurl

The Linux CLI and desktop download PDK catalogs and assets over HTTPS with libcurl
(`hefesto::curl`, static, HTTP and HTTPS only, `src/platform_net/src/curl_transport.cpp`; capability
`hefesto-licensing`, decision D-13). TLS comes from the system OpenSSL 3, which is not bundled.
macOS uses the system URLSession framework instead and contains no libcurl or OpenSSL.

```text
COPYRIGHT AND PERMISSION NOTICE

Copyright (c) 1996 - 2026, Daniel Stenberg, <daniel@haxx.se>, and many
contributors, see the THANKS file.

All rights reserved.

Permission to use, copy, modify, and distribute this software for any purpose
with or without fee is hereby granted, provided that the above copyright
notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS. IN
NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of a copyright holder shall not
be used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization of the copyright holder.
```

## Tcl

The embedded, sandboxed script interpreter (decision X-3; `cmake/Tcl.cmake`, target
`hefesto::tcl`; `openspec/changes/m4-scripting/design.md` D3, D5). Tcl's C sources are compiled
into the core without Tk and without dynamic loading, and Tcl's own `library/` scripts
(init.tcl, clock.tcl, msgcat, http, opt, platform, tcltest, msgs, tzdata, encodings) are compiled
into the binary. Its regular-expression engine (Henry Spencer's, as modified for Tcl) is covered
by the same terms. The bundled zlib copy is not used (Hefesto's zlib above is linked instead).

```text
This software is copyrighted by the Regents of the University of
California, Sun Microsystems, Inc., Scriptics Corporation, ActiveState
Corporation and other parties.  The following terms apply to all files
associated with the software unless explicitly disclaimed in
individual files.

The authors hereby grant permission to use, copy, modify, distribute,
and license this software and its documentation for any purpose, provided
that existing copyright notices are retained in all copies and that this
notice is included verbatim in any distributions. No written agreement,
license, or royalty fee is required for any of the authorized uses.
Modifications to this software may be copyrighted by their authors
and need not follow the licensing terms described here, provided that
the new terms are clearly indicated on the first page of each file where
they apply.

IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY
FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY
DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE
IS PROVIDED ON AN "AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE
NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
MODIFICATIONS.

GOVERNMENT USE: If you are acquiring this software on behalf of the
U.S. government, the Government shall have only "Restricted Rights"
in the software and related documentation as defined in the Federal
Acquisition Regulations (FARs) in Clause 52.227.19 (c) (2).  If you
are acquiring the software on behalf of the Department of Defense, the
software shall be classified as "Commercial Computer Software" and the
Government shall have only "Restricted Rights" as defined in Clause
252.227-7014 (b) (3) of DFARs.  Notwithstanding the foregoing, the
authors grant the U.S. Government and others acting in its behalf
permission to use and distribute the software in accordance with the
terms specified in this license.
```

LibTomMath, the arbitrary-precision integer library inside Tcl (`libtommath/`):

```text
                          The LibTom license

This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>
```

## Zstandard

The core's PDK manager decodes `.tar.zst` release assets with the Zstandard streaming decoder
(`hefesto::zstd`, built from the release sources' `lib/common` and `lib/decompress` only).

```text
BSD License

For Zstandard software

Copyright (c) Meta Platforms, Inc. and affiliates. All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

 * Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

 * Neither the name Facebook, nor Meta, nor the names of its contributors may
   be used to endorse or promote products derived from this software without
   specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

## Magic VLSI (ported logic)

Hefesto ports algorithms and file formats from Magic 8.3.694 (decision P-10, capability
`hefesto-licensing`, requirement "Attribution for permissive sources"). Every ported file names
the Magic sources it follows and keeps this notice by reference. In M1:

| Area | Hefesto files | Magic sources |
|---|---|---|
| technology files | `src/core/src/tech/` (reader, sections, types, contacts, compose, styles, loader, scaling, `tech`/`scalegrid`, built-in `minimum` technology) | `utils/tech.c`, `utils/path.c`, `database/DBtech*.c`, `database/DBtcontact.c`, `database/DBtpaint.c`, `database/DBtechtype.c`, `cif/CIFtech.c`, `commands/CmdTZ.c`, `scmos/minimum.tech.in` |
| layout database | `src/core/src/db/` (corner-stitched planes, paint and erase, split tiles, hierarchy, flattening, labels and ports, properties, commands) | `tiles/*.c`, `database/DBpaint.c`, `database/DBpaint2.c`, `database/DBcellsrch.c`, `database/DBcellname.c`, `database/DBcellsubr.c`, `database/DBlabel.c`, `database/DBprop.c`, `database/DBtiles.c`, `commands/Cmd*.c` |
| cell files | `src/core/src/cell/` (`.mag` reader and writer, elements, search paths, I/O safety, timestamps, crash backups and archives, `getcell`/`flatten`, commands), `src/platform_posix/` (file locks) | `database/DBio.c`, `database/DBtimestmp.c`, `database/DBcellname.c`, `utils/path.c`, `utils/lookup.c`, `utils/flock.c`, `commands/CmdAB.c`, `commands/CmdCD.c`, `commands/CmdFI.c`, `commands/CmdLQ.c`, `commands/CmdRS.c`, `tcltk/tools.tcl` |
| display styles and fonts | `src/core/src/display/` (`.cmap`, `.dstyle`, glyphs, outline fonts, scene order) | `graphics/grCMap.c`, `graphics/grDStyle.c`, `graphics/grGlyphs.c`, `database/DBlabel.c`, `dbwind/DBWdisplay.c` |
| windows and view commands | `src/core/src/view/` | `windows/windView.c`, `windows/windTrans.c`, `windows/windMove.c`, `windows/windCmdSZ.c`, `windows/windCmdAM.c`, `windows/windMain.c`, `dbwind/DBWprocs.c`, `dbwind/DBWdisplay.c` (DBWAreaChanged), `commands/CmdE.c`, `commands/CmdSubrs.c`, `commands/CmdWizard.c` |
| layout drawing | `src/render/vulkan/` (fill, stipple, outline and `cross` rules, label size cut-off), `src/shell/desktop/src/screen_paint.cpp` and `canvas_overlay.cpp` (box, cell names, label text sizes) | `graphics/grClip.c`, `graphics/grText.c`, `dbwind/DBWdisplay.c`, `dbwind/DBWtools.c` |
| shared helpers | `src/core/src/util/record_order.cpp` (natural name order), `src/core/src/cmd/coordinates.cpp` (coordinate units) | `database/DBcellname.c` (`strcmpbynum`), `commands/CmdSubrs.c` (`cmdScaleCoord`) |
| app startup | `src/core/src/app/` (startup order and options) | `utils/main.c`, `tcltk/tclmagic.c` |

In M2 (`openspec/changes/m2-interactive-editing`):

| Area | Hefesto files | Magic sources |
|---|---|---|
| keywords, coordinates, units | `src/core/src/cmd/{keywords,coordinates,units}.cpp` | `utils/lookup.c`, `utils/geometry.c` (`GeoNameToPos`), `commands/CmdSubrs.c`, `commands/CmdTZ.c` (`CmdUnits`), `dbwind/DBWprocs.c` (`DBWPrintValue`) |
| box, paint and editing commands | `src/core/src/db/` (`commands_box`, `commands_paint`, `commands_misc`, `layer_list`, `polygon`, `edit_util`) | `commands/CmdAB.c` (`CmdBox`), `commands/CmdCD.c` (`CmdCorner`, `CmdContact`, `CmdDrop`), `commands/CmdE.c`, `commands/CmdFI.c` (`CmdFill`), `commands/CmdLQ.c`, `commands/CmdTZ.c` (`CmdXor`), `dbwind/DBWtools.c`, `cif/CIFrdpoly.c`, `cif/CIFrdutils.c` |
| undo | `src/core/src/undo/` | `utils/undo.c`, `database/DBundo.c`, `windows/windCmdSZ.c` |
| selection and connectivity | `src/core/src/select/`, `src/core/src/connect/` | `select/sel*.c`, `database/DBconnect.c`, `commands/CmdRS.c`, `commands/CmdTZ.c`, `commands/CmdWizard.c` |
| hierarchy, labels and properties | `src/core/src/cell/` (`edit`, `down`, `xload`, placement, `flush`, `findlabel`, `goto`, `element`), `src/core/src/db/{labels,property,commands_labels,commands_setlabel}.cpp` | `commands/CmdCD.c`, `commands/CmdE.c`, `commands/CmdFI.c`, `commands/CmdLQ.c`, `commands/CmdRS.c`, `database/DBlabel.c`, `database/DBprop.c`, `dbwind/DBWelement.c` |
| dispatch, macros, tools and script helpers | `src/core/src/ui/`, `src/core/src/cmd/{registry,dispatcher}.cpp` | `textio/txCommands.c`, `textio/txInput.c`, `windows/windClient.c`, `windows/windSend.c`, `windows/windCmdAM.c`, `windows/windCmdSZ.c`, `utils/macros.c`, `dbwind/DBWbuttons.c`, `tcltk/tools.tcl`, `tcltk/wrapper.tcl`, `tcltk/toolkit.tcl` |
| highlights, feedback, color window, tile overlay | `src/core/src/view/{highlights,feedback,feedback_cmd,color_window,color_cmd,wizard_cmds}.cpp`, `src/render/vulkan/` (highlight batches) | `dbwind/DBWhlights.c`, `dbwind/DBWfdback.c`, `cmwind/CMW*.c`, `windows/windDebug.c`, `commands/CmdWizard.c`, `graphics/grMain.c` |
| wiring tool | `src/core/src/wiring/` | `wiring/wireTech.c`, `wiring/wireOps.c`, `wiring/wireUndo.c`, `commands/CmdTZ.c` (`CmdWire`), `cif/CIFrdpt.c` (`CIFPaintWirePath`), `select/selCreate.c` (`selFindChunk`), `tcltk/tools.tcl` |

In M3 (`openspec/changes/m3-drc`):

| Area | Hefesto files | Magic sources |
|---|---|---|
| DRC rule compiler, cifoutput catalog, `tech drc` | `src/core/src/drc/rules/`, `src/core/src/wiring/wiring.cpp` (default width) | `drc/DRCtech.c`, `drc/DRCcif.c`, `drc/DRCprint.c`, `cif/CIFtech.c` (style, scale and `squares` reading, `CIFGetContactSize`), `utils/tech.c`, `utils/fraction.c`, `utils/lookup.c`, `commands/CmdTZ.c` (`tech drc`) |
| DRC check kernel | `src/core/src/drc/check/` | `drc/DRCbasic.c`, `drc/DRCextend.c`, `utils/maxrect.c` |
| DRC engine (marking, squares, interactions, arrays, background checker) | `src/core/src/drc/engine/`, the `drc::check_this` calls in `src/core/src/{db,select,cell,undo,wiring}/`, `src/core/src/cmd/dispatcher.cpp` (background state per command) | `drc/DRCcontin.c`, `drc/DRCsubcell.c`, `drc/DRCarray.c`, `drc/DRCmain.c`, `database/DBcellsrch.c`, `database/DBcellbox.c`, `database/DBcellcopy.c`, `database/DBio.c`, `textio/txCommands.c` (`TxTclDispatch`) |
| `drc` command and reporting (including the option help text) | `src/core/src/drc/commands/` | `commands/CmdCD.c` (`CmdDrc`, `cmdDrcOption`), `drc/DRCmain.c`, `drc/DRCtech.c`, `dbwind/DBWprocs.c`, `dbwind/DBWtools.c`, `utils/lookup.c` |
| mask hints | `src/core/src/hints/`, the hint paths of `src/core/src/db/{property,flatten,commands_labels}.cpp` and `src/core/src/cell/mag_reader_records.cpp` | `database/DBprop.c`, `database/DBio.c`, `database/DBcellcopy.c`, `database/DBcellsrch.c`, `database/DBtiles.c`, `drc/DRCbasic.c` (exception scoping), `cif/CIFgen.c` (`CIFOP_MASKHINTS`), `cif/CIFrdcl.c` (`gds maskhints`) |

Magic's license (`LICENSE` at the pinned commit):

```text
Copyright (C) 1985, 1990 Regents of the University of California.

Permission to use, copy, modify, and distribute this
software and its documentation for any purpose and without
fee is hereby granted, provided that the above copyright
notice appear in all copies.  The University of California
makes no representations about the suitability of this
software for any purpose.  It is provided "as is" without
express or implied warranty.  Export of this software outside
of the United States of America may require an export license.
```

## Inter

UI font of the desktop shell (`hefesto-design-system` "Typography"). The full SIL Open Font License
1.1 text is `assets/fonts/Inter-OFL.txt`.

```text
Copyright (c) 2016 The Inter Project Authors (https://github.com/rsms/inter)

This Font Software is licensed under the SIL Open Font License, Version 1.1.
```

## JetBrains Mono

Monospace font of the desktop shell (coordinates, names, commands). The full SIL Open Font License
1.1 text is `assets/fonts/JetBrainsMono-OFL.txt`.

```text
Copyright 2020 The JetBrains Mono Project Authors (https://github.com/JetBrains/JetBrainsMono)

This Font Software is licensed under the SIL Open Font License, Version 1.1.
```

## zlib

Used by the core for deflate and gzip (`hefesto::zlib`, built from the release sources).

```text
Copyright notice:

 (C) 1995-2022 Jean-loup Gailly and Mark Adler

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Jean-loup Gailly        Mark Adler
  jloup@gzip.org          madler@alumni.caltech.edu
```

## nlohmann/json

Used by the core and the CLI for JSON reading and writing.

```text
MIT License 

Copyright (c) 2013-2022 Niels Lohmann

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## Raven

The desktop GUI library (decision P-11), used only by the desktop shell. Licensed under the
Apache License 2.0; the license text is the same as Hefesto's `LICENSE`. Raven's NOTICE file:

```text
Raven
Copyright 2026 Oscar Soirey

This product is licensed under the Apache License, Version 2.0 (see LICENSE).
It includes third-party components under their own licenses; see THIRD_PARTY.md.
```

Raven compiles the following components into the library. Raven marks its own change to
`stb_truetype.h` with `Raven:` and subsets the fonts; under their licenses these are modified
versions.

### stb_truetype and stb_image

```text
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2017 Sean Barrett
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
```

### nanosvg

```text
Copyright (c) 2013-14 Mikko Mononen memon@inside.org

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
claim that you wrote the original software. If you use this software
in a product, an acknowledgment in the product documentation would be
appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
```

### Roboto

Roboto Regular (Google), embedded as Raven's default font. Licensed under the Apache License 2.0;
the license text is the same as Hefesto's `LICENSE`.

Raven compiles Roboto and Roboto Mono into its code. That conflicts with `hefesto-licensing`
("Fonts SHALL be kept as separate data files"); the conflict is a recorded M0 deviation with a plan
to close it before the first distributed build (`docs/decisions/raven-go-no-go.md`, section 3).

### Roboto Mono

Roboto Mono Regular, embedded as Raven's fixed-width font.

```text
Copyright 2015 The Roboto Mono Project Authors (https://github.com/googlefonts/robotomono)

This Font Software is licensed under the SIL Open Font License, Version 1.1.
This license is copied below, and is also available with a FAQ at:
https://openfontlicense.org


-----------------------------------------------------------
SIL OPEN FONT LICENSE Version 1.1 - 26 February 2007
-----------------------------------------------------------

PREAMBLE
The goals of the Open Font License (OFL) are to stimulate worldwide
development of collaborative font projects, to support the font creation
efforts of academic and linguistic communities, and to provide a free and
open framework in which fonts may be shared and improved in partnership
with others.

The OFL allows the licensed fonts to be used, studied, modified and
redistributed freely as long as they are not sold by themselves. The
fonts, including any derivative works, can be bundled, embedded, 
redistributed and/or sold with any software provided that any reserved
names are not used by derivative works. The fonts and derivatives,
however, cannot be released under any other type of license. The
requirement for fonts to remain under this license does not apply
to any document created using the fonts or their derivatives.

DEFINITIONS
"Font Software" refers to the set of files released by the Copyright
Holder(s) under this license and clearly marked as such. This may
include source files, build scripts and documentation.

"Reserved Font Name" refers to any names specified as such after the
copyright statement(s).

"Original Version" refers to the collection of Font Software components as
distributed by the Copyright Holder(s).

"Modified Version" refers to any derivative made by adding to, deleting,
or substituting -- in part or in whole -- any of the components of the
Original Version, by changing formats or by porting the Font Software to a
new environment.

"Author" refers to any designer, engineer, programmer, technical
writer or other person who contributed to the Font Software.

PERMISSION & CONDITIONS
Permission is hereby granted, free of charge, to any person obtaining
a copy of the Font Software, to use, study, copy, merge, embed, modify,
redistribute, and sell modified and unmodified copies of the Font
Software, subject to the following conditions:

1) Neither the Font Software nor any of its individual components,
in Original or Modified Versions, may be sold by itself.

2) Original or Modified Versions of the Font Software may be bundled,
redistributed and/or sold with any software, provided that each copy
contains the above copyright notice and this license. These can be
included either as stand-alone text files, human-readable headers or
in the appropriate machine-readable metadata fields within text or
binary files as long as those fields can be easily viewed by the user.

3) No Modified Version of the Font Software may use the Reserved Font
Name(s) unless explicit written permission is granted by the corresponding
Copyright Holder. This restriction only applies to the primary font name as
presented to the users.

4) The name(s) of the Copyright Holder(s) or the Author(s) of the Font
Software shall not be used to promote, endorse or advertise any
Modified Version, except to acknowledge the contribution(s) of the
Copyright Holder(s) and the Author(s) or with their explicit written
permission.

5) The Font Software, modified or unmodified, in part or in whole,
must be distributed entirely under this license, and must not be
distributed under any other license. The requirement for fonts to
remain under this license does not apply to any document created
using the Font Software.

TERMINATION
This license becomes null and void if any of the above conditions are
not met.

DISCLAIMER
THE FONT SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO ANY WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT
OF COPYRIGHT, PATENT, TRADEMARK, OR OTHER RIGHT. IN NO EVENT SHALL THE
COPYRIGHT HOLDER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
INCLUDING ANY GENERAL, SPECIAL, INDIRECT, INCIDENTAL, OR CONSEQUENTIAL
DAMAGES, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF THE USE OR INABILITY TO USE THE FONT SOFTWARE OR FROM
OTHER DEALINGS IN THE FONT SOFTWARE.
```

### Wayland protocol descriptions

```text
wayland.xml comes from Wayland (https://gitlab.freedesktop.org/wayland/wayland):

Copyright © 2008-2012 Kristian Høgsberg
Copyright © 2010-2012 Intel Corporation
Copyright © 2011 Benjamin Franzke
Copyright © 2012 Collabora, Ltd.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice (including the next
paragraph) shall be included in all copies or substantial portions of the
Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

---

The above is the version of the MIT "Expat" License used by X.org:

    http://cgit.freedesktop.org/xorg/xserver/tree/COPYING

========================================================================

The other files come from wayland-protocols (https://gitlab.freedesktop.org/wayland/wayland-protocols).
Each also states its own copyright holders at its top.

Copyright © 2008-2013 Kristian Høgsberg
Copyright © 2010-2013 Intel Corporation
Copyright © 2013      Rafael Antognolli
Copyright © 2013      Jasper St. Pierre
Copyright © 2014      Jonas Ådahl
Copyright © 2014      Jason Ekstrand
Copyright © 2014-2015 Collabora, Ltd.
Copyright © 2015      Red Hat Inc.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice (including the next
paragraph) shall be included in all copies or substantial portions of the
Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

---

The above is the version of the MIT "Expat" License used by X.org:

    http://cgit.freedesktop.org/xorg/xserver/tree/COPYING
```

## MoltenVK

The Vulkan-on-Metal layer bundled in the macOS package (capability `hefesto-build-and-distribution`,
requirement "Dependency policy"). Copyright The Brenwill Workshop Ltd. Licensed under the Apache
License 2.0; the license text is the same as Hefesto's `LICENSE`. M0 uses the MoltenVK of the
developer's Vulkan SDK and bundles nothing; the exact notice and pin are copied here from the
pinned release when macOS packaging starts bundling it.

## Catch2 (build-time only, not shipped)

The unit-test framework. It is linked only into test executables, which are not part of any
package. Listed for completeness.

```text
Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
```

## Test tooling (not shipped)

`tools/golden/harness/magic_hash/` builds the golden reference tool `magic-hash-harness` from
Magic's own `utils/hash.c` at the pinned commit (fetched and checked by SHA-256 at capture time,
never committed). Its stand-in `shim/utils/magic.h` copies a few definitions from Magic's
`utils/magic.h` and keeps Magic's notice, reproduced here:

```text
Copyright (C) 1985, 1990 Regents of the University of California.
Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted, provided
that the above copyright notice appear in all copies.  The University of
California makes no representations about the suitability of this software
for any purpose.  It is provided "as is" without express or implied
warranty.  Export of this software outside of the United States of America
may require an export license.
```

## PDK script excerpts in test inputs (not shipped)

A few test inputs are verbatim copies or excerpts of PDK Tcl files, each under the Apache License
2.0 and carrying its origin in a header comment where the file format allows one:

| File(s) | Origin | Copyright |
|---|---|---|
| `tests/data/pcell/native/helpers_sky130.tcl` | open_pdks `c6df28c1` (ciel `5740c4a8`) `sky130A.tcl:279-353` | the open_pdks authors (Tim Edwards / efabless) |
| `tests/data/pcell/native/helpers_sg13g2.tcl` | IHP-Open-PDK `5e6d592e` `ihp-sg13g2-util.tcl:688-787` | IHP GmbH (IHP PDK Authors) |
| `tests/data/script/binding/pdk_root/sky130A/libs.tech/magic/sky130A.tcl` | open_pdks `c6df28c1` `sky130A.tcl:282-284,350-353` | the open_pdks authors |
| `tests/data/script/binding/pdk_root/ihp-sg13g2/libs.tech/magic/ihp-sg13g2-util.tcl` | IHP-Open-PDK `5e6d592e` `ihp-sg13g2-util.tcl:691-694` | IHP GmbH (IHP PDK Authors) |
| `tests/golden/SCRIPT/cases/SCRIPT-PDK-18/input/old/sky130A.magicrc` | open_pdks `0fe599b2` `sky130A/libs.tech/magic/sky130A.magicrc` (the M3 pin; design D15 "Older rc smoke") | the open_pdks authors |
| `tests/golden/SCRIPT/cases/SCRIPT-PDK-18/input/old/ihp-sg13g2.magicrc` | IHP-Open-PDK `ddb601a4` `ihp-sg13g2/libs.tech/magic/ihp-sg13g2.magicrc` | IHP GmbH (IHP PDK Authors) |

The two `.magicrc` files are kept byte for byte (the case compares Magic's and Hefesto's runs of
them), so their notice is this entry. The Apache License 2.0 text is in `LICENSE`. The golden PDK
cases' other PDK inputs are not committed: they are external inputs copied from `.cache/pdks` at
capture and replay time (`tools/golden/goldenlib/external.py`).

## X11 keysym names

`src/core/src/ui/keysyms.cpp` embeds the keysym names and values Magic's macro files use, as
data from X11 `keysymdef.h` (design D7 of `openspec/changes/m2-interactive-editing`, Q-UI-12), so
key names work on every platform without Xlib. Shipped in all packages (core). No X11 code or
library is linked.

```text
Copyright 1987, 1994, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.


Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.
```

## Components to be added in later milestones

Each entry is filled in, with its pin and notice text, by the change that adds the component
(requirements "Attribution for permissive sources", "Third-party libraries for cryptography,
networking and data formats" and "Bundled fonts and PDK data licensing" of `hefesto-licensing`).

| Component | Expected license | Where | Notes |
|---|---|---|---|
| Magic scmos tech files (including the scmos MOSIS notice) | UC Berkeley permissive notice + MOSIS notice | all, if bundled | M1 tests generate scmos.tech from build/ref/magic; not shipped |
| ngspice | modified BSD | all | GPL parts excluded |
| ↳ KLU, numparam | LGPLv2 / LGPLv2+ | all | static linking on iPadOS: Q-SIM-1 |
| ↳ OSDI | MPL-2.0 | all | plus each compact model compiled in (Q-SIM-2) |
| ↳ sparse | per ngspice | all | capability `simulation-engine-embedding` |
| cgltf (or tinygltf) | MIT | all | glTF writer |
| libsodium (or Monocypher) | ISC (BSD-2-Clause or CC0-1.0) | Linux | |
| libcurl | curl license (MIT-style) | Linux | |
| OpenSSL 3.x | Apache-2.0 | Linux, where the package bundles it | libcurl's TLS library |
| FreeSans | GPL with font exception | where bundled for font labels | kept as a separate data file |
| Sparse linear solver (e.g. Eigen) | MPL-2.0 | all, if used | Q-RELIABILITY-9 |
| PDK packs (sky130, gf180mcu, IHP) | Apache-2.0, to confirm per pack | installed with the pack | upstream LICENSE and NOTICE kept |
