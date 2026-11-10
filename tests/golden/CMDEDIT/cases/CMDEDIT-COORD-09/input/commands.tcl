# CMDEDIT-COORD-09: tech fine; every units mode with and without print: box values 0 0 15 20000, box size, box values.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/fine -noprompt} result; puts $result
catch {load a} result; puts $result
catch {units internal} result; puts $result
catch {box values 0 0 15 20000} result; puts $result
catch {box size} result; puts $result
catch {box values} result; puts $result
catch {units internal print} result; puts $result
catch {box values 0 0 15 20000} result; puts $result
catch {box size} result; puts $result
catch {box values} result; puts $result
catch {units lambda} result; puts $result
catch {box values 0 0 15 20000} result; puts $result
catch {box size} result; puts $result
catch {box values} result; puts $result
catch {units lambda print} result; puts $result
catch {box values 0 0 15 20000} result; puts $result
catch {box size} result; puts $result
catch {box values} result; puts $result
catch {units user} result; puts $result
catch {box values 0 0 15 20000} result; puts $result
catch {box size} result; puts $result
catch {box values} result; puts $result
catch {units user print} result; puts $result
catch {box values 0 0 15 20000} result; puts $result
catch {box size} result; puts $result
catch {box values} result; puts $result
catch {units microns} result; puts $result
catch {box values 0 0 15 20000} result; puts $result
catch {box size} result; puts $result
catch {box values} result; puts $result
catch {units microns print} result; puts $result
catch {box values 0 0 15 20000} result; puts $result
catch {box size} result; puts $result
catch {box values} result; puts $result
catch {units default} result; puts $result
catch {box values 0 0 15 20000} result; puts $result
catch {box size} result; puts $result
catch {box values} result; puts $result
catch {units default print} result; puts $result
catch {box values 0 0 15 20000} result; puts $result
catch {box size} result; puts $result
catch {box values} result; puts $result
quit -noprompt
