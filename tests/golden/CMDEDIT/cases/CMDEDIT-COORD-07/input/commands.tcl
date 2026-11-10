# CMDEDIT-COORD-07: units queries and modes from a fresh session (Q-CMDEDIT-6).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {units} result; puts $result
catch {units microns print} result; puts $result
catch {units} result; puts $result
catch {units internal} result; puts $result
catch {units} result; puts $result
catch {units default} result; puts $result
catch {units print} result; puts $result
catch {units} result; puts $result
quit -noprompt
