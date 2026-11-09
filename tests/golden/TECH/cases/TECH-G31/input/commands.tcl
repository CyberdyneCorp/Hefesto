# TECH-G31: Names with dots, explicit suffix, legacy suffix, missing file.
# Reference script (Magic, Tcl): results printed with puts.
catch {tech load input/SCN4M_SUBM.20 -noprompt} result; puts $result
puts [tech filename]
catch {tech load input/tiny.tech -noprompt} result; puts $result
puts [tech filename]
catch {tech load input/old -noprompt} result; puts $result
puts [tech filename]
catch {tech load input/nothere -noprompt} result; puts $result
puts [tech filename]
quit -noprompt
