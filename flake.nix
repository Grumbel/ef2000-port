# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

{
  description = "EF2000 Linux original-code main-menu port and reverse-engineering tools";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; };
      inherit (pkgs) lib;
      python = pkgs.python3.withPackages (p: [ p.unicorn ]);
      versionBase = lib.fileContents ./VERSION;
      gitRev = self.shortRev or self.dirtyShortRev or "dirty";
      version =
        if lib.hasInfix "-dev" versionBase
        then "${versionBase}.${toString (self.revCount or 0)}+g${gitRev}"
        else versionBase;
      source = lib.fileset.toSource {
        root = ./.;
        fileset = lib.fileset.unions [
          ./runtime ./tools ./tests ./probes ./docs ./data ./dev ./CMakeLists.txt ./README.md
          ./VERSION ./LICENSES ./REUSE.toml ./TODO.md ./.gitignore ./flake.nix ./flake.lock
        ];
      };
      package = pkgs.stdenv.mkDerivation {
        pname = "ef2000";
        inherit version;
        src = source;

        nativeBuildInputs = [ python pkgs.makeWrapper ];
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
          runHook postBuild
        '';

        doCheck = true;
        checkPhase = ''
          runHook preCheck
          unset EF2000_EXE
          PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tests -v
          runHook postCheck
        '';

        installPhase = ''
          runHook preInstall
          mkdir -p "$out/bin" "$out/lib/ef2000" "$out/libexec/ef2000" \
            "$out/share/doc/ef2000" "$out/share/licenses/ef2000" \
            "$out/share/ef2000" "$out/share/applications" "$out/share/metainfo" \
            "$out/share/man/man6" "$out/share/icons/hicolor/scalable/apps"
          cp -r objects/native64 objects/qemu "$out/lib/ef2000/"
          cp -r runtime tools probes "$out/libexec/ef2000/"
          cp VERSION "$out/libexec/ef2000/"
          cp -r docs README.md TODO.md VERSION "$out/share/doc/ef2000/"
          cp LICENSES/GPL-3.0-or-later.txt "$out/share/licenses/ef2000/"
          cp LICENSES/CC0-1.0.txt "$out/share/licenses/ef2000/"
          cp data/ef2000.desktop "$out/share/applications/ef2000.desktop"
          cp data/ef2000.metainfo.xml "$out/share/metainfo/ef2000.metainfo.xml"
          cp data/ef2000.6 "$out/share/man/man6/ef2000.6"
          cp data/icons/ef2000.svg "$out/share/icons/hicolor/scalable/apps/ef2000.svg"
          cp data/icons/ef2000.bmp "$out/share/ef2000/ef2000.bmp"
          cp -r data/icons/hicolor/* "$out/share/icons/hicolor/"

          makeWrapper ${python}/bin/python3 "$out/bin/ef2000" \
            --add-flags "$out/libexec/ef2000/tools/run_startup.py" \
            --add-flags "--runtime-objects $out/lib/ef2000" \
            --set EF2000_VERSION ${lib.escapeShellArg version} \
            --set EF2000_SDL_LIBRARY ${pkgs.sdl3}/lib/libSDL3.so \
            --set EF2000_ICON_BMP "$out/share/ef2000/ef2000.bmp" \
            --prefix PATH : ${lib.makeBinPath [ pkgs.binutils pkgs.qemu-user ]}
          makeWrapper ${python}/bin/python3 "$out/bin/ef2000-unicorn-probe" \
            --add-flags "$out/libexec/ef2000/tools/unicorn_backend.py"
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
    in {
      packages.${system} = { default = package; ef2000 = package; ef2000-startup = package; };
      apps.${system} = let
        native = { type = "app"; program = "${package}/bin/ef2000"; };
      in { default = native; ef2000 = native;
           unicorn-probe = { type = "app"; program = "${package}/bin/ef2000-unicorn-probe"; }; };
      checks.${system} = self.packages.${system} // {
        app-default = pkgs.runCommand "ef2000-app-check" {} ''
          test -x ${package}/bin/ef2000
          touch "$out"
        '';
        app-ef2000 = self.checks.${system}.app-default;
        help = pkgs.runCommand "ef2000-help-check" {} ''
          ${package}/bin/ef2000 --help > "$out"
        '';
        version = pkgs.runCommand "ef2000-version-check" {} ''
          test "$(${package}/bin/ef2000 --version)" = ${lib.escapeShellArg "ef2000 ${version}"}
          touch "$out"
        '';
        layout = pkgs.runCommand "ef2000-layout-check" {} ''
          for path in bin/ef2000 share/ef2000 share/applications/ef2000.desktop \
            share/metainfo/ef2000.metainfo.xml share/man/man6/ef2000.6 \
            share/icons/hicolor/48x48/apps/ef2000.png; do
            test -e "${package}/$path"
          done
          touch "$out"
        '';
        integration = pkgs.runCommand "ef2000-integration-check" {
          nativeBuildInputs = [ pkgs.desktop-file-utils pkgs.appstream ];
        } ''
          desktop-file-validate ${package}/share/applications/ef2000.desktop
          appstreamcli validate --no-net ${package}/share/metainfo/ef2000.metainfo.xml
          touch "$out"
        '';
        man = pkgs.runCommand "ef2000-man-check" { nativeBuildInputs = [ pkgs.mandoc ]; } ''
          mandoc -T lint ${package}/share/man/man6/ef2000.6
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
      devShells.${system}.default = pkgs.mkShell {
        inputsFrom = [ package ];
        packages = [ python ] ++ devCommands ++ (with pkgs; [
          gcc binutils qemu-user dosbox-x cmake ninja gdb ccache
        ]);
        EF2000_SDL_LIBRARY = "${pkgs.sdl3}/lib/libSDL3.so";
        EF2000_VERSION = version;
        shellHook = ''
          export EF2000_SOURCE="$PWD"
          export EF2000_BUILD_DIR="''${EF2000_BUILD_DIR:-/tmp/ef2000-build}"
          export EF2000_ICON_BMP="$EF2000_SOURCE/data/icons/ef2000.bmp"
        '';
      };
    };
}
