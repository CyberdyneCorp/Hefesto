# MISC-FL-02: Font labels rotated 90 and -90 (stored 270), a one-word offset, a font index; save.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load a} result; puts $result
catch {box -20 -20 20 20} result; puts $result
catch {paint metal1} result; puts $result
catch {box 0 0 0 0} result; puts $result
catch {label R90 FreeSans 10 90 0 0 center m1} result; puts $result
catch {box 5 5 5 5} result; puts $result
catch {label Rm90 FreeSans 10 -90 0 0 center m1} result; puts $result
catch {box -5 -5 -5 -5} result; puts $result
catch {label Off 1 4 0 {2 1} sw m1} result; puts $result
catch {save a} result; puts $result
quit -noprompt
