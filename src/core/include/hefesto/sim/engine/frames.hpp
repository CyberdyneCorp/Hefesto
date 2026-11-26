// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The frames of the simulation worker protocol (openspec/changes/
// m9-lvs-simulation design D9 "Worker", Q-SIM-21 (a); capability
// simulation-engine-embedding "Crash containment on desktop", "Worker channel
// limits"). Header-only and free of process and platform headers, so the
// core's WorkerEngine and the hefesto-simd worker share one codec.
//
// On the wire a frame is a 4-byte little-endian length N, then N bytes: the
// frame body. platform::WorkerChannel adds and strips the length on the core
// side; hefesto-simd uses append_length()/parse_length(). A body is one kind
// byte and its payload; every integer and double is little-endian, a string
// is a u32 byte count and the bytes, a list is a u32 count and its items:
//
//   hello  worker -> core  u32 protocol, str engine_version
//   load   core -> worker  u8 flags (1 first, 2 last), u32 seed, list<str>
//                          init_commands, list<str> lines; a deck larger than
//                          one frame is sent as several load frames, the seed
//                          and init commands in the first
//   cmd    core -> worker  u8 command (run, pause, resume, cancel, reset, quit)
//   event  worker -> core  u8 event kind, then
//            output        u8 stream (0 output, 1 error), str text
//            progress      str status, f64 point (percent, or -1)
//            state         u8 state (sim::EngineState's order)
//            reply         u8 request (0 load, else the command), u8 ok, str message
//            vectors_done  u32 vector count
//            data          u32 count, then count times: str name, f64 value
//                          (one streamed point; added in M9 R2)
//   vec    worker -> core  str name, str type, u8 complex, u64 length, u64
//                          offset, u32 count, f64[count] real, f64[count] imag
//                          when complex; a vector longer than one frame is
//                          sent as several chunks
//   beat   worker -> core  u64 sequence
//
// decode() never reads outside the body and never allocates more than the
// body could hold: a short, overlong or inconsistent body is `malformed`, an
// unknown kind byte is `unknown_kind`, and a length above the limit is
// `oversize` (64 MiB; Q-SIM-25).
//
// Owner: SIMD.

#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace hefesto::sim::frames {

inline constexpr std::uint32_t kProtocolVersion = 1;
/// The largest frame body in either direction (Q-SIM-25).
inline constexpr std::uint32_t kMaxFrameBytes = 64U * 1024U * 1024U;
/// Values per vec chunk: 8 MiB of doubles per part, well inside the limit.
inline constexpr std::size_t kVectorChunkValues = 1U << 20U;
/// Bytes of deck text per load frame (the rest of the limit is headroom).
inline constexpr std::size_t kLoadChunkBytes = 32U * 1024U * 1024U;

enum class Kind : std::uint8_t { hello = 1, load = 2, cmd = 3, event = 4, vec = 5, beat = 6 };
enum class Command : std::uint8_t { run = 1, pause = 2, resume = 3, cancel = 4, reset = 5, quit = 6 };
enum class EventKind : std::uint8_t { output = 1, progress = 2, state = 3, reply = 4, vectors_done = 5, data = 6 };
enum class Stream : std::uint8_t { output = 0, error = 1 };
/// sim::EngineState in the same order (the core checks the range).
enum class State : std::uint8_t { idle = 0, loaded = 1, running = 2, paused = 3, failed = 4 };

inline constexpr std::uint8_t kLoadFirst = 1;
inline constexpr std::uint8_t kLoadLast = 2;
/// Reply::request of a load; the other requests are a Command.
inline constexpr std::uint8_t kRequestLoad = 0;

struct Hello {
  std::uint32_t protocol = kProtocolVersion;
  std::string engine_version;
  friend bool operator==(const Hello&, const Hello&) = default;
};
struct Load {
  std::uint8_t flags = kLoadFirst | kLoadLast;
  std::uint32_t seed = 1;
  std::vector<std::string> init_commands;
  std::vector<std::string> lines;
  friend bool operator==(const Load&, const Load&) = default;
};
struct Cmd {
  Command command = Command::run;
  friend bool operator==(const Cmd&, const Cmd&) = default;
};
struct Output {
  Stream stream = Stream::output;
  std::string text;
  friend bool operator==(const Output&, const Output&) = default;
};
struct Progress {
  std::string status;
  double point = -1.0;
  friend bool operator==(const Progress&, const Progress&) = default;
};
struct StateChange {
  State state = State::idle;
  friend bool operator==(const StateChange&, const StateChange&) = default;
};
struct Reply {
  std::uint8_t request = kRequestLoad;
  bool ok = true;
  std::string message;
  friend bool operator==(const Reply&, const Reply&) = default;
};
struct VectorsDone {
  std::uint32_t count = 0;
  friend bool operator==(const VectorsDone&, const VectorsDone&) = default;
};
struct VecChunk {
  std::string name;
  std::string type;
  bool complex = false;
  std::uint64_t length = 0;  // values in the whole vector
  std::uint64_t offset = 0;  // index of real[0] in the vector
  std::vector<double> real;
  std::vector<double> imag;  // same size as real when complex, else empty
  friend bool operator==(const VecChunk&, const VecChunk&) = default;
};
struct Beat {
  std::uint64_t sequence = 0;
  friend bool operator==(const Beat&, const Beat&) = default;
};
/// Added in M9 R2: one value of a streamed point.
struct DataValue {
  std::string name;
  double value = 0.0;
  friend bool operator==(const DataValue&, const DataValue&) = default;
};
/// Added in M9 R2: one streamed point (ngspice's SendData), the scale first;
/// the worker sends at most kMaxDataValues values per frame.
struct Data {
  std::vector<DataValue> values;
  friend bool operator==(const Data&, const Data&) = default;
};
inline constexpr std::size_t kMaxDataValues = 1U << 16U;

using Message = std::variant<Hello, Load, Cmd, Output, Progress, StateChange, Reply, VectorsDone, VecChunk, Beat, Data>;

enum class Error : std::uint8_t { none, malformed, unknown_kind, oversize };

struct Decoded {
  Error error = Error::none;
  std::string detail;  // what is wrong, when error != none
  std::optional<Message> message;
};

[[nodiscard]] inline std::string_view error_name(Error error) noexcept {
  switch (error) {
    case Error::none: return "none";
    case Error::malformed: return "malformed";
    case Error::unknown_kind: return "unknown kind";
    case Error::oversize: return "oversize";
  }
  return "unknown";
}

// --- encoding ---------------------------------------------------------------

namespace detail {

class Writer {
 public:
  void u8(std::uint8_t v) { bytes_.push_back(static_cast<std::byte>(v)); }
  void u32(std::uint32_t v) { unsigned_le(v, 4); }
  void u64(std::uint64_t v) { unsigned_le(v, 8); }
  void f64(double v) { u64(std::bit_cast<std::uint64_t>(v)); }
  void str(std::string_view s) {
    u32(static_cast<std::uint32_t>(s.size()));
    const auto* p = reinterpret_cast<const std::byte*>(s.data());  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    bytes_.insert(bytes_.end(), p, p + s.size());
  }
  void strings(const std::vector<std::string>& list) {
    u32(static_cast<std::uint32_t>(list.size()));
    for (const std::string& s : list) {
      str(s);
    }
  }
  void doubles(const std::vector<double>& values) {
    for (const double v : values) {
      f64(v);
    }
  }
  [[nodiscard]] std::vector<std::byte> take() { return std::move(bytes_); }

 private:
  void unsigned_le(std::uint64_t v, int count) {
    for (int i = 0; i < count; ++i) {
      bytes_.push_back(static_cast<std::byte>((v >> (8 * i)) & 0xFFU));
    }
  }
  std::vector<std::byte> bytes_;
};

inline void encode_event_kind(Writer& w, EventKind kind) {
  w.u8(static_cast<std::uint8_t>(Kind::event));
  w.u8(static_cast<std::uint8_t>(kind));
}

struct EncodeVisitor {
  Writer& w;
  void operator()(const Hello& m) const {
    w.u8(static_cast<std::uint8_t>(Kind::hello));
    w.u32(m.protocol);
    w.str(m.engine_version);
  }
  void operator()(const Load& m) const {
    w.u8(static_cast<std::uint8_t>(Kind::load));
    w.u8(m.flags);
    w.u32(m.seed);
    w.strings(m.init_commands);
    w.strings(m.lines);
  }
  void operator()(const Cmd& m) const {
    w.u8(static_cast<std::uint8_t>(Kind::cmd));
    w.u8(static_cast<std::uint8_t>(m.command));
  }
  void operator()(const Output& m) const {
    encode_event_kind(w, EventKind::output);
    w.u8(static_cast<std::uint8_t>(m.stream));
    w.str(m.text);
  }
  void operator()(const Progress& m) const {
    encode_event_kind(w, EventKind::progress);
    w.str(m.status);
    w.f64(m.point);
  }
  void operator()(const StateChange& m) const {
    encode_event_kind(w, EventKind::state);
    w.u8(static_cast<std::uint8_t>(m.state));
  }
  void operator()(const Reply& m) const {
    encode_event_kind(w, EventKind::reply);
    w.u8(m.request);
    w.u8(m.ok ? 1 : 0);
    w.str(m.message);
  }
  void operator()(const VectorsDone& m) const {
    encode_event_kind(w, EventKind::vectors_done);
    w.u32(m.count);
  }
  void operator()(const VecChunk& m) const {
    w.u8(static_cast<std::uint8_t>(Kind::vec));
    w.str(m.name);
    w.str(m.type);
    w.u8(m.complex ? 1 : 0);
    w.u64(m.length);
    w.u64(m.offset);
    w.u32(static_cast<std::uint32_t>(m.real.size()));
    w.doubles(m.real);
    if (m.complex) {
      w.doubles(m.imag);
    }
  }
  void operator()(const Beat& m) const {
    w.u8(static_cast<std::uint8_t>(Kind::beat));
    w.u64(m.sequence);
  }
  void operator()(const Data& m) const {
    encode_event_kind(w, EventKind::data);
    w.u32(static_cast<std::uint32_t>(m.values.size()));
    for (const DataValue& v : m.values) {
      w.str(v.name);
      w.f64(v.value);
    }
  }
};

}  // namespace detail

/// The frame body of `message` (no length prefix). The caller keeps bodies
/// within kMaxFrameBytes (load_chunks() and vec chunking do).
[[nodiscard]] inline std::vector<std::byte> encode(const Message& message) {
  detail::Writer w;
  std::visit(detail::EncodeVisitor{w}, message);
  return w.take();
}

/// Splits a deck into load frames of at most kLoadChunkBytes of text each
/// (a line longer than that is a frame of its own). The first carries the
/// seed and init commands; the last has kLoadLast set.
[[nodiscard]] inline std::vector<Load> load_chunks(const std::vector<std::string>& lines, std::uint32_t seed,
                                                   const std::vector<std::string>& init_commands,
                                                   std::size_t chunk_bytes = kLoadChunkBytes) {
  std::vector<Load> chunks(1);
  chunks.back().flags = kLoadFirst;
  chunks.back().seed = seed;
  chunks.back().init_commands = init_commands;
  std::size_t used = 0;
  for (const std::string& line : lines) {
    if (used > 0 && used + line.size() > chunk_bytes) {
      chunks.emplace_back();
      chunks.back().flags = 0;
      used = 0;
    }
    chunks.back().lines.push_back(line);
    used += line.size() + 4;
  }
  chunks.back().flags = static_cast<std::uint8_t>(chunks.back().flags | kLoadLast);
  return chunks;
}

/// Appends the 4-byte little-endian length of `body` and then `body`.
inline void append_length(std::vector<std::byte>& out, std::span<const std::byte> body) {
  const auto size = static_cast<std::uint32_t>(body.size());
  for (int i = 0; i < 4; ++i) {
    out.push_back(static_cast<std::byte>((size >> (8 * i)) & 0xFFU));
  }
  out.insert(out.end(), body.begin(), body.end());
}

/// The body length a 4-byte prefix announces: Error::malformed for 0,
/// Error::oversize above `limit`.
[[nodiscard]] inline std::pair<std::uint32_t, Error> parse_length(std::span<const std::byte, 4> prefix,
                                                                  std::uint32_t limit = kMaxFrameBytes) {
  std::uint32_t size = 0;
  for (std::size_t i = 0; i < 4; ++i) {
    size |= static_cast<std::uint32_t>(prefix[i]) << (8U * i);
  }
  if (size == 0) {
    return {size, Error::malformed};
  }
  return {size, size > limit ? Error::oversize : Error::none};
}

// --- decoding ---------------------------------------------------------------

namespace detail {

/// Bounds-checked reader: every read past the end sets failed() and returns
/// a zero value; nothing is read outside the span.
class Reader {
 public:
  explicit Reader(std::span<const std::byte> bytes) : bytes_(bytes) {}

  std::uint8_t u8() { return static_cast<std::uint8_t>(unsigned_le(1)); }
  std::uint32_t u32() { return static_cast<std::uint32_t>(unsigned_le(4)); }
  std::uint64_t u64() { return unsigned_le(8); }
  double f64() { return std::bit_cast<double>(u64()); }
  std::string str() {
    const std::uint32_t size = u32();
    if (!take(size)) {
      return {};
    }
    const auto* p = reinterpret_cast<const char*>(bytes_.data() + pos_ - size);  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    return {p, size};
  }
  /// A list of strings; each item takes at least 4 bytes, so a count the
  /// rest of the body cannot hold fails before anything is allocated.
  std::vector<std::string> strings() {
    const std::uint32_t count = u32();
    if (failed_ || count > remaining() / 4) {
      failed_ = true;
      return {};
    }
    std::vector<std::string> list;
    list.reserve(count);
    for (std::uint32_t i = 0; i < count && !failed_; ++i) {
      list.push_back(str());
    }
    return list;
  }
  std::vector<double> doubles(std::uint32_t count) {
    if (failed_ || count > remaining() / 8) {
      failed_ = true;
      return {};
    }
    std::vector<double> values(count);
    for (double& v : values) {
      v = f64();
    }
    return values;
  }
  [[nodiscard]] bool failed() const noexcept { return failed_; }
  [[nodiscard]] std::size_t remaining() const noexcept { return bytes_.size() - pos_; }
  /// True when every byte was read and nothing failed.
  [[nodiscard]] bool complete() const noexcept { return !failed_ && pos_ == bytes_.size(); }

 private:
  bool take(std::size_t count) {
    if (failed_ || count > remaining()) {
      failed_ = true;
      return false;
    }
    pos_ += count;
    return true;
  }
  std::uint64_t unsigned_le(std::size_t count) {
    if (!take(count)) {
      return 0;
    }
    std::uint64_t v = 0;
    for (std::size_t i = 0; i < count; ++i) {
      v |= static_cast<std::uint64_t>(bytes_[pos_ - count + i]) << (8U * i);
    }
    return v;
  }

  std::span<const std::byte> bytes_;
  std::size_t pos_ = 0;
  bool failed_ = false;
};

inline Decoded malformed(std::string detail) { return Decoded{Error::malformed, std::move(detail), std::nullopt}; }

inline Decoded finish(Reader& r, Message message, std::string_view what) {
  if (!r.complete()) {
    return malformed(std::string{what} + " frame is truncated or has trailing bytes");
  }
  return Decoded{Error::none, {}, std::move(message)};
}

inline Decoded decode_load(Reader& r) {
  Load m;
  m.flags = r.u8();
  m.seed = r.u32();
  m.init_commands = r.strings();
  m.lines = r.strings();
  if ((m.flags & ~(kLoadFirst | kLoadLast)) != 0) {
    return malformed("load frame has unknown flags");
  }
  return finish(r, std::move(m), "load");
}

inline Decoded decode_cmd(Reader& r) {
  const std::uint8_t command = r.u8();
  if (command < static_cast<std::uint8_t>(Command::run) || command > static_cast<std::uint8_t>(Command::quit)) {
    return malformed("cmd frame has an unknown command " + std::to_string(command));
  }
  return finish(r, Cmd{static_cast<Command>(command)}, "cmd");
}

inline Decoded decode_output(Reader& r) {
  const std::uint8_t stream = r.u8();
  Output m{static_cast<Stream>(stream), r.str()};
  if (stream > static_cast<std::uint8_t>(Stream::error)) {
    return malformed("output event has an unknown stream");
  }
  return finish(r, std::move(m), "output event");
}

inline Decoded decode_state(Reader& r) {
  const std::uint8_t state = r.u8();
  if (state > static_cast<std::uint8_t>(State::failed)) {
    return malformed("state event has an unknown state " + std::to_string(state));
  }
  return finish(r, StateChange{static_cast<State>(state)}, "state event");
}

inline Decoded decode_reply(Reader& r) {
  Reply m;
  m.request = r.u8();
  const std::uint8_t ok = r.u8();
  m.ok = ok != 0;
  m.message = r.str();
  if (ok > 1 || m.request > static_cast<std::uint8_t>(Command::quit)) {
    return malformed("reply event has an unknown request or flag");
  }
  return finish(r, std::move(m), "reply event");
}

/// Each value takes at least 12 bytes, so a count the rest of the body
/// cannot hold fails before anything is allocated.
inline Decoded decode_data(Reader& r) {
  const std::uint32_t count = r.u32();
  if (r.failed() || count > r.remaining() / 12) {
    return malformed("data event has more values than its frame holds");
  }
  Data m;
  m.values.reserve(count);
  for (std::uint32_t i = 0; i < count && !r.failed(); ++i) {
    std::string name = r.str();
    m.values.push_back(DataValue{std::move(name), r.f64()});
  }
  return finish(r, std::move(m), "data event");
}

inline Decoded decode_event(Reader& r) {
  switch (static_cast<EventKind>(r.u8())) {
    case EventKind::output: return decode_output(r);
    case EventKind::progress: {
      Progress m;
      m.status = r.str();
      m.point = r.f64();
      return finish(r, std::move(m), "progress event");
    }
    case EventKind::state: return decode_state(r);
    case EventKind::reply: return decode_reply(r);
    case EventKind::vectors_done: return finish(r, VectorsDone{r.u32()}, "vectors_done event");
    case EventKind::data: return decode_data(r);
  }
  return malformed("event frame has an unknown event kind");
}

inline Decoded decode_vec(Reader& r) {
  VecChunk m;
  m.name = r.str();
  m.type = r.str();
  const std::uint8_t complex = r.u8();
  m.complex = complex == 1;
  m.length = r.u64();
  m.offset = r.u64();
  const std::uint32_t count = r.u32();
  m.real = r.doubles(count);
  if (m.complex) {
    m.imag = r.doubles(count);
  }
  if (complex > 1 || m.offset > m.length || count > m.length - m.offset) {
    return malformed("vec frame has an inconsistent chunk");
  }
  return finish(r, std::move(m), "vec");
}

}  // namespace detail

/// Decodes one frame body of at most `limit` bytes.
[[nodiscard]] inline Decoded decode(std::span<const std::byte> body, std::uint32_t limit = kMaxFrameBytes) {
  if (body.empty()) {
    return detail::malformed("empty frame");
  }
  if (body.size() > limit) {
    return Decoded{Error::oversize, "frame of " + std::to_string(body.size()) + " bytes", std::nullopt};
  }
  detail::Reader r{body.subspan(1)};
  switch (static_cast<Kind>(body.front())) {
    case Kind::hello: {
      Hello m;
      m.protocol = r.u32();
      m.engine_version = r.str();
      return detail::finish(r, std::move(m), "hello");
    }
    case Kind::load: return detail::decode_load(r);
    case Kind::cmd: return detail::decode_cmd(r);
    case Kind::event: return detail::decode_event(r);
    case Kind::vec: return detail::decode_vec(r);
    case Kind::beat: return detail::finish(r, Beat{r.u64()}, "beat");
  }
  return Decoded{Error::unknown_kind, "frame kind " + std::to_string(static_cast<unsigned>(body.front())),
                 std::nullopt};
}

}  // namespace hefesto::sim::frames
