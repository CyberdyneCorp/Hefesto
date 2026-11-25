// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The worker frame codec (hefesto/sim/engine/frames.hpp; openspec/changes/
// m9-lvs-simulation design D9). Owner: SIMD.

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "hefesto/sim/engine/frames.hpp"

namespace frames = hefesto::sim::frames;

namespace {

std::vector<std::byte> bytes(std::initializer_list<int> values) {
  std::vector<std::byte> out;
  for (const int v : values) {
    out.push_back(static_cast<std::byte>(v));
  }
  return out;
}

frames::Message round_trip(const frames::Message& message) {
  const frames::Decoded decoded = frames::decode(frames::encode(message));
  REQUIRE(decoded.error == frames::Error::none);
  REQUIRE(decoded.message.has_value());
  return *decoded.message;
}

}  // namespace

TEST_CASE("every frame kind survives encode and decode",
          "[cap:simulation-engine-embedding][req:Crash containment on desktop]") {
  frames::VecChunk vec;
  vec.name = "v(out)";
  vec.type = "voltage";
  vec.complex = true;
  vec.length = 5;
  vec.offset = 2;
  vec.real = {1.5, -0.0, std::numeric_limits<double>::infinity()};
  vec.imag = {0.25, 1e-300, -2.0};
  const std::vector<frames::Message> messages = {
      frames::Hello{frames::kProtocolVersion, "ngspice 47-hefesto1"},
      frames::Load{frames::kLoadFirst, 7, {"set ngbehavior=hsa"}, {"* title", "R1 a b 1k", ".end"}},
      frames::Cmd{frames::Command::cancel},
      frames::Output{frames::Stream::error, "Error: unknown subckt: xinv"},
      frames::Progress{"tran", 34.5},
      frames::StateChange{frames::State::paused},
      frames::Reply{static_cast<std::uint8_t>(frames::Command::run), false, "engine busy"},
      frames::VectorsDone{2},
      vec,
      frames::Beat{42},
  };
  for (const frames::Message& message : messages) {
    CHECK(round_trip(message) == message);
  }
}

TEST_CASE("frames are little-endian and length-prefixed",
          "[cap:simulation-engine-embedding][req:Crash containment on desktop]") {
  const std::vector<std::byte> body = frames::encode(frames::Beat{0x0102030405060708ULL});
  CHECK(body == bytes({6, 8, 7, 6, 5, 4, 3, 2, 1}));
  std::vector<std::byte> framed;
  frames::append_length(framed, body);
  CHECK(framed.size() == 13);
  CHECK(framed[0] == std::byte{9});
  const std::span<const std::byte, 4> prefix{framed.data(), 4};
  CHECK(frames::parse_length(prefix).first == 9);
  CHECK(frames::parse_length(prefix).second == frames::Error::none);
}

TEST_CASE("a zero or oversize length is refused",
          "[cap:simulation-engine-embedding][req:Worker channel limits]") {
  const std::vector<std::byte> zero = bytes({0, 0, 0, 0});
  CHECK(frames::parse_length(std::span<const std::byte, 4>{zero.data(), 4}).second == frames::Error::malformed);
  // 64 MiB is accepted, one byte more is oversize (Q-SIM-25).
  const std::vector<std::byte> limit = bytes({0, 0, 0, 4});
  CHECK(frames::parse_length(std::span<const std::byte, 4>{limit.data(), 4}).second == frames::Error::none);
  const std::vector<std::byte> above = bytes({1, 0, 0, 4});
  CHECK(frames::parse_length(std::span<const std::byte, 4>{above.data(), 4}).second == frames::Error::oversize);
  const std::vector<std::byte> lowered = bytes({11, 0, 0, 0});
  CHECK(frames::parse_length(std::span<const std::byte, 4>{lowered.data(), 4}, 10).second == frames::Error::oversize);
  std::vector<std::byte> body = frames::encode(frames::Output{frames::Stream::output, std::string(64, 'x')});
  CHECK(frames::decode(body, 16).error == frames::Error::oversize);
}

TEST_CASE("malformed and unknown frames are errors, never reads outside the frame",
          "[cap:simulation-engine-embedding][req:Worker channel limits]") {
  CHECK(frames::decode({}).error == frames::Error::malformed);
  CHECK(frames::decode(bytes({0})).error == frames::Error::unknown_kind);
  CHECK(frames::decode(bytes({99, 1, 2})).error == frames::Error::unknown_kind);
  // Truncated beat, trailing bytes after a beat.
  CHECK(frames::decode(bytes({6, 1, 2, 3})).error == frames::Error::malformed);
  CHECK(frames::decode(bytes({6, 1, 0, 0, 0, 0, 0, 0, 0, 9})).error == frames::Error::malformed);
  // A string whose length runs past the end.
  CHECK(frames::decode(bytes({1, 1, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0x7F, 'a'})).error == frames::Error::malformed);
  // A list count the body cannot hold fails before allocating.
  CHECK(frames::decode(bytes({2, 3, 1, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF})).error == frames::Error::malformed);
  // Unknown command, event kind, stream, state, load flags.
  CHECK(frames::decode(bytes({3, 0})).error == frames::Error::malformed);
  CHECK(frames::decode(bytes({3, 7})).error == frames::Error::malformed);
  CHECK(frames::decode(bytes({4, 9})).error == frames::Error::malformed);
  CHECK(frames::decode(bytes({4, 1, 2, 0, 0, 0, 0})).error == frames::Error::malformed);
  CHECK(frames::decode(bytes({4, 3, 5})).error == frames::Error::malformed);
  CHECK(frames::decode(bytes({2, 4, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0})).error == frames::Error::malformed);
}

TEST_CASE("a vec chunk must fit its vector",
          "[cap:simulation-engine-embedding][req:Worker channel limits]") {
  frames::VecChunk chunk;
  chunk.name = "v";
  chunk.length = 2;
  chunk.offset = 1;
  chunk.real = {1.0, 2.0};  // offset 1 + 2 values > length 2
  CHECK(frames::decode(frames::encode(chunk)).error == frames::Error::malformed);
  chunk.offset = 3;
  chunk.real = {};
  CHECK(frames::decode(frames::encode(chunk)).error == frames::Error::malformed);
  // A count larger than the body: the doubles are never allocated.
  std::vector<std::byte> body = frames::encode(frames::VecChunk{"v", "t", false, 10, 0, {}, {}});
  body[body.size() - 4] = std::byte{0xFF};
  body[body.size() - 1] = std::byte{0x7F};
  CHECK(frames::decode(body).error == frames::Error::malformed);
}

TEST_CASE("a large deck is split into load frames below the limit",
          "[cap:simulation-engine-embedding][req:Worker channel limits]") {
  const std::vector<std::string> deck(100, std::string(50, 'r'));
  const std::vector<frames::Load> chunks = frames::load_chunks(deck, 9, {"set x"}, 1000);
  REQUIRE(chunks.size() > 1);
  CHECK(chunks.front().flags == frames::kLoadFirst);
  CHECK(chunks.front().seed == 9);
  CHECK(chunks.front().init_commands == std::vector<std::string>{"set x"});
  CHECK(chunks.back().flags == frames::kLoadLast);
  std::size_t lines = 0;
  for (const frames::Load& chunk : chunks) {
    CHECK(frames::encode(chunk).size() <= 1000 + 64 + 4 * 50);
    lines += chunk.lines.size();
    if (&chunk != &chunks.front()) {
      CHECK(chunk.init_commands.empty());
    }
  }
  CHECK(lines == deck.size());
  const std::vector<frames::Load> single = frames::load_chunks({"a"}, 1, {});
  REQUIRE(single.size() == 1);
  CHECK(single.front().flags == (frames::kLoadFirst | frames::kLoadLast));
}
