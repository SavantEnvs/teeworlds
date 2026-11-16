#!/usr/bin/env bash
#
# mayhem/test.sh — runs Teeworlds' own gtest suite (built by mayhem/build.sh) and a known-answer probe whose
# output is compared with golden values here, so a neutered binary cannot pass.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
SRC="${SRC:-/mayhem}"
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

for bin in build/tests/testrunner build/tests/kat; do
  [ -x "$bin" ] || { echo "missing $bin: mayhem/build.sh did not produce it" >&2; exit 2; }
done

passed=0
failed=0

# 1) upstream's gtest suite (needs a writable cwd: the storage and file tests create files there)
work="$(mktemp -d)"
out="$(cd "$work" && "$SRC/build/tests/testrunner" 2>&1)"
echo "$out" | tail -n 5
gt_passed="$(echo "$out" | sed -n 's/^\[  PASSED  \] \([0-9][0-9]*\) tests\?\.$/\1/p' | tail -n 1)"
gt_failed="$(echo "$out" | sed -n 's/^\[  FAILED  \] \([0-9][0-9]*\) tests\?, listed below:$/\1/p' | tail -n 1)"
rm -rf "$work"
if [ -n "$gt_passed" ] && [ "$gt_passed" -gt 0 ]; then
  passed=$(( passed + gt_passed ))
else
  failed=$(( failed + 1 ))
  echo "FAIL: gtest suite reported no passing tests" >&2
fi
failed=$(( failed + ${gt_failed:-0} ))

# 2) known-answer probe: fixed inputs through the real codecs and parsers, golden output
kat_out="$(build/tests/kat 2>&1)"
expect() {
  if printf '%s\n' "$kat_out" | grep -qxF -- "$1"; then
    passed=$(( passed + 1 ))
  else
    failed=$(( failed + 1 ))
    echo "FAIL: expected line not found: $1" >&2
  fi
}
expect "varint 1000: a8 0f"
expect "varint -1: 40"
expect "varint block: 00 3f 7f 80 89 0f"
expect "varint back: 16 0 63 -64 123456"
expect "huffman teeworlds: 50 c2 09 9c a0 b8 b5 45 70 72 25 38 91 4e 15 37 00"
expect "huffman back: 9 teeworlds"
expect "packer: 44 ac 04 68 69 00"
expect "unpacker: -5 300 hi err=0"
expect "json: a1=2 b=x d=1 len=3"
expect "console probe: n=4 i=42 f=7.00 s=abc r=hello world"

emit_ctrf "gtest+kat" "$passed" "$failed" 0
