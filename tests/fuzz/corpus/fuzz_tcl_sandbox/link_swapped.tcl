# Security review S-05: a path checked while harmless is checked again after a link is moved in.
file mkdir sub out
set f [open out/secret w]; puts $f s; close $f
file link -symbolic sub/L ../out/secret
set p [file join [pwd] L]
file exists $p
catch {file rename sub/L $p}
catch {set f [open $p]; read $f; close $f}
