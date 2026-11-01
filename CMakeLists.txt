# SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.20)
project(ef2000 LANGUAGES C CXX ASM)
enable_testing()

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

# The native C++ Unicorn probe is optional for environments that only build
# the Linux guest harness. It is packaged unconditionally by the Nix flake.
find_path(UNICORN_INCLUDE_DIR unicorn/unicorn.h)
find_library(UNICORN_LIBRARY unicorn)
find_package(OpenSSL QUIET COMPONENTS Crypto)
if(UNICORN_INCLUDE_DIR AND UNICORN_LIBRARY AND OpenSSL_FOUND)
  add_executable(ef2000-unicorn-cpp runtime/unicorn_cpp.cpp runtime/dos_drive.cpp
                                  runtime/real_mode_cpu.cpp runtime/sb16_probe.cpp
                                  runtime/sound_blaster.cpp runtime/isa_audio.cpp
                                  runtime/pcm_audio.cpp runtime/wav_capture.cpp)
  target_compile_features(ef2000-unicorn-cpp PRIVATE cxx_std_17)
  target_compile_options(ef2000-unicorn-cpp PRIVATE -Wall -Wextra -Werror)
  target_include_directories(ef2000-unicorn-cpp PRIVATE ${UNICORN_INCLUDE_DIR})
  target_link_libraries(ef2000-unicorn-cpp PRIVATE ${UNICORN_LIBRARY} OpenSSL::Crypto)
  find_package(SDL3 QUIET CONFIG)
  if(SDL3_FOUND)
    target_link_libraries(ef2000-unicorn-cpp PRIVATE SDL3::SDL3)
    target_compile_definitions(ef2000-unicorn-cpp PRIVATE EF_UNICORN_SDL3=1)
  endif()
  add_test(NAME unicorn-cpp-gameport COMMAND ef2000-unicorn-cpp --self-test
           --joystick-event 0:0:0:0 --joystick-axes=0,1 --trace-joystick)
  set_tests_properties(unicorn-cpp-gameport PROPERTIES
                       PASS_REGULAR_EXPRESSION "gameport: axes=2 reads=1 writes=1")
  add_executable(ef2000-real-mode-test tests/real_mode_cpu_test.cpp runtime/real_mode_cpu.cpp
                                     runtime/sb16_probe.cpp runtime/sound_blaster.cpp
                                     runtime/isa_audio.cpp)
  target_compile_features(ef2000-real-mode-test PRIVATE cxx_std_17)
  target_compile_options(ef2000-real-mode-test PRIVATE -Wall -Wextra -Werror)
  target_include_directories(ef2000-real-mode-test PRIVATE ${UNICORN_INCLUDE_DIR})
  target_link_libraries(ef2000-real-mode-test PRIVATE ${UNICORN_LIBRARY})
  add_test(NAME real-mode-cpu COMMAND ef2000-real-mode-test)
  add_executable(ef2000-original-sb16-test tests/original_sound_driver_test.cpp
                                           runtime/real_mode_cpu.cpp runtime/sb16_probe.cpp
                                           runtime/sound_blaster.cpp runtime/isa_audio.cpp)
  target_compile_features(ef2000-original-sb16-test PRIVATE cxx_std_17)
  target_compile_options(ef2000-original-sb16-test PRIVATE -Wall -Wextra -Werror)
  target_include_directories(ef2000-original-sb16-test PRIVATE ${UNICORN_INCLUDE_DIR})
  target_link_libraries(ef2000-original-sb16-test PRIVATE ${UNICORN_LIBRARY} OpenSSL::Crypto)
  add_test(NAME original-sb16 COMMAND ef2000-original-sb16-test)
  set_tests_properties(original-sb16 PROPERTIES SKIP_REGULAR_EXPRESSION "original-sb16: skipped")
else()
  message(STATUS "C++ Unicorn probe disabled (requires Unicorn and OpenSSL headers/libraries)")
endif()

add_executable(ef2000-dos-drive-test tests/dos_drive_test.cpp runtime/dos_drive.cpp)
target_compile_features(ef2000-dos-drive-test PRIVATE cxx_std_17)
target_compile_options(ef2000-dos-drive-test PRIVATE -Wall -Wextra -Werror)
add_test(NAME dos-drive COMMAND ef2000-dos-drive-test)
add_executable(ef2000-sb16-probe-test tests/sb16_probe_test.cpp runtime/sb16_probe.cpp)
target_compile_features(ef2000-sb16-probe-test PRIVATE cxx_std_17)
target_compile_options(ef2000-sb16-probe-test PRIVATE -Wall -Wextra -Werror)
add_test(NAME sb16-probe COMMAND ef2000-sb16-probe-test)
add_executable(ef2000-isa-audio-test tests/isa_audio_test.cpp runtime/isa_audio.cpp)
target_compile_features(ef2000-isa-audio-test PRIVATE cxx_std_17)
target_compile_options(ef2000-isa-audio-test PRIVATE -Wall -Wextra -Werror)
add_test(NAME isa-audio COMMAND ef2000-isa-audio-test)
add_executable(ef2000-sound-blaster-test tests/sound_blaster_test.cpp
                                        runtime/sound_blaster.cpp runtime/isa_audio.cpp
                                        runtime/sb16_probe.cpp)
target_compile_features(ef2000-sound-blaster-test PRIVATE cxx_std_17)
target_compile_options(ef2000-sound-blaster-test PRIVATE -Wall -Wextra -Werror)
add_test(NAME sound-blaster COMMAND ef2000-sound-blaster-test)
add_executable(ef2000-pcm-audio-test tests/pcm_audio_test.cpp runtime/pcm_audio.cpp)
target_compile_features(ef2000-pcm-audio-test PRIVATE cxx_std_17)
target_compile_options(ef2000-pcm-audio-test PRIVATE -Wall -Wextra -Werror)
add_test(NAME pcm-audio COMMAND ef2000-pcm-audio-test)
add_executable(ef2000-wav-capture-test tests/wav_capture_test.cpp runtime/wav_capture.cpp)
target_compile_features(ef2000-wav-capture-test PRIVATE cxx_std_17)
target_compile_options(ef2000-wav-capture-test PRIVATE -Wall -Wextra -Werror)
add_test(NAME wav-capture COMMAND ef2000-wav-capture-test)
