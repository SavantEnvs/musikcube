#!/usr/bin/env bash
# mayhem/build.sh — builds musikcube's plugin fuzz harnesses (+ standalone reproducers) and the
# functional self-tests. The plugins are dlopen'd shared libraries upstream; here each plugin's
# sources are compiled straight into its harness (sanitized), and the harness drives the plugin
# through its exported extern "C" getters (GetTagReader / GetDecoderFactory / SetDebug).
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
: "${STANDALONE_FUZZ_MAIN:?STANDALONE_FUZZ_MAIN must be set (provided by the base image)}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"
OUT="$SRC/build/mayhem-out"     # build/ is gitignored upstream → removed by git clean -ffdX
OBJ="$SRC/build/mayhem-obj"
mkdir -p "$OUT" "$OBJ/fuzz" "$OBJ/test"

P="$SRC/src/plugins"
H="$SRC/mayhem/harnesses"
INC=(-I"$SRC/src" -I"$SRC/src/musikcore" -I"$SRC/src/3rdparty/include" -I"$H")
CXXSTD=(-std=c++17 -fPIC)

FFMPEG_LIBS=(-lavformat -lavcodec -lavutil -lswresample)

TAGLIB_SRCS=("$P/taglib_plugin/taglib_plugin.cpp" "$P/taglib_plugin/TaglibMetadataReader.cpp")
FFMPEG_SRCS=("$P/ffmpegdecoder/plugin.cpp" "$P/ffmpegdecoder/FfmpegDecoder.cpp")
OPENMPT_SRCS=("$P/libopenmptdecoder/plugin.cpp" "$P/libopenmptdecoder/OpenMptDecoder.cpp"
              "$P/libopenmptdecoder/OpenMptDataStream.cpp" "$P/libopenmptdecoder/OpenMptIndexerSource.cpp"
              "$P/libopenmptdecoder/Utility.cpp")

# compile <objdir> <flags...> -- <sources...>  → prints the object paths
compile() {
  local dir="$1"; shift
  local flags=()
  while [ "$1" != "--" ]; do flags+=("$1"); shift; done; shift
  local objs=()
  for s in "$@"; do
    local o="$dir/$(echo "${s#$SRC/}" | tr '/' '_').o"
    "$CXX" "${CXXSTD[@]}" "${INC[@]}" "${flags[@]}" -c "$s" -o "$o"
    objs+=("$o")
  done
  echo "${objs[@]}"
}

# 1) sanitized + instrumented plugin code and harnesses
FZ=($SANITIZER_FLAGS $DEBUG_FLAGS -O1 -fsanitize=fuzzer-no-link)
"$CC" $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$OBJ/fuzz/standalone_main.o"
"$CXX" $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o "$OBJ/fuzz/lsan_off.o"
COMMON_FUZZ=("$OBJ/fuzz/lsan_off.o")

link_target() {  # link_target <name> <objs...> -- <libs...>
  local name="$1"; shift
  local objs=() libs=()
  while [ "$1" != "--" ]; do objs+=("$1"); shift; done; shift
  libs=("$@")
  "$CXX" $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "${objs[@]}" "${COMMON_FUZZ[@]}" "${libs[@]}" -lz -lpthread -lm -o "$OUT/$name"
  "$CXX" $SANITIZER_FLAGS $DEBUG_FLAGS "$OBJ/fuzz/standalone_main.o" "${objs[@]}" "${COMMON_FUZZ[@]}" "${libs[@]}" -lz -lpthread -lm -o "$OUT/$name-standalone"
}

read -r -a o_tag <<< "$(compile "$OBJ/fuzz" "${FZ[@]}" -- "${TAGLIB_SRCS[@]}" "$H/fuzz_taglib.cpp")"
link_target fuzz_taglib "${o_tag[@]}" -- -ltag

read -r -a o_ff <<< "$(compile "$OBJ/fuzz" "${FZ[@]}" -- "${FFMPEG_SRCS[@]}" "$H/fuzz_ffmpeg_decoder.cpp")"
link_target fuzz_ffmpeg_decoder "${o_ff[@]}" -- "${FFMPEG_LIBS[@]}"

read -r -a o_mpt <<< "$(compile "$OBJ/fuzz" "${FZ[@]}" -- "${OPENMPT_SRCS[@]}" "$H/fuzz_openmpt_decoder.cpp")"
link_target fuzz_openmpt_decoder "${o_mpt[@]}" -- -lopenmpt

GME_SRCS=("$P/gmedecoder/plugin.cpp" "$P/gmedecoder/GmeDecoder.cpp" "$P/gmedecoder/GmeDataStream.cpp"
          "$P/gmedecoder/GmeIndexerSource.cpp")
# libgme: pinned static release build baked into the image (mayhem/Dockerfile), not Debian's .so
GME_DIR=/opt/toolchains/deps/libgme-0.6.4
[ -f "$GME_DIR/lib/libgme.a" ] || { echo "missing $GME_DIR (built by mayhem/Dockerfile)" >&2; exit 1; }
read -r -a o_gme <<< "$(compile "$OBJ/fuzz" -I"$GME_DIR/include" "${FZ[@]}" -- "${GME_SRCS[@]}" "$H/fuzz_gme_decoder.cpp")"
link_target fuzz_gme_decoder "${o_gme[@]}" -- "$GME_DIR/lib/libgme.a"

# libmusikcore (QueryRegistry target): the exact source list upstream's src/musikcore/CMakeLists.txt
# compiles into the shared library, built here as a sanitized static archive (same defines as the
# root CMakeLists: standalone ASIO for websocketpp, plus MCSDK_DEFINE_EXPORTS).
CORE="$SRC/src/musikcore"
mapfile -t CORE_SRCS < <(sed -n '/^set(CORE_SOURCES/,/^)/p' "$CORE/CMakeLists.txt" | grep -E '\.(c|cpp)$' | sed 's/^ *//')
[ "${#CORE_SRCS[@]}" -gt 50 ] || { echo "could not parse CORE_SOURCES from $CORE/CMakeLists.txt" >&2; exit 1; }
CORE_DEFS=(-DBOOST_DATE_TIME_NO_LIB -DBOOST_REGEX_NO_LIB -D_WEBSOCKETPP_CPP11_TYPE_TRAITS_
           -D_WEBSOCKETPP_CPP11_RANDOM_DEVICE_ -DASIO_STANDALONE -DMCSDK_DEFINE_EXPORTS)
CORE_INC=(-I"$CORE" -I"$SRC/src" -I"$SRC/src/3rdparty/include" -I"$SRC/src/3rdparty/include/md5"
          -I"$SRC/src/3rdparty/include/sqlite")
mkdir -p "$OBJ/core"
export OBJ CORE
export CORE_CFLAGS="$SANITIZER_FLAGS $DEBUG_FLAGS -O1 -fsanitize=fuzzer-no-link -fPIC ${CORE_DEFS[*]} ${CORE_INC[*]}"
printf '%s\n' "${CORE_SRCS[@]}" | xargs -P "$MAYHEM_JOBS" -I{} bash -c '
  set -e; s="$1"; o="$OBJ/core/$(echo "$s" | tr "/." "__").o"
  case "$s" in
    *.c) "$CC" $CORE_CFLAGS -c "$CORE/$s" -o "$o" ;;
    *)   "$CXX" -std=c++17 -Wno-deprecated-declarations $CORE_CFLAGS -c "$CORE/$s" -o "$o" ;;
  esac' _ {}
rm -f "$OBJ/libmusikcore.a"
ar rcs "$OBJ/libmusikcore.a" "$OBJ"/core/*.o
"$CXX" -std=c++17 "${CORE_DEFS[@]}" "${CORE_INC[@]}" "${FZ[@]}" -c "$H/fuzz_query_registry.cpp" -o "$OBJ/fuzz/fuzz_query_registry.o"
link_target fuzz_query_registry "$OBJ/fuzz/fuzz_query_registry.o" "$OBJ/libmusikcore.a" -- -lcurl -lssl -lcrypto -ldl

# 2) functional self-tests with NORMAL flags (no sanitizers), run by mayhem/test.sh
TF=(-O2 $COVERAGE_FLAGS)
read -r -a t_tag <<< "$(compile "$OBJ/test" "${TF[@]}" -- "${TAGLIB_SRCS[@]}" "$H/selftest_tags.cpp")"
"$CXX" $COVERAGE_FLAGS "${t_tag[@]}" -ltag -lz -o "$OUT/selftest_tags"
read -r -a t_dec <<< "$(compile "$OBJ/test" "${TF[@]}" -- "${FFMPEG_SRCS[@]}" "$H/selftest_decode.cpp")"
"$CXX" $COVERAGE_FLAGS "${t_dec[@]}" "${FFMPEG_LIBS[@]}" -o "$OUT/selftest_decode"

ls -l "$OUT"
