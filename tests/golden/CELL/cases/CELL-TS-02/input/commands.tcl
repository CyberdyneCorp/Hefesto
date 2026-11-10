# CELL-TS-02: Edit leaf alone and save it; then load top (timestamp mismatch processing and the DRC check areas it leaves); save top.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/scmos -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load input/leaf} result; puts $result
catch {box 10 0 14 4} result; puts $result
catch {paint metal1} result; puts $result
catch {save} result; puts $result
catch {load input/top} result; puts $result
catch {select top cell} result; puts $result
catch {expand} result; puts $result
catch {cellname flags leaf} result; puts $result
catch {cellname flags mid} result; puts $result
catch {cellname flags top} result; puts $result
catch {writeall force} result; puts $result
quit -noprompt
