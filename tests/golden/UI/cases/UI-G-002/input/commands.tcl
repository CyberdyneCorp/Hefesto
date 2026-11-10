# UI-G-002: The prefix e and the unique prefix extr are not command names in a script (Q-UI-41); the registered alias id is (ext, the alias of extract, waits for the extractor).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/c} result; puts $result
catch {e} result; puts $result
catch {extr help} result; puts $result
catch {id} result; puts $result
catch {id x} result; puts $result
quit -noprompt
