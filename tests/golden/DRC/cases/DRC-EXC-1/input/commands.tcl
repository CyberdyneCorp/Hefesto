# DRC-EXC-1: exemption SRAM spacing rule; MASKHINTS_SRAM over half of the layout; violations inside and outside.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint m1} result; puts $result
catch {box 12 0 22 10} result; puts $result
catch {paint m1} result; puts $result
catch {box 60 0 70 10} result; puts $result
catch {paint m1} result; puts $result
catch {box 72 0 82 10} result; puts $result
catch {paint m1} result; puts $result
catch {property MASKHINTS_SRAM -10 -10 40 30} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save top} result; puts $result
quit -noprompt
