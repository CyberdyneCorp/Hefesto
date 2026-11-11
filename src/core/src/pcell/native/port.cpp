// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "pcell/native/port.hpp"

#include "hefesto/pcell/framework/tcl_list.hpp"
#include "pcell/framework/codes.hpp"

namespace hefesto::pcell::native {

Dict::Dict(std::initializer_list<std::pair<std::string, Val>> entries) {
  for (const auto& [key, value] : entries) {
    set(key, value);
  }
}

Dict Dict::from(const ParameterDict& parameters) {
  Dict d;
  for (const auto& [key, value] : parameters.entries()) {
    d.set(key, Val{value});
  }
  return d;
}

ParameterDict Dict::to_parameters() const {
  std::vector<ParameterDict::Entry> out;
  out.reserve(entries_.size());
  for (const auto& [key, value] : entries_) {
    out.emplace_back(key, value.str());
  }
  return ParameterDict{std::move(out)};
}

void Dict::set(std::string_view key, Val value) {
  for (auto& [k, v] : entries_) {
    if (k == key) {
      v = std::move(value);
      return;
    }
  }
  entries_.emplace_back(std::string{key}, std::move(value));
}

const Val& Dict::get(std::string_view key) const {
  for (const auto& [k, v] : entries_) {
    if (k == key) {
      return v;
    }
  }
  throw TclError{"key \"" + std::string{key} + "\" not known in dictionary"};
}

bool Dict::has(std::string_view key) const {
  for (const auto& [k, v] : entries_) {
    if (k == key) {
      return true;
    }
  }
  return false;
}

Dict Dict::merged(const Dict& other) const {
  Dict out = *this;
  for (const auto& [k, v] : other.entries_) {
    out.set(k, v);
  }
  return out;
}

const Val& Locals::operator[](std::string_view name) const {
  const auto it = vars_.find(name);
  if (it == vars_.end()) {
    throw TclError{"can't read \"" + std::string{name} + "\": no such variable"};
  }
  return it->second;
}

void Locals::load(const Dict& parameters) {
  for (const auto& [key, value] : parameters.entries()) {
    set(key, value);
  }
}

void require_numbers(const Dict& parameters, const std::set<std::string, std::less<>>& text_keys) {
  for (const auto& [key, value] : parameters.entries()) {
    if (key == "parameters") {
      throw Unsupported{"a parameter named \"parameters\""};
    }
    if (!text_keys.contains(key) && !value.is_number()) {
      throw Unsupported{"parameter " + key + " is not a number"};
    }
  }
}

std::string Engine::run(std::initializer_list<std::string_view> words) {
  std::vector<std::string> w;
  w.reserve(words.size());
  for (const auto word : words) {
    w.emplace_back(word);
  }
  return host_.run(w);
}

List Engine::box_values() {
  auto words = split_tcl_list(run({"box", "values"}));
  List out;
  if (words.ok()) {
    for (auto& w : words.value()) {
      out.emplace_back(std::move(w));
    }
  }
  return out;
}

void Engine::puts(bool to_stderr, std::string_view text) {
  host_.puts(to_stderr, std::string{text} + "\n");
}

void Engine::puts_nonewline(bool to_stderr, std::string_view text) {
  host_.puts(to_stderr, text);
}

PdkHelpers Engine::helpers() {
  PdkHelpers h;
  h.family = family_;
  h.cif_scale_output = run({"cif", "scale", "output"});
  return h;
}

namespace {

Val helper_value(const std::optional<std::string>& value, std::string_view helper) {
  if (!value) {
    throw TclError{"magic::" + std::string{helper} + " cannot be reproduced for this value"};
  }
  return Val{*value};
}

}  // namespace

Val Engine::spice2float(const Val& v, std::string_view faultval) {
  const auto r = helpers().spice2float(v.str(), faultval);
  if (!r) {
    throw TclError{"magic::spice2float cannot be reproduced for \"" + v.str() + "\""};
  }
  if (r->faulted) {
    puts(true, "Value is not numeric!");
  }
  return Val{r->value};
}

Val Engine::three_digit_past_decimal(const Val& v) {
  return helper_value(helpers().three_digit_past_decimal(v.str()), "3digitpastdecimal");
}

Val Engine::i2u(const Val& v) {
  return helper_value(helpers().i2u(v.str()), "i2u");
}

Val Engine::u2i(const Val& v) {
  return helper_value(helpers().u2i(v.str()), "u2i");
}

void BufferedHost::puts(bool to_stderr, std::string_view text) {
  held_.push_back({false, to_stderr, {}, std::string{text}});
}

void BufferedHost::set_variable(std::string_view name, std::string_view value) {
  held_.push_back({true, false, std::string{name}, std::string{value}});
}

void BufferedHost::flush() {
  for (const auto& h : held_) {
    if (h.variable) {
      target_.set_variable(h.name, h.text);
    } else {
      target_.puts(h.to_stderr, h.text);
    }
  }
  held_.clear();
}

Val expr_text(const std::string& text) {
  const tcl::Result r = tcl::evaluate(text);
  if (r.status == tcl::Status::unsupported) {
    throw Unsupported{"expr " + text};
  }
  if (r.status == tcl::Status::error) {
    throw TclError{"can't evaluate expression \"" + text + "\""};
  }
  return Val::of(r.value);
}

List getbox(Engine& e) {
  const List curbox = e.box_values();
  const Val oscale{e.run({"cif", "scale", "out"})};
  List out;
  for (std::size_t i = 0; i < 4; ++i) {
    out.push_back(mul(i < curbox.size() ? curbox[i] : Val{}, oscale));
  }
  return out;
}

List unionbox(const List& box1, const List& box2) {
  List out;
  for (std::size_t i = 0; i < 4; ++i) {
    const Val& v = box1.at(i);
    const Val& o = box2.at(i);
    const bool keep = i < 2 ? lt(v, o) : gt(v, o);
    out.push_back(keep ? v : o);
  }
  return out;
}

std::string list_string(const List& values) {
  std::vector<std::string> words;
  words.reserve(values.size());
  for (const auto& v : values) {
    words.push_back(v.str());
  }
  return merge_tcl_list(words);
}

cmd::CommandResult tcl_error_result(std::string message) {
  return cmd::CommandResult::raised(diag::make(codes::kScriptError, std::move(message)));
}

}  // namespace hefesto::pcell::native
