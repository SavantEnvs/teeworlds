#!/usr/bin/env bash
#
# mayhem/build.sh — builds Teeworlds' fuzz harnesses (sanitized, fuzz + standalone) and, separately, its
# gtest suite and a known-answer probe with the project's normal flags. Runs offline, re-runnable.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${STANDALONE_FUZZ_MAIN:=/opt/mayhem/StandaloneFuzzTargetMain.c}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

SRC="${SRC:-/mayhem}"
cd "$SRC"

TARGETS="fuzz_huffman fuzz_varint fuzz_unpacker fuzz_json fuzz_datafile fuzz_map fuzz_snapshot fuzz_console"

# 1) The project's shared libraries (engine-shared, game-shared, json object libraries; md5 is only linked
#    upstream when OpenSSL is missing), sanitized
#    and instrumented for coverage. The client is not built: the harnesses only need the shared code.
#    Build type "Fuzz" is unknown to upstream's CMakeLists, so it adds no -DNDEBUG and no flags of its own.
# The compiler comes from the exported CC/CXX: passing -DCMAKE_C_COMPILER=clang on a re-run makes CMake see a
# "changed" compiler (cached path is /usr/bin/clang), wipe the cache and drop -DCLIENT=OFF.
# Relaxed UBSan checks (both benign by design upstream): pointer-overflow for C (json-parser sizing pass does NULL+offset
# arithmetic, json.c:408), and signed-integer-overflow for src/engine/shared/snapshot.cpp ONLY (snapshot delta
# coding wraps ints, snapshot.cpp:159): that one file is recompiled below; every other C++ file keeps the check.
cmake -S . -B build/fuzz -G Ninja \
  -DCMAKE_BUILD_TYPE=Fuzz \
  -DCMAKE_C_FLAGS="-O1 $SANITIZER_FLAGS $DEBUG_FLAGS -fno-sanitize=pointer-overflow -fsanitize=fuzzer-no-link" \
  -DCMAKE_CXX_FLAGS="-O1 $SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link" \
  -DCLIENT=OFF -DDOWNLOAD_GTEST=OFF -DDOWNLOAD_DEPENDENCIES=OFF -DPREFER_BUNDLED_LIBS=OFF
cmake --build build/fuzz -j"$MAYHEM_JOBS" --target engine-shared game-shared json

# snapshot.cpp again, with CMake's own compile line for it plus -fno-sanitize=signed-integer-overflow, into a
# separate object that replaces CMake's in the harness link.
SNAP_SRC="$SRC/src/engine/shared/snapshot.cpp"
SNAP_OBJ=build/fuzz/snapshot_nosio.o
SNAP_CMD=$(ninja -C build/fuzz -t commands engine-shared | grep -F -- "-c $SNAP_SRC" | tail -n1)
[ -n "$SNAP_CMD" ] || { echo "build.sh: no compile line for $SNAP_SRC in build/fuzz" >&2; exit 1; }
SNAP_CMD=$(printf '%s' "$SNAP_CMD" | sed -E 's# -MD -MT [^ ]+ -MF [^ ]+##; s# -o [^ ]+ -c # -fno-sanitize=signed-integer-overflow -o '"$SRC/$SNAP_OBJ"' -c #')
(cd build/fuzz && eval "$SNAP_CMD")

LIB_OBJS=$(find build/fuzz/CMakeFiles/engine-shared.dir build/fuzz/CMakeFiles/game-shared.dir \
  build/fuzz/CMakeFiles/json.dir -name '*.o' ! -name 'snapshot.cpp.o' | sort; echo "$SNAP_OBJ")
INCLUDES="-I$SRC/src -I$SRC/build/fuzz/src -I$SRC/mayhem/harness -DCONF_OPENSSL"
LIBS="-lz -lcrypto -lpthread"

# 2) Harnesses: libFuzzer binary and a -standalone run-once reproducer for each. LeakSanitizer is turned
#    off at build time by lsan_off.cc (leaks are not what this fleet looks for).
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o build/fuzz/lsan_off.o
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o build/fuzz/standalone_main.o
for t in $TARGETS; do
  $CXX -std=c++11 -O1 $SANITIZER_FLAGS $DEBUG_FLAGS $INCLUDES -c "$SRC/mayhem/harness/$t.cpp" -o "build/fuzz/$t.o"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "build/fuzz/$t.o" build/fuzz/lsan_off.o $LIB_OBJS $LIBS -o "$SRC/$t"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS "build/fuzz/$t.o" build/fuzz/lsan_off.o build/fuzz/standalone_main.o $LIB_OBJS $LIBS -o "$SRC/$t-standalone"
done

# 3) The test suite and the known-answer probe with the project's NORMAL flags (clean, separate build),
#    left in build/tests for mayhem/test.sh.
cmake -S . -B build/tests -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_FLAGS="$COVERAGE_FLAGS" -DCMAKE_CXX_FLAGS="$COVERAGE_FLAGS" \
  -DCLIENT=OFF -DDOWNLOAD_GTEST=OFF -DDOWNLOAD_DEPENDENCIES=OFF -DPREFER_BUNDLED_LIBS=OFF
cmake --build build/tests -j"$MAYHEM_JOBS" --target testrunner

TEST_OBJS=$(find build/tests/CMakeFiles/engine-shared.dir build/tests/CMakeFiles/game-shared.dir \
  build/tests/CMakeFiles/json.dir -name '*.o' | sort)
$CXX -std=c++11 -O2 $COVERAGE_FLAGS -I"$SRC/src" -I"$SRC/build/tests/src" -DCONF_OPENSSL \
  "$SRC/mayhem/harness/kat.cpp" $TEST_OBJS $LIBS -o build/tests/kat
