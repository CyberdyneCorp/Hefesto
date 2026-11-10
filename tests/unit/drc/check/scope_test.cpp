// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// drc-rule-semantics "Basic check scope" and "Error rectangle reporting":
// which tiles and edge segments the basic check examines, subcell-error
// tiles, the edge-plane skip, cancellation, statistics and the union of
// duplicate reports on an error plane.

#include <atomic>

#include "check_fixture.hpp"
#include "hefesto/db/plane.hpp"

using namespace hefesto;
using namespace hefesto::drc::test;
using drc::RuleSetBuilder;

#define SCOPE "[cap:drc-rule-semantics][req:Basic check scope]"
#define REPORTING "[cap:drc-rule-semantics][req:Error rectangle reporting]"

TEST_CASE("an edge on the left side of K is examined (spec scenario)", SCOPE) {
  Layout l;
  l.paint(l.t.m1, Rect{0, 0, 10, 10});
  l.paint(l.t.m1, Rect{12, 0, 20, 10});
  RuleSetBuilder b;
  add_spacing(b, mask_of({l.t.m1}), l.t.metal1, 3, b.why("s"));
  const auto rules = b.build();
  const Rect k{10, 0, 30, 10};
  CHECK(run_check(l, *rules, Options{.check = k, .clip = k}) == std::vector<Rect>{Rect{12, 0, 13, 10}});
}

TEST_CASE("edges left of K.xbot and below K.ybot are not examined", SCOPE) {
  Layout l;
  l.paint(l.t.poly, Rect{0, 0, 1, 10});
  RuleSetBuilder b;
  add_width(b, mask_of({l.t.poly}), l.t.active, 2, b.why("w"));
  const auto rules = b.build();
  CHECK(run_check(l, *rules) == std::vector<Rect>{Rect{1, 0, 2, 10}});
  // X = 0 is outside [K.xbot, K.xtop): the left edge is not examined.
  CHECK(run_check(l, *rules, Options{.check = Rect{1, -10, 20, 20}}).empty());
  // A K ending at the edge excludes it too (X must be < K.xtop).
  CHECK(run_check(l, *rules, Options{.check = Rect{-10, -10, 0, 20}}).empty());
}

TEST_CASE("same-type edges never carry a rule", SCOPE) {
  Layout l;
  l.paint(l.t.m1, Rect{0, 0, 10, 10});
  l.paint(l.t.m1, Rect{0, 10, 5, 20});  // internal m1|m1 tile boundaries
  RuleSetBuilder b;
  const auto anything = drc::test::make_rule(5, 5, TypeMask{}, TypeMask{}, drc::flags::kForward, l.t.metal1,
                                             l.t.metal1, b.why("never"));
  b.add(l.t.m1, l.t.m1, anything);
  const auto rules = b.build();
  CHECK(run_check(l, *rules).empty());
}

TEST_CASE("subcell-error tiles are errors clipped to K with why 2", SCOPE) {
  Layout l;
  REQUIRE(l.cell.plane(l.t.metal1).paint(Rect{0, 0, 10, 10}, drc::identity_paint_row(tech::kErrorS)).ok());
  RuleSetBuilder b;
  const auto rules = b.build();
  const auto reports =
      run_reports(l, *rules, Options{.check = Rect{5, -5, 20, 20}, .clip = Rect{6, 6, 7, 7}});
  REQUIRE(reports.size() == 1);
  CHECK(reports[0].area == Rect{5, 0, 10, 10});
  CHECK(reports[0].rule.why == drc::why::kOverlap);
}

TEST_CASE("no rules or a null K report nothing", SCOPE) {
  Layout l;
  l.paint(l.t.poly, Rect{0, 0, 1, 10});
  RuleSetBuilder b;
  add_width(b, mask_of({l.t.poly}), l.t.active, 2, b.why("w"));
  const auto rules = b.build();
  CHECK(run_check(l, *rules, Options{.check = Rect{5, 5, 5, 10}}).empty());
  drc::CheckInput input;
  input.cell = &l.cell;
  input.technology = l.t.tech.get();
  drc::CollectingErrorSink sink;
  CHECK(drc::basic_check(input, kEverywhere, kEverywhere, sink) == drc::CheckOutcome::completed);
  CHECK(sink.count() == 0);
}

TEST_CASE("rules of another edge plane are skipped, and a skipped trigger skips its rule", SCOPE) {
  Layout l;
  l.paint(l.t.m1, Rect{0, 0, 10, 10});
  l.paint(l.t.m1, Rect{12, 0, 20, 10});
  const TypeMask others = on_plane(l.t.metal1) & ~mask_of({l.t.m1});
  RuleSetBuilder b;
  const auto why = b.why("s");
  // A trigger whose edge plane is metal2, then the spacing rule it guards.
  b.add(l.t.m1, tech::kSpace,
        make_rule(1, 1, TypeMask{}, others, drc::flags::kTrigger, l.t.metal1, l.t.metal2, why));
  b.add(l.t.m1, tech::kSpace, make_rule(3, 3, others, others, drc::flags::kForward, l.t.metal1, l.t.metal1, why));
  const auto rules = b.build();
  CHECK(run_check(l, *rules).empty());

  RuleSetBuilder alone;
  alone.add(l.t.m1, tech::kSpace,
            make_rule(3, 3, others, others, drc::flags::kForward, l.t.metal1, l.t.metal2, alone.why("s")));
  CHECK(run_check(l, *alone.build()).empty());
}

TEST_CASE("a requested cancel stops the check", SCOPE) {
  Layout l;
  l.paint(l.t.poly, Rect{0, 0, 1, 10});
  RuleSetBuilder b;
  add_width(b, mask_of({l.t.poly}), l.t.active, 2, b.why("w"));
  const auto rules = b.build();
  std::atomic<bool> stop{true};
  drc::CheckInput input;
  input.cell = &l.cell;
  input.technology = l.t.tech.get();
  input.rules = rules.get();
  input.cancel.flag = &stop;
  drc::CollectingErrorSink sink;
  CHECK(drc::basic_check(input, kEverywhere, kEverywhere, sink) == drc::CheckOutcome::cancelled);
  CHECK(sink.count() == 0);
  stop = false;
  CHECK(drc::basic_check(input, kEverywhere, kEverywhere, sink) == drc::CheckOutcome::completed);
  CHECK(sink.count() == 1);
}

TEST_CASE("the kernel counts tiles, edges, rules and searches", SCOPE) {
  Layout l;
  l.paint(l.t.poly, Rect{0, 0, 1, 10});
  RuleSetBuilder b;
  add_width(b, mask_of({l.t.poly}), l.t.active, 2, b.why("w"));
  const auto rules = b.build();
  drc::CheckInput input;
  input.cell = &l.cell;
  input.technology = l.t.tech.get();
  input.rules = rules.get();
  drc::CollectingErrorSink sink;
  drc::CheckStatistics stats;
  sink.set_statistics(&stats);
  REQUIRE(drc::basic_check(input, kEverywhere, kEverywhere, sink) == drc::CheckOutcome::completed);
  CHECK(stats.tiles > 0);
  CHECK(stats.edges > 0);
  CHECK(stats.rules == 2);  // the left edge and the bottom edge of the poly
  CHECK(stats.slow == 1);  // the bottom edge is proved clean by the poly tile itself
  CHECK(stats.squares == 0);  // engine counters are not touched
}

namespace {

/// Paints every report on an error plane, as the engine does.
class PaintingSink final : public drc::ErrorSink {
 public:
  db::Plane plane;

 protected:
  void on_error(const Rect& area, const drc::Rule& /*rule*/) override {
    REQUIRE(plane.paint(area, drc::identity_paint_row(tech::kErrorP)).ok());
  }
};

}  // namespace

TEST_CASE("duplicate reports merge on the error plane and are counted per report", REPORTING) {
  Layout l;
  l.paint(l.t.poly, Rect{0, 0, 1, 10});
  RuleSetBuilder b;
  const TypeMask poly = mask_of({l.t.poly});
  add_width(b, poly, l.t.active, 2, b.why("w2"));
  add_width(b, poly, l.t.active, 3, b.why("w3"));
  const auto rules = b.build();
  const auto reports = run_reports(l, *rules);
  REQUIRE(reports.size() == 2);
  CHECK(reports[0].area == Rect{1, 0, 2, 10});
  CHECK(reports[0].rule.why == rules->why().find("w2"));
  CHECK(reports[1].area == Rect{1, 0, 3, 10});
  CHECK(reports[1].rule.why == rules->why().find("w3"));

  drc::CheckInput input;
  input.cell = &l.cell;
  input.technology = l.t.tech.get();
  input.rules = rules.get();
  PaintingSink sink;
  REQUIRE(drc::basic_check(input, kEverywhere, kEverywhere, sink) == drc::CheckOutcome::completed);
  CHECK(sink.count() == 2);
  std::vector<Rect> painted;
  sink.plane.enumerate(kEverywhere, TypeMask::of(tech::kErrorP), [&](const db::TileView& t) {
    painted.push_back(t.area);
    return true;
  });
  CHECK(painted == std::vector<Rect>{Rect{1, 0, 3, 10}});
}
