// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// PlatformServices: the bundle of host facilities the core may use
// (capability hefesto-architecture, requirement "Platform services
// abstraction"). Shells fill it with their own implementations; the core
// ships portable defaults that need only the C++ standard library.

#pragma once

#include <memory>
#include <vector>

#include "hefesto/platform/clock.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/platform/logger.hpp"
#include "hefesto/platform/process_runner.hpp"
#include "hefesto/platform/storage.hpp"
#include "hefesto/platform/task_executor.hpp"

namespace hefesto::platform {

/// Non-owning view of one implementation of each service.
struct PlatformServices {
  FileSystem& files;
  PdkStore& pdks;
  AutosaveService& autosave;
  Clock& clock;
  Logger& logger;
  TaskExecutor& tasks;
  ProcessRunner& processes;
};

struct PortableOptions {
  Path storage_root;               // app storage: autosave lives under <root>/autosave
  std::vector<Path> pdk_roots;     // technology search roots
  bool in_memory = false;          // MemoryFileSystem instead of the local disk
  bool inline_tasks = false;       // InlineExecutor instead of a thread pool
};

/// Owns the portable default services. The logger starts as a NullLogger;
/// a host replaces it with set_logger() (e.g. a RotatingFileLogger).
/// The process runner starts as UnsupportedProcessRunner; hosts that can
/// spawn processes install their own with set_process_runner().
/// view() holds references: configure the services before taking a view.
class PortableServices {
 public:
  explicit PortableServices(PortableOptions options);

  [[nodiscard]] PlatformServices view() noexcept;

  void set_logger(std::unique_ptr<Logger> logger);
  void set_process_runner(std::unique_ptr<ProcessRunner> runner);
  /// Replaces the PDK store (front ends install pdk::make_managed_pdk_store,
  /// hefesto-files-and-pdks). nullptr restores the directory store with no
  /// roots. Like the other setters, call it before taking view(): a view
  /// holds references to the services current at that time.
  void set_pdk_store(std::unique_ptr<PdkStore> store);

  [[nodiscard]] FileSystem& files() noexcept { return *files_; }
  [[nodiscard]] Clock& clock() noexcept { return *clock_; }

 private:
  std::unique_ptr<FileSystem> files_;
  std::unique_ptr<Clock> clock_;
  std::unique_ptr<PdkStore> pdks_;
  std::unique_ptr<AutosaveService> autosave_;
  std::unique_ptr<Logger> logger_;
  std::unique_ptr<TaskExecutor> tasks_;
  std::unique_ptr<ProcessRunner> processes_;
};

}  // namespace hefesto::platform
