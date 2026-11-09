# TECH-G10: repeat 3 / repeat 0 / repeat x around a deliberate connect error.
# Reference script (Magic, Tcl): results printed with puts.
# No state query follows the failed load: Magic keeps its partly loaded tables and
# Hefesto loads atomically (tech-loading-and-scaling, Q-TECH-10), so they differ by design.
catch {tech load input/tiny -noprompt} result; puts $result
quit -noprompt
