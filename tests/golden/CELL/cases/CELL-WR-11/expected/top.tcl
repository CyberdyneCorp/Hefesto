# Command script for generating cell top

suspendall
tech unlock *
set curunits [units]
units internal
load top -silent
box values 0 0 0 0
box values 16 18 74 24
paint checkpaint
box values -14 -14 74 18
paint checkpaint
box values 30 0 40 10
splitpaint ne polysilicon
box values 0 0 20 4
paint metal1
box position 50 0
getcell leaf
identify leaf_0
box values 2 2 2 2
label A ne metal1
select area label
port make 1
port connections n e
select clear
view
tech revert
units {*}$curunits
resumeall
