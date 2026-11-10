# DRC-MW-4: maxwidth m2 10 bend_ok via "slot" (via excluded); via covering half of a wide m2.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 20 20} result; puts $result
catch {paint m2} result; puts $result
catch {box 0 0 10 20} result; puts $result
catch {paint via} result; puts $result
catch {box 40 0 60 20} result; puts $result
catch {paint m2} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save top} result; puts $result
quit -noprompt
