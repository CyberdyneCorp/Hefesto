# Plot data files

Display-style and colour-map files of Magic 8.3.694 (commit
`fc5daa9e2856b89a0a3056f296ded1fd4474e39e`, directory `scmos/`), copied
unmodified (capability plot-output "Platform mechanism of plot output",
decision Q-MISC-44). The build embeds them in the core
(`src/core/src/plot/tech/sources.cmake`), so the PNM plot colours need no
Magic installation on any platform, iPadOS included.

| File | Used for |
|---|---|
| `mos.7bit.dstyle`, `mos.7bit.std.cmap` | the `default` and `draw <type>` colours of a `style pnm` block when no display style set of the `7bit` class is found (what Magic's null display loads) |
| `mos.7bit.mraster.cmap`, `mos.7bit.mraster_dstyle` | the `color` and `dstyle` lines without a file name, after the current directory and the technology's directory |

Their notices are Magic's (UC Berkeley) and, in the header of
`mos.7bit.dstyle`, the MOSIS / University of Southern California notice;
both are reproduced in `THIRD_PARTY_NOTICES.md`. Do not edit these files:
replace them only with the files of the pinned Magic commit.
