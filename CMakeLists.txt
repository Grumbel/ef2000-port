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

if(NOT WIN32)
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
endif()

# The native C++ Unicorn probe is optional for environments that only build
# the Linux guest harness. It is packaged unconditionally by the Nix flake.
find_path(UNICORN_INCLUDE_DIR unicorn/unicorn.h)
find_library(UNICORN_LIBRARY unicorn)
find_package(OpenSSL QUIET COMPONENTS Crypto)
if(UNICORN_INCLUDE_DIR AND UNICORN_LIBRARY AND (WIN32 OR OpenSSL_FOUND))
  add_executable(ef2000-unicorn-cpp runtime/unicorn_cpp.cpp runtime/dos_drive.cpp
                                  runtime/cd_audio.cpp
                                  runtime/real_mode_cpu.cpp runtime/sb16_probe.cpp
                                  runtime/sound_blaster.cpp runtime/isa_audio.cpp
                                  runtime/pcm_audio.cpp runtime/wav_capture.cpp)
  target_compile_features(ef2000-unicorn-cpp PRIVATE cxx_std_17)
  target_compile_options(ef2000-unicorn-cpp PRIVATE -Wall -Wextra -Werror)
  target_include_directories(ef2000-unicorn-cpp PRIVATE ${UNICORN_INCLUDE_DIR})
  target_link_libraries(ef2000-unicorn-cpp PRIVATE ${UNICORN_LIBRARY})
  if(WIN32)
    target_link_libraries(ef2000-unicorn-cpp PRIVATE advapi32)
  else()
    target_link_libraries(ef2000-unicorn-cpp PRIVATE OpenSSL::Crypto)
  endif()
  if(WIN32)
    find_package(SDL3 REQUIRED CONFIG)
  else()
    find_package(SDL3 QUIET CONFIG)
  endif()
  if(SDL3_FOUND)
    target_link_libraries(ef2000-unicorn-cpp PRIVATE SDL3::SDL3)
    target_compile_definitions(ef2000-unicorn-cpp PRIVATE EF_UNICORN_SDL3=1)
  endif()
  add_test(NAME unicorn-cpp-gameport COMMAND ef2000-unicorn-cpp --self-test
           --joystick-event 0:0:0:0 --joystick-axes=0,1 --trace-joystick)
  set_tests_properties(unicorn-cpp-gameport PROPERTIES
                       PASS_REGULAR_EXPRESSION "gameport: axes=2 reads=1 writes=1")
  add_test(NAME unicorn-cpp-bda COMMAND ef2000-unicorn-cpp --self-test-bda)
  set_tests_properties(unicorn-cpp-bda PROPERTIES
                       PASS_REGULAR_EXPRESSION "status=guest_exit interrupts=5")
  add_test(NAME unicorn-cpp-clock COMMAND ef2000-unicorn-cpp --self-test-clock)
  set_tests_properties(unicorn-cpp-clock PROPERTIES
                       PASS_REGULAR_EXPRESSION "status=wall_time_limit interrupts=0 ticks=[1-9].*ax=0x12345678")
  add_test(NAME unicorn-cpp-retrace COMMAND ef2000-unicorn-cpp --self-test-retrace)
  set_tests_properties(unicorn-cpp-retrace PROPERTIES
                       PASS_REGULAR_EXPRESSION "status=guest_exit")
  add_test(NAME unicorn-cpp-retrace-uncached COMMAND ef2000-unicorn-cpp
           --self-test-retrace --vga-clock-check 1 --block-clock-check 1)
  set_tests_properties(unicorn-cpp-retrace-uncached PROPERTIES
                       PASS_REGULAR_EXPRESSION "status=guest_exit")
  add_test(NAME unicorn-cpp-profile COMMAND ef2000-unicorn-cpp
           --self-test-retrace --profile)
  set_tests_properties(unicorn-cpp-profile PROPERTIES
                       PASS_REGULAR_EXPRESSION "profile: blocks=[0-9]+.*vga_reads=[1-9]")
  add_test(NAME unicorn-cpp-fast-copy COMMAND ef2000-unicorn-cpp
           --self-test-fast-copy)
  set_tests_properties(unicorn-cpp-fast-copy PROPERTIES
                       PASS_REGULAR_EXPRESSION "fast_copy: calls=1 bytes=1024 fallback_calls=0")
  add_test(NAME unicorn-cpp-hot-blocks COMMAND ef2000-unicorn-cpp
           --self-test-clock --wall-seconds 0.1 --profile-hot-blocks)
  set_tests_properties(unicorn-cpp-hot-blocks PROPERTIES
                       PASS_REGULAR_EXPRESSION "hot_blocks: samples=[1-9]")
  add_test(NAME unicorn-cpp-tcg-cache COMMAND ef2000-unicorn-cpp
           --self-test-clock --tcg-cache-mib 128)
  set_tests_properties(unicorn-cpp-tcg-cache PROPERTIES
                       PASS_REGULAR_EXPRESSION "status=wall_time_limit")
  add_executable(ef2000-real-mode-test tests/real_mode_cpu_test.cpp runtime/real_mode_cpu.cpp
                                     runtime/sb16_probe.cpp runtime/sound_blaster.cpp
                                     runtime/isa_audio.cpp)
  target_compile_features(ef2000-real-mode-test PRIVATE cxx_std_17)
  target_compile_options(ef2000-real-mode-test PRIVATE -Wall -Wextra -Werror)
  target_include_directories(ef2000-real-mode-test PRIVATE ${UNICORN_INCLUDE_DIR})
  target_link_libraries(ef2000-real-mode-test PRIVATE ${UNICORN_LIBRARY})
  add_test(NAME real-mode-cpu COMMAND ef2000-real-mode-test)
  if(NOT WIN32)
    add_executable(ef2000-original-sb16-test tests/original_sound_driver_test.cpp
                                             runtime/real_mode_cpu.cpp runtime/sb16_probe.cpp
                                             runtime/sound_blaster.cpp runtime/isa_audio.cpp)
    target_compile_features(ef2000-original-sb16-test PRIVATE cxx_std_17)
    target_compile_options(ef2000-original-sb16-test PRIVATE -Wall -Wextra -Werror)
    target_include_directories(ef2000-original-sb16-test PRIVATE ${UNICORN_INCLUDE_DIR})
    target_link_libraries(ef2000-original-sb16-test PRIVATE ${UNICORN_LIBRARY} OpenSSL::Crypto)
    add_test(NAME original-sb16 COMMAND ef2000-original-sb16-test)
    set_tests_properties(original-sb16 PROPERTIES SKIP_REGULAR_EXPRESSION "original-sb16: skipped")
  endif()
  add_executable(ef2000-cd-audio-test tests/cd_audio_test.cpp runtime/cd_audio.cpp)
  target_compile_features(ef2000-cd-audio-test PRIVATE cxx_std_17)
  target_compile_options(ef2000-cd-audio-test PRIVATE -Wall -Wextra -Werror)
  target_include_directories(ef2000-cd-audio-test PRIVATE ${UNICORN_INCLUDE_DIR})
  add_test(NAME cd-audio COMMAND ef2000-cd-audio-test)
else()
  message(STATUS "C++ Unicorn probe disabled (requires Unicorn; OpenSSL on non-Windows)")
endif()

add_executable(ef2000-dos-drive-test tests/dos_drive_test.cpp runtime/dos_drive.cpp)
target_compile_features(ef2000-dos-drive-test PRIVATE cxx_std_17)
target_compile_options(ef2000-dos-drive-test PRIVATE -Wall -Wextra -Werror)
add_test(NAME dos-drive COMMAND ef2000-dos-drive-test)
add_executable(ef2000-bios-disk-test tests/bios_disk_test.cpp)
target_compile_features(ef2000-bios-disk-test PRIVATE cxx_std_17)
target_compile_options(ef2000-bios-disk-test PRIVATE -Wall -Wextra -Werror)
add_test(NAME bios-disk-no-media COMMAND ef2000-bios-disk-test)
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

# First typed original-game logic reconstruction; deliberately asset-free and
# independent of the Unicorn and SDL runtimes.
add_executable(ef2000-highscores-test tests/ef2000_highscores_test.cpp
                                    runtime/ef2000_highscores.cpp)
target_compile_features(ef2000-highscores-test PRIVATE cxx_std_17)
target_compile_options(ef2000-highscores-test PRIVATE -Wall -Wextra -Werror)
add_test(NAME highscores-reconstruction COMMAND ef2000-highscores-test)

# Additional asset-free guest layout reconstruction from the original
# mission/navigation readers; no Unicorn dependency or DOS game assets.
add_executable(ef2000-mission-layout-test tests/ef2000_mission_layout_test.cpp
                                          runtime/ef2000_mission_layout.cpp)
target_compile_features(ef2000-mission-layout-test PRIVATE cxx_std_17)
target_compile_options(ef2000-mission-layout-test PRIVATE -Wall -Wextra -Werror)
add_test(NAME mission-layout-reconstruction COMMAND ef2000-mission-layout-test)

# SDL-independent standardized gamepad profile; can run without original assets.
add_executable(ef2000-gamepad-profile-test tests/gamepad_profile_test.cpp)
target_compile_features(ef2000-gamepad-profile-test PRIVATE cxx_std_17)
target_compile_options(ef2000-gamepad-profile-test PRIVATE -Wall -Wextra -Werror)
add_test(NAME gamepad-profile COMMAND ef2000-gamepad-profile-test)
