// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/diag/format.hpp"

#include <nlohmann/json.hpp>

#include <type_traits>

namespace hefesto::diag {

namespace {

using Json = nlohmann::ordered_json;

// Overload set for std::visit.
template <typename... F>
struct Overloaded : F... {
  using F::operator()...;
};
template <typename... F>
Overloaded(F...) -> Overloaded<F...>;

std::string location_suffix(const Location& location) {
  return std::visit(
      Overloaded{
          [](std::monostate) { return std::string{}; },
          [](const SourceLocation& s) { return " at " + s.file + ":" + std::to_string(s.line); },
          [](const LayoutLocation& l) {
            return " in " + l.cell + " (" + std::to_string(l.x) + ", " + std::to_string(l.y) + ")";
          },
      },
      location);
}

Json location_json(const Location& location) {
  return std::visit(Overloaded{
                        [](std::monostate) { return Json(nullptr); },
                        [](const SourceLocation& s) {
                          Json j = Json::object();
                          j["file"] = s.file;
                          j["line"] = s.line;
                          return j;
                        },
                        [](const LayoutLocation& l) {
                          Json j = Json::object();
                          j["cell"] = l.cell;
                          j["x"] = l.x;
                          j["y"] = l.y;
                          return j;
                        },
                    },
                    location);
}

Json related_json(const std::optional<ObjectRef>& related) {
  if (!related) {
    return Json(nullptr);
  }
  Json j = Json::object();
  j["kind"] = object_kind_name(related->kind);
  j["name"] = related->name;
  return j;
}

}  // namespace

std::optional<Format> parse_format(std::string_view name) noexcept {
  if (name == "text") {
    return Format::text;
  }
  if (name == "json") {
    return Format::json;
  }
  if (name == "magic") {
    return Format::magic;
  }
  return std::nullopt;
}

std::string format_text(const Diagnostic& diagnostic) {
  std::string out{severity_name(diagnostic.severity)};
  out += ": ";
  out += diagnostic.code;
  out += ": ";
  out += diagnostic.text;
  out += location_suffix(diagnostic.location);
  return out;
}

std::string format_json(const Diagnostic& diagnostic) {
  Json j = Json::object();
  j["severity"] = severity_name(diagnostic.severity);
  j["code"] = diagnostic.code;
  j["text"] = diagnostic.text;
  j["location"] = location_json(diagnostic.location);
  j["related"] = related_json(diagnostic.related);
  return j.dump(-1, ' ', false, Json::error_handler_t::replace);
}

std::string format_magic(const Diagnostic& diagnostic) {
  const auto* where = std::get_if<SourceLocation>(&diagnostic.location);
  const bool tech_line = where != nullptr && where->line > 0 && diagnostic.related.has_value() &&
                         diagnostic.related->kind == ObjectKind::section;
  if (!tech_line) {
    return diagnostic.text;
  }
  return where->file + ": line " + std::to_string(where->line) + ": section " + diagnostic.related->name + ":\n\t" +
         diagnostic.text;
}

std::string format(const Diagnostic& diagnostic, Format fmt) {
  switch (fmt) {
    case Format::json:
      return format_json(diagnostic);
    case Format::magic:
      return format_magic(diagnostic);
    case Format::text:
      break;
  }
  return format_text(diagnostic);
}

bool shown_in(const Diagnostic& diagnostic, Format fmt) noexcept {
  return !(diagnostic.native_only && fmt == Format::magic);
}

std::optional<std::string> render(const Diagnostic& diagnostic, Format fmt) {
  if (!shown_in(diagnostic, fmt)) {
    return std::nullopt;
  }
  return format(diagnostic, fmt);
}

}  // namespace hefesto::diag
