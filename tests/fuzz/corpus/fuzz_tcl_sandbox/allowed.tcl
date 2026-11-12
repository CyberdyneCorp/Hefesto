set f [open data.txt w]
puts $f [string repeat x 100]
close $f
file mkdir lib/sub
file copy data.txt lib/sub/copy.txt
cd lib
set l [glob -nocomplain */*]
cd ..
file rename data.txt moved.txt
set f [open moved.txt]
set t [read $f]
close $f
file delete -force lib
set env(PDK_ROOT)
set env(HOME) /x
source [file join [info library] parray.tcl]
file tempfile name [pwd]/tmpXXXX
