# Security review S-04: a relative link moved within the root must not write outside.
file mkdir sub
file link -symbolic sub/L ../escaped
catch {file rename sub/L L}
catch {set f [open L w]; puts $f x; close $f}
