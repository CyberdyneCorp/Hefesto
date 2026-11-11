# SCRIPT-GUI-02: set Opts(callback) {puts "cb $framename $layoutframe"}; openwrapper; set Winopts(.layout3,callback) {...}
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
set Opts(callback) {puts "cb $framename $layoutframe"}
probe {openwrapper}
set Winopts(.layout3,callback) {puts "wcb .layout3"}
probe {openwrapper}
probe {windownames layout}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
