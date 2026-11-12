#!/bin/bash
# Builds the synthetic-boundary ownership harness against a source tree with
# ASan/UBSan and runs every test in its own process.
#
#   tests/ownership/run.sh <repo>/ultralight <fixed|baseline>
#
# <source-root> provides the plugin sources and the bundled 4D Plugin API.
# The harness itself always comes from this script's directory, so the SAME
# harness can be run against an immutable baseline (git archive of the base
# SHA) and against the fixed tree.
#
# In "fixed" mode every test must pass and no sanitizer report is allowed.
# In "baseline" mode the tests named in EXPECTED_BASELINE_FAILURES must fail
# (a sanitizer report counts as that failure) and every other test (the
# controls) must pass cleanly; anything else is a harness or model error.
set -euo pipefail

SRC=$(cd "$1" && pwd)
MODE=$2
HERE=$(cd "$(dirname "$0")" && pwd)
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT

EXPECTED_BASELINE_FAILURES="
defect_live_load_html_view_param_read_as_object
hardening_live_evaluate_script_result_not_nul_terminated
defect_dormant_ob_get_a_s_result_not_cleared
defect_dormant_ob_get_n_b_wrong_kind_not_cleared
defect_dormant_ob_get_o_c_reference_not_released
defect_dormant_ob_set_empty_owner_leaks
hardening_ob_get_error_result_untouched
hardening_ob_stringify_error_untouched
"

SAN="-g -O1 -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=undefined"
# Same header search paths as the Xcode project. Nothing is rendered or
# fetched: ultralight_stubs.cpp replaces Ultralight/WebCore and libpng (the
# bundled a/libpng16.a is not built for arm64 and is not needed).
INC=(-I "$SRC/4D Plugin API" -I "$SRC/4D Plugin API/Classes" -I "$SRC/support" -I "$SRC" -I "$SRC/include" -I "$SRC/include/libpng16" -I "$HERE")
WARN="-w"

objs=()
cc() { # <lang> <file> [extra flags...]
    local lang=$1 file=$2; shift 2
    local o="$OUT/$(basename "$file").o"
    xcrun clang $SAN $WARN -x "$lang" "${INC[@]}" "$@" -c "$file" -o "$o"
    objs+=("$o")
}

cc c "$SRC/4D Plugin API/4DPluginAPI.c" -Wno-multichar -Wno-error=int-conversion
for f in "$SRC/4D Plugin API/Classes/"*.cpp; do
    cc objective-c++ "$f" -std=c++17 -fno-objc-arc
done
cc objective-c++ "$SRC/support/4DPlugin-JSON.cpp" -std=c++17 -fno-objc-arc
cc objective-c++ "$SRC/IFileSystem.cpp" -std=c++17 -fno-objc-arc
cc objective-c++ "$HERE/ultralight_stubs.cpp" -std=c++17 -fno-objc-arc
# engine_harness.cpp #includes 4DPlugin-Ultralight.cpp from $SRC.
cc objective-c++ "$HERE/engine_harness.cpp" -std=c++17 -fno-objc-arc

xcrun clang++ $SAN "${objs[@]}" -framework Foundation -framework CoreFoundation -framework CoreServices -framework CoreGraphics -o "$OUT/harness"

export ASAN_OPTIONS=abort_on_error=0:halt_on_error=1:detect_leaks=0
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1

bad=0
for t in $("$OUT/harness" --list); do
    if "$OUT/harness" "$t" >"$OUT/$t.log" 2>&1; then got=pass; else got=fail; fi
    expect=pass
    if [ "$MODE" = baseline ] && grep -qx "$t" <<<"$EXPECTED_BASELINE_FAILURES"; then expect=fail; fi
    if [ "$got" = "$expect" ]; then status=OK; else status=UNEXPECTED; bad=1; fi
    printf '%-12s %-5s (expected %-4s) %s\n' "$status" "$got" "$expect" "$t"
    if [ "$got" = fail ]; then sed 's/^/    | /' "$OUT/$t.log" | head -40; fi
    # A harness error is never expected; a sanitizer report is only an
    # acceptable way for an expected baseline failure to fail.
    if grep -q 'HARNESS ERROR' "$OUT/$t.log"; then
        echo "UNEXPECTED   harness error in $t"; bad=1
    elif grep -qE 'ERROR: AddressSanitizer|runtime error:' "$OUT/$t.log" && [ "$expect" = pass ]; then
        echo "UNEXPECTED   sanitizer report in $t"; bad=1
    fi
done

if [ $bad -ne 0 ]; then echo "RESULT ($MODE): FAILED"; exit 1; fi
echo "RESULT ($MODE): all outcomes as expected"
