# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

{
  description = "EF2000 Linux original-code main-menu port and reverse-engineering tools";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; };
      inherit (pkgs) lib;
      python = pkgs.python3.withPackages (p: [ p.unicorn ]);
      ghidraHome = "${pkgs.ghidra}/lib/ghidra";
      watcom = pkgs.open-watcom-v2;
      watcomCompat = pkgs.open-watcom-bin;
      versionBase = lib.fileContents ./VERSION;
      gitRev = self.shortRev or self.dirtyShortRev or "dirty";
      version =
        if lib.hasInfix "-dev" versionBase
        then "${versionBase}.${toString (self.revCount or 0)}+g${gitRev}"
        else versionBase;
      source = lib.fileset.toSource {
        root = ./.;
        fileset = lib.fileset.unions [
          ./runtime ./tools ./tests ./probes ./docs ./data ./dev ./decomp ./CMakeLists.txt ./README.md
          ./VERSION ./LICENSES ./REUSE.toml ./TODO.md ./.gitignore ./flake.nix ./flake.lock
        ];
      };
      windowsFor = target: crossPkgs: import ./nix/windows.nix {
        inherit pkgs crossPkgs target version source;
      };
      win64 = windowsFor "win64" pkgs.pkgsCross.mingwW64;
      win32 = windowsFor "win32" pkgs.pkgsCross.mingw32;
      package = pkgs.stdenv.mkDerivation {
        pname = "ef2000";
        inherit version;
        src = source;

        nativeBuildInputs = [ python pkgs.makeWrapper ];
        buildInputs = [ pkgs.unicorn pkgs.openssl pkgs.sdl3 ];
        # Guest transitions do not provide host TLS for stack canaries; the
        # freestanding harness also needs its linker script's fixed addresses.
        hardeningDisable = [ "pic" "stackprotector" "fortify" "fortify3" ];

        buildPhase = ''
          runHook preBuild
          commonFlags="-std=c11 -O2 -Wall -Wextra -Werror -ffreestanding -fno-pie -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables -fno-builtin"
          mkdir -p objects/native64 objects/qemu
          for backend in native64 qemu; do
            if [ "$backend" = native64 ]; then
              width=64
              defines="-D_GNU_SOURCE -DEF_NATIVE64"
              entry=runtime/native64.S
            else
              width=32
              defines=""
              entry=runtime/entry.S
            fi
            $CC -m$width $commonFlags $defines -c runtime/startup.c -o objects/$backend/startup.o
            $CC -m$width $commonFlags $defines -c runtime/x32.c -o objects/$backend/x32.o
            as --$width -I runtime "$entry" -o objects/$backend/entry.o
            as --$width -I runtime --defsym CHECK_SEGMENT_LIMIT=1 "$entry" -o objects/$backend/entry-limit.o
          done
          $CXX -std=c++17 -O2 -Wall -Wextra -Werror -DEF_UNICORN_SDL3 \
            runtime/unicorn_cpp.cpp runtime/dos_drive.cpp runtime/cd_audio.cpp runtime/real_mode_cpu.cpp \
            runtime/sb16_probe.cpp runtime/sound_blaster.cpp runtime/isa_audio.cpp \
            runtime/pcm_audio.cpp runtime/wav_capture.cpp \
            -lunicorn -lcrypto -lSDL3 \
            -o objects/ef2000-unicorn-cpp
          $CXX -std=c++17 -O2 -Wall -Wextra -Werror \
            tests/dos_drive_test.cpp runtime/dos_drive.cpp \
            -o objects/ef2000-dos-drive-test
          $CXX -std=c++17 -O2 -Wall -Wextra -Werror \
            tests/bios_disk_test.cpp \
            -o objects/ef2000-bios-disk-test
          $CXX -std=c++17 -O2 -Wall -Wextra -Werror \
            tests/real_mode_cpu_test.cpp runtime/real_mode_cpu.cpp runtime/sb16_probe.cpp \
            runtime/sound_blaster.cpp runtime/isa_audio.cpp \
            -lunicorn \
            -o objects/ef2000-real-mode-test
          $CXX -std=c++17 -O2 -Wall -Wextra -Werror \
            tests/original_sound_driver_test.cpp runtime/real_mode_cpu.cpp \
            runtime/sb16_probe.cpp runtime/sound_blaster.cpp runtime/isa_audio.cpp \
            -lunicorn -lcrypto \
            -o objects/ef2000-original-sb16-test
          $CXX -std=c++17 -O2 -Wall -Wextra -Werror \
            tests/cd_audio_test.cpp runtime/cd_audio.cpp \
            -o objects/ef2000-cd-audio-test
          $CXX -std=c++17 -O2 -Wall -Wextra -Werror \
            tests/sb16_probe_test.cpp runtime/sb16_probe.cpp \
            -o objects/ef2000-sb16-probe-test
          $CXX -std=c++17 -O2 -Wall -Wextra -Werror \
            tests/isa_audio_test.cpp runtime/isa_audio.cpp \
            -o objects/ef2000-isa-audio-test
          $CXX -std=c++17 -O2 -Wall -Wextra -Werror \
            tests/sound_blaster_test.cpp runtime/sound_blaster.cpp runtime/isa_audio.cpp \
            runtime/sb16_probe.cpp \
            -o objects/ef2000-sound-blaster-test
          $CXX -std=c++17 -O2 -Wall -Wextra -Werror \
            tests/pcm_audio_test.cpp runtime/pcm_audio.cpp \
            -o objects/ef2000-pcm-audio-test
          $CXX -std=c++17 -O2 -Wall -Wextra -Werror \
            tests/wav_capture_test.cpp runtime/wav_capture.cpp \
            -o objects/ef2000-wav-capture-test
          $CXX -std=c++17 -O2 -Wall -Wextra -Werror \
            tests/ef2000_highscores_test.cpp runtime/ef2000_highscores.cpp \
            -o objects/ef2000-highscores-test
          $CXX -std=c++17 -O2 -Wall -Wextra -Werror \
            tests/ef2000_mission_layout_test.cpp runtime/ef2000_mission_layout.cpp \
            -o objects/ef2000-mission-layout-test
          $CXX -std=c++17 -O2 -Wall -Wextra -Werror \
            tests/gamepad_profile_test.cpp \
            -o objects/ef2000-gamepad-profile-test
          $CXX -std=c++17 -O2 -Wall -Wextra -Werror \
            tests/overlay_windows_test.cpp \
            -o objects/ef2000-overlay-windows-test
          $CXX -std=c++17 -O2 -Wall -Wextra -Werror \
            tests/host_shortcuts_test.cpp \
            -o objects/ef2000-host-shortcuts-test
          $CXX -std=c++17 -O2 -Wall -Wextra -Werror \
            tests/nine_slice_test.cpp \
            -o objects/ef2000-nine-slice-test
          $CXX -std=c++17 -O2 -Wall -Wextra -Werror \
            tests/overlay_skin_test.cpp \
            -o objects/ef2000-overlay-skin-test
          $CXX -std=c++17 -O2 -Wall -Wextra -Werror \
            tests/menu_font_test.cpp \
            -o objects/ef2000-menu-font-test
          $CXX -std=c++17 -O2 -Wall -Wextra -Werror -Iruntime \
            tests/efpatch_test.cpp -o objects/ef2000-efpatch-test
          $CXX -std=c++17 -O2 -Wall -Wextra -Werror -Iruntime \
            tests/call_capture_test.cpp -o objects/ef2000-call-capture-test
          $CC -std=c11 -O2 -Wall -Wextra -Werror -Idecomp/include \
            tests/decomp_hiscore_test.c decomp/src/hiscore.c \
            -o objects/ef2000-decomp-hiscore-test
          runHook postBuild
        '';

        doCheck = true;
        checkPhase = ''
          runHook preCheck
          unset EF2000_EXE
          unset EF2000_DATA_DIR
          PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tests -v
          objects/ef2000-unicorn-cpp --self-test | grep -q 'status=guest_exit interrupts=2'
          objects/ef2000-efpatch-test
          objects/ef2000-call-capture-test call-capture-fixture.efcap
          objects/ef2000-decomp-hiscore-test
          objects/ef2000-unicorn-cpp --self-test --joystick-event 0:0:0:0 \
            --joystick-axes=0,1 --trace-joystick | grep -q 'gameport: axes=2 reads=1 writes=1'
          objects/ef2000-unicorn-cpp --self-test-bda | grep -q 'status=guest_exit interrupts=5'
          objects/ef2000-unicorn-cpp --self-test-clock | grep -Eq 'status=wall_time_limit interrupts=0 ticks=[1-9].*ax=0x12345678'
          objects/ef2000-unicorn-cpp --self-test-retrace | grep -q 'status=guest_exit'
          objects/ef2000-unicorn-cpp --self-test-retrace --vga-clock-check 1 --block-clock-check 1 | grep -q 'status=guest_exit'
          objects/ef2000-unicorn-cpp --self-test-retrace --profile | grep -Eq 'profile: blocks=[0-9]+.*vga_reads=[1-9]'
          objects/ef2000-unicorn-cpp --self-test-fast-copy | grep -q 'fast_copy: calls=1 bytes=1024 fallback_calls=0'
          objects/ef2000-unicorn-cpp --self-test-fast-copy --profile-copy-callers | \
            grep -q 'copy_caller: ret=0xab calls=1 bytes=1024 max=1024 ge1024=1 ge4096=0 vga_calls=0 vga_bytes=0 accelerated=1 accelerated_bytes=1024'
          objects/ef2000-unicorn-cpp --self-test-fast-copy --fast-copy-vga | \
            grep -q 'fast_copy_vga: calls=1 bytes=1024'
          objects/ef2000-unicorn-cpp --self-test-clock --wall-seconds 0.1 --profile-hot-blocks | grep -Eq 'hot_blocks: samples=[1-9]'
          objects/ef2000-unicorn-cpp --self-test-clock --tcg-cache-mib 128 | grep -q 'status=wall_time_limit'
          objects/ef2000-dos-drive-test
          objects/ef2000-bios-disk-test
          objects/ef2000-real-mode-test
          objects/ef2000-original-sb16-test
          objects/ef2000-cd-audio-test
          objects/ef2000-sb16-probe-test
          objects/ef2000-isa-audio-test
          objects/ef2000-sound-blaster-test
          objects/ef2000-pcm-audio-test
          objects/ef2000-wav-capture-test
          objects/ef2000-highscores-test
          objects/ef2000-mission-layout-test
          objects/ef2000-gamepad-profile-test
          objects/ef2000-overlay-windows-test
          objects/ef2000-host-shortcuts-test
          objects/ef2000-nine-slice-test
          objects/ef2000-overlay-skin-test
          objects/ef2000-menu-font-test
          runHook postCheck
        '';

        installPhase = ''
          runHook preInstall
          mkdir -p "$out/bin" "$out/lib/ef2000" "$out/libexec/ef2000" \
            "$out/share/doc/ef2000" "$out/share/licenses/ef2000" \
            "$out/share/ef2000" "$out/share/applications" "$out/share/metainfo" \
            "$out/share/man/man6" "$out/share/icons/hicolor/scalable/apps"
          cp -r objects/native64 objects/qemu "$out/lib/ef2000/"
          cp objects/ef2000-unicorn-cpp "$out/bin/ef2000-unicorn-cpp"
          cp -r runtime tools probes "$out/libexec/ef2000/"
          cp VERSION "$out/libexec/ef2000/"
          cp -r docs README.md TODO.md VERSION "$out/share/doc/ef2000/"
          cp LICENSES/GPL-3.0-or-later.txt "$out/share/licenses/ef2000/"
          cp LICENSES/GPL-3.0-linking-exception.txt "$out/share/licenses/ef2000/"
          cp LICENSES/CC0-1.0.txt "$out/share/licenses/ef2000/"
          cp data/ef2000.desktop "$out/share/applications/ef2000.desktop"
          cp data/io.github.grumbel.ef2000.metainfo.xml \
            "$out/share/metainfo/io.github.grumbel.ef2000.metainfo.xml"
          cp data/ef2000.6 "$out/share/man/man6/ef2000.6"
          cp data/icons/ef2000.svg "$out/share/icons/hicolor/scalable/apps/ef2000.svg"
          cp data/icons/ef2000.bmp "$out/share/ef2000/ef2000.bmp"
          # Install each expected size explicitly so the layout check validates
          # the same paths the package promises to ship.
          for size in 16x16 32x32 48x48 64x64 128x128 256x256; do
            install -Dm644 "data/icons/hicolor/$size/apps/ef2000.png" \
              "$out/share/icons/hicolor/$size/apps/ef2000.png"
          done

          makeWrapper ${python}/bin/python3 "$out/bin/ef2000" \
            --add-flags "$out/libexec/ef2000/tools/run_startup.py" \
            --add-flags "--runtime-objects $out/lib/ef2000" \
            --set EF2000_VERSION ${lib.escapeShellArg version} \
            --set EF2000_SDL_LIBRARY ${pkgs.sdl3}/lib/libSDL3.so \
            --set EF2000_ICON_BMP "$out/share/ef2000/ef2000.bmp" \
            --prefix PATH : ${lib.makeBinPath [ pkgs.binutils pkgs.qemu-user ]}
          makeWrapper ${python}/bin/python3 "$out/bin/ef2000-unicorn-probe" \
            --add-flags "$out/libexec/ef2000/tools/unicorn_backend.py" \
            --set EF2000_SDL_LIBRARY ${pkgs.sdl3}/lib/libSDL3.so \
            --set EF2000_ICON_BMP "$out/share/ef2000/ef2000.bmp"
          makeWrapper ${python}/bin/python3 "$out/bin/ef2000-unicorn" \
            --add-flags "$out/libexec/ef2000/tools/unicorn_app.py" \
            --set EF2000_VERSION ${lib.escapeShellArg version} \
            --set EF2000_SDL_LIBRARY ${pkgs.sdl3}/lib/libSDL3.so \
            --set EF2000_ICON_BMP "$out/share/ef2000/ef2000.bmp"
          ln -s ef2000 "$out/bin/ef2000-startup"
          makeWrapper ${pkgs.python3}/bin/python3 "$out/bin/ef2000-inspect" \
            --add-flags "$out/libexec/ef2000/tools/inspect_exe.py"
          makeWrapper ${pkgs.python3}/bin/python3 "$out/bin/ef2000-extract" \
            --add-flags "$out/libexec/ef2000/tools/extract_image.py"
          makeWrapper ${pkgs.python3}/bin/python3 "$out/bin/ef2000-probe-entry" \
            --add-flags "$out/libexec/ef2000/tools/probe_entry.py" \
            --prefix PATH : ${lib.makeBinPath [ pkgs.qemu-user ]}
          makeWrapper ${pkgs.python3}/bin/python3 "$out/bin/ef2000-reference" \
            --add-flags "$out/libexec/ef2000/tools/reference_run.py" \
            --prefix PATH : ${lib.makeBinPath [ pkgs.binutils pkgs.dosbox-x ]}
          makeWrapper ${pkgs.python3}/bin/python3 "$out/bin/ef2000-read-x32-probe" \
            --add-flags "$out/libexec/ef2000/tools/read_x32_probe.py"
          runHook postInstall
        '';

        meta = {
          description = "Original-code EF2000 main-menu port";
          platforms = [ system ];
          mainProgram = "ef2000";
          license = lib.licenses.gpl3Plus;
        };
      };
      devCommand = name: script: pkgs.writeShellScriptBin name ''
        set -euo pipefail
        exec ${pkgs.bash}/bin/bash ${script} "$@"
      '';
      devCommands = [
        (devCommand "ef2000-configure" ./dev/configure.sh)
        (devCommand "ef2000-build" ./dev/build.sh)
        (devCommand "ef2000-run" ./dev/run.sh)
        (devCommand "ef2000-run-gdb" ./dev/run-gdb.sh)
      ];
      # The headless workflow is a separate tool: keep Ghidra's large runtime
      # out of the main game package closure. Its scripts are read from the
      # Nix source tree, while projects/exports default to the caller's cwd.
      ghidraWorkflow = pkgs.writeShellScriptBin "ef2000-ghidra" ''
        set -euo pipefail
        if [ -z "''${GHIDRA_HOME:-}" ]; then
          export GHIDRA_HOME=${lib.escapeShellArg ghidraHome}
        fi
        if [ -z "''${EF2000_GHIDRA_WORKSPACE:-}" ]; then
          export EF2000_GHIDRA_WORKSPACE="$PWD/local/ghidra"
        fi
        exec ${pkgs.python3}/bin/python3 ${source}/tools/ghidra_ef2000.py "$@"
      '';
      # The wrapped Nixpkgs wcc386 supplies WATCOM, INCLUDE, and the
      # binl64/binl search path internally. Keep v2 explicit even when the
      # 1.9 compatibility package also exists in the development shell.
      watcomWorkflow = pkgs.writeShellScriptBin "ef2000-wcc" ''
        set -euo pipefail
        exec ${pkgs.python3}/bin/python3 ${source}/tools/ef2000_wcc.py \
          --compiler ${watcom}/bin/wcc386 "$@"
      '';
      # The original executable is supplied by the caller, never fetched or
      # included in Nix inputs. The replay compiles the test-only native ABI
      # into a temporary directory and runs original guest code in Unicorn.
      highscoresDifferential = pkgs.writeShellScriptBin "ef2000-highscores-differential" ''
        set -euo pipefail
        exec ${python}/bin/python3 ${source}/tools/highscores_differential.py \
          --compiler ${pkgs.stdenv.cc}/bin/g++ "$@"
      '';
      funcDifferential = pkgs.writeShellScriptBin "ef2000-func-differential" ''
        set -euo pipefail
        exec ${python}/bin/python3 ${source}/tools/func_differential.py "$@"
      '';
      omfInplace = pkgs.writeShellScriptBin "ef2000-omf-inplace" ''
        set -euo pipefail
        exec ${pkgs.python3}/bin/python3 ${source}/tools/omf_inplace.py "$@"
      '';
      # Asset decoding is pure Python; users supply DID archives and select a
      # writable output directory. Nothing proprietary enters the Nix store.
      guiAssetsWorkflow = pkgs.writeShellScriptBin "ef2000-gui-assets" ''
        set -euo pipefail
        exec ${pkgs.python3}/bin/python3 ${source}/tools/ef2000_gui_assets.py "$@"
      '';
    in {
      packages.${system} = {
        default = package;
        ef2000 = package;
        ef2000-startup = package;
        ef2000-ghidra = ghidraWorkflow;
        ef2000-wcc = watcomWorkflow;
        ef2000-highscores-differential = highscoresDifferential;
        ef2000-func-differential = funcDifferential;
        ef2000-omf-inplace = omfInplace;
        ef2000-gui-assets = guiAssetsWorkflow;
        ef2000-win64 = win64.dist;
        ef2000-win64-zip = win64.zip;
        ef2000-win32 = win32.dist;
        ef2000-win32-zip = win32.zip;
      };
      apps.${system} = let
        native = {
          type = "app";
          program = "${package}/bin/ef2000";
          meta.description = "Run the original EF2000 game with the native Linux backend";
        };
        unicorn = {
          type = "app";
          program = "${package}/bin/ef2000-unicorn";
          meta.description = "Experiment with original EF2000 menu and flight using Unicorn and SDL3";
        };
        unicorn-cpp = {
          type = "app";
          program = "${package}/bin/ef2000-unicorn-cpp";
          meta.description = "Experiment with original EF2000 menu and flight in native C++ Unicorn/SDL3";
        };
        ghidra = {
          type = "app";
          program = "${pkgs.ghidra}/bin/ghidra";
          meta.description = "Open Ghidra to inspect the extracted EF2000 X-32 image";
        };
      in { default = native; ef2000 = native;
           inherit unicorn;
           inherit unicorn-cpp;
           inherit ghidra;
           ghidra-ef2000 = {
             type = "app";
             program = "${ghidraWorkflow}/bin/ef2000-ghidra";
             meta.description = "Import, audit, repair, and export the EF2000 Ghidra project";
           };
           highscores-differential = {
             type = "app";
             program = "${highscoresDifferential}/bin/ef2000-highscores-differential";
             meta.description = "Compare original high-score guest routines with native C++ reconstruction";
           };
           func-differential = {
             type = "app";
             program = "${funcDifferential}/bin/ef2000-func-differential";
             meta.description = "Compare an original EF2000 function with its in-place EFPATCH1 replacement";
           };
           omf-inplace = {
             type = "app";
             program = "${omfInplace}/bin/ef2000-omf-inplace";
             meta.description = "Build a reviewed in-place EFPATCH1 from a compiled OpenWatcom OMF object";
           };
           gui-assets = {
             type = "app";
             program = "${guiAssetsWorkflow}/bin/ef2000-gui-assets";
             meta.description = "Inventory and extract original EF2000 menu artwork and GUI scripts";
           };
           ghidra-headless = {
             type = "app";
             program = "${ghidraHome}/support/analyzeHeadless";
             meta.description = "Run the Ghidra headless analyzer";
           };
           ef2000-unicorn = unicorn;
           unicorn-probe = {
             type = "app";
             program = "${package}/bin/ef2000-unicorn-probe";
             meta.description = "Probe the original EF2000 executable with portable Unicorn emulation";
           }; };
      checks.${system} = self.packages.${system} // {
        app-default = pkgs.runCommand "ef2000-app-check" {} ''
          test -x ${package}/bin/ef2000
          touch "$out"
        '';
        app-ef2000 = self.checks.${system}.app-default;
        app-unicorn = pkgs.runCommand "ef2000-unicorn-app-check" {} ''
          test -x ${package}/bin/ef2000-unicorn
          ${package}/bin/ef2000-unicorn --help > "$out"
          test "$(${package}/bin/ef2000-unicorn --version)" = ${lib.escapeShellArg "ef2000-unicorn ${version}"}
        '';
        app-unicorn-cpp = pkgs.runCommand "ef2000-unicorn-cpp-app-check" {} ''
          ${package}/bin/ef2000-unicorn-cpp --help > "$out"
          test -s "$out"
          ${package}/bin/ef2000-unicorn-cpp --self-test | grep -q 'status=guest_exit interrupts=2'
        '';
        ghidra-workflow = pkgs.runCommand "ef2000-ghidra-workflow-check" {
          nativeBuildInputs = [ pkgs.python3 ];
        } ''
          ${ghidraWorkflow}/bin/ef2000-ghidra --help > "$out"
          grep -q 'audit-flow-gaps' "$out"
          grep -q 'repair-boundaries' "$out"
          grep -q 'export-all' "$out"
          grep -q 'function-facts' "$out"
          grep -q 'subsystem-map' "$out"
          cd ${source}
          PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tests \
            -p 'test_ghidra*.py' -q
        '';
        watcom-omf = pkgs.runCommand "ef2000-watcom-omf-check" {
          nativeBuildInputs = [ watcom pkgs.file pkgs.python3 pkgs.stdenv.cc ];
        } ''
          (cd ${source} && PYTHONDONTWRITEBYTECODE=1 python3 -m unittest tests.test_ef2000_wcc -q)
          mkdir -p work
          ${watcomWorkflow}/bin/ef2000-wcc \
            ${source}/decomp/tests/fixture.c --output "$PWD/work/fixture.obj"
          test -s work/fixture.obj
          # wdis reads OMF records, unlike an ELF disassembler.
          ${watcom}/bin/wdis work/fixture.obj -l=work/fixture.asm
          test -s work/fixture.asm
          grep -qi 'ef_omf_fixture' work/fixture.asm
          ! grep -qi '__CHK' work/fixture.asm
          ${watcomWorkflow}/bin/ef2000-wcc \
            ${source}/decomp/src/hiscore.c --output "$PWD/work/hiscore.obj"
          ${watcom}/bin/wdis work/hiscore.obj -l=work/hiscore.asm
          grep -qi 'hiscore_qualifies' work/hiscore.asm
          ! grep -qi '__CHK' work/hiscore.asm
          ${watcomWorkflow}/bin/ef2000-wcc \
            ${source}/decomp/src/spatial.c --output "$PWD/work/spatial.obj"
          ${watcom}/bin/wdis work/spatial.obj -l=work/spatial.asm
          grep -qi 'reset_spatial_slot_count' work/spatial.asm
          grep -qi 'append_spatial_slot' work/spatial.asm
          ! grep -qi '__CHK' work/spatial.asm
          ${watcomWorkflow}/bin/ef2000-wcc \
            ${source}/decomp/src/indexed_objects.c --output "$PWD/work/indexed_objects.obj"
          ${watcom}/bin/wdis work/indexed_objects.obj -l=work/indexed_objects.asm
          grep -qi 'lookup_indexed_object' work/indexed_objects.asm
          ! grep -qi '__CHK' work/indexed_objects.asm
          ${watcomWorkflow}/bin/ef2000-wcc \
            ${source}/decomp/src/object_lookup.c --output "$PWD/work/object_lookup.obj"
          ${watcom}/bin/wdis work/object_lookup.obj -l=work/object_lookup.asm
          grep -qi 'lookup_filtered_indexed_object' work/object_lookup.asm
          ! grep -qi '__CHK' work/object_lookup.asm
          ${watcomWorkflow}/bin/ef2000-wcc \
            ${source}/decomp/src/object_state.c --output "$PWD/work/object_state.obj"
          ${watcom}/bin/wdis work/object_state.obj -l=work/object_state.asm
          grep -qi 'set_indexed_object_activation' work/object_state.asm
          grep -qi 'clear_indexed_object_activation' work/object_state.asm
          grep -qi 'set_indexed_object_state' work/object_state.asm
          grep -qi 'clear_indexed_object_state' work/object_state.asm
          ! grep -qi '__CHK' work/object_state.asm
          ${watcomWorkflow}/bin/ef2000-wcc \
            ${source}/decomp/src/world_grid.c --output "$PWD/work/world_grid.obj"
          ${watcom}/bin/wdis work/world_grid.obj -l=work/world_grid.asm
          grep -qi 'grid_slot_matches_object' work/world_grid.asm
          grep -qi 'enable_grid_slot_activation' work/world_grid.asm
          ! grep -qi '__CHK' work/world_grid.asm
          ${watcomWorkflow}/bin/ef2000-wcc \
            ${source}/decomp/src/object_control.c --output "$PWD/work/object_control.obj"
          ${watcom}/bin/wdis work/object_control.obj -l=work/object_control.asm
          grep -qi 'set_indexed_object_mode' work/object_control.asm
          grep -qi 'sync_current_object_slot' work/object_control.asm
          ! grep -qi '__CHK' work/object_control.asm
          ${watcomWorkflow}/bin/ef2000-wcc \
            ${source}/decomp/src/world_grid_queries.c --output "$PWD/work/world_grid_queries.obj"
          ${watcom}/bin/wdis work/world_grid_queries.obj -l=work/world_grid_queries.asm
          grep -qi 'grid_has_active_link' work/world_grid_queries.asm
          grep -qi 'grid_has_object_mode' work/world_grid_queries.asm
          grep -qi 'grid_is_current_or_external' work/world_grid_queries.asm
          grep -qi 'grid_event_matches' work/world_grid_queries.asm
          ! grep -qi '__CHK' work/world_grid_queries.asm
          ${watcomWorkflow}/bin/ef2000-wcc \
            ${source}/decomp/src/world_grid_control.c --output "$PWD/work/world_grid_control.obj"
          ${watcom}/bin/wdis work/world_grid_control.obj -l=work/world_grid_control.asm
          grep -qi 'link_grid_cell' work/world_grid_control.asm
          grep -qi 'reset_grid_cell_state' work/world_grid_control.asm
          ! grep -qi '__CHK' work/world_grid_control.asm
          ${watcomWorkflow}/bin/ef2000-wcc \
            ${source}/decomp/src/world_grid_links.c --output "$PWD/work/world_grid_links.obj"
          ${watcom}/bin/wdis work/world_grid_links.obj -l=work/world_grid_links.asm
          grep -qi 'detach_grid_link_on_mode_change' work/world_grid_links.asm
          grep -qi 'link_external_grid_cell' work/world_grid_links.asm
          grep -qi 'copy_grid_link_position' work/world_grid_links.asm
          grep -qi 'refresh_external_grid_position' work/world_grid_links.asm
          ! grep -qi '__CHK' work/world_grid_links.asm
          ${watcomWorkflow}/bin/ef2000-wcc \
            ${source}/decomp/src/world_grid_limits.c --output "$PWD/work/world_grid_limits.obj"
          ${watcom}/bin/wdis work/world_grid_limits.obj -l=work/world_grid_limits.asm
          grep -qi 'recompute_grid_limit' work/world_grid_limits.asm
          ! grep -qi '__CHK' work/world_grid_limits.asm
          ${watcomWorkflow}/bin/ef2000-wcc \
            ${source}/decomp/src/grid_node_pool.c --output "$PWD/work/grid_node_pool.obj"
          ${watcom}/bin/wdis work/grid_node_pool.obj -l=work/grid_node_pool.asm
          grep -qi 'recycle_oldest_grid_node' work/grid_node_pool.asm
          ! grep -qi '__CHK' work/grid_node_pool.asm
          ${watcomWorkflow}/bin/ef2000-wcc \
            ${source}/decomp/src/world_grid_neighbors.c --output "$PWD/work/world_grid_neighbors.obj"
          ${watcom}/bin/wdis work/world_grid_neighbors.obj -l=work/world_grid_neighbors.asm
          grep -qi 'find_grid_neighbor_with_detail' work/world_grid_neighbors.asm
          grep -qi 'grid_cell_distance' work/world_grid_neighbors.asm
          grep -qi 'grid_neighbor_distance' work/world_grid_neighbors.asm
          grep -qi 'grid_has_cross_object_relation' work/world_grid_neighbors.asm
          ! grep -qi '__CHK' work/world_grid_neighbors.asm
          ${watcomWorkflow}/bin/ef2000-wcc \
            ${source}/decomp/src/world_grid_category.c --output "$PWD/work/world_grid_category.obj"
          ${watcom}/bin/wdis work/world_grid_category.obj -l=work/world_grid_category.asm
          grep -qi 'refresh_grid_category' work/world_grid_category.asm
          grep -qi 'grid_is_visible_from_reference' work/world_grid_category.asm
          ! grep -qi '__CHK' work/world_grid_category.asm
          ${watcomWorkflow}/bin/ef2000-wcc \
            ${source}/decomp/src/object_geometry.c --output "$PWD/work/object_geometry.obj"
          ${watcom}/bin/wdis work/object_geometry.obj -l=work/object_geometry.asm
          grep -qi 'object_type_limit' work/object_geometry.asm
          grep -qi 'object_speed_from_clock' work/object_geometry.asm
          ! grep -qi '__CHK' work/object_geometry.asm
          ${watcomWorkflow}/bin/ef2000-wcc \
            ${source}/decomp/src/grid_point_distances.c --output "$PWD/work/grid_point_distances.obj"
          ${watcom}/bin/wdis work/grid_point_distances.obj -l=work/grid_point_distances.asm
          grep -qi 'grid_distance_to_point' work/grid_point_distances.asm
          grep -qi 'scaled_grid_distance_to_point' work/grid_point_distances.asm
          ! grep -qi '__CHK' work/grid_point_distances.asm
          ${watcomWorkflow}/bin/ef2000-wcc \
            ${source}/decomp/src/scene_nodes.c --output "$PWD/work/scene_nodes.obj"
          ${watcom}/bin/wdis work/scene_nodes.obj -l=work/scene_nodes.asm
          grep -qi 'scene_node_has_active_children' work/scene_nodes.asm
          grep -qi 'next_scene_node_with_active_children' work/scene_nodes.asm
          ! grep -qi '__CHK' work/scene_nodes.asm
          ${watcomWorkflow}/bin/ef2000-wcc \
            ${source}/decomp/src/scene_state.c --output "$PWD/work/scene_state.obj"
          ${watcom}/bin/wdis work/scene_state.obj -l=work/scene_state.asm
          grep -qi 'scene_lookup_named_word' work/scene_state.asm
          grep -qi 'record_scene_event' work/scene_state.asm
          ! grep -qi '__CHK' work/scene_state.asm
          $CC -std=c11 -Wall -Wextra -Werror -DEF_DECOMP_HOST_TEST=1 \
            -I${source}/decomp/include \
            ${source}/tests/decomp_object_control_test.c \
            ${source}/decomp/src/object_lookup.c \
            ${source}/decomp/src/object_state.c \
            ${source}/decomp/src/world_grid.c \
            ${source}/decomp/src/object_control.c \
            -o work/object_control_host_test
          work/object_control_host_test
          $CC -std=c11 -Wall -Wextra -Werror -DEF_DECOMP_HOST_TEST=1 \
            -I${source}/decomp/include \
            ${source}/tests/decomp_world_grid_batch_test.c \
            ${source}/decomp/src/object_lookup.c \
            ${source}/decomp/src/world_grid_queries.c \
            ${source}/decomp/src/world_grid_control.c \
            -o work/world_grid_batch_host_test
          work/world_grid_batch_host_test
          $CC -std=c11 -Wall -Wextra -Werror -DEF_DECOMP_HOST_TEST=1 \
            -I${source}/decomp/include \
            ${source}/tests/decomp_grid_helpers_test.c \
            ${source}/decomp/src/object_lookup.c \
            ${source}/decomp/src/world_grid_links.c \
            ${source}/decomp/src/world_grid_limits.c \
            -o work/grid_helpers_host_test
          work/grid_helpers_host_test
          $CC -std=c11 -O2 -Wall -Wextra -Werror -DEF_DECOMP_HOST_TEST=1 \
            -I${source}/decomp/include \
            ${source}/tests/decomp_world_grid_relations_test.c \
            ${source}/decomp/src/grid_node_pool.c \
            ${source}/decomp/src/world_grid_neighbors.c \
            ${source}/decomp/src/world_grid_category.c \
            -lm -o work/world_grid_relations_host_test
          work/world_grid_relations_host_test
          $CC -std=c11 -O2 -Wall -Wextra -Werror -DEF_DECOMP_HOST_TEST=1 \
            -I${source}/decomp/include \
            ${source}/tests/decomp_object_geometry_test.c \
            ${source}/decomp/src/object_geometry.c \
            ${source}/decomp/src/grid_point_distances.c \
            -lm -o work/object_geometry_host_test
          work/object_geometry_host_test
          $CC -std=c11 -O2 -Wall -Wextra -Werror -DEF_DECOMP_HOST_TEST=1 \
            -I${source}/decomp/include \
            ${source}/tests/decomp_scene_nodes_test.c \
            ${source}/decomp/src/scene_nodes.c \
            -o work/scene_nodes_host_test
          work/scene_nodes_host_test
          $CC -std=c11 -O2 -Wall -Wextra -Werror -DEF_DECOMP_HOST_TEST=1 \
            -I${source}/decomp/include \
            ${source}/tests/decomp_scene_state_test.c \
            ${source}/decomp/src/scene_state.c \
            -o work/scene_state_host_test
          work/scene_state_host_test
          cp work/fixture.asm "$out"
        '';
        highscores-reconstruction = pkgs.runCommand "ef2000-highscores-check" {
          nativeBuildInputs = [ pkgs.stdenv.cc ];
        } ''
          $CXX -std=c++17 -O2 -Wall -Wextra -Werror \
            ${source}/tests/ef2000_highscores_test.cpp \
            ${source}/runtime/ef2000_highscores.cpp \
            -o ef2000-highscores-test
          ./ef2000-highscores-test
          touch "$out"
        '';
        highscores-differential = pkgs.runCommand "ef2000-highscores-differential-check" {
          nativeBuildInputs = [ pkgs.python3 ];
        } ''
          ${highscoresDifferential}/bin/ef2000-highscores-differential --help > "$out"
          grep -q -- '--random-cases' "$out"
          cd ${source}
          unset EF2000_EXE
          PYTHONDONTWRITEBYTECODE=1 python3 -m unittest \
            tests.test_highscores_differential -q
        '';
        gui-assets = pkgs.runCommand "ef2000-gui-assets-check" {
          nativeBuildInputs = [ pkgs.python3 ];
        } ''
          ${guiAssetsWorkflow}/bin/ef2000-gui-assets --help > "$out"
          grep -q 'inventory' "$out"
          grep -q 'extract' "$out"
          cd ${source}
          unset EF2000_DATA_DIR
          PYTHONDONTWRITEBYTECODE=1 python3 -m unittest tests.test_gui_assets -q
        '';
        windows-layout = pkgs.runCommand "ef2000-windows-layout-check" {
          nativeBuildInputs = [ pkgs.file ];
        } ''
          for target in win64 win32; do
            case "$target" in
              win64) dist=${win64.dist} ;;
              win32) dist=${win32.dist} ;;
            esac
            dir="$dist/ef2000-${version}-$target"
            test -s "$dir/ef2000-unicorn-cpp.exe"
            test -s "$dir/SDL3.dll"
            test -n "$(find "$dir" -maxdepth 1 -iname '*unicorn*.dll' -print -quit)"
            file "$dir/ef2000-unicorn-cpp.exe" | grep -q 'PE32'
          done
          touch "$out"
        '';
        help = pkgs.runCommand "ef2000-help-check" {} ''
          ${package}/bin/ef2000 --help > "$out"
        '';
        version = pkgs.runCommand "ef2000-version-check" {} ''
          test "$(${package}/bin/ef2000 --version)" = ${lib.escapeShellArg "ef2000 ${version}"}
          touch "$out"
        '';
        layout = pkgs.runCommand "ef2000-layout-check" {} ''
          for path in bin/ef2000 share/ef2000 share/applications/ef2000.desktop \
            share/metainfo/io.github.grumbel.ef2000.metainfo.xml share/man/man6/ef2000.6.gz \
            share/icons/hicolor/48x48/apps/ef2000.png; do
            if [ ! -e "${package}/$path" ]; then
              echo "Missing installed EF2000 path: $path" >&2
              exit 1
            fi
          done
          touch "$out"
        '';
        integration = pkgs.runCommand "ef2000-integration-check" {
          nativeBuildInputs = [ pkgs.desktop-file-utils pkgs.appstream ];
        } ''
          desktop-file-validate ${package}/share/applications/ef2000.desktop
          appstreamcli validate --no-net \
            ${package}/share/metainfo/io.github.grumbel.ef2000.metainfo.xml
          touch "$out"
        '';
        man = pkgs.runCommand "ef2000-man-check" {
          nativeBuildInputs = [ pkgs.gzip pkgs.mandoc ];
        } ''
          set -o pipefail
          gzip -cd ${package}/share/man/man6/ef2000.6.gz | mandoc -T lint
          touch "$out"
        '';
        reuse = pkgs.runCommand "ef2000-reuse-check" {
          nativeBuildInputs = [ pkgs.reuse ];
        } ''
          cd ${source}
          reuse lint
          touch "$out"
        '';
      };
        func-differential = pkgs.runCommand "ef2000-func-differential-check" {
          nativeBuildInputs = [ python ];
        } ''
          ${funcDifferential}/bin/ef2000-func-differential --help > "$out"
          grep -q -- '--patch' "$out"
          cd ${source}
          PYTHONDONTWRITEBYTECODE=1 ${python}/bin/python3 -m unittest \
            tests.test_func_differential -q
        '';
      devShells.${system}.default = pkgs.mkShell {
        inputsFrom = [ package ];
        packages = [ python pkgs.ghidra ghidraWorkflow watcom watcomCompat watcomWorkflow highscoresDifferential omfInplace guiAssetsWorkflow ] ++ devCommands ++ (with pkgs; [
          gcc binutils qemu-user dosbox-x cmake ninja gdb ccache
        ]);
        GHIDRA_HOME = ghidraHome;
        WATCOM = "${watcom.unwrapped}";
        INCLUDE = "${watcom.unwrapped}/h:${watcom.unwrapped}/lh";
        EF2000_SDL_LIBRARY = "${pkgs.sdl3}/lib/libSDL3.so";
        EF2000_VERSION = version;
        shellHook = ''
          export EF2000_SOURCE="$PWD"
          export EF2000_BUILD_DIR="''${EF2000_BUILD_DIR:-/tmp/ef2000-build}"
          export EF2000_ICON_BMP="$EF2000_SOURCE/data/icons/ef2000.bmp"
          printf '\nEF2000 development shell (Debug build, SDL3, Unicorn)\n'
          printf '  Source:    %s\n' "$EF2000_SOURCE"
          printf '  Build:     %s\n' "$EF2000_BUILD_DIR"
          printf '  Build type: %s (set CMAKE_BUILD_TYPE to override)\n\n' "''${CMAKE_BUILD_TYPE:-Debug}"
          printf 'Development commands:\n'
          printf '  ef2000-configure [CMAKE_ARGS]   Configure Ninja and link ./build for clangd\n'
          printf '  ef2000-build [BUILD_ARGS]       Configure if needed, then build\n'
          printf '  ef2000-run [GAME_ARGS]          Build and run the original DOS game on Linux\n'
          printf '  ef2000-run-gdb [GAME_ARGS]      Build and debug the native game ELF in GDB\n\n'
          printf 'Portable original-code emulator (experimental, slower than native):\n'
          printf '  nix run .#unicorn -- --data-dir /path/to/EF2000\n'
          printf '                                  Start windowed Unicorn/SDL3 game frontend\n'
          printf '  nix run .#unicorn-probe -- /path/to/_EF2000.EXE\n'
          printf '                                  Run bounded headless startup diagnostics\n\n'
          printf '  nix run .#unicorn-cpp -- /path/to/_EF2000.EXE --windowed\n'
          printf '                                  Native C++ Unicorn/SDL3 menu and flight frontend\n\n'
          printf 'Windows MinGW cross-builds (original game assets are not included):\n'
          printf '  nix build .#ef2000-win64-zip    Windows x64 executable plus SDL3/Unicorn DLLs\n'
          printf '  nix build .#ef2000-win32-zip    Windows x86 executable plus SDL3/Unicorn DLLs\n\n'
          printf '  python tools/unicorn_backend.py /path/to/_EF2000.EXE --windowed\n'
          printf '                                  Direct Unicorn backend for development\n\n'
          printf 'Ghidra reverse engineering (original executable stays external):\n'
          printf '  ef2000-ghidra import /path/to/_EF2000.EXE\n'
          printf '                                  Import SHA-pinned X-32 image once\n'
          printf '  ef2000-ghidra decompile 0xe9a24 0x8b274\n'
          printf '                                  Export targeted pseudo-C and incoming references\n'
          printf '  ef2000-ghidra export-all --custom-storage-fallback\n'
          printf '                                  Export all recognized functions to pseudo-C\n'
          printf '  ef2000-ghidra function-facts\n'
          printf '                                  Export read-only per-function references and branches\n'
          printf '  ef2000-ghidra subsystem-map --seed 0x4aa0\n'
          printf '                                  Rank possible subsystem neighbors without rerunning Ghidra\n'
          printf '  ef2000-ghidra audit-flow-gaps\n'
          printf '                                  Audit suspicious fall-through decoding gaps\n'
          printf '  ef2000-ghidra audit-body-holes 0x9820\n'
          printf '                                  Inspect disconnected function-body spans\n'
          printf '  ef2000-ghidra repair-boundaries\n'
          printf '                                  Audit nine pinned function-body repairs (read-only)\n'
          printf '  nix run .#ghidra-ef2000 -- --help\n'
          printf '                                  Same workflow without entering the dev shell\n'
          printf '  ghidra                          Open Ghidra GUI\n\n'
          printf 'OpenWatcom (v2 default, 1.9 available for comparison):\n'
          printf '  ef2000-wcc decomp/src/MODULE.c  Compile recovered C to decomp/build/MODULE.obj\n'
          printf '  wdis decomp/build/MODULE.obj    Disassemble OpenWatcom OMF\n\n'
          printf '  ef2000-omf-inplace --help       Pack a reviewed, in-place-only OMF patch\n\n'
          printf 'Original/native high-score differential replay (requires your EF2000.EXE):\n'
          printf '  ef2000-highscores-differential /path/to/_EF2000.EXE --random-cases 300\n'
          printf '                                  Compare guest routines to reconstructed C++\n'
          printf '  nix run .#highscores-differential -- /path/to/_EF2000.EXE\n\n'
          printf 'GUI archaeology (requires original DID archives):\n'
          printf '  ef2000-gui-assets inventory /path/to/EF2000\n'
          printf '  ef2000-gui-assets extract /path/to/EF2000 gui/mainmenu.gui iff/256/newgui03.lbm --out local/gui-assets\n'
          printf '                                  Export menu scripts, structured JSON and PNGs\n\n'
          printf 'Examples:\n'
          printf '  ef2000-run --data-dir /path/to/EF2000 --windowed\n'
          printf '  ef2000-run-gdb --data-dir /path/to/EF2000 --verbose\n'
          printf '  ef2000-run --help               List game, display and diagnostic options\n'
          printf '  nix develop -c ef2000-run --data-dir /path/to/EF2000\n\n'
          printf 'The original game assets are external; all guest writes go to a run overlay.\n'
          printf 'Run logs and extracted startup.elf are saved in each run output directory.\n\n'
        '';
      };
    };
}
