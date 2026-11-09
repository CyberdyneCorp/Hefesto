# UI-G-023: At startup: view get; windowpositions; windownames all.
# Reference script (Magic, Tcl): one result line per command.
catch {view get} result; puts $result
catch {windowpositions} result; puts $result
catch {windownames all} result; puts $result
quit -noprompt
