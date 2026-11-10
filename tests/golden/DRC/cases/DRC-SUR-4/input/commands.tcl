# DRC-SUR-4: surround m1c m1 1 exact_width (contact in metal) and surround via m2 1 exact_width (another plane, Q-DRC-18); exact and too-wide surrounds.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load top} result; puts $result
catch {box -1 -1 21 21} result; puts $result
catch {paint m1} result; puts $result
catch {box 0 0 20 20} result; puts $result
catch {paint m1c} result; puts $result
catch {box 28 -1 51 21} result; puts $result
catch {paint m1} result; puts $result
catch {box 30 0 50 20} result; puts $result
catch {paint m1c} result; puts $result
catch {box 59 -1 65 5} result; puts $result
catch {paint m2} result; puts $result
catch {box 60 0 64 4} result; puts $result
catch {paint via} result; puts $result
catch {box 68 -1 75 5} result; puts $result
catch {paint m2} result; puts $result
catch {box 70 0 74 4} result; puts $result
catch {paint via} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save top} result; puts $result
quit -noprompt
