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

## Components to be added in later milestones

Each entry is filled in, with its pin and notice text, by the change that adds the component
(requirements "Attribution for permissive sources", "Third-party libraries for cryptography,
networking and data formats" and "Bundled fonts and PDK data licensing" of `hefesto-licensing`).

| Component | Expected license | Where | Notes |
|---|---|---|---|
| Magic (ported logic; scmos/minimum tech files, including the scmos MOSIS notice) | UC Berkeley permissive notice | all | |
| Tcl | Tcl/Tk license (BSD-style) | all | X-3 |
| ngspice | modified BSD | all | GPL parts excluded |
| ↳ KLU, numparam | LGPLv2 / LGPLv2+ | all | static linking on iPadOS: Q-SIM-1 |
| ↳ OSDI | MPL-2.0 | all | plus each compact model compiled in (Q-SIM-2) |
| ↳ sparse | per ngspice | all | capability `simulation-engine-embedding` |
| Zstandard (zstd) | BSD-3-Clause | all | PDK assets |
| cgltf (or tinygltf) | MIT | all | glTF writer |
| libsodium (or Monocypher) | ISC (BSD-2-Clause or CC0-1.0) | Linux | |
| libcurl | curl license (MIT-style) | Linux | |
| OpenSSL 3.x | Apache-2.0 | Linux, where the package bundles it | libcurl's TLS library |
| Inter | OFL-1.1 | all | UI font (`hefesto-design-system`) |
| JetBrains Mono | OFL-1.1 | all | UI font |
| FreeSans | GPL with font exception | where bundled for font labels | kept as a separate data file |
| Sparse linear solver (e.g. Eigen) | MPL-2.0 | all, if used | Q-RELIABILITY-9 |
| PDK packs (sky130, gf180mcu, IHP) | Apache-2.0, to confirm per pack | installed with the pack | upstream LICENSE and NOTICE kept |
