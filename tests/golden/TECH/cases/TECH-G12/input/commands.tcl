# TECH-G12: requires 8.3.700, magic-8.2.1 and 8.3.
# Reference script (Magic, Tcl): results printed with puts.
# No state query follows the failed load: Magic keeps its partly loaded tables and
# Hefesto loads atomically (tech-loading-and-scaling, Q-TECH-10), so they differ by design.
catch {tech load input/tiny -noprompt} result; puts $result
quit -noprompt
