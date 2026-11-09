// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/platform/services.hpp"

#include <utility>

namespace hefesto::platform {

namespace {

std::unique_ptr<FileSystem> make_files(bool in_memory) {
  if (in_memory) {
    return std::make_unique<MemoryFileSystem>();
  }
  return std::make_unique<LocalFileSystem>();
}

std::unique_ptr<TaskExecutor> make_tasks(bool inline_tasks) {
  if (inline_tasks) {
    return std::make_unique<InlineExecutor>();
  }
  return std::make_unique<ThreadPoolExecutor>();
}

}  // namespace

PortableServices::PortableServices(PortableOptions options)
    : files_(make_files(options.in_memory)),
      clock_(std::make_unique<SystemClock>()),
      pdks_(std::make_unique<DirectoryPdkStore>(*files_, std::move(options.pdk_roots))),
      autosave_(std::make_unique<FileAutosaveService>(*files_, options.storage_root / "autosave")),
      logger_(std::make_unique<NullLogger>()),
      tasks_(make_tasks(options.inline_tasks)),
      processes_(std::make_unique<UnsupportedProcessRunner>()) {}

PlatformServices PortableServices::view() noexcept {
  return PlatformServices{
      .files = *files_,
      .pdks = *pdks_,
      .autosave = *autosave_,
      .clock = *clock_,
      .logger = *logger_,
      .tasks = *tasks_,
      .processes = *processes_,
  };
}

void PortableServices::set_logger(std::unique_ptr<Logger> logger) {
  logger_ = logger ? std::move(logger) : std::make_unique<NullLogger>();
}

void PortableServices::set_process_runner(std::unique_ptr<ProcessRunner> runner) {
  processes_ = runner ? std::move(runner) : std::make_unique<UnsupportedProcessRunner>();
}

void PortableServices::set_pdk_store(std::unique_ptr<PdkStore> store) {
  pdks_ = store ? std::move(store) : std::make_unique<DirectoryPdkStore>(*files_, std::vector<Path>{});
}

}  // namespace hefesto::platform
