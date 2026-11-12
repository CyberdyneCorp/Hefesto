// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Ports of `<pdk>::fixed_check` and `<pdk>::fixed_draw` (PDK ref: open_pdks
// c6df28c1, sky130A-fix.tcl:250-376 and gf180mcuD-bjt.tcl:228-354, and
// IHP-Open-PDK 5e6d592e ihp-sg13g2-fix.tcl:500-612; the two procedures are
// the same text in the three PDKs), including their defects: an invalid NY
// or NY < 1 resets `nx`, not `ny`. sg13g2 builds the placed cell first
// (generate_base_cell).

#include "pcell/native/fixed.hpp"

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/pcell/framework/tcl_list.hpp"
#include "pcell/native/port.hpp"

namespace hefesto::pcell::native {

namespace {

// Every helper call the procedure makes must be reproducible before it
// prints anything.
void require_helpers(Engine& e, const Locals& v, std::string_view faultval) {
  const PdkHelpers h = e.helpers();
  for (const char* key : {"deltax", "deltay"}) {
    const auto r = h.spice2float(v[key].str(), faultval);
    if (!r || !h.three_digit_past_decimal(r->value)) {
      throw Unsupported{"spice2float/3digitpastdecimal"};
    }
  }
}

// `$value < 1` must not be a big-integer comparison (Unsupported).
void require_comparable(const Val& value) {
  static_cast<void>(value.is_number());
}

// `parameters` is the procedure's dictionary: returned unchanged (with its
// string representation) unless a `dict set` modified it.
ParameterDict fixed_check(Engine& e, ParameterDict parameters) {
  Locals v;
  v.load(Dict::from(parameters));
  require_helpers(e, v, "-1");
  require_comparable(v["nx"]);
  require_comparable(v["ny"]);

  // Normalize distance units to microns
  v.set("deltax", e.three_digit_past_decimal(e.spice2float(v["deltax"], "-1")));
  v.set("deltay", e.three_digit_past_decimal(e.spice2float(v["deltay"], "-1")));

  // nx, ny must be integer
  if (!string_is_int(v["nx"])) {
    e.puts(true, "NX must be an integer!");
    parameters.set("nx", "1");
  }
  if (!string_is_int(v["ny"])) {
    e.puts(true, "NY must be an integer!");
    parameters.set("nx", "1");
  }
  // Number of devices in X and Y must be at least 1
  if (lt(v["nx"], "1")) {
    e.puts(true, "NX must be >= 1");
    parameters.set("nx", "1");
  }
  if (lt(v["ny"], "1")) {
    e.puts(true, "NY must be >= 1");
    parameters.set("nx", "1");
  }
  // Step less than zero violates DRC
  if (lt(v["deltax"], "0")) {
    e.puts(true, "X step must be >= 0");
    parameters.set("deltax", "0");
  }
  if (lt(v["deltay"], "0")) {
    e.puts(true, "Y step must be >= 0");
    parameters.set("deltay", "0");
  }
  return parameters;
}

// expr $a + $b (unbraced: both are substituted as text and parsed again).
Val expr_sum(const Val& a, const Val& b) {
  return expr_text(a.str() + " + " + b.str());
}

void require_draw_inputs(Engine& e, const Locals& v) {
  const PdkHelpers h = e.helpers();
  for (const char* key : {"deltax", "deltay", "xstep", "ystep"}) {
    if (!h.spice2float(v[key].str())) {
      throw Unsupported{"spice2float"};
    }
  }
  require_comparable(v["nx"]);
  require_comparable(v["ny"]);
  // The array pitch sums must be reproducible too.
  const auto sum = [&](const char* step, const char* delta) {
    const auto s = h.spice2float(v[step].str());
    const auto d = h.spice2float(v[delta].str());
    if (tcl::evaluate(s->value + " + " + d->value).status == tcl::Status::unsupported) {
      throw Unsupported{"array pitch"};
    }
  };
  sum("xstep", "deltax");
  sum("ystep", "deltay");
}

cmd::CommandResult fixed_draw(Engine& e, const std::string& devname, const Dict& parameters) {
  Locals v;
  v.load(parameters);
  require_draw_inputs(e, v);

  // Instantiate the cell. The name corresponds to the cell in the
  // sky130_fd_pr_* (gf180mcu primdev) directory.
  const Val instname{e.run({"getcell", devname})};

  v.set("deltax", e.spice2float(v["deltax"]));
  v.set("deltay", e.spice2float(v["deltay"]));
  v.set("xstep", e.spice2float(v["xstep"]));
  v.set("ystep", e.spice2float(v["ystep"]));

  // Array stepping
  if (gt(v["nx"], "1") || gt(v["ny"], "1")) {
    v.set("xstep", expr_sum(v["xstep"], v["deltax"]));
    v.set("ystep", expr_sum(v["ystep"], v["deltay"]));
    e.run({"box", "size", um(v["xstep"]), um(v["ystep"])});
    e.run({"array", v["nx"].str(), v["ny"].str()});
  }
  e.run({"select", "cell", instname.str()});
  e.run({"expand"});
  return cmd::CommandResult::success(instname.str());
}

// `[cellname list exists <cell>] == 0`: the list form answers the name or 0.
bool missing(Engine& e, const std::string& cell) {
  return e.run({"cellname", "list", "exists", cell}) == "0";
}

// `box values {*}$curbox`.
std::vector<std::string> box_values(const std::string& curbox) {
  std::vector<std::string> words{"box", "values"};
  auto values = split_tcl_list(curbox);
  if (values.ok()) {
    words.insert(words.end(), values.value().begin(), values.value().end());
  }
  return words;
}

// sg13g2::<cell>_generate (ihp-sg13g2-fix.tcl:268-580; the eight procedures
// differ only in the cell name, and schottky's guard). `parameters` is the
// procedure argument, which the sourced layout script sees as a local.
void generate_base_cell(Engine& e, const std::string& cell, const BaseCellScript& base,
                        const std::string& parameters) {
  if (!missing(e, base.guard)) {
    return;
  }
  // Q-SCRIPT-45 with magic-compat OFF: what `load` replaces comes back.
  const std::function<void()> restore = e.host().magic_compat() ? std::function<void()>{}
                                                                 : e.host().keep_edit_context();
  e.run({"suspendall"});
  // Save critical values before creating and editing a new cell
  const std::string curcell = e.run({"cellname", "list", "window"});
  const std::string curbox = e.run({"box", "values"});
  // Stop the tag method from messing with this procedure
  const std::string ltag = e.run({"tag", "load"});
  e.run({"tag", "load", ""});
  e.run({"load", cell, "-silent"});
  e.run({"tech", "unlock", "*"});
  static_cast<void>(e.host().evaluate(merge_tcl_list(std::vector<std::string>{
      "apply", "{parameters} {source ${sg13g2::script_path}/" + cell + ".tcl}", parameters})));
  // Return to our regularly scheduled program
  e.run({"load", curcell});
  e.run({"tag", "load", ltag});
  e.run({"tech", "revert"});
  e.run(box_values(curbox));
  e.run({"resumeall"});
  if (restore) {
    restore();
  }
}

class FixedDevice final : public NativeGenerator {
 public:
  FixedDevice(HelperFamily family, std::string defaults, std::string cell, std::optional<BaseCellScript> base = {})
      : family_(family), defaults_(std::move(defaults)), cell_(std::move(cell)), base_(std::move(base)) {}

  [[nodiscard]] ParameterDict defaults() const override {
    auto parsed = ParameterDict::from_tcl(defaults_);
    return parsed.ok() ? std::move(parsed).value() : ParameterDict{};
  }

  [[nodiscard]] std::optional<diag::Outcome<ParameterDict>> check(NativeHost& host,
                                                                 const ParameterDict& parameters) const override {
    BufferedHost buffered{host};
    auto result = run_check([&] {
      Engine e{buffered, family_};
      return fixed_check(e, parameters);
    });
    if (result) {
      buffered.flush();
    }
    return result;
  }

  [[nodiscard]] std::optional<cmd::CommandResult> draw(NativeHost& host,
                                                       const ParameterDict& parameters) const override {
    return run_port([&] {
      Engine e{host, family_};
      if (base_) {
        Locals v;
        v.load(Dict::from(parameters));
        require_draw_inputs(e, v);
        static_cast<void>(host.evaluate(""));  // Unsupported before anything is drawn
        if (missing(e, cell_)) {
          generate_base_cell(e, cell_, *base_, parameters.to_tcl());
        }
      }
      return fixed_draw(e, cell_, Dict::from(parameters));
    });
  }

 private:
  HelperFamily family_;
  std::string defaults_;
  std::string cell_;
  std::optional<BaseCellScript> base_;
};

}  // namespace

std::unique_ptr<NativeGenerator> make_fixed_device(HelperFamily family, std::string defaults, std::string cell) {
  return std::make_unique<FixedDevice>(family, std::move(defaults), std::move(cell));
}

std::unique_ptr<NativeGenerator> make_generated_fixed_device(std::string defaults, std::string cell,
                                                            BaseCellScript base) {
  return std::make_unique<FixedDevice>(HelperFamily::sg13g2, std::move(defaults), std::move(cell), std::move(base));
}

}  // namespace hefesto::pcell::native
