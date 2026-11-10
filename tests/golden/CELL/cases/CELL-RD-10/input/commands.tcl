# CELL-RD-10: Sections << checkpaint >> and << error_p >>; drc why over the error (M3: the DRC state).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/scmos -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load input/a} result; puts $result
catch {drc off} result; puts $result
catch {box 0 0 2 2} result; puts $result
catch {drc why} result; puts $result
catch {drc list count total} result; puts $result
catch {cellname list allcells} result; puts $result
catch {save a_out} result; puts $result
quit -noprompt
