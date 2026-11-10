# UI-G-002: The ambiguous prefix e, then extr help vs ext help (unique prefixes of extract).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/c} result; puts $result
catch {e} result; puts $result
catch {extr help} result; puts $result
catch {ext help} result; puts $result
quit -noprompt
