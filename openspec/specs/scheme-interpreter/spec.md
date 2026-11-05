# Scheme Interpreter (informative)

## Purpose

Documents Magic's optional built-in mini-Scheme interpreter (selected at build time with `--with-interpreter=scheme`, mutually exclusive with the Tcl interpreter). It is legacy: current Magic distributions are built with Tcl. Hefesto has no obligation to implement it; this capability records its behavior so that old `.scm` scripts can be recognised and, if desired, translated. Scripting in Hefesto is covered by capability `ui-command-dispatch`.

## Requirements

### Requirement: Interpreter role
When Magic is built with the Scheme interpreter, every command line SHALL first be given to the interpreter: a line in parenthesised Scheme syntax is evaluated, and the `magic` primitive issues ordinary Magic commands; other lines are executed as Magic commands. Hefesto MAY omit this feature.

Compatibility: informative
(Magic ref: textio/txCommands.c:1730-1750, lisp/lispMain.c:60-130, scripts/configure.in:527-540)

#### Scenario: Not required in Hefesto
- **GIVEN** a Hefesto build
- **WHEN** a user types `(magic "box 0 0 10 10")`
- **THEN** Hefesto MAY reject it as an unknown command

### Requirement: Language summary
The interpreter SHALL be documented as providing: special forms `quote lambda let let* letrec cond begin define set! if`; predicates `boolean? symbol? list? pair? number? string? procedure? null? eqv? zero? positive? negative?`; list operations `car cdr cons set-car! set-cdr! list length`; `eval apply truncate`; strings `string-append symbol->string string->symbol number->string string->number string-length string-compare string-ref string-set! substring`; I/O and system `load-scm save-scm spawn wait collect-garbage error showframe display-object print-object`; and Magic access `magic getpoint getbox getpaint getselpaint getlabel getsellabel getcellnames`. Predefined variables include `technology` and `edit-cell`; echo/garbage-collection behaviour is controlled by `scm-echo-parser-input`, `scm-echo-parser-output`, `scm-echo-result` and `scm-gc-frequency`. Library scripts shipped with Magic: `box.scm default.scm draw.scm drc.scm gate.scm help.scm label.scm layout.scm prs.scm sel.scm stack.scm`.

Compatibility: informative
(Magic ref: lisp/lispA-Z.c, lisp/lispMain.c:150-230, lisp/README, lisp/scm, doc/latexfiles/tutscm1.tex)

#### Scenario: Script recognition
- **GIVEN** a file `stack.scm` from Magic's distribution
- **WHEN** a Hefesto migration tool scans it
- **THEN** it can identify calls to the `magic` primitive as Magic commands to translate

### Requirement: Platform note
The interpreter's `spawn` and `wait` primitives create processes; on hosts without process creation they SHALL be considered unavailable (see Q-LEGACY-71 for the general platform policy).

Compatibility: informative
(Magic ref: lisp/lispIO.c, lisp/lispA-Z.c)

#### Scenario: Spawn unavailable on iPadOS
- **GIVEN** an iPadOS host
- **WHEN** a script would call `spawn`
- **THEN** the call is reported as unsupported
