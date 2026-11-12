// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Script consent, trust and the sandbox on the desktop (board
// M4-Script-Consent; openspec/changes/m4-scripting tasks 10.3): the prompt
// and its answers, the desktop ConsentHost through the core's consent rule,
// the PDK panel's trust state and commands, the declined state, denials and
// the long-running script card.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

#include "hefesto/script/events.hpp"
#include "hefesto/script/pdk/trust.hpp"
#include "hefesto/shell/pdk_panel_model.hpp"
#include "hefesto/shell/script_consent_model.hpp"

using namespace hefesto;
using namespace hefesto::shell;
using script::ConsentDecision;

namespace {

const std::string kHash = "5740c4a81c42f9fe4af5ef2ee258a99151053d28";
const script::PdkIdentity kSky130{"sky130", "sky130A", kHash};
const std::string kRc = "/Users/me/.ciel/ciel/sky130/versions/" + kHash + "/sky130A/libs.tech/magic/sky130A.magicrc";

std::vector<std::string> answer_labels(const ConsentPromptView& view) {
  std::vector<std::string> out;
  for (const ConsentAnswer& a : view.answers) out.push_back(a.label + (a.primary ? "*" : ""));
  return out;
}

}  // namespace

TEST_CASE("the consent prompt shows the script's origin and three answers",
          "[cap:hefesto-security][req:Consent before implicit script execution]") {
  ConsentContext context;
  context.document = "ota_5t";
  context.digest_verified = true;
  context.then_runs = "sky130A.tcl";
  const ConsentPromptView view = consent_prompt({kSky130, kRc, false}, context);
  CHECK(view.title == "Run the sky130A PDK scripts?");
  CHECK(view.body.starts_with("Opening ota_5t needs the sky130A technology."));
  CHECK(view.script_path == kRc);
  REQUIRE(view.origin.size() == 4);
  CHECK(view.origin[0].first == "Script");
  CHECK(view.origin[0].second.size() < kRc.size());  // middle-ellipsized; the whole path is the tooltip
  CHECK(view.origin[0].second.ends_with("magic/sky130A.magicrc"));
  CHECK(view.origin[1] == std::pair<std::string, std::string>{"PDK", "sky130A · 5740c4a"});
  CHECK(view.origin[2].second == "ciel 5740c4a · ● digest verified");
  CHECK(view.origin[3] == std::pair<std::string, std::string>{"Then runs", "sky130A.tcl"});
  CHECK(answer_labels(view) == std::vector<std::string>{"Don't run", "Trust sky130A 5740c4a", "Run once*"});
  // Return runs the default (Run once); Escape answers Don't run.
  CHECK(view.default_decision() == ConsentDecision::run_once);
  CHECK(ConsentPromptView::escape_decision() == ConsentDecision::decline);
  // The dialog's buttons map back to the decisions.
  const PromptDialog dialog = consent_dialog(view);
  CHECK(dialog.title == view.title);
  REQUIRE(dialog.buttons.size() == 3);
  CHECK(dialog.buttons[2].primary);
  // Board tile 1: a shield alert, the origin as a detail block, Don't run on the left.
  CHECK(dialog.text == view.body);
  CHECK(dialog.icon == "shield");
  CHECK(dialog.leading_buttons == 1);
  REQUIRE(dialog.details.size() == 4);
  CHECK(dialog.details[0].key == "Script");
  CHECK(dialog.details[0].tooltip == kRc);  // the whole path
  CHECK(dialog.details[0].value.ends_with("sky130A.magicrc"));
  CHECK(dialog.details[0].value.size() < kRc.size());
  CHECK(dialog.details[2].value == "ciel 5740c4a");
  CHECK(dialog.details[2].chip == "● digest verified");
  CHECK(dialog.details[2].tone == DialogDetail::Tone::ok);
  CHECK(consent_decision(view, 0) == ConsentDecision::decline);  // dismissed (Escape)
  CHECK(consent_decision(view, 1) == ConsentDecision::decline);
  CHECK(consent_decision(view, 2) == ConsentDecision::trust_version);
  CHECK(consent_decision(view, 3) == ConsentDecision::run_once);
  CHECK(consent_decision(view, 9) == ConsentDecision::decline);
}

TEST_CASE("a script outside the PDK store has no Trust answer",
          "[cap:hefesto-security][req:Consent before implicit script execution]") {
  ConsentContext context;
  context.document = "chip_top.mag";
  context.in_store = false;
  const script::PdkIdentity unknown{"sky130", "sky130A", ""};
  const ConsentPromptView view =
      consent_prompt({unknown, "/Users/me/Downloads/pdk/sky130A/libs.tech/magic/sky130A.magicrc", true}, context);
  CHECK(view.title == "Run a startup script from a downloaded folder?");
  CHECK(view.warning);
  CHECK(view.origin[1].second == "sky130A · version unknown");
  CHECK(view.origin[2].second == "▲ not in the PDK store · not verified");
  CHECK(view.origin[3].first == "Found via");
  CHECK(answer_labels(view) == std::vector<std::string>{"Don't run*", "Run once"});
  CHECK(view.default_decision() == ConsentDecision::decline);
  // Board tile 4: the narrower alert, the release row a warning chip.
  const PromptDialog dialog = consent_dialog(view, "/Users/me");
  CHECK(dialog.width < consent_dialog(consent_prompt({kSky130, kRc, false}, ConsentContext{})).width);
  CHECK(dialog.details[0].value.starts_with("~/"));
  CHECK(dialog.details[2].value.empty());
  CHECK(dialog.details[2].chip == "▲ not in the PDK store · not verified");
  CHECK(dialog.details[2].tone == DialogDetail::Tone::warning);
}

TEST_CASE("the consent alert splits each origin row into value, chip and note",
          "[cap:hefesto-security][req:Consent before implicit script execution]") {
  const DialogDetail then_runs = consent_detail("Then runs", "sky130A.tcl + 8 part files (device generators)");
  CHECK(then_runs.value == "sky130A.tcl");
  CHECK(then_runs.note == "+ 8 part files (device generators)");
  CHECK(then_runs.chip.empty());
  const DialogDetail unchecked = consent_detail("Release", "ciel 5740c4a · ◆ digest not checked");
  CHECK(unchecked.value == "ciel 5740c4a");
  CHECK(unchecked.chip == "◆ digest not checked");
  CHECK(unchecked.tone == DialogDetail::Tone::neutral);
  const DialogDetail plain = consent_detail("PDK", "sky130A · 5740c4a");
  CHECK(plain.value == "sky130A · 5740c4a");
  CHECK(plain.chip.empty());
  CHECK(plain.note.empty());
  // The script path from the home folder.
  CHECK(home_relative("/Users/me/pdk/a.magicrc", "/Users/me") == "~/pdk/a.magicrc");
  CHECK(home_relative("/Users/me/pdk/a.magicrc", "/Users/me/") == "~/pdk/a.magicrc");
  CHECK(home_relative("/Users/meg/a.magicrc", "/Users/me") == "/Users/meg/a.magicrc");
  CHECK(home_relative("/Users/me/a.magicrc", "") == "/Users/me/a.magicrc");
}

TEST_CASE("the desktop ConsentHost asks once per PDK version",
          "[cap:hefesto-security][req:Consent before implicit script execution]") {
  std::vector<ConsentPromptView> asked;
  ConsentDecision next = ConsentDecision::run_once;
  DesktopConsentHost host([&](const ConsentPromptView& v) { asked.push_back(v); return next; },
                          [](const platform::Path& p) { return p.string().find("/.ciel/") != std::string::npos; });
  host.set_document("ota_5t");
  script::TrustStore store;
  script::ScriptEvents events;
  events.add(host);  // the prompt learns `implicit` from the published request

  // Through the core's rule: an untrusted version asks the host.
  auto outcome = script::request_consent(store, events, &host, kSky130, kRc, false);
  CHECK(outcome.run);
  REQUIRE(asked.size() == 1);
  CHECK(asked[0].title == "Run the sky130A PDK scripts?");
  CHECK(asked[0].body.starts_with("Opening ota_5t"));
  // A second script of the same version (or a second window) reuses the answer.
  outcome = script::request_consent(store, events, &host, kSky130, kRc, false);
  CHECK(outcome.run);
  CHECK(asked.size() == 1);
  CHECK(host.answer(kSky130) == ConsentDecision::run_once);
  CHECK_FALSE(host.declined());

  // Another version asks again; declining is remembered as the declined PDK.
  const script::PdkIdentity other{"sky130", "sky130A", "0fe599b2afb6708d281543108caf8310912f54af"};
  next = ConsentDecision::decline;
  outcome = script::request_consent(store, events, &host, other, "/Users/me/Downloads/sky130A.magicrc", true);
  CHECK_FALSE(outcome.run);
  REQUIRE(asked.size() == 2);
  CHECK(asked[1].title == "Run a startup script from a downloaded folder?");
  CHECK(asked[1].origin.back().first == "Found via");  // implicit: technology auto-discovery
  CHECK(host.declined() == other);
  CHECK(host.script_of(other) == platform::Path{"/Users/me/Downloads/sky130A.magicrc"});

  // Review scripts...: forget the answer and ask again.
  host.forget(other);
  next = ConsentDecision::run_once;
  CHECK(host.ask(other, "/Users/me/Downloads/sky130A.magicrc") == ConsentDecision::run_once);
  CHECK(asked.size() == 3);
  CHECK_FALSE(host.declined());
  events.remove(host);
}

TEST_CASE("Trust this version is recorded per PDK version",
          "[cap:hefesto-security][req:Consent before implicit script execution]") {
  DesktopConsentHost host([](const ConsentPromptView&) { return ConsentDecision::trust_version; },
                          [](const platform::Path&) { return true; });
  script::TrustStore store;
  script::ScriptEvents events;
  CHECK(script::request_consent(store, events, &host, kSky130, kRc, false).run);
  CHECK(store.trusted(kSky130));
  const script::PdkIdentity other{"sky130", "sky130A", "0fe599b2afb6708d281543108caf8310912f54af"};
  CHECK_FALSE(store.trusted(other));
}

namespace {

/// A scratch folder laid out like a PDK's libs.tech/magic.
class MagicFolder {
 public:
  MagicFolder()
      : dir_(std::filesystem::temp_directory_path() /
             ("hefesto-consent-" + std::to_string(std::random_device{}()))) {
    std::filesystem::create_directories(dir_);
  }
  ~MagicFolder() {
    std::error_code ec;
    std::filesystem::remove_all(dir_, ec);
  }
  MagicFolder(const MagicFolder&) = delete;
  MagicFolder& operator=(const MagicFolder&) = delete;

  void touch(const std::string& name) const { std::ofstream(dir_ / name) << "# " << name << "\n"; }
  [[nodiscard]] const std::filesystem::path& path() const noexcept { return dir_; }

 private:
  std::filesystem::path dir_;
};

}  // namespace

TEST_CASE("the consent prompt names what the startup file runs next",
          "[cap:hefesto-security][req:Consent before implicit script execution]") {
  CHECK(then_runs_text("sky130A.tcl", 8) == "sky130A.tcl + 8 part files (device generators)");
  CHECK(then_runs_text("x.tcl", 1) == "x.tcl + 1 part file (device generators)");
  CHECK(then_runs_text("x.tcl", 0) == "x.tcl");
  CHECK(then_runs_text("", 3).empty());

  // sky130A's libs.tech/magic: sky130A.tcl sources eight sky130A-*.tcl part files.
  const MagicFolder folder;
  folder.touch("sky130A.magicrc");
  CHECK(startup_then_runs(folder.path() / "sky130A.magicrc", "sky130A").empty());  // no device script
  folder.touch("sky130A.tcl");
  for (const char* part : {"util", "misc", "via", "fet", "cap", "res", "dio", "fix"}) {
    folder.touch(std::string{"sky130A-"} + part + ".tcl");
  }
  folder.touch("sky130A-BindKeys");     // not a part file (not .tcl)
  folder.touch("sky130A-drc.tech");     // not a part file
  folder.touch("other.tcl");            // not this variant's
  CHECK(startup_then_runs(folder.path() / "sky130A.magicrc", "sky130A") ==
        "sky130A.tcl + 8 part files (device generators)");
  CHECK(startup_then_runs(folder.path() / "sky130A.magicrc", "").empty());
}

TEST_CASE("the consent prompt says whether the store release's digest verified",
          "[cap:hefesto-security][req:Consent before implicit script execution]") {
  pdk::Provenance installed;
  installed.source = "github:fossi-foundation/ciel-releases";
  installed.assets = {{"common.tar.zst", "ab12"}, {"sky130_fd_sc_hd.tar.zst", "cd34"}};
  CHECK(release_digest_verified(installed));
  pdk::Provenance unknown_asset = installed;
  unknown_asset.assets.back().second.clear();  // installed with -accept-unverified
  CHECK_FALSE(release_digest_verified(unknown_asset));
  pdk::Provenance external = installed;
  external.source = "external";  // installed by ciel or volare: nothing was checked by Hefesto
  CHECK_FALSE(release_digest_verified(external));
  CHECK_FALSE(release_digest_verified(pdk::Provenance{}));

  // The desktop host asks its origin only for a script in the store.
  std::vector<ConsentPromptView> asked;
  int described = 0;
  DesktopConsentHost host([&](const ConsentPromptView& v) { asked.push_back(v); return ConsentDecision::run_once; },
                          [](const platform::Path& p) { return p.string().find("/.ciel/") != std::string::npos; });
  host.set_origin([&](const script::PdkIdentity& pdk, const platform::Path&, ConsentContext& context) {
    ++described;
    CHECK(pdk == kSky130);
    context.digest_verified = true;
    context.then_runs = "sky130A.tcl + 8 part files (device generators)";
  });
  CHECK(host.ask(kSky130, kRc) == ConsentDecision::run_once);
  REQUIRE(asked.size() == 1);
  REQUIRE(asked[0].origin.size() == 4);
  CHECK(asked[0].origin[2].second == "ciel 5740c4a · ● digest verified");
  CHECK(asked[0].origin[3] == std::pair<std::string, std::string>{"Then runs",
                                                                  "sky130A.tcl + 8 part files (device generators)"});
  const script::PdkIdentity downloaded{"sky130", "sky130A", ""};
  CHECK(host.ask(downloaded, "/Users/me/Downloads/sky130A.magicrc") == ConsentDecision::run_once);
  CHECK(described == 1);
  REQUIRE(asked.size() == 2);
  CHECK(asked[1].origin[2].second == "▲ not in the PDK store · not verified");
}

TEST_CASE("the PDK panel shows and changes script trust", "[cap:hefesto-pdk-manager][req:Script trust for managed PDKs]") {
  pdk::Family sky130;
  sky130.name = "sky130";
  sky130.variants = {pdk::Variant{"sky130A", {}, {}}, pdk::Variant{"sky130B", {}, {}}};
  pdk::Family gf;
  gf.name = "gf180mcu";
  gf.variants = {pdk::Variant{"gf180mcuD", {}, {}}};
  const pdk::Catalog catalog({sky130, gf});
  std::vector<pdk::InstalledVersion> installed(2);
  installed[0].family = "sky130";
  installed[0].variant = "sky130A";
  installed[0].version.hash = kHash;
  installed[0].enabled = true;
  installed[1].family = "gf180mcu";
  installed[1].variant = "gf180mcuD";
  installed[1].version.hash = kHash;
  const auto rows =
      pdk_rows(catalog, installed, [](const pdk::InstalledVersion& v) { return v.variant == "sky130A"; });
  REQUIRE(rows.size() == 3);
  CHECK(rows[0].scripts.label() == "● scripts trusted");
  CHECK(trust_action(rows[0]) == PdkAction::untrust);
  CHECK(rows[1].scripts.text.empty());  // sky130B is not installed
  CHECK_FALSE(trust_action(rows[1]).has_value());
  CHECK(rows[2].scripts.label() == "◆ scripts ask before running");
  CHECK(trust_action(rows[2]) == PdkAction::trust);
  // The buttons are logged commands (PDKINT's `pdk trust` / `pdk untrust`).
  CHECK(pdk_action_label(PdkAction::trust) == "Trust scripts");
  CHECK(pdk_action_label(PdkAction::untrust) == "Revoke trust");
  CHECK(pdk_command(PdkAction::trust, "sky130", "sky130A", kHash) == "pdk trust sky130A " + kHash);
  CHECK(pdk_command(PdkAction::untrust, "sky130", "sky130A", kHash) == "pdk untrust sky130A " + kHash);
  CHECK_FALSE(pdk_command(PdkAction::trust, "sky130", "sky130A", "").has_value());
  // Without a trust source every installed version asks.
  CHECK(pdk_rows(catalog, installed)[0].scripts.label() == "◆ scripts ask before running");
}

TEST_CASE("the PDK panel shows when the scripts were trusted",
          "[cap:hefesto-pdk-manager][req:Script trust for managed PDKs]") {
  // Board M4-Script-Consent tile 5: "● scripts trusted 11 Nov 2026".
  pdk::Family sky130;
  sky130.name = "sky130";
  sky130.variants = {pdk::Variant{"sky130A", {}, {}}, pdk::Variant{"sky130B", {}, {}}};
  const pdk::Catalog catalog({sky130});
  std::vector<pdk::InstalledVersion> installed(2);
  installed[0].family = installed[1].family = "sky130";
  installed[0].variant = "sky130A";
  installed[1].variant = "sky130B";
  installed[0].version.hash = installed[1].version.hash = kHash;
  const auto rows = pdk_rows(catalog, installed, [](const pdk::InstalledVersion& v) {
    return v.variant == "sky130A" ? ScriptTrustState{true, "2026-11-11"} : ScriptTrustState{false, "2026-10-02"};
  });
  REQUIRE(rows.size() == 2);
  CHECK(rows[0].scripts_date == "11 Nov 2026");
  CHECK(rows[0].scripts_label() == "● scripts trusted 11 Nov 2026");
  CHECK(rows[1].scripts_date.empty());  // a date without trust is not shown
  CHECK(rows[1].scripts_label() == "◆ scripts ask before running");
  // A version trusted before dates were recorded shows no date.
  CHECK(pdk_rows(catalog, installed, [](const pdk::InstalledVersion&) { return true; })[0].scripts_label() ==
        "● scripts trusted");
  CHECK(display_date("2026-10-02") == "2 Oct 2026");
  CHECK(display_date("2026-13-02") == "2026-13-02");
  CHECK(display_date("yesterday") == "yesterday");
}

TEST_CASE("declined scripts open the layout read-only", "[cap:hefesto-security][req:Consent before implicit script execution]") {
  const DeclinedView view = declined_view(kSky130, "ota_5t");
  CHECK(view.chip == "▲ read-only");
  CHECK(view.banner ==
        "▲ The sky130A scripts were not run, so ota_5t opened read-only with the minimum technology. Its sky130A "
        "layers are not shown, and editing and saving are off.");
  CHECK(view.action == "Review scripts...");
  CHECK(view.place_device_tooltip == "No PDK device scripts (the sky130A scripts were not run)");
}

TEST_CASE("a read-only layout shows the chip and turns the editing tools off",
          "[cap:hefesto-security][req:Consent before implicit script execution]") {
  // Board M4-Script-Consent tile 6: "▲ read-only" after the cell name,
  // "minimum · read-only · magic-compat on" under it.
  CHECK(read_only_chip().label() == "▲ read-only");
  CHECK(read_only_subtitle("minimum · magic-compat on") == "minimum · read-only · magic-compat on");
  CHECK(read_only_subtitle("sky130A · 1.0.588 · magic-compat off") == "sky130A · 1.0.588 · read-only · magic-compat off");
  CHECK(read_only_subtitle("minimum") == "minimum · read-only");
  for (const char* icon : {"paint", "wire", "label", "place"}) CHECK(rail_tool_edits(icon));
  for (const char* icon : {"box", "select", "measure", "fit"}) CHECK_FALSE(rail_tool_edits(icon));
  CHECK(read_only_tool_tooltip("Paint tool") == "Paint tool: off, the layout is read-only");
}

TEST_CASE("Review scripts asks again and reopens the layout on yes",
          "[cap:hefesto-security][req:Consent before implicit script execution]") {
  CHECK_FALSE(review_plan(kSky130, ConsentDecision::decline).reopen);
  const ReviewPlan once = review_plan(kSky130, ConsentDecision::run_once);
  CHECK(once.reopen);
  CHECK_FALSE(once.trust_command.has_value());
  const ReviewPlan trust = review_plan(kSky130, ConsentDecision::trust_version);
  CHECK(trust.reopen);
  CHECK(trust.trust_command == "pdk trust sky130A " + kHash);
  // A script outside the store has no version to trust.
  CHECK_FALSE(review_plan({"sky130", "sky130A", ""}, ConsentDecision::trust_version).trust_command.has_value());

  // The desktop host asks again after forget(); a failed reopen counts as
  // declined again.
  int asked = 0;
  DesktopConsentHost host([&asked](const ConsentPromptView&) {
    ++asked;
    return asked == 1 ? ConsentDecision::decline : ConsentDecision::run_once;
  }, [](const platform::Path&) { return true; });
  CHECK(host.ask(kSky130, kRc) == ConsentDecision::decline);
  REQUIRE(host.declined() == kSky130);
  host.forget(kSky130);
  CHECK(host.ask(kSky130, kRc) == ConsentDecision::run_once);
  CHECK_FALSE(host.declined().has_value());
  host.restore_decline(kSky130);
  CHECK(host.declined() == kSky130);
  CHECK(asked == 2);
}

TEST_CASE("sandbox denials are announced in the status bar", "[cap:hefesto-security][req:Script sandbox enforcement]") {
  const script::DeniedOperation socket{"socket", "example.com 80", {"/p/fetch_models.tcl", 12}};
  // Board tile 7: the status bar ends with Show (a click opens the Diagnostics tab).
  CHECK(denial_status(socket) == "■ Sandbox blocked socket in fetch_models.tcl:12 · Show");
  CHECK(denial_status({"exec", "curl", {}}) == "■ Sandbox blocked exec in typed text · Show");
  // The collector the window drains after each command.
  ScriptEventCollector collector;
  script::ScriptEvents events;
  events.add(collector);
  events.publish_denied(socket);
  events.publish_skipped_statement({"wm geometry . 1x1", {"/p/m.tcl", 3}});
  events.publish_menu_model_changed();
  CHECK(collector.denied.size() == 1);
  CHECK(collector.skipped.size() == 1);
  CHECK(collector.menus_changed);
  events.remove(collector);
}

TEST_CASE("a long-running script shows the Interrupt card after 2 s",
          "[cap:hefesto-scripting][req:Long-running script progress and interrupt]") {
  using namespace std::chrono_literals;
  RunningScript running{"/pdk/sky130A.tcl", "/pdk/sky130A-res.tcl", 2378, 1900ms};
  CHECK_FALSE(interrupt_card(running).has_value());
  running.elapsed = 14s;
  const auto card = interrupt_card(running);
  REQUIRE(card.has_value());
  CHECK(card->title == "Running sky130A.tcl · 14 s");
  CHECK(card->detail == "source sky130A-res.tcl · line 2378");
  CHECK(card->action == "Interrupt");
  CHECK(card->shortcut == "Ctrl-C");
  CHECK(middle_ellipsis("abcdefghij", 5) == "ab…ij");
  CHECK(short_version("") == "version unknown");
}

TEST_CASE("the canvas is dimmed while a long-running script's card shows",
          "[cap:hefesto-scripting][req:Long-running script progress and interrupt]") {
  // Board note 8: the layout drawn at 35 % under the card, in both themes.
  for (const ThemeMode mode : {ThemeMode::dark, ThemeMode::light}) {
    const ThemeTokens& canvas = canvas_overlay_tokens(mode);
    CHECK_FALSE(script_dim(false, canvas).has_value());
    const auto scrim = script_dim(true, canvas);
    REQUIRE(scrim.has_value());
    CHECK(scrim->r == canvas.canvas.r);
    CHECK(scrim->g == canvas.canvas.g);
    CHECK(scrim->b == canvas.canvas.b);
    CHECK(scrim->a == kScriptDimAlpha);
    CHECK(1.0F - scrim->a == Catch::Approx(0.35F));
  }
}
