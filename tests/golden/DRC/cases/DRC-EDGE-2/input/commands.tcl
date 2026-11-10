# DRC-EDGE-2: edge4way nfet ndiff 2 ndiff,ndc ndiff 2 "ov"; diffusion extension 1 on one side, 2 on the other.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load top} result; puts $result
catch {box 2 0 7 10} result; puts $result
catch {paint ndiff} result; puts $result
catch {box 3 -2 5 12} result; puts $result
catch {paint poly} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save top} result; puts $result
quit -noprompt
