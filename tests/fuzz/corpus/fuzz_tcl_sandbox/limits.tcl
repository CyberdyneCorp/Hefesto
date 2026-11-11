# Security review S-07, S-08: process-wide or stack-sized settings stay out of reach.
catch {interp recursionlimit {} 100000000}
catch {encoding system [encoding system]}
catch {encoding dirs [encoding dirs]}
proc r {n} { if {$n > 0} { r [expr {$n - 1}] } }
catch {r 5000}
