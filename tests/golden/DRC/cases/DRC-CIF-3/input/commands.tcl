# DRC-CIF-3: cifspacing NW PW 600 touching_illegal and cifspacing NW NW 600 touching_ok; *halo; CAP.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 1000 1000} result; puts $result
catch {paint nwell} result; puts $result
catch {box 1400 0 2400 1000} result; puts $result
catch {paint pwell} result; puts $result
catch {box 0 1300 1000 2000} result; puts $result
catch {paint nwell} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save top} result; puts $result
catch {drc *halo} result; puts $result
quit -noprompt
