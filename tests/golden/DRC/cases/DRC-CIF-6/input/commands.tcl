# DRC-CIF-6: Mask errors at negative coordinates and below one internal unit: output style fine (10 centimicrons per unit), cifwidth MET1 25.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {cif ostyle fine} result; puts $result
catch {drc style default} result; puts $result
catch {drc *halo} result; puts $result
catch {load top} result; puts $result
catch {box -30 -20 -28 -5} result; puts $result
catch {paint m1} result; puts $result
catch {box -10 -10 -7 10} result; puts $result
catch {paint m1} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save top} result; puts $result
quit -noprompt
