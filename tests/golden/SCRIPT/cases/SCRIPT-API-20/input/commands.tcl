# SCRIPT-API-20: layout with instance x1 (inv), arrayed x2, labels and ports; run every form in the result-format table
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
load top
select cell x2
array 2 1
select clear
select cell x1
probe {box values}
probe {box position}
probe {box size}
probe {box width}
probe {box height}
probe {box exists}
probe {snap}
probe {cellname list exists inv}
probe {cellname list exists nand2}
probe {cellname list self}
probe {cellname list children top}
probe {cellname list parents inv}
probe {cellname list instances inv}
probe {cellname list topcells}
probe {cellname list allcells}
probe {cellname list modified}
probe {cellname list window}
probe {cellname list flags inv}
probe {cellname list property inv nosuch}
probe {cellname list writeable inv}
probe {cellname list filepath inv}
probe {instance list celldef x1}
probe {instance list exists x1}
probe {instance list self}
probe {instance list celldef nosuch}
probe {instance list children x1}
probe {instance list orientation}
probe {instance list orientation -def}
probe {instance list abutment}
probe {what -list}
probe {array -list count}
probe {array -list pitch}
probe {array -list position}
probe {goto VDD}
probe {goto nosuch}
probe {port first}
probe {port last}
probe {property}
probe {tech name}
probe {tech version}
probe {tech lambda}
probe {extract list style}
probe {extract listall style}
probe {drc list style}
probe {drc listall style}
probe {drc status}
probe {macro list}
probe {windownames}
probe {windowcaption}
probe {view get}
probe {view bbox}
probe {path search}
probe {path cell}
probe {path sys}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
