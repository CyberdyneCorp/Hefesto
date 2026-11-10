// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Magic's help strings (see magic_help.hpp). Transcribed from the
// WindAddCommand calls of windows/windClient.c (`*window`),
// dbwind/DBWcommands.c (`layout`) and cmwind/CMWmain.c (`color`) at
// fc5daa9e, keeping the branches compiled in the Tcl build (MAGIC_WRAPPER,
// the optional modules of the reference build) as checked against the help
// listing of golden case UI-G-005. Owner: DISPATCH.

#include "hefesto/ui/magic_help.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iterator>

namespace hefesto::ui {

namespace {

// A C array, not a deduced std::array: Clang 14 with libstdc++ rejects very
// large deduced initializers.
const MagicHelpEntry kEntries[] = {
    {"*window", "*bypass", "*bypass command\trun command independently of the command line"},
    {"*window", "*crash", "*crash\t\t\tcause a core dump"},
    {"*window", "*files", "*files\t\t\tprint out currently open files"},
    {"*window", "*grstats", "*grstats\t\tprint out stats on graphics"},
    {"*window", "*pause", "*pause\t[args]\t\tprint args and wait for <cr>"},
    {"*window", "*winddebug", "*winddebug\t\tset debugging mode"},
    {"*window", "*winddump", "*winddump\t\tprint out debugging info"},
    {"*window", "center", "center [x y]\t\tcenter window on the cursor or indicated coordinate"},
    {"*window", "closewindow", "closewindow [name]\tclose a window"},
    {"*window", "cursor", "cursor\t\t\treturn magic coordinates of the cursor"},
    {"*window", "grow", "grow\t\t\tblow a window up to full-screen size or back again"},
    {"*window", "help", "help [pattern]\t\tprint out synopses for all commands valid\n\t\t\tin the current window (or just those\n\t\t\tcontaining pattern)"},
    {"*window", "imacro", "imacro [char [string]] define or print an interactive macro called char"},
    {"*window", "logcommands", "logcommands [file [update]]\n\t\t\tlog all commands into a file"},
    {"*window", "macro", "macro [char [string]]  define or print a macro called char"},
    {"*window", "openwindow", "openwindow [cell][name]\n\t\t\topen a new window with indicated name, bound to indicated cell"},
    {"*window", "over", "over\t\t\tmove a window over (on top of) the rest"},
    {"*window", "pushbutton", "pushbutton button act\tpush a mouse button"},
    {"*window", "quit", "quit\t\t\texit magic"},
    {"*window", "redo", "redo [count]\t\tredo commands"},
    {"*window", "redraw", "redraw\t\t\tredraw the display"},
    {"*window", "reset", "reset\t\t\treset the display"},
    {"*window", "scroll", "scroll dir [amount]\tscroll the window"},
    {"*window", "setpoint", "setpoint [x y [WID]]\tforce to cursor (point) to x,y in window WID"},
    {"*window", "sleep", "sleep seconds\t\tsleep for a number of seconds"},
    {"*window", "specialopen", "specialopen [coords] type [args]\n\t\t\topen a special window"},
    {"*window", "underneath", "underneath\t\tmove a window underneath the rest"},
    {"*window", "undo", "undo [count]\t\tundo commands"},
    {"*window", "updatedisplay", "updatedisplay [suspend|resume]\n\t\t\tforce display update, or suspend/resume updates"},
    {"*window", "version", "version\t\t\tprint out version info"},
    {"*window", "view", "view [get]              zoom window out so everything is visible"},
    {"*window", "windowborder", "windowborder [on|off]\ttoggle border drawing for new windows"},
    {"*window", "windowcaption", "windowcaption [on|off]\ttoggle title caption for new windows"},
    {"*window", "windownames", "windownames [all|type]\tget name of current or all windows"},
    {"*window", "windowpositions", "windowpositions [file]\tprint out window positions"},
    {"*window", "windowscrollbars", "windowscrollbars [on|off]\n\t\t\ttoggle scroll bars for new windows"},
    {"*window", "xview", "xview               \tzoom window out so everything is unexpanded"},
    {"*window", "zoom", "zoom amount\t\tzoom window by amount"},
    {"layout", "*coord", "*coord\t\t\tshow coordinates of various things"},
    {"layout", "*extract", "*extract [args]\tdebug the circuit extractor"},
    {"layout", "*garoute", "*garoute [cmd [args]]\tdebug the gate-array router"},
    {"layout", "*groute", "*groute [cmd [args]]\tdebug the global router"},
    {"layout", "*iroute", "*iroute [cmd [args]]\tdebug the interactive router"},
    {"layout", "*mzroute", "*mzroute [cmd [args]]\tdebug the maze router"},
    {"layout", "*plow", "*plow cmd [args]\tdebug plowing"},
    {"layout", "*psearch", "*psearch plane count\tinvoke point search over box area"},
    {"layout", "*seeflags", "*seeflags [flag]\tdisplay channel flags over channel"},
    {"layout", "*showmem", "*showmem [file]\tprint internal memory usage"},
    {"layout", "*showtech", "*showtech [file]\tprint internal technology tables"},
    {"layout", "*tilestats", "*tilestats [file]\tprint statistics on tile utilization"},
    {"layout", "*tsearch", "*tsearch plane count\tinvoke area search over box area"},
    {"layout", "*watch", "*watch [plane]\t\tenable verbose tile plane redisplay"},
    {"layout", "addpath", "addpath [path]\t\tappend to current search path"},
    {"layout", "antennacheck", "antennacheck [path]\tcheck for antenna violations"},
    {"layout", "archive", "archive write|read file\n\t\t\twrite or read the archive file \"file\"."},
    {"layout", "array", "array xsize ysize\tOR\narray xlo xhi ylo yhi\n\t\t\tarray everything in selection"},
    {"layout", "box", "box [dir [amount]]\tmove box dist units in direction or (with\n\t\t\tno arguments) show box size"},
    {"layout", "calma", "calma option\t\tCalma GDS-II stream file processor; type\n\t\t\t\"calma help\" for information on options"},
    {"layout", "cellname", "cellname [list] children|parents|exists|self [name]\tOR\ncellname [list] allcells|topcells\n\t\t\tlist cells by relationship to cell \"name\", or\n\t\t\tthe current selection if no name given.  \"list\"\n\t\t\treturns result as a list. \"exists + name\" returns\n\t\t\ttrue or false (also see command instance)"},
    {"layout", "channels", "channels\t\tsee channels (feedback) without doing routing"},
    {"layout", "cif", "cif option\t\tCIF processor; type \"cif help\"\n\t\t\tfor information on options"},
    {"layout", "clockwise", "clockwise [deg]\t\trotate selection and box clockwise"},
    {"layout", "contact", "contact type\t\tpaint contact type on the intersection of its\n\t\t\tresidues"},
    {"layout", "copy", "copy [dir [amount]]\tOR\ncopy to x y\t\tcopy selection:  what used to be at box\n\t\t\tlower-left will be copied at cursor location (or,\n\t\t\tcopy will appear amount units in dir from original);\n\t\t\tsecond form will copy to location x y"},
    {"layout", "corner", "corner d1 d2 [layers]\tmake L-shaped wires inside box, filling\n\t\t\tfirst in direction d1, then in d2"},
    {"layout", "crash", "crash save|recover [file]\n\t\t\trecover the crash file \"file\", or the first\n\t\t\tcrash file discovered."},
    {"layout", "crosshair", "crosshair x y | off\tenable and move or disable the screen crosshair"},
    {"layout", "def", "def [options]\tDEF-format input; type \"def help\"\n\t\t\tfor information on options"},
    {"layout", "delete", "delete\t\t\tdelete everything in selection"},
    {"layout", "down", "down\t\t\tload selected cell into a window"},
    {"layout", "drc", "drc option\t\tdesign rule checker; type \"drc help\"\n\t\t\tfor information on options"},
    {"layout", "drop", "drop layers\t\tcopy layers from edit cell into\n\t\t\tsubcells containing selected paint"},
    {"layout", "dump", "dump cell [child refPointC] [parent refPointP]\n\t\t\tcopy contents of cell into edit cell, so that\n\t\t\trefPointC (or lower left) of cell is at refPointP\n\t\t\t(or box lower-left); refPoints are either labels\n\t\t\tor a pair of coordinates (e.g, 100 200)"},
    {"layout", "edit", "edit\t\tuse selected cell as new edit cell"},
    {"layout", "element", "element option\tadd a generic drawing element to the layout"},
    {"layout", "erase", "erase [layers]|cursor\t\terase mask information"},
    {"layout", "expand", "expand [toggle]\t\texpand everything under box, or toggle\n\t\t\texpanded/unexpanded cells in selection"},
    {"layout", "ext", "ext option\t\tOR"},
    {"layout", "ext2sim", "ext2sim [args]\tconvert extracted file(s) to a sim format file; type\n\t\t\t\"ext2sim help\" for information on options"},
    {"layout", "ext2spice", "ext2spice [args]\tconvert extracted file(s) to a SPICE format file; type\n\t\t\t\"ext2spice help\" for information on options"},
    {"layout", "extract", "extract option\t\tcircuit extractor; type \"extract help\"\n\t\t\tfor information on options"},
    {"layout", "extresist", "extresist [args]\tpatch .ext file with resistance info"},
    {"layout", "exttosim", "exttosim [args]\tconvert extracted file(s) to a sim format file; type\n\t\t\t\"exttosim help\" for information on options"},
    {"layout", "exttospice", "exttospice [args]\tconvert extracted file(s) to a SPICE format file; type\n\t\t\t\"exttospice help\" for information on options"},
    {"layout", "feedback", "feedback option\t\tfind out about problems; type \"feedback help\"\n\t\t\tfor information on options"},
    {"layout", "fill", "fill dir [layers]\tfill layers from one side of box to other"},
    {"layout", "findbox", "findbox [zoom]\t\tcenter the window on the box and optionally zoom in"},
    {"layout", "findlabel", "findlabel lab\t\tset the box to the location of this label"},
    {"layout", "flatten", "flatten destname\tflatten edit cell into cell destname"},
    {"layout", "flush", "flush [cellname]\tflush changes to cellname (edit cell default)"},
    {"layout", "garoute", "garoute [cmd [args]]\tgate-array router"},
    {"layout", "gds", "gds option\t\talias for the \"calma\" command"},
    {"layout", "get", "get\tOR"},
    {"layout", "getcell", "getcell cell [child refPointC] [parent refPointP]\n\t\t\tget cell as a subcell of the edit cell, so that\n\t\t\trefPointC (or lower left) of cell is at refPointP\n\t\t\t(or box lower-left); refPoints are either labels\n\t\t\tor a pair of coordinates (e.g, 100 200)"},
    {"layout", "getnode", "getnode option\t\tget node names of all selected paint"},
    {"layout", "goto", "goto nodename\t\tgoto the node named nodename"},
    {"layout", "grid", "grid [xSpacing [ySpacing [xOrigin yOrigin]]]\n\t\t\ttoggle grid on/off (and set parameters)"},
    {"layout", "id", "id\tOR"},
    {"layout", "identify", "identify use_id\t\tset the use identifier of the selected cell"},
    {"layout", "instance", "instance [list] children|parents|exists|self [name]\tOR\ninstance [list] allcells|topcells\n\t\t\tList cells by relationship to cell \"name\", or\n\t\t\tthe current selection if no name given.  \"list\"\n\t\t\treturns result as a list. \"exists + name\" returns\n\t\t\ttrue or false (also see command cellname)"},
    {"layout", "iroute", "iroute [cmd [args]]\tdo interactive point to point route"},
    {"layout", "label", "label str [pos [layer]]\n\t\t\tplace a label"},
    {"layout", "lef", "lef [options]\tLEF-format input/output; type \"lef help\"\n\t\t\tfor information on options"},
    {"layout", "load", "load [cellname]\tload a cell into a window"},
    {"layout", "locking", "locking [enable|disable]\tenable or disable file locking"},
    {"layout", "move", "move [dir [amount]]\tOR\nmove to x y\t\tmove box and selection, either by amount\n\t\t\tin dir, or pick up by box lower-left and put\n\t\t\tdown at cursor position; second form will\n\t\t\tput box at location x y"},
    {"layout", "netlist", "netlist option\t\tnetlist operation; type \"netlist help\"\n\t\t\tfor information on options"},
    {"layout", "orient", "orient [orientation]\t\torient selection and box"},
    {"layout", "paint", "paint layers|cursor\t\tpaint mask information"},
    {"layout", "path", "path [search|cell|sys] [[+]path]\n\t\t\tset or print search paths"},
    {"layout", "plot", "plot type [args]\thardcopy plotting; type \"plot help\"\n\t\t\tfor information on types and args"},
    {"layout", "plow", "plow option [args]\tstretching and compaction; type \"plow help\"\n\t\t\tfor information on options"},
    {"layout", "polygon", "polygon type x1 y1 x2 y2 [x3 y3. . .] xn yn\n\t\t\tdraw a polygon from a list of point pairs"},
    {"layout", "port", "port [index] [direction...]\n\t\t\tdeclare a label to be a subcircuit port"},
    {"layout", "property", "property [name] [value]\n\t\t\tadd a property (string) to the edit cell"},
    {"layout", "random", "random [seed [value]]\t\tgenerate random number or set random seed"},
    {"layout", "rotate", "rotate [+/-][deg]\trotate selection and box (counter)clockwise"},
    {"layout", "route", "route\t\t\troute the current cell"},
    {"layout", "save", "save [filename]\t\tsave edit cell on disk"},
    {"layout", "scalegrid", "scalegrid a b\t\tscale magic units vs. lambda by a / b"},
    {"layout", "see", "see [no] layers|allSame\tchange what's displayed in the window"},
    {"layout", "select", "select [option]\t\tchange selection; type \"select help\"\n\t\t\tfor information on options"},
    {"layout", "setlabel", "setlabel [option [value]]\tplace a label"},
    {"layout", "shell", "shell [command]\t\texecute a command in a subshell"},
    {"layout", "sideways", "sideways\t\tflip selection and box sideways"},
    {"layout", "snap", "snap [internal|lambda|user]\n\t\t\tcause box to snap to the selected grid when moved\n\t\t\tby the cursor"},
    {"layout", "spliterase", "spliterase dir [layer]\terase layers from diagonal corner dir of the\n\t\t\tedit box"},
    {"layout", "splitpaint", "splitpaint dir layer [layer2]\n\t\t\tsplit box diagonally with layer in corner dir and\n\t\t\tlayer2 in the opposite corner (default layer2 is space)"},
    {"layout", "straighten", "straighten direction\tstraighten jogs by pulling in direction"},
    {"layout", "stretch", "stretch [dir [amount]]\tstretch box and selection"},
    {"layout", "tech", "tech option\ttechnology handling; type \"tech help\"\n\t\t\tfor information on options"},
    {"layout", "unexpand", "unexpand\t\tunexpand subcells under box"},
    {"layout", "units", "units [type]\t\tset type of units parsed and displayed"},
    {"layout", "upsidedown", "upsidedown\t\tflip selection and box upside down"},
    {"layout", "what", "what\t\t\tprint out information about what's selected"},
    {"layout", "wire", "wire option [args]\twiring-style user interface; type\n\t\t\t\"wire help\" for information on options"},
    {"layout", "writeall", "writeall [force]\twrite out all modified cells to disk"},
    {"layout", "xload", "xload [cellname]\tload a cell into a window unexpanded"},
    {"layout", "xor", "xor destname\t\tflatten current top level cell into destname\n\t\t\tand xor with existing contents"},
    {"color", "color", "color [color-#]\t        specify color to edit, or print current intensities"},
    {"color", "load", "load [techStyle displayStyle monitorType]\n                        load new color map techStyle.displayStyle.monitorType"},
    {"color", "pushbutton", "pushbutton button\tinvoke a button press in the color window"},
    {"color", "save", "save [techStyle displayStyle monitorType]\n                        save color map to techStyle.displayStyle.monitorType"},
};

enum class EntryMatch : std::uint8_t { none, prefix, exact };

// One entry of Lookup(): letters match in either case, the name ends at a
// space (not at a tab).
EntryMatch lookup_entry(std::string_view word, std::string_view entry) noexcept {
  std::size_t i = 0;
  while (i < word.size() && i < entry.size() && entry[i] != ' ' &&
         std::tolower(static_cast<unsigned char>(entry[i])) == std::tolower(static_cast<unsigned char>(word[i]))) {
    ++i;
  }
  if (i < word.size()) {
    return EntryMatch::none;
  }
  return i == entry.size() || entry[i] == ' ' ? EntryMatch::exact : EntryMatch::prefix;
}

}  // namespace

bool magic_lookup_ambiguous(std::string_view client, std::string_view word) noexcept {
  int prefixes = 0;
  for (const MagicHelpEntry& e : kEntries) {
    if (e.client != client) {
      continue;
    }
    const EntryMatch m = lookup_entry(word, e.text);
    if (m == EntryMatch::exact) {
      return false;
    }
    prefixes += m == EntryMatch::prefix ? 1 : 0;
  }
  return prefixes > 1;
}

std::span<const MagicHelpEntry> magic_help_entries() noexcept {
  return {std::begin(kEntries), std::end(kEntries)};
}

std::string_view magic_help_text(std::string_view client, std::string_view name) noexcept {
  const auto* it = std::find_if(std::begin(kEntries), std::end(kEntries), [client, name](const MagicHelpEntry& e) {
    return e.client == client && e.name == name;
  });
  return it == std::end(kEntries) ? std::string_view{} : it->text;
}

}  // namespace hefesto::ui
