# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

# Cross-compile the C++ Unicorn/SDL3 runner, not the Linux-only native64/QEMU
# harness or the Python probes. No original EF2000 data is redistributed.
# Based on the cross-build layout used by Grumbel/sdl-jstest.
{ pkgs, crossPkgs, target, version, source }:

let
  lib = pkgs.lib;
  stdenv = crossPkgs.stdenv;

  # nixpkgs' SDL3 derivation pulls in host-specific Unix dependencies when
  # cross-compiled. Build the Windows backend directly from the pinned source.
  sdl3 = stdenv.mkDerivation {
    pname = "ef2000-sdl3-${target}";
    inherit (pkgs.sdl3) src version;
    nativeBuildInputs = [ pkgs.cmake pkgs.ninja ];
    cmakeFlags = [
      "-DCMAKE_BUILD_TYPE=Release"
      "-DSDL_SHARED=ON"
      "-DSDL_STATIC=OFF"
      "-DSDL_TESTS=OFF"
      "-DSDL_TEST_LIBRARY=OFF"
      "-DSDL_EXAMPLES=OFF"
    ];
  };

  # The nixpkgs Unicorn package is marked Unix-only, but upstream supports
  # MinGW. Restrict its backends to x86 (the original guest is 32-bit x86).
  unicorn = stdenv.mkDerivation {
    pname = "ef2000-unicorn-${target}";
    inherit (pkgs.unicorn) src version;
    nativeBuildInputs = [ pkgs.cmake pkgs.ninja pkgs.pkg-config ];
    # Unicorn's MinGW build still compiles QEMU's POSIX thread backend and
    # links -lpthread. The default MinGW toolchain's mcfgthreads does not
    # provide pthread.h; winpthreads must be an explicit target dependency.
    buildInputs = [ crossPkgs.windows.pthreads ];
    cmakeFlags = [
      "-DCMAKE_BUILD_TYPE=Release"
      "-DBUILD_SHARED_LIBS=ON"
      "-DUNICORN_BUILD_TESTS=OFF"
      "-DUNICORN_INSTALL=ON"
      "-DUNICORN_LEGACY_STATIC_ARCHIVE=OFF"
      "-DUNICORN_ARCH=x86"
    ];
    # Upstream's pkg-config file contains cross-host absolute paths that are
    # not used by this project (CMake links the import library directly).
    postInstall = ''
      rm -f "$out/lib/pkgconfig/unicorn.pc"
    '';
  };

  build = stdenv.mkDerivation {
    pname = "ef2000-${target}-build";
    inherit version;
    src = source;
    nativeBuildInputs = [ pkgs.cmake pkgs.ninja ];
    cmakeFlags = [
      "-DCMAKE_BUILD_TYPE=Release"
      "-DUNICORN_INCLUDE_DIR=${unicorn}/include"
      "-DUNICORN_LIBRARY=${unicorn}/lib/libunicorn.dll.a"
      "-DSDL3_DIR=${sdl3}/lib/cmake/SDL3"
    ];
    # The other CMake targets are synthetic Linux harnesses and diagnostics.
    buildPhase = ''
      runHook preBuild
      cmake --build . --target ef2000-unicorn-cpp
      runHook postBuild
    '';
    installPhase = ''
      runHook preInstall
      mkdir -p "$out/bin"
      cp ef2000-unicorn-cpp.exe "$out/bin/"
      runHook postInstall
    '';
  };

  dirName = "ef2000-${version}-${target}";
  objdump = "${stdenv.cc.bintools.bintools}/bin/${stdenv.cc.targetPrefix}objdump";

  dist = pkgs.runCommand "ef2000-${version}-${target}" { } ''
    d="$out/${dirName}"
    mkdir -p "$d"
    cp ${build}/bin/ef2000-unicorn-cpp.exe "$d/"
    cp ${sdl3}/bin/*.dll ${unicorn}/bin/*.dll "$d/"

    # Bundle MinGW runtime libraries (libstdc++, libgcc, winpthreads, ...)
    # imported by the executable or its SDL3/Unicorn DLLs. Windows system
    # DLLs are supplied by the OS and are not redistributed.
    # Re-scan imports after copying libraries: libstdc++/libgcc can themselves
    # depend on another MinGW runtime DLL that the executable doesn't name.
    for pass in 1 2 3 4; do
      for dll in $(${objdump} -p "$d"/*.exe "$d"/*.dll 2>/dev/null \
                   | sed -n 's/^\tDLL Name: //p' | sort -u); do
        if [ -n "$(find "$d" -maxdepth 1 -iname "$dll" -print -quit)" ]; then
          continue
        fi
        f=$(find ${lib.concatStringsSep " " (map toString [
          stdenv.cc.cc stdenv.cc.libc
          crossPkgs.windows.pthreads crossPkgs.windows.mcfgthreads
        ])} -iname "$dll" -print -quit 2>/dev/null || true)
        if [ -n "$f" ]; then cp -L "$f" "$d/"; fi
      done
    done

    cp ${../LICENSES/GPL-3.0-or-later.txt} "$d/LICENSE.txt"
    cp ${../LICENSES/GPL-3.0-linking-exception.txt} "$d/LICENSE-EXCEPTION.txt"
    cp ${pkgs.unicorn.src}/COPYING "$d/LICENSE-unicorn.txt"
    cp ${pkgs.sdl3.src}/LICENSE.txt "$d/LICENSE-SDL3.txt"
    cp ${../README.md} "$d/README.md"
    cp ${../docs/windows.md} "$d/WINDOWS.md"
  '';

  zip = pkgs.runCommand "ef2000-${version}-${target}-zip" {
    nativeBuildInputs = [ pkgs.zip ];
  } ''
    mkdir -p "$out"
    cd ${dist}
    zip -r -9 "$out/${dirName}.zip" "${dirName}"
  '';
in {
  inherit sdl3 unicorn build dist zip;
}
