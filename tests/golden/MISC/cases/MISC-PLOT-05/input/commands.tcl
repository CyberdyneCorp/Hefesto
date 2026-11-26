# MISC-PLOT-05: plot parameters listing; each parameter set to valid and invalid values (unique prefix and mixed case names too); the listing again
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/plottest -noprompt
load top
probe {plot parameters}
probe {plot parameters showcellnames false}
probe {plot parameters ps_cellidfont Courier}
probe {plot parameters ps_namefont Times-Roman}
probe {plot parameters ps_labelfont Helvetica-Bold}
probe {plot parameters ps_cellidsize 9}
probe {plot parameters ps_namesize 11}
probe {plot parameters ps_labelsize 7}
probe {plot parameters ps_boundary false}
probe {plot parameters ps_width 600}
probe {plot parameters ps_height 700}
probe {plot parameters ps_margin 0}
probe {plot parameters pnmmaxmem 1024}
probe {plot parameters pnmdownsample 2}
probe {plot parameters pnmbackground 128}
probe {plot parameters PS_Width 612}
probe {plot parameters ps_mar 36}
probe {plot parameters showcellnames maybe}
probe {plot parameters ps_cellidsize 0}
probe {plot parameters ps_namesize -1}
probe {plot parameters ps_labelsize x}
probe {plot parameters ps_boundary 2}
probe {plot parameters ps_width 0}
probe {plot parameters ps_height abc}
probe {plot parameters ps_margin -1}
probe {plot parameters pnmmaxmem 0}
probe {plot parameters pnmdownsample -1}
probe {plot parameters pnmbackground 256}
probe {plot parameters pnmbackground x}
puts {-- Q-MISC-51 begin --}
probe {plot parameters dotsperinch 300}
probe {plot parameters dotsperinch 0}
probe {plot parameters plottype hprtl}
probe {plot parameters plottype nosuch}
probe {plot parameters swathheight 0}
probe {plot parameters width 0}
probe {plot parameters pnmplotRTL maybe}
probe {plot parameters cellidfont vr7}
puts {-- Q-MISC-51 end --}
probe {plot parameters}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
