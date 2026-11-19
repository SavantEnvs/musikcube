#!/usr/bin/env bash
# mayhem/test.sh — known-answer tests for the musikcube plugins built by mayhem/build.sh
# (normal flags, no sanitizers): each self-test drives a plugin through its exported SDK
# entry point on a committed fixture and its output is diffed against a golden file.
#   tags_mp3   taglib_plugin  GetTagReader()->Read()        on testdata/tags.mp3 (ID3v2.3 tags)
#   tags_flac  taglib_plugin  GetTagReader()->Read()        on testdata/tags.flac (Xiph comments, replaygain)
#   decode_wav ffmpegdecoder  GetDecoderFactory() decoder    on testdata/tone.wav (stereo PCM)
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SRC:=/mayhem}"
cd "$SRC"

emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

OUT="$SRC/build/mayhem-out"
TD="$SRC/mayhem/testdata"
for b in selftest_tags selftest_decode; do
  [ -x "$OUT/$b" ] || { echo "missing $OUT/$b — run mayhem/build.sh first" >&2; emit_ctrf musikcube-selftest 0 1; exit 1; }
done

passed=0 failed=0
run_case() {  # run_case <name> <expected-file> <cmd...>
  local name="$1" expected="$2"; shift 2
  local got
  got="$("$@" 2>/dev/null)"
  if diff -u "$expected" <(printf '%s\n' "$got"); then
    echo "PASS $name"; passed=$((passed + 1))
  else
    echo "FAIL $name"; failed=$((failed + 1))
  fi
}

run_case tags_mp3   "$TD/tags.mp3.expected" "$OUT/selftest_tags"   "$TD/tags.mp3"
run_case tags_flac  "$TD/tags.flac.expected" "$OUT/selftest_tags"   "$TD/tags.flac"
run_case decode_wav "$TD/tone.wav.expected" "$OUT/selftest_decode" "$TD/tone.wav"

emit_ctrf musikcube-selftest "$passed" "$failed"
