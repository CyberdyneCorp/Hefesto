// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Linux confinement (design D9): no-new-privileges, a seccomp filter that
// denies network sockets and process creation, and a Landlock ruleset that
// allows writes only under the job directory where the kernel supports
// Landlock. Without Landlock the seccomp filter is applied alone and the job
// log says so (simulation-engine-embedding "Worker confinement").
//
// Owner: SIMD.

#include <fcntl.h>
#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <sched.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#if __has_include(<linux/landlock.h>) && defined(SYS_landlock_create_ruleset)
#include <linux/landlock.h>
#define HEFESTO_HAVE_LANDLOCK 1
#else
#define HEFESTO_HAVE_LANDLOCK 0
#endif

#include "sandbox/sandbox.hpp"

namespace hefesto::simd {

namespace {

#if defined(__x86_64__)
constexpr std::uint32_t kArch = AUDIT_ARCH_X86_64;
#elif defined(__aarch64__)
constexpr std::uint32_t kArch = AUDIT_ARCH_AARCH64;
#else
#error "hefesto-simd confinement: unsupported Linux architecture"
#endif

constexpr std::uint32_t kAllow = SECCOMP_RET_ALLOW;
constexpr std::uint32_t kKill = SECCOMP_RET_KILL_PROCESS;
constexpr std::uint32_t errno_ret(int error) { return SECCOMP_RET_ERRNO | (static_cast<std::uint32_t>(error) & 0xFFFFU); }

sock_filter stmt(std::uint16_t code, std::uint32_t k) { return sock_filter{code, 0, 0, k}; }
sock_filter jump(std::uint16_t code, std::uint32_t k, std::uint8_t jt, std::uint8_t jf) {
  return sock_filter{code, jt, jf, k};
}

constexpr std::uint32_t kNrOffset = offsetof(seccomp_data, nr);
constexpr std::uint32_t kArchOffset = offsetof(seccomp_data, arch);
constexpr std::uint32_t kArg0Offset = offsetof(seccomp_data, args);  // low 32 bits on little-endian hosts

// Returns `ret` when the syscall number is `nr` (the accumulator holds it).
void deny(std::vector<sock_filter>& program, long nr, std::uint32_t ret) {
  program.push_back(jump(BPF_JMP | BPF_JEQ | BPF_K, static_cast<std::uint32_t>(nr), 0, 1));
  program.push_back(stmt(BPF_RET | BPF_K, ret));
}

// When the syscall is `nr`: allow it if (arg0 test) holds, else return `ret`;
// reloads the syscall number for the following checks.
void deny_unless_arg0(std::vector<sock_filter>& program, long nr, std::uint16_t test, std::uint32_t value,
                      std::uint32_t ret) {
  program.push_back(jump(BPF_JMP | BPF_JEQ | BPF_K, static_cast<std::uint32_t>(nr), 0, 4));
  program.push_back(stmt(BPF_LD | BPF_W | BPF_ABS, kArg0Offset));
  program.push_back(jump(static_cast<std::uint16_t>(BPF_JMP | test | BPF_K), value, 0, 1));
  program.push_back(stmt(BPF_RET | BPF_K, kAllow));
  program.push_back(stmt(BPF_RET | BPF_K, ret));
}

std::vector<sock_filter> seccomp_program() {
  std::vector<sock_filter> p;
  p.push_back(stmt(BPF_LD | BPF_W | BPF_ABS, kArchOffset));
  p.push_back(jump(BPF_JMP | BPF_JEQ | BPF_K, kArch, 1, 0));
  p.push_back(stmt(BPF_RET | BPF_K, kKill));
  p.push_back(stmt(BPF_LD | BPF_W | BPF_ABS, kNrOffset));
#if defined(__x86_64__)
  p.push_back(jump(BPF_JMP | BPF_JGE | BPF_K, 0x40000000U, 0, 1));  // the x32 ABI
  p.push_back(stmt(BPF_RET | BPF_K, kKill));
  deny(p, SYS_fork, errno_ret(EPERM));
  deny(p, SYS_vfork, errno_ret(EPERM));
#endif
  deny(p, SYS_execve, errno_ret(EPERM));
  deny(p, SYS_execveat, errno_ret(EPERM));
#if defined(SYS_clone3)
  deny(p, SYS_clone3, errno_ret(ENOSYS));  // the C library falls back to clone, checked below
#endif
  deny_unless_arg0(p, SYS_clone, BPF_JSET, CLONE_THREAD, errno_ret(EPERM));
  p.push_back(stmt(BPF_LD | BPF_W | BPF_ABS, kNrOffset));
  deny_unless_arg0(p, SYS_socket, BPF_JEQ, AF_UNIX, errno_ret(EACCES));
  p.push_back(stmt(BPF_RET | BPF_K, kAllow));
  return p;
}

bool apply_seccomp(std::string& error) {
  std::vector<sock_filter> program = seccomp_program();
  sock_fprog fprog{static_cast<unsigned short>(program.size()), program.data()};
  if (::prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &fprog, 0, 0) != 0) {
    error = std::string{"seccomp filter not applied: "} + std::strerror(errno);
    return false;
  }
  return true;
}

#if HEFESTO_HAVE_LANDLOCK

constexpr std::uint64_t kFileWrite = LANDLOCK_ACCESS_FS_WRITE_FILE;
constexpr std::uint64_t kTreeWrite = LANDLOCK_ACCESS_FS_WRITE_FILE | LANDLOCK_ACCESS_FS_REMOVE_DIR |
                                     LANDLOCK_ACCESS_FS_REMOVE_FILE | LANDLOCK_ACCESS_FS_MAKE_CHAR |
                                     LANDLOCK_ACCESS_FS_MAKE_DIR | LANDLOCK_ACCESS_FS_MAKE_REG |
                                     LANDLOCK_ACCESS_FS_MAKE_SOCK | LANDLOCK_ACCESS_FS_MAKE_FIFO |
                                     LANDLOCK_ACCESS_FS_MAKE_BLOCK | LANDLOCK_ACCESS_FS_MAKE_SYM;

class Fd {
 public:
  explicit Fd(int fd) : fd_(fd) {}
  Fd(const Fd&) = delete;
  Fd& operator=(const Fd&) = delete;
  ~Fd() {
    if (fd_ >= 0) {
      ::close(fd_);
    }
  }
  [[nodiscard]] int get() const noexcept { return fd_; }

 private:
  int fd_;
};

bool allow_writes_beneath(int ruleset, const std::string& path, std::uint64_t access) {
  const Fd dir{::open(path.c_str(), O_PATH | O_CLOEXEC)};
  if (dir.get() < 0) {
    return false;
  }
  landlock_path_beneath_attr rule{};
  rule.allowed_access = access;
  rule.parent_fd = dir.get();
  return ::syscall(SYS_landlock_add_rule, ruleset, LANDLOCK_RULE_PATH_BENEATH, &rule, 0) == 0;
}

// Applies the write rules; false (with a job-log note) when Landlock is
// unavailable, true when applied. `error` is set when the kernel has
// Landlock and applying it failed.
bool apply_landlock(const Confinement& confinement, std::string& note, std::string& error) {
  const long abi = ::syscall(SYS_landlock_create_ruleset, nullptr, 0, LANDLOCK_CREATE_RULESET_VERSION);
  if (abi < 1) {
    note = std::string{"confinement: Landlock unavailable on this kernel ("} + std::strerror(errno) +
           "); seccomp filter only, file writes are not confined";
    return false;
  }
  landlock_ruleset_attr attr{};
  attr.handled_access_fs = kTreeWrite;
  const Fd ruleset{static_cast<int>(::syscall(SYS_landlock_create_ruleset, &attr, sizeof attr, 0))};
  const bool rules = ruleset.get() >= 0 && allow_writes_beneath(ruleset.get(), "/dev/null", kFileWrite) &&
                     (confinement.job_dir.empty() || allow_writes_beneath(ruleset.get(), confinement.job_dir, kTreeWrite));
  if (!rules || ::syscall(SYS_landlock_restrict_self, ruleset.get(), 0) != 0) {
    error = std::string{"Landlock ruleset not applied: "} + std::strerror(errno);
    return false;
  }
  note = "confinement: Landlock ruleset applied (writes only in the job directory)";
  return true;
}

#else

bool apply_landlock(const Confinement& /*confinement*/, std::string& note, std::string& /*error*/) {
  note = "confinement: Landlock unavailable (built without <linux/landlock.h>); seccomp filter only, file writes "
         "are not confined";
  return false;
}

#endif

}  // namespace

ConfinementResult confine(const Confinement& confinement) {
  ConfinementResult result;
  if (::prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
    result.error = std::string{"PR_SET_NO_NEW_PRIVS failed: "} + std::strerror(errno);
    return result;
  }
  std::string note;
  apply_landlock(confinement, note, result.error);
  if (!result.error.empty()) {
    return result;
  }
  result.notes.push_back(note);
  if (!apply_seccomp(result.error)) {
    return result;
  }
  result.notes.emplace_back("confinement: seccomp filter applied (no network sockets, no process creation)");
  result.ok = true;
  return result;
}

}  // namespace hefesto::simd
