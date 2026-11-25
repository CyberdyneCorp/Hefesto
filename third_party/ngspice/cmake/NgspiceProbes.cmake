# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# The host probes ngspice's configure makes for the files this build compiles,
# done with CMake's check modules. Sets NGSPICE_HAVE_DEFINES (the #define lines
# for config.h) and the HAVE_ variables config.h.in tests with
# #cmakedefine.

include(CheckIncludeFile)
include(CheckSymbolExists)
include(CheckTypeSize)
include(CheckStructHasMember)
include(CheckCSourceCompiles)

set(_ng_headers
  arpa/inet.h ctype.h dirent.h dlfcn.h fcntl.h float.h getopt.h inttypes.h limits.h
  malloc.h netdb.h netinet/in.h pwd.h stdbool.h stddef.h stdint.h stdio.h stdlib.h
  string.h strings.h sys/file.h sys/ioctl.h sys/param.h sys/select.h sys/socket.h
  sys/stat.h sys/sysctl.h sys/timeb.h sys/time.h sys/types.h sys/wait.h termios.h
  unistd.h)
set(_ng_functions
  access:unistd.h acosh:math.h asinh:math.h atanh:math.h clock_gettime:time.h
  dirname:libgen.h dup2:unistd.h endpwent:pwd.h erfc:math.h fork:unistd.h
  ftime:sys/timeb.h getcwd:unistd.h gethostbyname:netdb.h getopt_long:getopt.h
  getrlimit:sys/resource.h getrusage:sys/resource.h gettimeofday:sys/time.h
  isatty:unistd.h localtime:time.h logb:math.h memset:string.h modf:math.h
  popen:stdio.h qsort:stdlib.h scalb:math.h scalbn:math.h select:sys/select.h
  snprintf:stdio.h socket:sys/socket.h strchr:string.h strdup:string.h
  strerror:string.h strncasecmp:strings.h strrchr:string.h strstr:string.h
  strtol:stdlib.h tcgetattr:termios.h tcsetattr:termios.h time:time.h
  times:sys/times.h tsearch:search.h tdestroy:search.h utimes:sys/time.h
  vfork:unistd.h vprintf:stdio.h)

set(NGSPICE_HAVE_DEFINES "")
foreach(_h IN LISTS _ng_headers)
  string(TOUPPER "HAVE_${_h}" _var)
  string(REGEX REPLACE "[/.]" "_" _var "${_var}")
  check_include_file("${_h}" ${_var})
  if(${_var})
    string(APPEND NGSPICE_HAVE_DEFINES "#define ${_var} 1\n")
  endif()
endforeach()

set(CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE -D_DEFAULT_SOURCE)
if(NOT APPLE)
  set(CMAKE_REQUIRED_LIBRARIES m)
endif()
foreach(_entry IN LISTS _ng_functions)
  string(REPLACE ":" ";" _pair "${_entry}")
  list(GET _pair 0 _fn)
  list(GET _pair 1 _hdr)
  string(TOUPPER "HAVE_${_fn}" _var)
  check_symbol_exists(${_fn} "${_hdr}" ${_var})
  if(${_var})
    string(APPEND NGSPICE_HAVE_DEFINES "#define ${_var} 1\n")
  endif()
endforeach()
if(HAVE_FORK)
  string(APPEND NGSPICE_HAVE_DEFINES "#define HAVE_WORKING_FORK 1\n")
endif()
if(HAVE_VFORK)
  string(APPEND NGSPICE_HAVE_DEFINES "#define HAVE_WORKING_VFORK 1\n")
endif()
check_symbol_exists(sin math.h HAVE_LIBM)
string(APPEND NGSPICE_HAVE_DEFINES "#define HAVE_LIBM 1\n#define HAVE_LIBPTHREAD 1\n")

check_symbol_exists(isinf math.h HAVE_DECL_ISINF)
check_symbol_exists(isnan math.h HAVE_DECL_ISNAN)
check_symbol_exists(sigsetjmp setjmp.h HAVE_SIGSETJMP)
# The signal-handler types are probed without _GNU_SOURCE, as configure does: some files
# include <signal.h> before ngspice.h defines it, so glibc's sighandler_t is not visible there.
unset(CMAKE_REQUIRED_DEFINITIONS)
check_c_source_compiles("#include <signal.h>\nint main(void){sig_t s = 0; (void)s; return 0;}" HAVE_SIG_T)
check_c_source_compiles("#include <signal.h>\nint main(void){sighandler_t s = 0; (void)s; return 0;}"
                        HAVE_SIGHANDLER_T)
check_c_source_compiles("#include <signal.h>\nint main(void){__sighandler_t s = 0; (void)s; return 0;}"
                        HAVE___SIGHANDLER_T)
check_struct_has_member("struct tm" tm_zone time.h HAVE_STRUCT_TM_TM_ZONE)
set(HAVE_TM_ZONE ${HAVE_STRUCT_TM_TM_ZONE})
check_c_source_compiles("int main(void){_Bool b = 0; return (int)b;}" HAVE__BOOL)
unset(CMAKE_REQUIRED_DEFINITIONS)
unset(CMAKE_REQUIRED_LIBRARIES)
if(EXISTS /proc/meminfo)
  set(HAVE__PROC_MEMINFO 1)
endif()
