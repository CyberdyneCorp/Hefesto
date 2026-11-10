# DRC-EXC-3: exception SRAM width rule; a vertical 2-wide wire with a hint touching or straddling its left or right edge (Q-DRC-11), one cell each.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load l} result; puts $result
catch {box 0 0 2 20} result; puts $result
catch {paint m1} result; puts $result
catch {property MASKHINTS_SRAM -10 -10 0 30} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save l} result; puts $result
catch {load r} result; puts $result
catch {box 0 0 2 20} result; puts $result
catch {paint m1} result; puts $result
catch {property MASKHINTS_SRAM 2 -10 10 30} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save r} result; puts $result
catch {load ls} result; puts $result
catch {box 0 0 2 20} result; puts $result
catch {paint m1} result; puts $result
catch {property MASKHINTS_SRAM -10 -10 1 30} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save ls} result; puts $result
catch {load rs} result; puts $result
catch {box 0 0 2 20} result; puts $result
catch {paint m1} result; puts $result
catch {property MASKHINTS_SRAM 1 -10 10 30} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save rs} result; puts $result
quit -noprompt
