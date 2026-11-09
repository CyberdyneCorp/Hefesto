# CELL-WR-10: Round trip of sky130_fd_sc_hd__tapvpwrvgnd_1 and __a21bo_1 (the two documented exceptions of task 4.2).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/sky130A -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load input/sky130_fd_sc_hd__tapvpwrvgnd_1} result; puts $result
catch {save sky130_fd_sc_hd__tapvpwrvgnd_1} result; puts $result
catch {load input/sky130_fd_sc_hd__a21bo_1} result; puts $result
catch {save sky130_fd_sc_hd__a21bo_1} result; puts $result
quit -noprompt
