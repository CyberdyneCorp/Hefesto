# Technology Styles Section

## Purpose

Defines the `styles` section of a technology file. It covers the `styletype` statement, which selects the family of display-style and color-map files, and the mapping of each tile type to one or more display styles. The formats of the display-style (`.dstyle`) and color-map (`.cmap`) files, and how styles are rendered, belong to capability `display-style-files` and the display capabilities. They are referred to here only by name.

## Requirements

### Requirement: styles section syntax

Each line of the `styles` section SHALL be one of:

```ebnf
styles-line   = "styletype" , style-family , { search-dir }
              | type-name , style-ref , { style-ref } ;
style-family  = token ;                 (* at most 49 characters are kept *)
style-ref     = integer | style-long-name ;
```

Compatibility: exact

- **Too few tokens.** A line with fewer than 2 tokens reports `Badly formed line in "style" section`. The line is ignored, and this does not fail the load.
- **Keyword.** `styletype` is matched exactly and case-sensitively. Any other first token is resolved as a single type name (see capability `tech-layers-and-planes`). If it does not resolve, the loader reports the resolution error and rejects the line, which fails the load.
- **Prerequisites.** The section requires only `types` (see capability `tech-file-format`). The contact section may come before or after it. The stacked-type propagation in "Contact style propagation" applies only to stacked types that exist when the style line is read, so it needs `styles` to come after `contact`.

(Magic ref: dbwind/DBWdisplay.c:1727-1806, utils/main.c:640-641)

#### Scenario: Several styles on one line
- **GIVEN** the example technology from capability `tech-file-format` and the line `nfet 6 7`
- **WHEN** it is loaded
- **THEN** ntransistor is drawn with the tech styles numbered 6 and 7

#### Scenario: Unknown type in styles
- **GIVEN** the line `nosuchlayer 3`
- **WHEN** it is loaded
- **THEN** `Unrecognized layer (type) name "nosuchlayer"` is reported and the load fails

### Requirement: styletype statement

A `styletype <family> [dir ...]` line SHALL select the style family `<family>`, truncated to 49 characters, and immediately load the display resources for it. The steps run in this order:

Compatibility: behavioral

1. **Color map.** For each search location in turn (each explicit `dir` argument in order, then the system library search path), try to read the color map `<family>.<display-cmap-type>.<monitor-type>.cmap`. If that file is missing, try the legacy suffix `.cmap1`. The working directory `.` is searched first for each location. Stop at the first success.
   - If the current display needs no color map (a headless or monochrome driver), this step succeeds at once.
   - If every location fails, the line is rejected.
2. **Display styles.** Read `<family>.<display-style-type>.dstyle`, falling back to `.dstyle5`, from `.` and then from the location where the color map was found. With no explicit `dir` arguments, that location is the system search path. If this fails, the line is rejected.
3. **Clear the mapping.** Reset the type-to-style mapping. It is sized to the number of tech styles defined by the display-style file, and starts empty.
4. **Cursors.** Load the cursor glyphs from the same locations. If this fails, the line is rejected.

The display-type parts of the file names (`<display-cmap-type>`, `<display-style-type>` and the monitor type) come from the active display driver. The `.dstyle` and `.cmap` file formats are specified in capability `display-style-files`.

Hefesto SHALL resolve `<family>` to its own display-style resources. Hefesto MAY bundle the standard `mos` family. A technology file SHALL load successfully in a headless session without any display files (see Q-TECH-24). The file search order above is `informative` for Hefesto.

The style mapping is reset only by a `styletype` line. A `styles` section without one keeps adding to the mapping left by the previous technology (see Q-TECH-24).

(Magic ref: dbwind/DBWdisplay.c:1745-1775, dbwind/DBWdisplay.c:1651-1670, graphics/grDStyle.c:405-440, graphics/grCMap.c:105-147)

#### Scenario: Standard family
- **GIVEN** the line `styletype mos`, and a display driver whose style type is `7bit`
- **WHEN** it is loaded
- **THEN** the display-style file `mos.7bit.dstyle` is loaded and the type-to-style mapping is cleared

### Requirement: Type to style mapping

For a line `<type> <s1> [<s2> ...]`, the loader SHALL resolve each style reference to a tech style and add the type to that style's type set. A tech style is a style defined in the display-style file's tech-style part, beyond the fixed system styles.

Compatibility: behavioral

1. **Integer reference:** the tech style whose declared style number equals the integer. It is the first such style in display-style file order.
2. **Name reference:** the tech style whose long name equals the token exactly (case-sensitive).
3. **No match:** the loader reports `Invalid style "<s>" for tile type <t>`. The line continues with the next reference and is not rejected.

A type may map to several styles, and a style may hold several types. A type with no mapping is not drawn.

The style numbers and names are references into the display-style file. Their visual meaning is defined in capability `display-style-files`.

(Magic ref: dbwind/DBWdisplay.c:1688-1709, dbwind/DBWdisplay.c:1779-1803)

#### Scenario: Style by long name
- **GIVEN** a display-style file that defines a tech style with long name `metal1`
- **WHEN** the line `metal1 metal1` is loaded
- **THEN** type metal1 is mapped to that style

#### Scenario: Invalid style keeps the line
- **GIVEN** the line `poly 1 nosuchstyle 20`, where style `nosuchstyle` does not exist
- **WHEN** it is loaded
- **THEN** `Invalid style "nosuchstyle" for tile type poly` is reported
- **AND** poly is mapped to styles 1 and 20, and the load does not fail

### Requirement: Contact style propagation

When a user contact c (types 9..U−1) is mapped to a style, the loader SHALL also map to that style every stacked type that exists at that moment, has c as a component contact, and has the same home plane as c.

Compatibility: behavioral

Because the propagation follows the home plane, a stacked type takes its styles from the component contact whose home plane is the stacked type's home plane.

(Magic ref: dbwind/DBWdisplay.c:1790-1801)

#### Scenario: Stacked type inherits the via styles
- **GIVEN** the example technology, where pc+m2c has home plane metal1 and m2c has home plane metal1, and the styles section comes after the contact section
- **WHEN** the line `m2c 20 21 33` is loaded
- **THEN** pc+m2c is also mapped to styles 20, 21 and 33
- **AND** the line `pc 1 20 32` does not map pc+m2c, because pc's home plane is active
