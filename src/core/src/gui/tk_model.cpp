// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The headless widget model of hefesto-cli's wrapper mode (see tk_model.hpp).
// The model is Tcl state of the project's interpreter (namespace
// ::hefesto::tk), so a script sees Tk's results and errors exactly where it
// looks for them.
// (Magic ref: tcltk/wrapper.tcl:986-1512, tcltk/toolbar.tcl:8-230;
// Tk 8.6 generic/tkCmds.c, tkMenu.c, tkEntry.c, ttk/ttkTreeview.c)

#include "hefesto/gui/tk_model.hpp"

#include <array>
#include <functional>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/gui/commands.hpp"
#include "hefesto/pcell/framework/dialog.hpp"
#include "hefesto/script/binding/tags.hpp"
#include "hefesto/script/interp.hpp"

namespace hefesto::gui {

namespace {

// The widget tree: class, children (creation order, as `winfo children`
// lists them), options; the `.winmenu` of wrapper.tcl:273.
constexpr std::string_view kTree = R"tcl(
namespace eval ::hefesto::tk {
   variable class
   variable kids
   variable opt
   variable entries
   variable binds
   variable serial
   array set class {. Tk}
   array set kids {. {}}
   variable textual {Label Button Menubutton Checkbutton Radiobutton Message Labelframe
      TLabel TButton TMenubutton TCheckbutton TRadiobutton TLabelframe}
}
proc ::hefesto::tk::parent_of {w} {
   set i [string last . $w]
   if {$i <= 0} {return .}
   return [string range $w 0 [expr {$i - 1}]]
}
proc ::hefesto::tk::exists {w} {
   variable class
   return [info exists class($w)]
}
proc ::hefesto::tk::need {w} {
   variable class
   if {![info exists class($w)]} {return -code error "bad window path name \"$w\""}
}
proc ::hefesto::tk::key {w} {
   return [string map {* \\* ? \\? [ \\[ ] \\] \\ \\\\} $w]
}
# A window in the tree without a widget command (the layout window, whose
# command is the engine's window command).
proc ::hefesto::tk::node {cls w args} {
   variable class
   variable kids
   variable opt
   variable entries
   variable serial
   if {![string match .?* $w]} {return -code error "bad window path name \"$w\""}
   set p [parent_of $w]
   need $p
   if {[info exists class($w)]} {
      return -code error "window name \"[string range $w [expr {[string last . $w] + 1}] end]\" already exists in parent"
   }
   if {[llength $args] % 2} {return -code error "value for \"[lindex $args end]\" missing"}
   set class($w) $cls
   set kids($w) {}
   lappend kids($p) $w
   foreach {k v} $args {set opt($w\ $k) $v}
   if {$cls eq "Menu"} {
      set entries($w) {}
      if {![info exists opt($w\ -tearoff)] || $opt($w\ -tearoff)} {lappend entries($w) {type tearoff}}
   }
   if {$cls eq "Treeview"} {
      set serial($w) 0
      set root "$w "
      set opt($root) {}
   }
   return $w
}
proc ::hefesto::tk::create {cls w args} {
   node $cls $w {*}$args
   proc ::$w {args} "::hefesto::tk::widget [list $w] {*}\$args"
   return $w
}
proc ::hefesto::tk::drop {w} {
   variable class
   variable kids
   variable opt
   variable entries
   variable binds
   foreach c $kids($w) {drop $c}
   set p [parent_of $w]
   set i [lsearch -exact $kids($p) $w]
   if {$i >= 0} {set kids($p) [lreplace $kids($p) $i $i]}
   set cls $class($w)
   unset class($w) kids($w)
   array unset opt "[key $w] *"
   array unset entries [key $w]
   array unset binds "[key $w]\n*"
   if {[llength [info commands ::$w]]} {rename ::$w {}}
}
proc ::hefesto::tk::toplevel_of {w} {
   variable class
   while {$w ne "." && $class($w) ne "Toplevel"} {set w [parent_of $w]}
   return $w
}
namespace eval ::ttk {}
namespace eval ::hefesto::tk {
   foreach {cmd cls} {frame Frame toplevel Toplevel label Label button Button menubutton Menubutton
         checkbutton Checkbutton radiobutton Radiobutton entry Entry canvas Canvas scrollbar Scrollbar
         menu Menu panedwindow Panedwindow listbox Listbox text Text scale Scale message Message
         labelframe Labelframe spinbox Spinbox
         ttk::frame TFrame ttk::label TLabel ttk::button TButton ttk::entry TEntry ttk::treeview Treeview
         ttk::scrollbar TScrollbar ttk::checkbutton TCheckbutton ttk::radiobutton TRadiobutton
         ttk::menubutton TMenubutton ttk::combobox TCombobox ttk::labelframe TLabelframe
         ttk::panedwindow TPanedwindow ttk::notebook TNotebook ttk::separator TSeparator
         ttk::progressbar TProgressbar ttk::scale TScale ttk::spinbox TSpinbox ttk::sizegrip TSizegrip} {
      if {$cmd eq "label"} {set cmd tk_label}
      if {[llength [info commands ::$cmd]]} continue
      proc ::$cmd {w args} "::hefesto::tk::create $cls \$w {*}\$args"
   }
}
)tcl";

// Tk's commands on windows: winfo, wm, bind, event, destroy and the
// geometry managers (which place nothing).
constexpr std::string_view kCommands = R"tcl(
proc ::winfo {option args} {
   set w [lindex $args 0]
   switch -- $option {
      exists {return [::hefesto::tk::exists $w]}
      children {::hefesto::tk::need $w; return $::hefesto::tk::kids($w)}
      parent {::hefesto::tk::need $w; return [expr {$w eq "." ? "" : [::hefesto::tk::parent_of $w]}]}
      toplevel {::hefesto::tk::need $w; return [::hefesto::tk::toplevel_of $w]}
      class {::hefesto::tk::need $w; return $::hefesto::tk::class($w)}
      name {::hefesto::tk::need $w; return [expr {$w eq "." ? "magic" : [string range $w [expr {[string last . $w] + 1}] end]}]}
      ismapped - viewable {::hefesto::tk::need $w; return 1}
      manager {::hefesto::tk::need $w; return {}}
      width - height - reqwidth - reqheight {::hefesto::tk::need $w; return 1}
      depth {::hefesto::tk::need $w; return 24}
      visual {::hefesto::tk::need $w; return truecolor}
      id {::hefesto::tk::need $w; return 0x0}
      default {return 0}
   }
}
proc ::wm {option w args} {
   ::hefesto::tk::need $w
   if {$w ne "." && $::hefesto::tk::class($w) ne "Toplevel"} {
      return -code error "window \"$w\" isn't a top-level window"
   }
   set k "$w wm:$option"
   switch -- $option {
      title - iconname - geometry - protocol {
         if {$option eq "protocol"} {return {}}
         if {[llength $args] == 0} {
            if {[info exists ::hefesto::tk::opt($k)]} {return $::hefesto::tk::opt($k)}
            if {$option eq "geometry"} {return 1x1+0+0}
            return [expr {$w eq "." ? "magic" : [string range $w [expr {[string last . $w] + 1}] end]}]
         }
         set ::hefesto::tk::opt($k) [lindex $args 0]
      }
      withdraw {set ::hefesto::tk::opt($w\ wm:state) withdrawn}
      deiconify {set ::hefesto::tk::opt($w\ wm:state) normal}
      iconify {set ::hefesto::tk::opt($w\ wm:state) iconic}
      state {
         if {[info exists ::hefesto::tk::opt($w\ wm:state)]} {return $::hefesto::tk::opt($w\ wm:state)}
         return normal
      }
   }
   return {}
}
proc ::bind {tag args} {
   if {[string index $tag 0] eq "."} {::hefesto::tk::need $tag}
   set k "$tag\n[lindex $args 0]"
   switch -- [llength $args] {
      0 {
         set out {}
         foreach n [array names ::hefesto::tk::binds "[::hefesto::tk::key $tag]\n*"] {
            lappend out [lindex [split $n \n] 1]
         }
         return $out
      }
      1 {return [expr {[info exists ::hefesto::tk::binds($k)] ? $::hefesto::tk::binds($k) : ""}]}
   }
   set script [lindex $args 1]
   if {$script eq ""} {
      unset -nocomplain ::hefesto::tk::binds($k)
   } elseif {[string index $script 0] eq "+" && [info exists ::hefesto::tk::binds($k)]} {
      append ::hefesto::tk::binds($k) \n [string range $script 1 end]
   } else {
      set ::hefesto::tk::binds($k) [string trimleft $script +]
   }
   return {}
}
# `event generate W <seq> ?-x X -y Y -keysym K?`: W's binding for the
# sequence runs at the global level; a binding error is a background error
# (Tk's dialog, no console text), so it is dropped.
proc ::event {option args} {
   if {$option ne "generate"} {return {}}
   set w [lindex $args 0]
   ::hefesto::tk::need $w
   set seq [lindex $args 1]
   array set o {-x 0 -y 0 -keysym ??}
   array set o [lrange $args 2 end]
   foreach s [list $seq [string map {ButtonPress- Button-} $seq] [string map {Button- ButtonPress-} $seq]] {
      if {[info exists ::hefesto::tk::binds($w\n$s)]} {
         set script [string map [list %W $w %x $o(-x) %y $o(-y) %K $o(-keysym) %% %] $::hefesto::tk::binds($w\n$s)]
         catch {uplevel #0 $script}
         break
      }
   }
   return {}
}
proc ::destroy {args} {
   foreach w $args {
      if {$w ne "." && [::hefesto::tk::exists $w]} {::hefesto::tk::drop $w}
   }
   return {}
}
proc ::focus {args} {return {}}
proc ::lower {args} {return {}}
proc ::pack {args} {return {}}
proc ::place {args} {return {}}
proc ::tkwait {args} {return {}}
proc ::option {args} {return {}}
proc ::bell {args} {return {}}
proc ::grab {args} {return {}}
proc ::tk_popup {args} {return {}}
proc ::tk {option args} {
   switch -- $option {
      scaling {return 1.0}
      windowingsystem {return x11}
      appname {return magic}
   }
   return {}
}
proc ::image {option args} {
   switch -- $option {
      create {
         set name [lindex $args 1]
         if {$name eq "" || [string index $name 0] eq "-"} {set name image[incr ::hefesto::tk::images]}
         return $name
      }
      width - height {return 0}
   }
   return {}
}
proc ::font {option args} {
   if {$option eq "measure"} {return [expr {7 * [string length [lindex $args end]]}]}
   if {$option eq "metrics"} {return 13}
   return {}
}
proc ::tk_getOpenFile {args} {return {}}
proc ::tk_getSaveFile {args} {return {}}
proc ::tk_chooseDirectory {args} {return {}}
proc ::tk_messageBox {args} {return ok}
# Magic's Tcl dispatcher tries Tk's `label` and `grid` first, then Magic's.
proc ::label {args} {
   if {[string index [lindex $args 0] 0] eq "."} {return [::tk_label {*}$args]}
   return [uplevel 1 [list ::magic::label {*}$args]]
}
proc ::grid {args} {
   set first [lindex $args 0]
   if {[string index $first 0] eq "." || $first in {configure rowconfigure columnconfigure forget remove
         propagate info slaves bbox size location anchor content}} {
      return {}
   }
   return [uplevel 1 [list ::magic::grid {*}$args]]
}
if {![llength [info commands ::raise]]} {
   proc ::raise {w args} {::hefesto::tk::need $w; return {}}
}
::menu .winmenu -tearoff 0
)tcl";

// The widget commands: cget/configure, and the subcommands of menus,
// buttons, entries, ttk::treeview, canvases and listboxes the managers'
// scripts use.
constexpr std::string_view kWidgets = R"tcl(
proc ::hefesto::tk::widget {w args} {
   variable class
   if {![info exists class($w)]} {return -code error "invalid command name \"$w\""}
   set sub [lindex $args 0]
   set rest [lrange $args 1 end]
   if {$sub eq "cget"} {return [cget $w [lindex $rest 0]]}
   if {$sub eq "configure"} {return [configure $w {*}$rest]}
   switch -- $class($w) {
      Menu {return [menu_cmd $w $sub {*}$rest]}
      Button - TButton - Checkbutton - TCheckbutton - Radiobutton - TRadiobutton {return [button_cmd $w $sub]}
      Entry - TEntry - TCombobox - Spinbox - TSpinbox {return [entry_cmd $w $sub {*}$rest]}
      Treeview {return [tree_cmd $w $sub {*}$rest]}
      Canvas {return [canvas_cmd $w $sub]}
      Listbox {return [listbox_cmd $w $sub {*}$rest]}
   }
   return {}
}
proc ::hefesto::tk::cget {w name} {
   variable opt
   variable class
   variable textual
   if {[info exists opt($w\ $name)]} {return $opt($w\ $name)}
   if {$name eq "-text" && $class($w) ni $textual} {return -code error "unknown option \"$name\""}
   return {}
}
proc ::hefesto::tk::configure {w args} {
   variable opt
   if {[llength $args] == 0} {return {}}
   if {[llength $args] == 1} {return [list [lindex $args 0] {} {} {} [cget $w [lindex $args 0]]]}
   foreach {k v} $args {
      if {$k eq "-text"} {cget $w -text}
      set opt($w\ $k) $v
   }
   return {}
}
proc ::hefesto::tk::button_cmd {w sub} {
   variable class
   if {$sub ne "invoke" || [cget $w -state] eq "disabled"} {return {}}
   set var [cget $w -variable]
   if {[string match *Checkbutton $class($w)]} {
      if {$var eq ""} {set var [string range $w [expr {[string last . $w] + 1}] end]}
      set on [expr {[cget $w -onvalue] eq "" ? 1 : [cget $w -onvalue]}]
      set off [expr {[cget $w -offvalue] eq "" ? 0 : [cget $w -offvalue]}]
      upvar #0 $var v
      set v [expr {[info exists v] && $v eq $on ? $off : $on}]
   } elseif {[string match *Radiobutton $class($w)] && $var ne ""} {
      upvar #0 $var v
      set v [cget $w -value]
   }
   set c [cget $w -command]
   if {$c eq ""} {return {}}
   return [uplevel #0 $c]
}
)tcl";

// Menus (Tk's tkMenu.c indices: `end`/`last`, a number clamped to the last
// entry, `none`, or the first label matching a pattern).
constexpr std::string_view kMenus = R"tcl(
proc ::hefesto::tk::menu_index {m idx {insert 0}} {
   variable entries
   set n [llength $entries($m)]
   if {$idx in {end last}} {
      if {$insert} {return $n}
      return [expr {$n == 0 ? "none" : $n - 1}]
   }
   if {$idx in {none active} || [string index $idx 0] eq "@"} {return none}
   if {[string is integer -strict $idx]} {
      if {$insert} {return [expr {$idx > $n ? $n : $idx}]}
      if {$n == 0} {return none}
      return [expr {$idx >= $n ? $n - 1 : ($idx < 0 ? 0 : $idx)}]
   }
   set i 0
   foreach e $entries($m) {
      if {[dict exists $e -label] && [string match $idx [dict get $e -label]]} {return $i}
      incr i
   }
   return -code error "bad menu entry index \"$idx\""
}
proc ::hefesto::tk::menu_cmd {m sub args} {
   variable entries
   switch -- $sub {
      add {
         lappend entries($m) [dict create type [lindex $args 0] {*}[lrange $args 1 end]]
      }
      insert {
         set i [menu_index $m [lindex $args 0] 1]
         set entries($m) [linsert $entries($m) $i [dict create type [lindex $args 1] {*}[lrange $args 2 end]]]
      }
      delete {
         set a [menu_index $m [lindex $args 0]]
         set b [expr {[llength $args] > 1 ? [menu_index $m [lindex $args 1]] : $a}]
         if {$a ne "none"} {set entries($m) [lreplace $entries($m) $a $b]}
      }
      index {return [menu_index $m [lindex $args 0]]}
      type {
         set i [menu_index $m [lindex $args 0]]
         return [expr {$i eq "none" ? "" : [dict get [lindex $entries($m) $i] type]}]
      }
      entrycget {
         set i [menu_index $m [lindex $args 0]]
         if {$i eq "none"} {return {}}
         set e [lindex $entries($m) $i]
         set name [lindex $args 1]
         return [expr {[dict exists $e $name] ? [dict get $e $name] : ""}]
      }
      entryconfigure {
         set i [menu_index $m [lindex $args 0]]
         if {$i ne "none"} {
            set e [lindex $entries($m) $i]
            foreach {k v} [lrange $args 1 end] {dict set e $k $v}
            lset entries($m) $i $e
         }
      }
      invoke {
         set i [menu_index $m [lindex $args 0]]
         if {$i eq "none"} {return {}}
         return [menu_invoke [lindex $entries($m) $i]]
      }
   }
   return {}
}
proc ::hefesto::tk::menu_invoke {e} {
   set type [dict get $e type]
   if {$type in {separator tearoff cascade}} {return {}}
   if {[dict exists $e -state] && [dict get $e -state] eq "disabled"} {return {}}
   if {$type in {check radio} && [dict exists $e -variable]} {
      upvar #0 [dict get $e -variable] v
      if {$type eq "check"} {
         set v [expr {[info exists v] && $v eq "1" ? 0 : 1}]
      } elseif {[dict exists $e -value]} {
         set v [dict get $e -value]
      }
   }
   if {![dict exists $e -command]} {return {}}
   return [uplevel #0 [dict get $e -command]]
}
)tcl";

// Entries (text, or the -textvariable), ttk::treeview items (Tk's own ids
// I001, I002, ... in hexadecimal), canvases and listboxes.
constexpr std::string_view kItems = R"tcl(
proc ::hefesto::tk::etext {w} {
   set var [cget $w -textvariable]
   if {$var ne ""} {
      upvar #0 $var v
      return [expr {[info exists v] ? $v : ""}]
   }
   return [cget $w __text]
}
proc ::hefesto::tk::eset {w text} {
   variable opt
   set var [cget $w -textvariable]
   if {$var ne ""} {
      upvar #0 $var v
      set v $text
   } else {
      set opt($w\ __text) $text
   }
}
proc ::hefesto::tk::eindex {text idx} {
   set n [string length $text]
   if {$idx in {end insert}} {return $n}
   if {[string is integer -strict $idx]} {return [expr {$idx < 0 ? 0 : ($idx > $n ? $n : $idx)}]}
   return 0
}
proc ::hefesto::tk::entry_cmd {w sub args} {
   set t [etext $w]
   switch -- $sub {
      get {return $t}
      set {eset $w [lindex $args 0]}
      index {return [eindex $t [lindex $args 0]]}
      insert {
         set i [eindex $t [lindex $args 0]]
         eset $w [string range $t 0 [expr {$i - 1}]][lindex $args 1][string range $t $i end]
      }
      delete {
         set a [eindex $t [lindex $args 0]]
         set b [expr {[llength $args] > 1 ? [eindex $t [lindex $args 1]] : $a + 1}]
         if {$b > $a} {eset $w [string replace $t $a [expr {$b - 1}]]}
      }
   }
   return {}
}
proc ::hefesto::tk::tree_need {w id} {
   variable opt
   if {![info exists opt($w\ $id)]} {return -code error "Item $id not found"}
}
proc ::hefesto::tk::tree_cmd {w sub args} {
   variable opt
   variable serial
   set id [lindex $args 0]
   switch -- $sub {
      insert {
         tree_need $w $id
         set o [lrange $args 2 end]
         if {[dict exists $o -id]} {
            set new [dict get $o -id]
            dict unset o -id
            if {[info exists opt($w\ $new)]} {return -code error "Item $new already exists"}
         } else {
            set new [format I%03X [incr serial($w)]]
         }
         set kids $opt($w\ $id)
         set at [lindex $args 1]
         set opt($w\ $id) [expr {$at eq "end" ? [linsert $kids end $new] : [linsert $kids $at $new]}]
         set opt($w\ $new) {}
         set opt($w\ item:$new) $o
         set opt($w\ parent:$new) $id
         return $new
      }
      children {
         tree_need $w $id
         if {[llength $args] > 1} {set opt($w\ $id) [lindex $args 1]; return {}}
         return $opt($w\ $id)
      }
      exists {return [info exists opt($w\ $id)]}
      parent {tree_need $w $id; return $opt($w\ parent:$id)}
      item {
         tree_need $w $id
         set o [expr {[info exists opt($w\ item:$id)] ? $opt($w\ item:$id) : {}}]
         if {[llength $args] == 1} {return $o}
         if {[llength $args] == 2} {
            set name [lindex $args 1]
            return [expr {[dict exists $o $name] ? [dict get $o $name] : ""}]
         }
         foreach {k v} [lrange $args 1 end] {dict set o $k $v}
         set opt($w\ item:$id) $o
      }
      delete - detach {foreach i $id {tree_drop $w $i}}
   }
   return {}
}
proc ::hefesto::tk::tree_drop {w id} {
   variable opt
   if {$id eq "" || ![info exists opt($w\ $id)]} {return}
   foreach c $opt($w\ $id) {tree_drop $w $c}
   set p $opt($w\ parent:$id)
   set i [lsearch -exact $opt($w\ $p) $id]
   if {$i >= 0} {set opt($w\ $p) [lreplace $opt($w\ $p) $i $i]}
   unset opt($w\ $id) opt($w\ parent:$id)
   unset -nocomplain opt($w\ item:$id)
}
proc ::hefesto::tk::canvas_cmd {w sub} {
   if {$sub eq "create"} {return [incr ::hefesto::tk::serial($w)]}
   return {}
}
proc ::hefesto::tk::listbox_cmd {w sub args} {
   variable opt
   set l [cget $w __items]
   switch -- $sub {
      insert {set opt($w\ __items) [linsert $l [expr {[lindex $args 0] eq "end" ? "end" : [lindex $args 0]}] {*}[lrange $args 1 end]]}
      delete {set opt($w\ __items) {}}
      get {return [lindex $l [lindex $args 0]]}
      size {return [llength $l]}
   }
   return {}
}
)tcl";

// openwrapper's widgets (wrapper.tcl:1080-1190,1384): the menus come from
// the view model as {key {kind label command variable} ...} pairs.
constexpr std::string_view kWrapper = R"tcl(
proc ::hefesto::tk::wrapper {f menus} {
   set l $f.pane.top
   set win $l.magic
   create Toplevel $f
   create Panedwindow $f.pane -orient vertical
   create Frame $l
   create Frame $f.pane.bot
   create Frame $l.xscroll
   create Frame $l.yscroll
   foreach {s lb ub} {x left right y down up} {
      create Canvas $l.${s}scroll.bar
      create Button $l.${s}scroll.lb -command "$win scroll $lb .1 w"
      create Button $l.${s}scroll.ub -command "$win scroll $ub .1 w"
   }
   create Button $l.zb -borderwidth 1 -command "$win zoom 2"
   create Frame $l.titlebar
   create Label $l.titlebar.caption -text "Loaded: none Editing: none Tool: box"
   create Label $l.titlebar.message -text ""
   create Label $l.titlebar.pos -text ""
   create Frame $l.titlebar.mbuttons
   foreach {key text} {file File edit Edit cell Cell win Window layers Layers drc Drc opts Options} {
      create Menubutton $l.titlebar.mbuttons.$key -text $text -relief raised \
            -menu $l.titlebar.mbuttons.$key.toolmenu -borderwidth 2
   }
   create Checkbutton $l.titlebar.drcbutton -text DRC -anchor w -borderwidth 2 -variable Opts(drc) \
         -command { if { $Opts(drc) } { drc on } else { drc off } }
   node Magic $win
   if {[llength [info commands ::hefesto::tk::window_command]]} {::hefesto::tk::window_command $win}
   create Frame $l.toolbar
   foreach {key items} $menus {
      set m [create Menu $l.titlebar.mbuttons.$key.toolmenu -tearoff 0]
      foreach item $items {
         lassign $item kind text command variable
         switch -- $kind {
            separator {$m add separator}
            check {$m add check -label $text -variable $variable -command $command}
            default {$m add command -label $text -command $command}
         }
      }
   }
   .winmenu add radio -label $f -variable Opts(target) -value $win
   bind $win <Enter> "focus %W ; set Opts(focus) $f"
   return $win
}
# toolbar.tcl's maketoolbar: the rows of the view model's layers.
proc ::hefesto::tk::toolbar_ready {l} {
   global Winopts Opts
   set top [winfo toplevel $l]
   if {[info exists Winopts($top,suspend)] && $Winopts($top,suspend) > 0} {return 0}
   if {![info exists Opts(toolbar)] || $Opts(toolbar) == 0} {set Opts(toolbar) 1}
   winfo children $l.toolbar
   return 1
}
proc ::hefesto::tk::toolbar {l layers locked} {
   foreach c [winfo children $l.toolbar] {destroy $c}
   create Canvas $l.toolbar.canvas
   create Frame $l.toolbar.canvas.frame
   foreach layer $layers {
      set row $l.toolbar.canvas.frame.f$layer
      create Frame $row
      create Label $row.l -text $layer
      if {$layer in $locked} {
         create Button $row.p -image pale_$layer
      } else {
         create Button $row.b -image img_$layer
      }
   }
   create Scrollbar $l.toolbar.vscroll -orient vertical -command [list $l.toolbar.canvas yview]
}
)tcl";

// The procedures of wrapper.tcl's tags (wrapper.tcl:567-583) over the model:
// a technology load rebuilds each layout window's toolbar
// (techrebuild, wrapper.tcl:823-845); the others redraw labels and scroll
// bars, which the model does not keep.
constexpr std::string_view kTagProcs = R"tcl(
proc ::hefesto::tk::techrebuild {win {cmdstr ""}} {
   global Opts
   if {$win eq ""} {
      foreach w [magic::windownames layout] {::hefesto::tk::techrebuild $w $cmdstr}
      return
   }
   if {![::hefesto::tk::exists $win]} {return}
   set l [::hefesto::tk::parent_of $win]
   if {$cmdstr eq "load"} {
      set Opts(toolbar) 0
      magic::maketoolbar $l
   } elseif {$cmdstr in {lock unlock revert}} {
      magic::maketoolbar $l
   }
}
foreach p {scrollupdate toolupdate drcupdate cursorview} {
   if {![llength [info commands ::magic::$p]]} {proc ::magic::$p {args} {}}
}
proc ::magic::captions {{subcommand {}}} {
   global Opts
   if {$subcommand ne {} && $subcommand ne "writeable" && $subcommand ne "load"} {return}
   foreach win [magic::windownames layout] {
      catch {
         set caption [$win windowcaption]
         set edit [expr {[lindex $caption 1] eq "EDITING" ? [lindex $caption 2] : $caption}]
         if {![info exists Opts(tool)]} {set Opts(tool) unknown}
         [winfo parent $win].titlebar.caption configure -text \
               "Loaded: [lindex $caption 0] Editing: $edit Tool: $Opts(tool)    Technology: [tech name]"
      }
   }
}
proc ::magic::mgrupdate {win {cmdstr ""}} {
   if {$cmdstr in {read delete rename writeable}} {magic::captions}
}
# wrapper.tcl:655-691: the position label. Magic reads `box values` again
# only from the outermost Tcl level (`box` calls `box`); the engine's tags
# run at the global level, so the nested call is recognized by a flag.
proc ::magic::boxview {win {cmdstr ""}} {
   if {$cmdstr in {exists help ""}} {return}
   if {$cmdstr eq "remove"} {
      set f [winfo parent $win]
      if {$f ne "."} {$f.titlebar.pos configure -text "no box"}
      return
   }
   if {[info level] > 1 || [info exists ::hefesto::tk::boxview]} {return}
   if {$win eq {}} {
      foreach w [magic::windownames layout] {magic::boxview $w}
      return
   }
   set f [winfo parent $win]
   if {$f eq "."} {return}
   set ::hefesto::tk::boxview 1
   set code [catch {
      set units [units list]
      units microns noprint
      set b [$win box values]
      set v {}
      foreach i {0 1 2 3} {
         set c [lindex $b $i]
         lappend v [expr {$c == int($c) ? int($c) : $c}]
      }
      units {*}$units
      $f.titlebar.pos configure -text [format "box (%+g %+g) to (%+g %+g) microns" {*}$v]
   } result]
   unset ::hefesto::tk::boxview
   return -code $code $result
}
# tools.tcl:14-50: suspend counts per wrapper frame in Winopts.
proc ::magic::suspendall {} {
   global Winopts
   foreach win [magic::windownames layout] {
      if {$win == 0} continue
      set f [winfo toplevel $win]
      if {$f eq "."} {set f $win}
      if {[incr Winopts($f,suspend)] == 1} {$win updatedisplay suspend}
   }
}
proc ::magic::resumeall {} {
   global Winopts
   foreach win [magic::windownames layout] {
      if {$win == 0} continue
      set f [winfo toplevel $win]
      if {$f eq "."} {set f $win}
      if {$Winopts($f,suspend) <= 0} {error "resume called without suspend"}
      if {[incr Winopts($f,suspend) -1] <= 0} {
         unset Winopts($f,suspend)
         $win updatedisplay resume
      }
   }
}
namespace eval :: {namespace import -force ::magic::suspendall ::magic::resumeall}
# The PDK menus of toolkit.tcl (add_toolkit_menu and its items), which the
# engine keeps in its own model, become menu buttons and menus as Magic's
# procedures make them (toolkit.tcl:144-186).
proc ::hefesto::tk::toolkit {command code result op} {
   if {$code != 0} return
   set words [lrange $command 1 end]
   switch -- [namespace tail [lindex $command 0]] {
      add_toolkit_menu {
         lassign $words f text library
         if {$library eq ""} {set library toolkit}
         catch {
            menubutton $f.titlebar.mbuttons.$library -text $text -relief raised \
                  -menu $f.titlebar.mbuttons.$library.toolmenu -borderwidth 2
            menu $f.titlebar.mbuttons.$library.toolmenu -tearoff 0
         }
      }
      add_toolkit_button {
         set library [expr {[llength $words] > 3 ? [lindex $words 3] : "toolkit"}]
         catch {[lindex $words 0].titlebar.mbuttons.$library.toolmenu add command -label [lindex $words 1] \
               -command "magic::gencell $library::[lindex $words 2] {} [lrange $words 4 end]"}
      }
      add_toolkit_command {
         set library [expr {[llength $words] > 3 ? [lindex $words 3] : "toolkit"}]
         catch {[lindex $words 0].titlebar.mbuttons.$library.toolmenu add command -label [lindex $words 1] \
               -command "[lindex $words 2] [lrange $words 4 end]"}
      }
      add_toolkit_separator {
         set library [expr {[llength $words] > 1 ? [lindex $words 1] : "toolkit"}]
         catch {[lindex $words 0].titlebar.mbuttons.$library.toolmenu add separator}
      }
   }
}
foreach c {add_toolkit_menu add_toolkit_button add_toolkit_command add_toolkit_separator} {
   foreach n [list ::magic::$c ::$c] {
      if {[llength [info commands $n]]} {trace add execution $n leave ::hefesto::tk::toolkit}
   }
}
if {![llength [info commands ::magic::techrebuild]]} {
   proc ::magic::techrebuild {args} {::hefesto::tk::techrebuild {*}$args}
}
)tcl";

// The manager windows (cellmgr.tcl:84-150,248-290, libmgr.tcl:47-120,206-263,
// wrapper.tcl:305-531), filled from the view models: `create` builds a
// window once (withdrawn); any other option is ignored until it exists.
constexpr std::string_view kManagers = R"tcl(
proc ::hefesto::tk::manager_window {w option} {
   if {[exists $w]} {return [expr {$option ne "create"}]}
   if {$option ne "create"} {return 0}
   create Toplevel $w
   wm withdraw $w
   return 1
}
proc ::hefesto::tk::target_bar {w} {
   create Label $w.target.name -text "Target window:"
   create Menubutton $w.target.list -text default -menu $w.target.list.winmenu
}
proc ::hefesto::tk::cellmanager {option rows} {
   set w .cellmgr
   set new [expr {![exists $w]}]
   if {![manager_window $w $option]} return
   if {$new} {
      foreach f {actionbar box target} {create Frame $w.$f}
      create Treeview $w.box.view -show tree -selectmode browse -columns 1
      create Scrollbar $w.box.vert -orient vertical -command "$w.box.view yview"
      foreach {b text action} {done Zoom zoom edit Edit edit load Load load expand Expand expand place Place place} {
         create Button $w.actionbar.$b -text $text -command "magic::instcallback $action"
      }
      create Button $w.actionbar.refresh -text Refresh -command {magic::cellmanager update}
      target_bar $w
   }
   foreach row $rows {
      lassign $row id text instance parent
      if {![$w.box.view exists $id]} {
         $w.box.view insert $parent end -id $id -text $text
         $w.box.view item $id -values [list $instance]
      }
   }
}
proc ::hefesto::tk::libmanager {option dirs rows} {
   if {$option ni {update create save}} return
   set w .libmgr
   set new [expr {![exists $w]}]
   if {![manager_window $w $option]} return
   if {$new} {
      set ::filtered 1
      foreach f {actionbar box target} {create Frame $w.$f}
      create Treeview $w.box.view -selectmode browse -columns 0
      create Scrollbar $w.box.vert -orient vertical -command "$w.box.view yview"
      foreach {b text action} {load Load load place Place place pick Pick pick} {
         create Button $w.actionbar.$b -text $text -command "magic::libcallback $action"
      }
      create Button $w.actionbar.refresh -text Refresh -command {magic::libmanager update}
      create Checkbutton $w.actionbar.filter -text Filter -variable filtered -command {magic::libmanager update}
      target_bar $w
   }
   $w.box.view delete [$w.box.view children {}]
   foreach dir $dirs {
      if {![$w.box.view exists $dir]} {$w.box.view insert {} end -id $dir -text $dir}
   }
   foreach row $rows {
      lassign $row dir cell tech
      if {![$w.box.view exists $dir$cell.mag]} {
         $w.box.view insert $dir end -id $dir$cell.mag -text $cell -values [list $tech]
      }
   }
}
proc ::hefesto::tk::techmanager {option menus labels} {
   set w .techmgr
   set new [expr {![exists $w]}]
   if {![manager_window $w $option]} return
   if {$new} {
      create Frame $w.title
      create Label $w.title.tlab -text "Technology: "
      create Menubutton $w.title.tname -text "(none)" -menu $w.title.tname.menu
      create Label $w.title.tvers -text ""
      create Label $w.subtitle -text ""
      create Frame $w.lambda0
      create Label $w.lambda0.llab -text "Microns per lambda (CIF): "
      create Label $w.lambda0.lval -text 1
      create Frame $w.lambda1
      create Label $w.lambda1.llab -text "Internal units per lambda: "
      create Entry $w.lambda1.lval0 -width 3
      create Label $w.lambda1.ldiv -text " / "
      create Entry $w.lambda1.lval1 -width 3
      foreach {f text} {cif0 "CIF input style: " cif1 "CIF output style: "} {
         create Frame $w.$f
         create Label $w.$f.llab -text $text
         create Menubutton $w.$f.lstyle -text "" -menu $w.$f.lstyle.menu
         create Label $w.$f.llab2 -text " Microns/lambda="
         create Label $w.$f.llambda -text ""
      }
      foreach {f text} {extract "Extract style: " drc "DRC style: "} {
         create Frame $w.$f
         create Label $w.$f.llab -text $text
         create Menubutton $w.$f.lstyle -text "" -menu $w.$f.lstyle.menu
      }
      foreach m {title.tname cif0.lstyle cif1.lstyle extract.lstyle drc.lstyle} {create Menu $w.$m.menu -tearoff 0}
   }
   if {$option in {init create}} {
      foreach {m entries} $menus {
         if {$option eq "init"} {$w.$m.menu delete 0 end}
         foreach entry $entries {$w.$m.menu add command -label [lindex $entry 0] -command [lindex $entry 1]}
      }
   }
   foreach {label text} $labels {$label configure -text $text}
}
)tcl";

// The parameter dialog `.params` of toolkit.tcl's gencell_dialog
// (toolkit.tcl:1594-1760, add_entry/add_checkbox/add_message/
// add_selectlist 1289-1480): rebuilt from the engine's dialog model
// (::hefesto::tk::params_model, a list `{mode type library cell instance
// widgets buttons}`, empty while closed) after each gencell command.
constexpr std::string_view kParams = R"tcl(
proc ::hefesto::tk::params_sync {args} {
   variable params_shown
   if {![llength [info commands ::hefesto::tk::params_model]]} return
   set model [::hefesto::tk::params_model]
   if {[info exists params_shown] && $params_shown eq $model && ($model eq "" || [exists .params])} return
   set params_shown $model
   if {[exists .params]} {destroy .params}
   if {$model eq ""} return
   lassign $model mode type library cell instance widgets buttons
   set p .params
   create Toplevel $p
   create Frame $p.title
   create Label $p.title.lab1 -text "[expr {$mode eq "edit" ? "Edit device" : "New device"}]:"
   create Label $p.title.lab2 -text $type
   create Label $p.title.lab3 -text Library:
   create Label $p.title.lab4 -text $library
   create Label $p.title.clab -text Cellname:
   create Label $p.title.glab -text $cell
   create Label $p.title.ilab -text Instance:
   create Entry $p.title.ient
   $p.title.ient insert 0 $instance
   create TSeparator $p.sep
   create Frame $p.body
   create Canvas $p.body.area
   create Scrollbar $p.body.sb -command "$p.body.area yview"
   create Frame $p.buttons
   set e [create Frame $p.body.area.edits]
   foreach w $widgets {
      lassign $w kind pname text
      create Label $e.${pname}_lab -text $text
      switch -- $kind {
         entry {create Entry $e.${pname}_ent -textvariable magic::${pname}_val}
         checkbox {create Checkbutton $e.${pname}_chk -variable magic::${pname}_val}
         message {create Label $e.${pname}_txt -text [lindex $w 3]}
         default {
            create Menubutton $e.${pname}_sel -menu $e.${pname}_sel.menu -text [lindex $w 3]
            create Menu $e.${pname}_sel.menu -tearoff 0
         }
      }
   }
   foreach b $buttons name {apply okay reset close} {
      if {$b eq ""} continue
      lassign $b text script closes
      if {$closes} {set script [expr {$script eq "" ? "destroy .params" : "$script ; destroy .params"}]}
      create Button $p.buttons.$name -text $text -command $script
   }
}
foreach c {gencell gencell_dialog gencell_change gencell_create gencell_update} {
   foreach n [list ::magic::$c ::$c] {
      if {[llength [info commands $n]]} {trace add execution $n leave ::hefesto::tk::params_sync}
   }
}
)tcl";

constexpr std::string_view kInstalled = "::hefesto::tk::class(.)";

std::string tcl_list(const std::vector<std::string>& words) {
  std::string out;
  for (const std::string& word : words) {
    if (!out.empty()) {
      out += ' ';
    }
    out += script::quote_list_element(word);
  }
  return out;
}

std::string_view kind_name(WrapperMenuItem::Kind kind) {
  switch (kind) {
    case WrapperMenuItem::Kind::separator:
      return "separator";
    case WrapperMenuItem::Kind::check:
      return "check";
    default:
      return "command";
  }
}

// The menus as `{key items ...}` for ::hefesto::tk::wrapper, keyed by
// openwrapper's menu button names in the view model's menu order.
std::string menu_words(const WrapperMenuModel& menus) {
  static constexpr std::array<std::string_view, 7> kKeys = {"file", "edit", "cell", "win", "layers", "drc", "opts"};
  std::vector<std::string> words;
  for (std::size_t i = 0; i < menus.menus.size() && i < kKeys.size(); ++i) {
    std::vector<std::string> items;
    for (const WrapperMenuItem& item : menus.menus[i].items) {
      items.push_back(tcl_list({std::string{kind_name(item.kind)}, item.label, item.command, item.variable}));
    }
    words.emplace_back(kKeys[i]);
    words.push_back(tcl_list(items));
  }
  return tcl_list(words);
}

}  // namespace

cmd::CommandResult install_tk_model(script::Interpreter& interp) {
  if (has_tk_model(interp)) {
    return cmd::CommandResult::success();
  }
  for (const std::string_view part : {kTree, kCommands, kWidgets, kMenus, kItems, kWrapper, kTagProcs, kManagers, kParams}) {
    cmd::CommandResult r = interp.evaluate(part, cmd::Origin::internal);
    if (!r.ok()) {
      return r;
    }
  }
  std::string tags;
  for (const auto& [command, script] : gui_script_tags(GuiScript::wrapper)) {
    tags += tcl_list({"magic::tag", "add", command, script}) + "\n";
  }
  return interp.evaluate(tags, cmd::Origin::internal);
}

bool has_tk_model(const script::Interpreter& interp) {
  return interp.get_var(kInstalled).has_value();
}

cmd::CommandResult build_wrapper_widgets(script::Interpreter& interp, std::string_view frame,
                                         const WrapperMenuModel& menus) {
  return interp.evaluate(tcl_list({"::hefesto::tk::wrapper", std::string{frame}, menu_words(menus)}),
                         cmd::Origin::internal);
}

cmd::CommandResult build_toolbar_widgets(script::Interpreter& interp, std::string_view layoutframe,
                                         const std::function<ToolbarModel()>& toolbar) {
  const std::string frame{layoutframe};
  cmd::CommandResult ready = interp.evaluate(tcl_list({"::hefesto::tk::toolbar_ready", frame}), cmd::Origin::internal);
  if (!ready.ok() || ready.value.to_string() != "1") {
    return ready.ok() ? cmd::CommandResult::success() : ready;
  }
  const ToolbarModel model = toolbar();
  return interp.evaluate(
      tcl_list({"::hefesto::tk::toolbar", frame, tcl_list(model.layers), tcl_list(model.locked)}),
      cmd::Origin::internal);
}

namespace {

std::string cell_rows(const CellManagerModel& model) {
  std::vector<std::string> rows;
  for (const CellManagerRow& row : model.rows) {
    const std::size_t slash = row.id.rfind('/');
    const std::string parent = slash == std::string::npos ? std::string{} : row.id.substr(0, slash);
    rows.push_back(tcl_list({row.id, row.cell, row.instance, parent}));
  }
  return tcl_list(rows);
}

std::string library_rows(const LibraryManagerModel& model) {
  std::vector<std::string> rows;
  for (const LibraryManagerRow& row : model.rows) {
    rows.push_back(tcl_list({row.directory, row.cell, row.technology}));
  }
  return tcl_list(rows);
}

// A style menu's entries: `magic::<command> <style> ;  magic::techmanager update`.
std::string style_entries(const std::vector<std::string>& styles, const std::string& command) {
  std::vector<std::string> entries;
  for (const std::string& style : styles) {
    entries.push_back(tcl_list({style, "magic::" + command + " " + style + " ;  magic::techmanager update"}));
  }
  return tcl_list(entries);
}

std::string tech_menus(const TechManagerModel& model) {
  std::vector<std::string> techs;
  for (const TechMenuEntry& entry : model.tech_menu) {
    const std::string command = entry.command.starts_with("tech ") ? "magic::" + entry.command : entry.command;
    techs.push_back(tcl_list({entry.label, command + " ;  magic::techmanager update"}));
  }
  return tcl_list({"title.tname", tcl_list(techs), "cif0.lstyle", style_entries(model.cif_input_styles, "cif istyle"),
                   "cif1.lstyle", style_entries(model.cif_output_styles, "cif ostyle"), "extract.lstyle",
                   style_entries(model.extract_styles, "extract style"), "drc.lstyle",
                   style_entries(model.drc_styles, "drc style")});
}

std::string tech_labels(const TechManagerModel& model) {
  return tcl_list({".techmgr.title.tname", model.technology, ".techmgr.title.tvers", model.version,
                   ".techmgr.subtitle", model.description, ".techmgr.lambda0.lval", model.microns_per_lambda,
                   ".techmgr.cif0.lstyle", model.cif_input_style, ".techmgr.cif1.lstyle", model.cif_output_style,
                   ".techmgr.cif1.llambda", model.microns_per_lambda, ".techmgr.extract.lstyle",
                   model.extract_style, ".techmgr.drc.lstyle", model.drc_style});
}

}  // namespace

std::string params_dialog_words(const pcell::DialogModel* dialog) {
  if (dialog == nullptr) {
    return {};
  }
  std::vector<std::string> widgets;
  for (const pcell::DialogWidget& w : dialog->widgets) {
    static constexpr std::array<std::string_view, 5> kKinds = {"entry", "checkbox", "selectlist", "selectindex",
                                                              "message"};
    widgets.push_back(tcl_list({std::string{kKinds[static_cast<std::size_t>(w.kind)]}, w.parameter, w.label, w.value}));
  }
  std::vector<std::string> buttons;
  for (const pcell::DialogButton& b : dialog->buttons) {
    buttons.push_back(tcl_list({b.text, b.script, b.closes ? "1" : "0"}));
  }
  const bool edit = dialog->mode == pcell::DialogMode::edit_device;
  return tcl_list({edit ? "edit" : "new", dialog->device, dialog->library, dialog->cell_name, dialog->instance,
                   tcl_list(widgets), tcl_list(buttons)});
}

cmd::CommandResult update_manager_widgets(cmd::CommandContext& ctx, script::Interpreter& interp,
                                          ManagerWindow window, std::string_view option) {
  const std::string opt{option};
  std::string script;
  switch (window) {
    case ManagerWindow::cell:
      script = tcl_list({"::hefesto::tk::cellmanager", opt, cell_rows(cell_manager_model(ctx))});
      break;
    case ManagerWindow::library: {
      const auto filtered = interp.get_var("filtered");
      const LibraryManagerModel model = library_manager_model(ctx, !filtered || filtered->to_string() != "0");
      script = tcl_list({"::hefesto::tk::libmanager", opt, tcl_list(model.directories), library_rows(model)});
      break;
    }
    case ManagerWindow::technology: {
      const TechManagerModel model = tech_manager_model(ctx);
      script = tcl_list({"::hefesto::tk::techmanager", opt, tech_menus(model), tech_labels(model)});
      break;
    }
  }
  return interp.evaluate(script, cmd::Origin::internal);
}

std::string tk_file_types(const std::vector<std::pair<std::string, std::string>>& types) {
  std::string out;
  for (const auto& [description, patterns] : types) {
    const std::string desc = description.find(' ') == std::string::npos ? description : '"' + description + '"';
    out += out.empty() ? "{" : " {";
    out += desc + (patterns == "*" ? " {*}}" : " {" + patterns + " {" + patterns + "}}}");
  }
  return "{" + out + "}";
}

std::optional<platform::Path> TkFilePicker::pick(const platform::FilePickerRequest& request) {
  script::Interpreter* interp = editor_.script.interp.get();
  if (interp == nullptr) {
    return std::nullopt;
  }
  const bool open = request.mode == platform::FilePickerRequest::Mode::open;
  std::string script = std::string{open ? "tk_getOpenFile" : "tk_getSaveFile"} + " -filetypes " +
                       tk_file_types(request.file_types);
  if (!request.title.empty()) {
    script += " -title " + script::quote_list_element(request.title);
  }
  const cmd::CommandResult chosen = interp->evaluate(script, cmd::Origin::internal);
  if (!chosen.ok() || chosen.value.to_string().empty()) {
    return std::nullopt;
  }
  return platform::Path{chosen.value.to_string()};
}

const std::vector<std::string>& wrapper_global_commands() {
  static const std::vector<std::string> kNames = [] {
    std::vector<std::string> names = gui_entry_command_names();
    names.insert(names.end(), {"drc_save_report", "drc_load_report", "saveallandquit"});
    return names;
  }();
  return kNames;
}

cmd::CommandResult import_wrapper_commands(script::Interpreter& interp) {
  std::string script;
  for (const std::string& name : wrapper_global_commands()) {
    if (!interp.has_command("::" + name) && interp.has_command("::magic::" + name)) {
      script += "namespace import ::magic::" + name + "\n";
    }
  }
  return script.empty() ? cmd::CommandResult::success() : interp.evaluate(script, cmd::Origin::internal);
}

}  // namespace hefesto::gui
