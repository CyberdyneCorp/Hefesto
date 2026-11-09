// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// SHA-256 per FIPS 180-4, section 6.2.

#include "hefesto/pdk/sha256.hpp"

#include <algorithm>
#include <fstream>
#include <vector>

#include "hefesto/diag/codes.hpp"

namespace hefesto::pdk {

namespace {

constexpr std::array<std::uint32_t, 64> kRound = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

constexpr std::uint32_t rotr(std::uint32_t x, int n) noexcept {
  return (x >> n) | (x << (32 - n));
}

}  // namespace

Sha256::Sha256() noexcept
    : state_{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19} {}

void Sha256::compress(const std::uint8_t* block) noexcept {
  std::array<std::uint32_t, 64> w{};
  for (std::size_t i = 0; i < 16; ++i) {
    w[i] = (std::uint32_t{block[4 * i]} << 24) | (std::uint32_t{block[4 * i + 1]} << 16) |
           (std::uint32_t{block[4 * i + 2]} << 8) | std::uint32_t{block[4 * i + 3]};
  }
  for (std::size_t i = 16; i < 64; ++i) {
    const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
    const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }
  auto v = state_;  // a..h
  for (std::size_t i = 0; i < 64; ++i) {
    const std::uint32_t s1 = rotr(v[4], 6) ^ rotr(v[4], 11) ^ rotr(v[4], 25);
    const std::uint32_t ch = (v[4] & v[5]) ^ (~v[4] & v[6]);
    const std::uint32_t t1 = v[7] + s1 + ch + kRound[i] + w[i];
    const std::uint32_t s0 = rotr(v[0], 2) ^ rotr(v[0], 13) ^ rotr(v[0], 22);
    const std::uint32_t maj = (v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]);
    const std::uint32_t t2 = s0 + maj;
    v = {t1 + t2, v[0], v[1], v[2], v[3] + t1, v[4], v[5], v[6]};
  }
  for (std::size_t i = 0; i < 8; ++i) {
    state_[i] += v[i];
  }
}

void Sha256::update(std::string_view data) noexcept {
  total_bytes_ += data.size();
  std::size_t offset = 0;
  while (offset < data.size()) {
    const std::size_t take = std::min(buffer_.size() - buffered_, data.size() - offset);
    for (std::size_t i = 0; i < take; ++i) {
      buffer_[buffered_ + i] = static_cast<std::uint8_t>(data[offset + i]);
    }
    buffered_ += take;
    offset += take;
    if (buffered_ == buffer_.size()) {
      compress(buffer_.data());
      buffered_ = 0;
    }
  }
}

std::array<std::uint8_t, 32> Sha256::finish() noexcept {
  const std::uint64_t bit_length = total_bytes_ * 8;
  buffer_[buffered_++] = 0x80;
  if (buffered_ > 56) {
    std::fill(buffer_.begin() + static_cast<std::ptrdiff_t>(buffered_), buffer_.end(), std::uint8_t{0});
    compress(buffer_.data());
    buffered_ = 0;
  }
  std::fill(buffer_.begin() + static_cast<std::ptrdiff_t>(buffered_), buffer_.begin() + 56, std::uint8_t{0});
  for (std::size_t i = 0; i < 8; ++i) {
    buffer_[56 + i] = static_cast<std::uint8_t>(bit_length >> (56 - 8 * i));
  }
  compress(buffer_.data());
  std::array<std::uint8_t, 32> out{};
  for (std::size_t i = 0; i < 8; ++i) {
    for (std::size_t b = 0; b < 4; ++b) {
      out[4 * i + b] = static_cast<std::uint8_t>(state_[i] >> (24 - 8 * b));
    }
  }
  return out;
}

std::string Sha256::finish_hex() {
  static constexpr std::string_view kHex = "0123456789abcdef";
  std::string hex;
  hex.reserve(64);
  for (const std::uint8_t byte : finish()) {
    hex.push_back(kHex[byte >> 4]);
    hex.push_back(kHex[byte & 0x0f]);
  }
  return hex;
}

diag::Outcome<std::string> sha256_file_hex(const platform::Path& path) {
  std::ifstream in{path, std::ios::binary};
  if (!in) {
    return diag::make(diag::codes::kPlatformRead, "cannot read " + path.string());
  }
  Sha256 hash;
  std::vector<char> chunk(std::size_t{1} << 20);
  while (in) {
    in.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
    const auto got = in.gcount();
    if (got > 0) {
      hash.update(std::string_view{chunk.data(), static_cast<std::size_t>(got)});
    }
  }
  if (in.bad()) {
    return diag::make(diag::codes::kPlatformRead, "error while reading " + path.string());
  }
  return hash.finish_hex();
}

}  // namespace hefesto::pdk
