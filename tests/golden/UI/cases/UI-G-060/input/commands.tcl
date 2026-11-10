# UI-G-060: specialopen color 47; color; color get; color rgb; color next; color 0x1ff; color last.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/c} result; puts $result
catch {specialopen color 47} result; puts $result
catch {color} result; puts $result
catch {color get} result; puts $result
catch {color rgb} result; puts $result
catch {color next} result; puts $result
catch {color 0x1ff} result; puts $result
catch {color last} result; puts $result
catch {color get} result; puts $result
quit -noprompt
