# DRC-NM-3: spacing m1 m1 3 touching_ok; two parallel 45-degree m1 wires 2 apart horizontally (1.4 perpendicular).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/drctest -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 2 2} result; puts $result
catch {splitpaint nw m1} result; puts $result
catch {box 0 2 2 4} result; puts $result
catch {splitpaint se m1} result; puts $result
catch {box 2 2 4 4} result; puts $result
catch {splitpaint nw m1} result; puts $result
catch {box 2 4 4 6} result; puts $result
catch {splitpaint se m1} result; puts $result
catch {box 4 4 6 6} result; puts $result
catch {splitpaint nw m1} result; puts $result
catch {box 4 6 6 8} result; puts $result
catch {splitpaint se m1} result; puts $result
catch {box 6 6 8 8} result; puts $result
catch {splitpaint nw m1} result; puts $result
catch {box 6 8 8 10} result; puts $result
catch {splitpaint se m1} result; puts $result
catch {box 4 -4 6 -2} result; puts $result
catch {splitpaint nw m1} result; puts $result
catch {box 4 -2 6 0} result; puts $result
catch {splitpaint se m1} result; puts $result
catch {box 6 -2 8 0} result; puts $result
catch {splitpaint nw m1} result; puts $result
catch {box 6 0 8 2} result; puts $result
catch {splitpaint se m1} result; puts $result
catch {box 8 0 10 2} result; puts $result
catch {splitpaint nw m1} result; puts $result
catch {box 8 2 10 4} result; puts $result
catch {splitpaint se m1} result; puts $result
catch {box 10 2 12 4} result; puts $result
catch {splitpaint nw m1} result; puts $result
catch {box 10 4 12 6} result; puts $result
catch {splitpaint se m1} result; puts $result
catch {select top cell} result; puts $result
catch {box grow center 100} result; puts $result
catch {drc check} result; puts $result
catch {drc catchup} result; puts $result
catch {drc listall why} result; puts $result
catch {drc list count total} result; puts $result
catch {save top} result; puts $result
quit -noprompt
