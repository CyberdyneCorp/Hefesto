# NL-P01: Magic's node visit order on the 17 DEF-write .ext inputs: ext2sim .nodes and .al per target
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
if {[catch {
drc off
probe {ext2sim labels on}
probe {ext2sim alias on}
tech load input/LEFDEF-317/lefdef -noprompt
probe {ext2sim -p input/LEFDEF-317 top}
probe {file rename -force top.nodes LEFDEF-317.nodes}
probe {file rename -force top.al LEFDEF-317.al}
probe {file delete top.sim}
tech load input/LEFDEF-318/lefdef -noprompt
probe {ext2sim -p input/LEFDEF-318 top}
probe {file rename -force top.nodes LEFDEF-318.nodes}
probe {file rename -force top.al LEFDEF-318.al}
probe {file delete top.sim}
tech load input/LEFDEF-319/lefdef -noprompt
probe {ext2sim -p input/LEFDEF-319 top}
probe {file rename -force top.nodes LEFDEF-319.nodes}
probe {file rename -force top.al LEFDEF-319.al}
probe {file delete top.sim}
tech load input/LEFDEF-320/lefdef -noprompt
probe {ext2sim -p input/LEFDEF-320 top}
probe {file rename -force top.nodes LEFDEF-320.nodes}
probe {file rename -force top.al LEFDEF-320.al}
probe {file delete top.sim}
tech load input/LEFDEF-321/lefdef -noprompt
probe {ext2sim -p input/LEFDEF-321 top}
probe {file rename -force top.nodes LEFDEF-321.nodes}
probe {file rename -force top.al LEFDEF-321.al}
probe {file delete top.sim}
tech load input/LEFDEF-322/lefdef -noprompt
probe {ext2sim -p input/LEFDEF-322 top}
probe {file rename -force top.nodes LEFDEF-322.nodes}
probe {file rename -force top.al LEFDEF-322.al}
probe {file delete top.sim}
tech load input/LEFDEF-323/lefdef -noprompt
probe {ext2sim -p input/LEFDEF-323 top}
probe {file rename -force top.nodes LEFDEF-323.nodes}
probe {file rename -force top.al LEFDEF-323.al}
probe {file delete top.sim}
tech load input/LEFDEF-324/lefdef -noprompt
probe {ext2sim -p input/LEFDEF-324 top}
probe {file rename -force top.nodes LEFDEF-324.nodes}
probe {file rename -force top.al LEFDEF-324.al}
probe {file delete top.sim}
tech load input/LEFDEF-325/lefdef -noprompt
probe {ext2sim -p input/LEFDEF-325 top}
probe {file rename -force top.nodes LEFDEF-325.nodes}
probe {file rename -force top.al LEFDEF-325.al}
probe {file delete top.sim}
tech load input/LEFDEF-326/lefdef -noprompt
probe {ext2sim -p input/LEFDEF-326 top}
probe {file rename -force top.nodes LEFDEF-326.nodes}
probe {file rename -force top.al LEFDEF-326.al}
probe {file delete top.sim}
tech load input/LEFDEF-327/lefdef -noprompt
probe {ext2sim -p input/LEFDEF-327 top}
probe {file rename -force top.nodes LEFDEF-327.nodes}
probe {file rename -force top.al LEFDEF-327.al}
probe {file delete top.sim}
tech load input/LEFDEF-328/lefdef -noprompt
probe {ext2sim -p input/LEFDEF-328 top}
probe {file rename -force top.nodes LEFDEF-328.nodes}
probe {file rename -force top.al LEFDEF-328.al}
probe {file delete top.sim}
tech load input/LEFDEF-330/lefdef -noprompt
probe {ext2sim -p input/LEFDEF-330 top}
probe {file rename -force top.nodes LEFDEF-330.nodes}
probe {file rename -force top.al LEFDEF-330.al}
probe {file delete top.sim}
tech load input/LEFDEF-331/lefdef -noprompt
probe {ext2sim -p input/LEFDEF-331 c1}
probe {file rename -force c1.nodes LEFDEF-331.nodes}
probe {file rename -force c1.al LEFDEF-331.al}
probe {file delete c1.sim}
tech load input/LEFDEF-332/lefdef -noprompt
probe {ext2sim -p input/LEFDEF-332 top}
probe {file rename -force top.nodes LEFDEF-332.nodes}
probe {file rename -force top.al LEFDEF-332.al}
probe {file delete top.sim}
tech load input/LEFDEF-333/lefdef -noprompt
probe {ext2sim -p input/LEFDEF-333 top}
probe {file rename -force top.nodes LEFDEF-333.nodes}
probe {file rename -force top.al LEFDEF-333.al}
probe {file delete top.sim}
tech load input/LEFDEF-334/lefdef -noprompt
probe {ext2sim -p input/LEFDEF-334 top}
probe {file rename -force top.nodes LEFDEF-334.nodes}
probe {file rename -force top.al LEFDEF-334.al}
probe {file delete top.sim}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
