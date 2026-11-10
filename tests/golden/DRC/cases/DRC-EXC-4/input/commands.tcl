# DRC-EXC-4: exception SRAM width rule; a horizontal 2-high wire with a hint touching or straddling its bottom or top edge (Q-DRC-11), one cell each.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load b} result; puts $result
catch {box 0 0 20 2} result; puts $result
catch {paint m1} result; puts $result
catch {property MASKHINTS_SRAM -10 -10 30 0} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save b} result; puts $result
catch {load t} result; puts $result
catch {box 0 0 20 2} result; puts $result
catch {paint m1} result; puts $result
catch {property MASKHINTS_SRAM -10 2 30 10} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save t} result; puts $result
catch {load bs} result; puts $result
catch {box 0 0 20 2} result; puts $result
catch {paint m1} result; puts $result
catch {property MASKHINTS_SRAM -10 -10 30 1} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save bs} result; puts $result
catch {load ts} result; puts $result
catch {box 0 0 20 2} result; puts $result
catch {paint m1} result; puts $result
catch {property MASKHINTS_SRAM -10 1 30 10} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save ts} result; puts $result
quit -noprompt
