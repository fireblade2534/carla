#!/bin/bash
# CARLA 0.9.16 OSM2ODR build, adapted for CachyOS/Arch.
# Client: system Clang + libstdc++.  Unreal server: bundled Clang 10 + libc++.

DOC_STRING="Build OSM2ODR."
USAGE_STRING='Usage: BuildOSM2ODR.sh [--build|--rebuild|--clean]'

REMOVE_INTERMEDIATE=false
BUILD_OSM2ODR=false
CURRENT_OSM2ODR_COMMIT=1da3c07e39f3e2e0d97f8f709a7255a0df8c6200

OPTS=$(getopt -o h --long help,rebuild,build,clean,carsim,no-pull,chrono,chrono-path: -n 'parse-options' -- "$@") || exit 1
eval set -- "$OPTS"
while [[ $# -gt 0 ]]; do
  case "$1" in
    --rebuild) REMOVE_INTERMEDIATE=true; BUILD_OSM2ODR=true; shift ;;
    --build)   BUILD_OSM2ODR=true; shift ;;
    --clean)   REMOVE_INTERMEDIATE=true; shift ;;
    --chrono-path) shift 2 ;;
    --chrono|--carsim|--no-pull) shift ;;
    -h|--help) echo "$DOC_STRING"; echo "$USAGE_STRING"; exit 0 ;;
    --) shift; break ;;
    *) shift ;;
  esac
done

source "$(dirname "$0")/Environment.sh"

if ! { ${REMOVE_INTERMEDIATE} || ${BUILD_OSM2ODR}; }; then
  fatal_error "Nothing selected to be done."
fi

if ${REMOVE_INTERMEDIATE}; then
  log "Cleaning OSM2ODR intermediate build directories."
  rm -rf "${OSM2ODR_BUILD_FOLDER}" "${OSM2ODR_SERVER_BUILD_FOLDER}"
fi

if ${BUILD_OSM2ODR}; then
  log "Building OSM2ODR."

  if [[ ! -d "${OSM2ODR_SOURCE_FOLDER}" ]]; then
    mkdir -p "${CARLA_BUILD_FOLDER}"
    cd "${CARLA_BUILD_FOLDER}" || exit 1
    curl -fL --retry 5 --retry-max-time 120 \
      -o OSM2ODR.zip \
      "https://github.com/carla-simulator/sumo/archive/${CURRENT_OSM2ODR_COMMIT}.zip" || exit 1
    unzip -qq OSM2ODR.zip || exit 1
    rm -f OSM2ODR.zip
    mv "sumo-${CURRENT_OSM2ODR_COMMIT}" "${OSM2ODR_SOURCE_FOLDER}" || exit 1
  fi

  # Host-side Python/client library must not use UE4's Clang 10 against GCC 16.
  log "Configuring OSM2ODR client using system Clang and libstdc++."
  env -u LDFLAGS CC=/usr/bin/clang CXX=/usr/bin/clang++ PATH="/usr/bin:$PATH" \
    cmake -S "${OSM2ODR_SOURCE_FOLDER}" -B "${OSM2ODR_BUILD_FOLDER}" \
      -G "Ninja" \
      -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
      -DCMAKE_C_COMPILER=/usr/bin/clang \
      -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
      -DCMAKE_CXX_FLAGS="-fPIC -std=c++14 -stdlib=libstdc++" \
      -DCMAKE_INSTALL_PREFIX="${LIBCARLA_INSTALL_CLIENT_FOLDER}" \
      -DPROJ_INCLUDE_DIR="${CARLA_BUILD_FOLDER}/proj-install/include" \
      -DPROJ_LIBRARY="${CARLA_BUILD_FOLDER}/proj-install/lib/libproj.a" \
      -DXercesC_INCLUDE_DIR="${CARLA_BUILD_FOLDER}/xerces-c-3.2.3-install/include" \
      -DXercesC_LIBRARY="${CARLA_BUILD_FOLDER}/xerces-c-3.2.3-install/lib/libxerces-c.a" || exit 1

  cmake --build "${OSM2ODR_BUILD_FOLDER}" --target osm2odr || exit 1
  cmake --build "${OSM2ODR_BUILD_FOLDER}" --target install || exit 1

  # Only the server-facing variant uses Unreal's bundled C++ toolchain.
  UE4_CLANG_BIN="${UE4_ROOT}/Engine/Extras/ThirdPartyNotUE/SDKs/HostLinux/Linux_x64/v17_clang-10.0.1-centos7/x86_64-unknown-linux-gnu/bin"
  LLVM_INCLUDE="${UE4_ROOT}/Engine/Source/ThirdParty/Linux/LibCxx/include/c++/v1"
  LLVM_LIBPATH="${UE4_ROOT}/Engine/Source/ThirdParty/Linux/LibCxx/lib/Linux/x86_64-unknown-linux-gnu"
  UE4_SYSROOT="${UE4_CLANG_BIN}/.."

  # Unreal's fixed sysroot doesn't include CachyOS's system zlib headers.
  # CachyOS's zconf.h additionally includes zlib_name_mangling.h. Expose
  # these three zlib headers, not all of /usr/include: that would shadow the
  # fixed sysroot's C headers and break Unreal's libc++.
  OSM2ODR_ZLIB_HEADERS="${CARLA_BUILD_FOLDER}/osm2odr-zlib-headers"
  mkdir -p "${OSM2ODR_ZLIB_HEADERS}" || exit 1
  for zlib_header in zlib.h zconf.h zlib_name_mangling.h; do
    if [[ ! -f "/usr/include/${zlib_header}" ]]; then
      fatal_error "Missing /usr/include/${zlib_header}. Install CachyOS's zlib package."
    fi
    ln -sfn "/usr/include/${zlib_header}" "${OSM2ODR_ZLIB_HEADERS}/${zlib_header}" || exit 1
  done

  # Use the same fixed sysroot as Unreal's other server dependencies.
  # -nostdinc++ prevents Clang from adding another C++ header tree, which
  # can break libc++'s #include_next <string.h> / <stdlib.h> wrappers.
  log "Checking Unreal libc++ headers with UE4 Clang 10."
  printf '#include <cstring>\n#include <string>\n#include <vector>\n#include <zlib.h>\nint main() { return std::strcmp("a", "b") + Z_OK; }\n' | \
    env -u CPATH -u CPLUS_INCLUDE_PATH -u C_INCLUDE_PATH \
      "${UE4_CLANG_BIN}/clang++" -x c++ -std=c++14 -stdlib=libc++ -nostdinc++ \
      -isystem "${LLVM_INCLUDE}" -isystem "${OSM2ODR_ZLIB_HEADERS}" \
      --sysroot="${UE4_SYSROOT}" -fsyntax-only - || exit 1

  log "Configuring OSM2ODR server using UE4 Clang 10 and libc++."
  env -u LDFLAGS -u CPATH -u CPLUS_INCLUDE_PATH -u C_INCLUDE_PATH CC="${UE4_CLANG_BIN}/clang" CXX="${UE4_CLANG_BIN}/clang++" \
    cmake -S "${OSM2ODR_SOURCE_FOLDER}" -B "${OSM2ODR_SERVER_BUILD_FOLDER}" \
      -G "Ninja" \
      -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
      -DCMAKE_POLICY_DEFAULT_CMP0056=NEW \
      -DCMAKE_C_COMPILER="${UE4_CLANG_BIN}/clang" \
      -DCMAKE_CXX_COMPILER="${UE4_CLANG_BIN}/clang++" \
      -DCMAKE_C_FLAGS="--sysroot=${UE4_SYSROOT} -fuse-ld=/usr/bin/ld.lld" \
      -DCMAKE_CXX_FLAGS="-fPIC -std=c++14 -stdlib=libc++ -nostdinc++ -isystem ${LLVM_INCLUDE} -isystem ${OSM2ODR_ZLIB_HEADERS} --sysroot=${UE4_SYSROOT} -fuse-ld=/usr/bin/ld.lld" \
      -DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=/usr/bin/ld.lld -L${LLVM_LIBPATH}" \
      -DCMAKE_SHARED_LINKER_FLAGS="-fuse-ld=/usr/bin/ld.lld -L${LLVM_LIBPATH}" \
      -DCMAKE_INSTALL_PREFIX="${LIBCARLA_INSTALL_SERVER_FOLDER}" \
      -DPROJ_INCLUDE_DIR="${CARLA_BUILD_FOLDER}/proj-install-server/include" \
      -DPROJ_LIBRARY="${CARLA_BUILD_FOLDER}/proj-install-server/lib/libproj.a" \
      -DXercesC_INCLUDE_DIR="${CARLA_BUILD_FOLDER}/xerces-c-3.2.3-install-server/include" \
      -DXercesC_LIBRARY="${CARLA_BUILD_FOLDER}/xerces-c-3.2.3-install-server/lib/libxerces-c.a" || exit 1

  cmake --build "${OSM2ODR_SERVER_BUILD_FOLDER}" --target osm2odr || exit 1
  cmake --build "${OSM2ODR_SERVER_BUILD_FOLDER}" --target install || exit 1
fi

log "OSM2ODR Success!"
