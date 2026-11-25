// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// UniqueRenamer: ExtUnique.c (capability extract-nodes-and-labels "Unique
// naming of duplicate labels"; openspec/changes/m7-extraction design.md
// D8). Q-EXTRACT-23 (replicate, both modes): the node visited later keeps
// the name unless the earlier node holds a port with that text.
// Q-EXTRACT-6 (fix, both modes): revert() restores the temporary labels in
// every renamed cell, the top cell included, and the cell's modified
// flags. (Magic ref: extract/ExtUnique.c:83-408, extract/ExtMain.c:842-848,
// extract/ExtMain.c:1108-1115)
//
// Owner: EXTNODE (design D3).

#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/extract/node/names.hpp"
#include "hefesto/extract/node/unique.hpp"
#include "hefesto/extract/tech/style.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::extract {

namespace {

constexpr std::string_view kCodeProcessing = "EXTRACT-NODE-002";
constexpr std::string_view kCodeWarnings = "EXTRACT-NODE-003";
constexpr std::string_view kCodeNoStyle = "EXTRACT-NODE-004";

/// Magic's EXT_UNIQ_* option of one run.
enum class Option : std::uint8_t { all, tagged, noports, notopports, temp, temp_noports };

Option option_of(UniqueMode mode, bool temporary) {
  const bool noports = mode == UniqueMode::noports || mode == UniqueMode::notopports;
  if (temporary) {
    return noports ? Option::temp_noports : Option::temp;
  }
  switch (mode) {
    case UniqueMode::all:
      return Option::all;
    case UniqueMode::noports:
      return Option::noports;
    case UniqueMode::notopports:
      return Option::notopports;
    case UniqueMode::tagged:
      break;
  }
  return Option::tagged;
}

enum class Action : std::uint8_t { rename, skip, warn };

/// extMakeUnique's choice for a duplicated text.
Action unique_action(std::string_view text, bool port, Option option) {
  if (option == Option::all || option == Option::temp) {
    return Action::rename;
  }
  const bool noports = option == Option::noports || option == Option::notopports;
  if ((noports || option == Option::temp_noports) && !port) {
    return Action::rename;
  }
  if (!text.empty() && text.back() == '#') {
    return Action::rename;
  }
  if (!text.empty() && text.back() == '!') {
    return Action::skip;
  }
  return noports && port ? Action::skip : Action::warn;
}

}  // namespace

/// One extUniqueCell run.
class UniquePass {
 public:
  UniquePass(UniqueRenamer& renamer, db::CellDef& cell, const UniqueContext& ctx, Option option)
      : renamer_(renamer), cell_(cell), ctx_(ctx), option_(option) {}

  diag::Status run() {
    if (!find_regions()) {
      return diag::make(kCodeNoStyle, "No extraction style for " + cell_.name() + ".");
    }
    for (const db::Label& label : cell_.labels()) {
      if (label_role(label.text) == LabelRole::name) {
        texts_.insert(label.text);
      }
    }
    for (std::size_t i = 0; i < cell_.labels().size(); ++i) {
      position_.push_back(i);
    }
    std::size_t warnings = 0;
    for (std::size_t lp = 0; lp < nodes_.size(); ++lp) {
      for (std::size_t k = 0; k < nodes_[lp].labels.size(); ++k) {
        warnings += visit(lp, k);
      }
    }
    renamer_.warnings_ = warnings;
    if (warnings != 0 && ctx_.sink != nullptr) {
      ctx_.sink->emit(diag::make(kCodeWarnings, cell_.name() + ": " + std::to_string(warnings) + " warnings"));
    }
    return diag::Done{};
  }

 private:
  static constexpr std::size_t kProcessed = static_cast<std::size_t>(-1);

  /// The substrate marking, the label regions and their labels.
  bool find_regions() {
    if (ctx_.style == nullptr || ctx_.technology == nullptr) {
      return false;
    }
    const ExtStyle& style = *ctx_.style;
    RegionFinder finder(*ctx_.technology);
    finder.keep_clients = true;
    NodePassOptions substrate = ctx_.substrate;
    substrate.substrate_only = true;
    (void)finder.find_nodes(cell_, style, substrate);
    RegionCallbacks callbacks;
    auto found = finder.find(cell_, db::whole_plane(), style.active, ConnectTable{style.node_conn}, callbacks);
    if (found.ok()) {
      nodes_ = std::move(found).value();
    }
    attach_labels(cell_, finder, nodes_, style, *ctx_.technology, LabelPassOptions{}, ctx_.compat);
    reset(cell_);
    gone_.resize(nodes_.size());
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
      gone_[i].assign(nodes_[i].labels.size(), false);
    }
    return true;
  }

  /// One label of the main loop: the first node seen with a text keeps
  /// it unless a later node or an earlier port claims it.
  std::size_t visit(std::size_t lp, std::size_t k) {
    if (gone_[lp][k] || label_role(nodes_[lp].labels[k].text) != LabelRole::name) {
      return 0;
    }
    const std::string text = nodes_[lp].labels[k].text;
    const auto [it, inserted] = last_.try_emplace(text, lp);
    if (inserted || it->second == lp || it->second == kProcessed) {
      return 0;
    }
    const std::size_t last = it->second;
    if (!nodes_[lp].labels[k].port) {
      if (const std::optional<std::size_t> port = port_with_text(last, text)) {
        return make_unique(last, *port);  // the earlier port keeps the name
      }
    }
    const std::size_t warnings = make_unique(lp, k);
    last_[text] = kProcessed;
    return warnings;
  }

  [[nodiscard]] std::optional<std::size_t> port_with_text(std::size_t node, const std::string& text) const {
    const auto& labels = nodes_[node].labels;
    for (std::size_t m = 0; m < labels.size(); ++m) {
      if (!gone_[node][m] && labels[m].text == text && labels[m].port) {
        return m;
      }
    }
    return std::nullopt;
  }

  /// extMakeUnique: keeps label k of node `keep`, renames the text on
  /// every other node, or warns.
  std::size_t make_unique(std::size_t keep, std::size_t k) {
    const LabelRegion& kept = nodes_[keep].labels[k];
    switch (unique_action(kept.text, kept.port, option_)) {
      case Action::skip:
        return 0;
      case Action::warn:
        return warn(kept.text);
      case Action::rename:
        break;
    }
    const std::string name = kept.text;
    std::size_t suffix = 0;
    for (std::size_t j = 0; j < nodes_.size(); ++j) {
      if (j != keep && rename_in(j, name, suffix)) {
        ++suffix;  // one suffix per renamed node
      }
    }
    return 0;
  }

  bool rename_in(std::size_t j, const std::string& name, std::size_t& suffix) {
    bool renamed = false;
    for (std::size_t m = 0; m < nodes_[j].labels.size(); ++m) {
      if (gone_[j][m] || nodes_[j].labels[m].text != name) {
        continue;
      }
      std::string unique = name + "_uq" + std::to_string(suffix);
      while (texts_.contains(unique)) {
        unique = name + "_uq" + std::to_string(++suffix);
      }
      replace(nodes_[j].labels[m].label, unique);
      gone_[j][m] = true;
      renamed = true;
    }
    return renamed;
  }

  std::size_t warn(const std::string& text) {
    std::size_t n = 0;
    for (std::size_t j = 0; j < nodes_.size(); ++j) {
      for (std::size_t m = 0; m < nodes_[j].labels.size(); ++m) {
        const LabelRegion& label = nodes_[j].labels[m];
        if (gone_[j][m] || label.text != text) {
          continue;
        }
        ++n;
        const db::Point p = label.position;
        renamer_.feedback_.push_back(
            {db::Rect{p.x - 1, p.y - 1, p.x + 1, p.y + 1},
             "Non-global label \"" + text + "\" attached to more than one unconnected node: " +
                 number_name(nodes_, j, *ctx_.technology, SubstrateName{}),
             true});
      }
    }
    return n;
  }

  /// DBRemoveLabel + DBPutFontLabel with the new text: the label moves to
  /// the end of the list; a port takes 1 + the largest port number.
  void replace(std::size_t original, const std::string& text) {
    const std::size_t at = position_[original];
    db::Label label = cell_.labels()[at];
    std::uint32_t port = 0;
    if (label.is_port()) {
      for (const db::Label& l : cell_.labels()) {
        port = std::max(port, l.port_index + 1);
      }
    }
    UniqueRenamer::Rename rename{&cell_, label.text, text, label.rect, label.type, ctx_.temporary,
                                 cell_.flags().modified, cell_.flags().needs_new_timestamp};
    label.text = text;
    label.port_index = port;
    db::remove_label(cell_, at);
    (void)db::put_label(cell_, std::move(label));
    for (std::size_t& p : position_) {
      if (p > at) {
        --p;
      }
    }
    position_[original] = cell_.labels().size() - 1;
    renamer_.renames_.push_back(std::move(rename));
  }

  UniqueRenamer& renamer_;
  db::CellDef& cell_;
  const UniqueContext& ctx_;
  Option option_;
  NodeRegionList nodes_;
  std::vector<std::vector<bool>> gone_;  // Magic's NULLed ll_label
  std::unordered_set<std::string> texts_;
  std::unordered_map<std::string, std::size_t> last_;
  std::vector<std::size_t> position_;  // original label index -> current index
};

diag::Status UniqueRenamer::apply(db::CellDef& cell, UniqueMode mode) {
  return apply(cell, mode, UniqueContext{});
}

diag::Status UniqueRenamer::apply(db::CellDef& cell, UniqueMode mode, const UniqueContext& context) {
  warnings_ = 0;
  if (cell.properties().find("LEFview") != nullptr) {
    return diag::Done{};  // abstract views keep their labels
  }
  if (context.sink != nullptr && context.style != nullptr) {
    context.sink->emit(diag::make(kCodeProcessing, "Processing " + cell.name()));
  }
  UniquePass pass(*this, cell, context, option_of(mode, context.temporary));
  return pass.run();
}

namespace {

/// A reverted port takes the port number of the first other label with
/// its restored text.
void restore_port_number(std::vector<db::Label>& labels, std::size_t i) {
  for (std::size_t j = 0; j < labels.size(); ++j) {
    if (j != i && labels[j].text == labels[i].text) {
      labels[i].port_index = labels[j].port_index;
      return;
    }
  }
}

/// ExtRevertUniqueCell for one temporary label.
void revert_label(std::vector<db::Label>& labels, const std::string& renamed, const db::Rect& rect,
                  tech::TileType type) {
  for (std::size_t i = labels.size(); i-- > 0;) {
    db::Label& label = labels[i];
    if (label.text != renamed || label.rect != rect || label.type != type) {
      continue;
    }
    const std::size_t cut = label.text.rfind('_');
    if (cut != std::string::npos) {
      label.text.resize(cut);
    }
    if (label.is_port()) {
      restore_port_number(labels, i);
    }
    return;
  }
}

}  // namespace

void UniqueRenamer::revert() {
  std::vector<db::CellDef*> restored;
  for (const Rename& r : renames_) {
    if (!r.temporary || r.cell == nullptr) {
      continue;
    }
    revert_label(r.cell->labels(), r.renamed, r.rect, r.type);
    if (std::find(restored.begin(), restored.end(), r.cell) == restored.end()) {
      restored.push_back(r.cell);
      r.cell->flags().modified = r.was_modified;  // the renaming was the cell's only change
      r.cell->flags().needs_new_timestamp = r.needed_timestamp;
    }
  }
  renames_.clear();
  feedback_.clear();
}

}  // namespace hefesto::extract
