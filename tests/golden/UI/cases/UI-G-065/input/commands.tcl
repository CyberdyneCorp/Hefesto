# UI-G-065: The UI-G-063/064 areas plus a quoted text, then feedback save f.txt.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load input/c} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {feedback add "a"} result; puts $result
catch {feedback add "b" medium 0 0 10 10} result; puts $result
catch {feedback add "c" outline 1 1} result; puts $result
catch {feedback add "p" solid 0 0 10 0 10 10 0 10} result; puts $result
catch {feedback add "t" pale 0 0 10 0 0 10} result; puts $result
catch {feedback add {say "hi"} dotted} result; puts $result
catch {feedback save f.txt} result; puts $result
quit -noprompt
