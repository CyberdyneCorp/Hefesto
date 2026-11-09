# Bundled fonts

The desktop app's UI fonts (capability `hefesto-design-system`, requirement
"Typography"; licensing per `hefesto-licensing`). Both are licensed under the
SIL Open Font License 1.1; the license texts are beside the font files and the
fonts are listed in `THIRD_PARTY_NOTICES.md`. They are loaded at startup by
the desktop shell (`src/shell/desktop/src/raven_ui.cpp`), never embedded in
code. Only the weights the forge type scale uses are kept.

| Family | Version | Files | License | Source archive | SHA-256 of the archive |
|---|---|---|---|---|---|
| Inter | 4.1 | `Inter-Regular.ttf`, `Inter-Medium.ttf`, `Inter-SemiBold.ttf`, `Inter-Bold.ttf` (from `extras/ttf/`) | OFL-1.1, `Inter-OFL.txt` | https://github.com/rsms/inter/releases/download/v4.1/Inter-4.1.zip | `9883fdd4a49d4fb66bd8177ba6625ef9a64aa45899767dde3d36aa425756b11e` |
| JetBrains Mono | 2.304 | `JetBrainsMono-Regular.ttf`, `JetBrainsMono-Medium.ttf` (from `fonts/ttf/`) | OFL-1.1, `JetBrainsMono-OFL.txt` | https://github.com/JetBrains/JetBrainsMono/releases/download/v2.304/JetBrainsMono-2.304.zip | `6f6376c6ed2960ea8a963cd7387ec9d76e3f629125bc33d1fdcd7eb7012f7bbf` |

The files are unmodified copies from the release archives. To update, download
the new release, replace the files and license texts, and update this table and
the notices entry.
