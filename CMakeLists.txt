# SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.20)
project(ef2000 LANGUAGES C ASM)

# Development-only harness objects consumed by run_startup.py --runtime-objects.
# The proprietary game payload is linked by the runner, never by this project.
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(opt_flags -O0 -g)
else()
  set(opt_flags -O2 -g)
endif()

set(guest_flags -std=c11 -Wall -Wextra -Werror -ffreestanding -fno-pie
  -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables -fno-builtin)

foreach(backend native64 qemu)
  if(backend STREQUAL "native64")
    set(width 64)
    set(defines -D_GNU_SOURCE -DEF_NATIVE64)
    set(entry native64.S)
  else()
    set(width 32)
    set(defines)
    set(entry entry.S)
  endif()
  set(out "${CMAKE_BINARY_DIR}/objects/${backend}")
  set(objects)
  foreach(name startup x32)
    add_custom_command(
      OUTPUT "${out}/${name}.o"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${out}"
      COMMAND "${CMAKE_C_COMPILER}" -m${width} ${guest_flags} ${opt_flags} ${defines}
              -MMD -MF "${out}/${name}.d"
              -c "${CMAKE_SOURCE_DIR}/runtime/${name}.c" -o "${out}/${name}.o"
      DEPENDS "${CMAKE_SOURCE_DIR}/runtime/${name}.c"
      DEPFILE "${out}/${name}.d"
      VERBATIM)
    list(APPEND objects "${out}/${name}.o")
  endforeach()
  foreach(kind entry entry-limit)
    set(limit_flag)
    if(kind STREQUAL "entry-limit")
      set(limit_flag --defsym CHECK_SEGMENT_LIMIT=1)
    endif()
    add_custom_command(
      OUTPUT "${out}/${kind}.o"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${out}"
      COMMAND as --${width} -I "${CMAKE_SOURCE_DIR}/runtime" ${limit_flag}
              "${CMAKE_SOURCE_DIR}/runtime/${entry}" -o "${out}/${kind}.o"
      DEPENDS "${CMAKE_SOURCE_DIR}/runtime/${entry}"
              "${CMAKE_SOURCE_DIR}/runtime/guest_boot.inc"
      VERBATIM)
    list(APPEND objects "${out}/${kind}.o")
  endforeach()
  add_custom_target("harness-${backend}" ALL DEPENDS ${objects})
endforeach()
