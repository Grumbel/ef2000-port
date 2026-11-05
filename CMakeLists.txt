# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

cmake_minimum_required(VERSION 3.20)
project(ef2000 LANGUAGES C CXX ASM)
enable_testing()

add_executable(ef2000-efpatch-test tests/efpatch_test.cpp)
target_compile_features(ef2000-efpatch-test PRIVATE cxx_std_17)
target_include_directories(ef2000-efpatch-test PRIVATE runtime)
add_test(NAME efpatch-format COMMAND ef2000-efpatch-test)
add_executable(ef2000-call-capture-test tests/call_capture_test.cpp)
target_compile_features(ef2000-call-capture-test PRIVATE cxx_std_17)
target_include_directories(ef2000-call-capture-test PRIVATE runtime)
add_test(NAME call-capture-format COMMAND ef2000-call-capture-test
  "${CMAKE_CURRENT_BINARY_DIR}/call-capture-fixture.efcap")
add_executable(ef2000-decomp-hiscore-test
  tests/decomp_hiscore_test.c decomp/src/hiscore.c)
target_compile_features(ef2000-decomp-hiscore-test PRIVATE c_std_11)
target_include_directories(ef2000-decomp-hiscore-test PRIVATE decomp/include)
add_test(NAME decomp-hiscore-host-model COMMAND ef2000-decomp-hiscore-test)
add_executable(ef2000-decomp-spatial-test
  tests/decomp_spatial_test.c decomp/src/spatial.c)
target_compile_features(ef2000-decomp-spatial-test PRIVATE c_std_11)
target_include_directories(ef2000-decomp-spatial-test PRIVATE decomp/include)
add_test(NAME decomp-spatial-host-model COMMAND ef2000-decomp-spatial-test)
add_executable(ef2000-decomp-indexed-objects-test
  tests/decomp_indexed_objects_test.c decomp/src/indexed_objects.c)
target_compile_features(ef2000-decomp-indexed-objects-test PRIVATE c_std_11)
target_include_directories(ef2000-decomp-indexed-objects-test PRIVATE decomp/include)
add_test(NAME decomp-indexed-objects-host-model COMMAND ef2000-decomp-indexed-objects-test)
add_executable(ef2000-decomp-object-state-test
  tests/decomp_object_state_test.c decomp/src/object_lookup.c
  decomp/src/object_state.c)
target_compile_features(ef2000-decomp-object-state-test PRIVATE c_std_11)
target_compile_definitions(ef2000-decomp-object-state-test PRIVATE EF_DECOMP_HOST_TEST=1)
target_include_directories(ef2000-decomp-object-state-test PRIVATE decomp/include)
add_test(NAME decomp-object-state-host-model COMMAND ef2000-decomp-object-state-test)
add_executable(ef2000-decomp-object-control-test
  tests/decomp_object_control_test.c decomp/src/object_lookup.c
  decomp/src/object_state.c decomp/src/world_grid.c
  decomp/src/object_control.c)
target_compile_features(ef2000-decomp-object-control-test PRIVATE c_std_11)
target_compile_options(ef2000-decomp-object-control-test PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(ef2000-decomp-object-control-test PRIVATE EF_DECOMP_HOST_TEST=1)
target_include_directories(ef2000-decomp-object-control-test PRIVATE decomp/include)
add_test(NAME decomp-object-control-host-model COMMAND ef2000-decomp-object-control-test)
add_executable(ef2000-decomp-world-grid-batch-test
  tests/decomp_world_grid_batch_test.c decomp/src/object_lookup.c
  decomp/src/world_grid_queries.c decomp/src/world_grid_control.c)
target_compile_features(ef2000-decomp-world-grid-batch-test PRIVATE c_std_11)
target_compile_options(ef2000-decomp-world-grid-batch-test PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(ef2000-decomp-world-grid-batch-test PRIVATE EF_DECOMP_HOST_TEST=1)
target_include_directories(ef2000-decomp-world-grid-batch-test PRIVATE decomp/include)
add_test(NAME decomp-world-grid-batch-host-model COMMAND ef2000-decomp-world-grid-batch-test)
add_executable(ef2000-decomp-grid-helpers-test
  tests/decomp_grid_helpers_test.c decomp/src/object_lookup.c
  decomp/src/world_grid_links.c decomp/src/world_grid_limits.c)
target_compile_features(ef2000-decomp-grid-helpers-test PRIVATE c_std_11)
target_compile_options(ef2000-decomp-grid-helpers-test PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(ef2000-decomp-grid-helpers-test PRIVATE EF_DECOMP_HOST_TEST=1)
target_include_directories(ef2000-decomp-grid-helpers-test PRIVATE decomp/include)
add_test(NAME decomp-grid-helpers-host-model COMMAND ef2000-decomp-grid-helpers-test)
add_executable(ef2000-decomp-world-grid-relations-test
  tests/decomp_world_grid_relations_test.c
  decomp/src/grid_node_pool.c decomp/src/world_grid_neighbors.c
  decomp/src/world_grid_category.c)
target_compile_features(ef2000-decomp-world-grid-relations-test PRIVATE c_std_11)
target_compile_options(ef2000-decomp-world-grid-relations-test PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(ef2000-decomp-world-grid-relations-test PRIVATE EF_DECOMP_HOST_TEST=1)
target_include_directories(ef2000-decomp-world-grid-relations-test PRIVATE decomp/include)
target_link_libraries(ef2000-decomp-world-grid-relations-test PRIVATE m)
add_test(NAME decomp-world-grid-relations-host-model COMMAND ef2000-decomp-world-grid-relations-test)
add_executable(ef2000-decomp-object-geometry-test
  tests/decomp_object_geometry_test.c decomp/src/object_geometry.c
  decomp/src/grid_point_distances.c)
target_compile_features(ef2000-decomp-object-geometry-test PRIVATE c_std_11)
target_compile_options(ef2000-decomp-object-geometry-test PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(ef2000-decomp-object-geometry-test PRIVATE EF_DECOMP_HOST_TEST=1)
target_include_directories(ef2000-decomp-object-geometry-test PRIVATE decomp/include)
target_link_libraries(ef2000-decomp-object-geometry-test PRIVATE m)
add_test(NAME decomp-object-geometry-host-model COMMAND ef2000-decomp-object-geometry-test)
add_executable(ef2000-decomp-scene-nodes-test
  tests/decomp_scene_nodes_test.c decomp/src/scene_nodes.c)
target_compile_features(ef2000-decomp-scene-nodes-test PRIVATE c_std_11)
target_compile_options(ef2000-decomp-scene-nodes-test PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(ef2000-decomp-scene-nodes-test PRIVATE EF_DECOMP_HOST_TEST=1)
target_include_directories(ef2000-decomp-scene-nodes-test PRIVATE decomp/include)
add_test(NAME decomp-scene-nodes-host-model COMMAND ef2000-decomp-scene-nodes-test)
add_executable(ef2000-decomp-scene-state-test
  tests/decomp_scene_state_test.c decomp/src/scene_state.c)
target_compile_features(ef2000-decomp-scene-state-test PRIVATE c_std_11)
target_compile_options(ef2000-decomp-scene-state-test PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(ef2000-decomp-scene-state-test PRIVATE EF_DECOMP_HOST_TEST=1)
target_include_directories(ef2000-decomp-scene-state-test PRIVATE decomp/include)
add_test(NAME decomp-scene-state-host-model COMMAND ef2000-decomp-scene-state-test)
add_executable(ef2000-decomp-scene-indexed-test
  tests/decomp_scene_indexed_test.c decomp/src/scene_indexed.c)
target_compile_features(ef2000-decomp-scene-indexed-test PRIVATE c_std_11)
target_compile_options(ef2000-decomp-scene-indexed-test PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(ef2000-decomp-scene-indexed-test PRIVATE EF_DECOMP_HOST_TEST=1)
target_include_directories(ef2000-decomp-scene-indexed-test PRIVATE decomp/include)
add_test(NAME decomp-scene-indexed-host-model COMMAND ef2000-decomp-scene-indexed-test)
add_executable(ef2000-decomp-buffer-plane-test
  tests/decomp_buffer_plane_test.c decomp/src/buffer_cursor.c
  decomp/src/plane_sources.c decomp/src/object_table_queries.c
  decomp/src/scene_ui_flag.c decomp/src/indexed_objects.c)
target_compile_features(ef2000-decomp-buffer-plane-test PRIVATE c_std_11)
target_compile_options(ef2000-decomp-buffer-plane-test PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(ef2000-decomp-buffer-plane-test PRIVATE EF_DECOMP_HOST_TEST=1)
target_include_directories(ef2000-decomp-buffer-plane-test PRIVATE decomp/include)
add_test(NAME decomp-buffer-plane-host-model COMMAND ef2000-decomp-buffer-plane-test)
add_executable(ef2000-decomp-scene-controls-vectors-test
  tests/decomp_scene_controls_vectors_test.c
  decomp/src/scene_controls.c decomp/src/vector_math.c)
target_compile_features(ef2000-decomp-scene-controls-vectors-test PRIVATE c_std_11)
target_compile_options(ef2000-decomp-scene-controls-vectors-test PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(ef2000-decomp-scene-controls-vectors-test PRIVATE EF_DECOMP_HOST_TEST=1)
target_include_directories(ef2000-decomp-scene-controls-vectors-test PRIVATE decomp/include)
add_test(NAME decomp-scene-controls-vectors-host-model COMMAND ef2000-decomp-scene-controls-vectors-test)
add_executable(ef2000-decomp-runtime-link-controls-test
  tests/decomp_runtime_link_controls_test.c
  decomp/src/link_resolution.c decomp/src/runtime_control.c)
target_compile_features(ef2000-decomp-runtime-link-controls-test PRIVATE c_std_11)
target_compile_options(ef2000-decomp-runtime-link-controls-test PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(ef2000-decomp-runtime-link-controls-test PRIVATE EF_DECOMP_HOST_TEST=1)
target_include_directories(ef2000-decomp-runtime-link-controls-test PRIVATE decomp/include)
add_test(NAME decomp-runtime-link-controls-host-model COMMAND ef2000-decomp-runtime-link-controls-test)
add_executable(ef2000-decomp-command-packets-test
  tests/decomp_command_packets_test.c decomp/src/command_packets.c)
target_compile_features(ef2000-decomp-command-packets-test PRIVATE c_std_11)
target_compile_options(ef2000-decomp-command-packets-test PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(ef2000-decomp-command-packets-test PRIVATE EF_DECOMP_HOST_TEST=1)
target_include_directories(ef2000-decomp-command-packets-test PRIVATE decomp/include)
add_test(NAME decomp-command-packets-host-model COMMAND ef2000-decomp-command-packets-test)
add_executable(ef2000-decomp-runtime-predicates-test
  tests/decomp_runtime_predicates_test.c decomp/src/runtime_predicates.c)
target_compile_features(ef2000-decomp-runtime-predicates-test PRIVATE c_std_11)
target_compile_options(ef2000-decomp-runtime-predicates-test PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(ef2000-decomp-runtime-predicates-test PRIVATE EF_DECOMP_HOST_TEST=1)
target_include_directories(ef2000-decomp-runtime-predicates-test PRIVATE decomp/include)
add_test(NAME decomp-runtime-predicates-host-model COMMAND ef2000-decomp-runtime-predicates-test)
add_executable(ef2000-decomp-ui-grid-helpers-test
  tests/decomp_ui_grid_helpers_test.c decomp/src/ui_grid_helpers.c)
target_compile_features(ef2000-decomp-ui-grid-helpers-test PRIVATE c_std_11)
target_compile_options(ef2000-decomp-ui-grid-helpers-test PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(ef2000-decomp-ui-grid-helpers-test PRIVATE EF_DECOMP_HOST_TEST=1)
target_include_directories(ef2000-decomp-ui-grid-helpers-test PRIVATE decomp/include)
add_test(NAME decomp-ui-grid-helpers-host-model COMMAND ef2000-decomp-ui-grid-helpers-test)
add_executable(ef2000-decomp-tactical-io-test
  tests/decomp_tactical_io_test.c decomp/src/tactical_io.c)
target_compile_features(ef2000-decomp-tactical-io-test PRIVATE c_std_11)
target_compile_options(ef2000-decomp-tactical-io-test PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(ef2000-decomp-tactical-io-test PRIVATE EF_DECOMP_HOST_TEST=1)
target_include_directories(ef2000-decomp-tactical-io-test PRIVATE decomp/include)
add_test(NAME decomp-tactical-io-host-model COMMAND ef2000-decomp-tactical-io-test)
add_executable(ef2000-decomp-record-callers-test
  tests/decomp_record_callers_test.c decomp/src/record_callers.c)
target_compile_features(ef2000-decomp-record-callers-test PRIVATE c_std_11)
target_compile_options(ef2000-decomp-record-callers-test PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(ef2000-decomp-record-callers-test PRIVATE EF_DECOMP_HOST_TEST=1)
target_include_directories(ef2000-decomp-record-callers-test PRIVATE decomp/include)
add_test(NAME decomp-record-callers-host-model COMMAND ef2000-decomp-record-callers-test)
add_executable(ef2000-decomp-ui-dispatch-test
  tests/decomp_ui_dispatch_test.c decomp/src/ui_dispatch.c)
target_compile_features(ef2000-decomp-ui-dispatch-test PRIVATE c_std_11)
target_compile_options(ef2000-decomp-ui-dispatch-test PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(ef2000-decomp-ui-dispatch-test PRIVATE EF_DECOMP_HOST_TEST=1)
target_include_directories(ef2000-decomp-ui-dispatch-test PRIVATE decomp/include)
add_test(NAME decomp-ui-dispatch-host-model COMMAND ef2000-decomp-ui-dispatch-test)
add_executable(ef2000-decomp-packet-status-test
  tests/decomp_packet_status_test.c decomp/src/packet_status.c)
target_compile_features(ef2000-decomp-packet-status-test PRIVATE c_std_11)
target_compile_options(ef2000-decomp-packet-status-test PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(ef2000-decomp-packet-status-test PRIVATE EF_DECOMP_HOST_TEST=1)
target_include_directories(ef2000-decomp-packet-status-test PRIVATE decomp/include)
add_test(NAME decomp-packet-status-host-model COMMAND ef2000-decomp-packet-status-test)
add_executable(ef2000-decomp-record-transitions-test
  tests/decomp_record_transitions_test.c decomp/src/record_transitions.c)
target_compile_features(ef2000-decomp-record-transitions-test PRIVATE c_std_11)
target_compile_options(ef2000-decomp-record-transitions-test PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(ef2000-decomp-record-transitions-test PRIVATE EF_DECOMP_HOST_TEST=1)
target_include_directories(ef2000-decomp-record-transitions-test PRIVATE decomp/include)
add_test(NAME decomp-record-transitions-host-model COMMAND ef2000-decomp-record-transitions-test)
add_executable(ef2000-decomp-ui-operations-test
  tests/decomp_ui_operations_test.c decomp/src/ui_operations.c)
target_compile_features(ef2000-decomp-ui-operations-test PRIVATE c_std_11)
target_compile_options(ef2000-decomp-ui-operations-test PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(ef2000-decomp-ui-operations-test PRIVATE EF_DECOMP_HOST_TEST=1)
target_include_directories(ef2000-decomp-ui-operations-test PRIVATE decomp/include)
add_test(NAME decomp-ui-operations-host-model COMMAND ef2000-decomp-ui-operations-test)
add_executable(ef2000-decomp-ui-secondary-test
  tests/decomp_ui_secondary_test.c decomp/src/ui_secondary.c)
target_compile_features(ef2000-decomp-ui-secondary-test PRIVATE c_std_11)
target_compile_options(ef2000-decomp-ui-secondary-test PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(ef2000-decomp-ui-secondary-test PRIVATE EF_DECOMP_HOST_TEST=1)
target_include_directories(ef2000-decomp-ui-secondary-test PRIVATE decomp/include)
add_test(NAME decomp-ui-secondary-host-model COMMAND ef2000-decomp-ui-secondary-test)
add_executable(ef2000-decomp-parser-records-test
  tests/decomp_parser_records_test.c decomp/src/parser_records.c)
target_compile_features(ef2000-decomp-parser-records-test PRIVATE c_std_11)
target_compile_options(ef2000-decomp-parser-records-test PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(ef2000-decomp-parser-records-test PRIVATE EF_DECOMP_HOST_TEST=1)
target_include_directories(ef2000-decomp-parser-records-test PRIVATE decomp/include)
add_test(NAME decomp-parser-records-host-model COMMAND ef2000-decomp-parser-records-test)
add_executable(ef2000-decomp-parser-enums-test
  tests/decomp_parser_enums_test.c decomp/src/parser_enums.c)
target_compile_features(ef2000-decomp-parser-enums-test PRIVATE c_std_11)
target_compile_options(ef2000-decomp-parser-enums-test PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(ef2000-decomp-parser-enums-test PRIVATE EF_DECOMP_HOST_TEST=1)
target_include_directories(ef2000-decomp-parser-enums-test PRIVATE decomp/include)
add_test(NAME decomp-parser-enums-host-model COMMAND ef2000-decomp-parser-enums-test)
add_executable(ef2000-decomp-parser-line-records-test
  tests/decomp_parser_line_records_test.c decomp/src/parser_line_records.c)
target_compile_features(ef2000-decomp-parser-line-records-test PRIVATE c_std_11)
target_compile_options(ef2000-decomp-parser-line-records-test PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(ef2000-decomp-parser-line-records-test PRIVATE EF_DECOMP_HOST_TEST=1)
target_include_directories(ef2000-decomp-parser-line-records-test PRIVATE decomp/include)
add_test(NAME decomp-parser-line-records-host-model COMMAND ef2000-decomp-parser-line-records-test)
add_executable(ef2000-decomp-runtime-small-ops-test
  tests/decomp_runtime_small_ops_test.c decomp/src/runtime_small_ops.c)
target_compile_features(ef2000-decomp-runtime-small-ops-test PRIVATE c_std_11)
target_compile_options(ef2000-decomp-runtime-small-ops-test PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(ef2000-decomp-runtime-small-ops-test PRIVATE EF_DECOMP_HOST_TEST=1)
target_include_directories(ef2000-decomp-runtime-small-ops-test PRIVATE decomp/include)
add_test(NAME decomp-runtime-small-ops-host-model COMMAND ef2000-decomp-runtime-small-ops-test)
add_executable(ef2000-decomp-runtime-indexed-test
  tests/decomp_runtime_indexed_test.c decomp/src/runtime_indexed.c)
target_compile_features(ef2000-decomp-runtime-indexed-test PRIVATE c_std_11)
target_compile_options(ef2000-decomp-runtime-indexed-test PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(ef2000-decomp-runtime-indexed-test PRIVATE EF_DECOMP_HOST_TEST=1)
target_include_directories(ef2000-decomp-runtime-indexed-test PRIVATE decomp/include)
add_test(NAME decomp-runtime-indexed-host-model COMMAND ef2000-decomp-runtime-indexed-test)
add_executable(ef2000-decomp-event-wrappers-test
  tests/decomp_event_wrappers_test.c decomp/src/event_wrappers.c)
target_compile_features(ef2000-decomp-event-wrappers-test PRIVATE c_std_11)
target_compile_options(ef2000-decomp-event-wrappers-test PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(ef2000-decomp-event-wrappers-test PRIVATE EF_DECOMP_HOST_TEST=1)
target_include_directories(ef2000-decomp-event-wrappers-test PRIVATE decomp/include)
add_test(NAME decomp-event-wrappers-host-model COMMAND ef2000-decomp-event-wrappers-test)

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
  add_test(NAME unicorn-cpp-patch-hits COMMAND ef2000-unicorn-cpp --self-test-patch-hits)
  set_tests_properties(unicorn-cpp-patch-hits PROPERTIES
                       PASS_REGULAR_EXPRESSION "patch_hit: addr=0x200 count=2")
  add_test(NAME unicorn-cpp-guest-hash COMMAND ef2000-unicorn-cpp
           --self-test-patch-hits --hash-guest 0x300:4)
  set_tests_properties(unicorn-cpp-guest-hash PROPERTIES
                       PASS_REGULAR_EXPRESSION "guest_hash: addr=0x300 length=4 sha256=df3f619804a92fdb4057192dc43dd748ea778adc52bc498ce80524c014b81119")
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
  add_test(NAME unicorn-cpp-archive-lookup COMMAND ef2000-unicorn-cpp
           --self-test-archive-lookup)
  set_tests_properties(unicorn-cpp-archive-lookup PROPERTIES
                       PASS_REGULAR_EXPRESSION "archive-lookup: archive=unknown path=\"test.bbm\"")
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

# Host overlay move/resize/focus behavior, independent of SDL and DOS assets.
add_executable(ef2000-overlay-windows-test tests/overlay_windows_test.cpp)
target_compile_features(ef2000-overlay-windows-test PRIVATE cxx_std_17)
target_compile_options(ef2000-overlay-windows-test PRIVATE -Wall -Wextra -Werror)
add_test(NAME overlay-windows COMMAND ef2000-overlay-windows-test)

add_executable(ef2000-host-shortcuts-test tests/host_shortcuts_test.cpp)
target_compile_features(ef2000-host-shortcuts-test PRIVATE cxx_std_17)
target_compile_options(ef2000-host-shortcuts-test PRIVATE -Wall -Wextra -Werror)
add_test(NAME host-shortcuts COMMAND ef2000-host-shortcuts-test)

# Optional EF2000 frame texture geometry; no SDL dependency or original data.
add_executable(ef2000-nine-slice-test tests/nine_slice_test.cpp)
target_compile_features(ef2000-nine-slice-test PRIVATE cxx_std_17)
target_compile_options(ef2000-nine-slice-test PRIVATE -Wall -Wextra -Werror)
add_test(NAME nine-slice COMMAND ef2000-nine-slice-test)

add_executable(ef2000-overlay-skin-test tests/overlay_skin_test.cpp)
target_compile_features(ef2000-overlay-skin-test PRIVATE cxx_std_17)
target_compile_options(ef2000-overlay-skin-test PRIVATE -Wall -Wextra -Werror)
add_test(NAME overlay-skin COMMAND ef2000-overlay-skin-test)

# Original menu font atlas layout with no SDL or proprietary assets.
add_executable(ef2000-menu-font-test tests/menu_font_test.cpp)
target_compile_features(ef2000-menu-font-test PRIVATE cxx_std_17)
target_compile_options(ef2000-menu-font-test PRIVATE -Wall -Wextra -Werror)
add_test(NAME menu-font COMMAND ef2000-menu-font-test)
add_executable(ef2000-decomp-marker-packets-test
  tests/decomp_marker_packets_test.c decomp/src/marker_packets.c)
target_compile_features(ef2000-decomp-marker-packets-test PRIVATE c_std_11)
target_compile_options(ef2000-decomp-marker-packets-test PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(ef2000-decomp-marker-packets-test PRIVATE EF_DECOMP_HOST_TEST=1)
target_include_directories(ef2000-decomp-marker-packets-test PRIVATE decomp/include)
add_test(NAME decomp-marker-packets-host-model COMMAND ef2000-decomp-marker-packets-test)
add_executable(ef2000-decomp-status-values-test
  tests/decomp_status_values_test.c decomp/src/status_values.c)
target_compile_features(ef2000-decomp-status-values-test PRIVATE c_std_11)
target_compile_options(ef2000-decomp-status-values-test PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(ef2000-decomp-status-values-test PRIVATE EF_DECOMP_HOST_TEST=1)
target_include_directories(ef2000-decomp-status-values-test PRIVATE decomp/include)
add_test(NAME decomp-status-values-host-model COMMAND ef2000-decomp-status-values-test)
