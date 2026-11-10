# DRC-SUB-1: Why texts with %d, %c and %a; drc why under units default, lambda, internal, microns.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 2 10} result; puts $result
catch {paint m1} result; puts $result
catch {box 5 0 7 10} result; puts $result
catch {paint m1} result; puts $result
catch {box 20 0 22 10} result; puts $result
catch {paint m2} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save top} result; puts $result
catch {select top cell} result; puts $result
catch {units default} result; puts $result
catch {drc why} result; puts $result
catch {units lambda} result; puts $result
catch {drc why} result; puts $result
catch {units internal} result; puts $result
catch {drc why} result; puts $result
catch {units microns} result; puts $result
catch {drc why} result; puts $result
quit -noprompt
