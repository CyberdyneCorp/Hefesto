// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// A PDK manager over an in-memory catalog (MapFetcher) and a temporary PDK
// root, shared by the manager, progress and pack tests.

#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "fakes.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/pdk/manager.hpp"
#include "temp_dir.hpp"

namespace hefesto::test::pdk_harness {

namespace pdk = hefesto::pdk;
namespace fs = std::filesystem;
using hefesto::test::TarWriter;


inline constexpr const char* kBase = "https://catalog.example/pdks";
inline const std::string kHashA = "0fe599b2afb6708d281543108caf8310912f54af";
inline const std::string kHashB = "bdc9412b3e468c102d01b7cf6337be06ec6e9c9a";

struct Harness {
  hefesto::test::TempDir dir;
  hefesto::platform::LocalFileSystem files;
  hefesto::test::FakeHost host;
  hefesto::test::MapFetcher fetcher;
  hefesto::test::CopyDecompressor decompressor;
  pdk::PdkManager manager{files, host, fetcher, pdk::Catalog::builtin()};
  hefesto::diag::CollectingSink sink;
  pdk::InstallOptions options;
  fs::path root;

  Harness() {
    root = dir.path() / "root";
    manager.set_root(root);
    manager.set_decompressor(&decompressor);
    options.source.location = kBase;
    options.accept_license = true;
    options.accept_unverified = true;
    publish(kHashA, "2024-08-17T01:34:30Z");
  }

  [[nodiscard]] const pdk::Family& sky130() const { return *manager.catalog().find_family("sky130"); }
  [[nodiscard]] pdk::Selection variant(const char* name) const { return manager.catalog().resolve(name).value(); }

  // A version with `common` (both variants' libs.tech) and three libraries.
  void publish(const std::string& hash, const std::string& date) {
    std::string family = std::string{"{\"pdk\":\"sky130\",\"versions\":["};
    family += "{\"version\":\"" + hash + "\",\"date\":\"" + date + "\"}]}";
    fetcher.documents[std::string{kBase} + "/sky130/manifest.json"] = family;
    std::string assets;
    const auto add = [&](const std::string& content, const std::string& tar) {
      const std::string url = std::string{kBase} + "/sky130/" + hash + "/" + content + ".tar.zst";
      fetcher.documents[url] = tar;
      assets += std::string{assets.empty() ? "" : ","} + "{\"content\":\"" + content + "\",\"filename\":\"" + content +
                ".tar.zst\",\"url\":\"" + url + "\",\"sha256\":\"" + pdk::sha256_hex(tar) +
                "\",\"size\":" + std::to_string(tar.size()) + "}";
    };
    add("common", TarWriter{}
                      .file("sky130A/libs.tech/magic/sky130A.tech", "tech A\n")
                      .file("sky130A/libs.tech/magic/sky130A.magicrc", "set VDD VPWR\n")
                      .file("sky130B/libs.tech/magic/sky130B.tech", "tech B\n")
                      .finish());
    for (const char* lib : {"sky130_fd_sc_hd", "sky130_fd_sc_hs", "sky130_fd_pr"}) {
      add(lib, TarWriter{}
                   .file(std::string{"sky130A/libs.ref/"} + lib + "/mag/x.mag", "magic\n")
                   .file(std::string{"sky130B/libs.ref/"} + lib + "/mag/x.mag", "magic\n")
                   .finish());
    }
    fetcher.documents[std::string{kBase} + "/sky130/" + hash + "/manifest.json"] = "{\"assets\":[" + assets + "]}";
  }

  void corrupt(const std::string& hash, const std::string& content) {
    fetcher.documents[std::string{kBase} + "/sky130/" + hash + "/" + content + ".tar.zst"][600] ^= 0x55;
  }

  hefesto::diag::Status install(const char* variant_name, const std::string& hash, std::vector<std::string> libs) {
    return manager.install(variant(variant_name), hash, libs, options, sink);
  }

  [[nodiscard]] fs::path version_dir(const std::string& hash) const { return root / "ciel/sky130/versions" / hash; }
};


}  // namespace hefesto::test::pdk_harness
