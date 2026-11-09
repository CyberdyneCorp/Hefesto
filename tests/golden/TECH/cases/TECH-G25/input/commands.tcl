# TECH-G25: tiny.tech plus pdc and 'compose pdc pdiff m1': P/E dump of pdc, pdiff, metal1.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt}
catch {snap internal}
catch {load paint_dump}
catch {box 0 0 1 1}
catch {paint pdc}
catch {paint pdc}
catch {box 3 0 4 1}
catch {paint pdc}
catch {paint pdiff}
catch {box 6 0 7 1}
catch {paint pdc}
catch {paint metal1}
catch {box 0 3 1 4}
catch {paint pdiff}
catch {paint pdc}
catch {box 3 3 4 4}
catch {paint pdiff}
catch {paint pdiff}
catch {box 6 3 7 4}
catch {paint pdiff}
catch {paint metal1}
catch {box 0 6 1 7}
catch {paint metal1}
catch {paint pdc}
catch {box 3 6 4 7}
catch {paint metal1}
catch {paint pdiff}
catch {box 6 6 7 7}
catch {paint metal1}
catch {paint metal1}
catch {save paint_dump}
catch {load erase_dump}
catch {box 0 0 1 1}
catch {paint pdc}
catch {erase pdc}
catch {box 3 0 4 1}
catch {paint pdc}
catch {erase pdiff}
catch {box 6 0 7 1}
catch {paint pdc}
catch {erase metal1}
catch {box 0 3 1 4}
catch {paint pdiff}
catch {erase pdc}
catch {box 3 3 4 4}
catch {paint pdiff}
catch {erase pdiff}
catch {box 6 3 7 4}
catch {paint pdiff}
catch {erase metal1}
catch {box 0 6 1 7}
catch {paint metal1}
catch {erase pdc}
catch {box 3 6 4 7}
catch {paint metal1}
catch {erase pdiff}
catch {box 6 6 7 7}
catch {paint metal1}
catch {erase metal1}
catch {save erase_dump}
quit -noprompt
