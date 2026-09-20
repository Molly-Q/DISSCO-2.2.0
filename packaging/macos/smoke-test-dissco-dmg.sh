#!/usr/bin/env bash
# Launch LASSIE from a packaged DMG to catch bundles that macOS refuses to run.

set -euo pipefail

if [[ $# -ne 1 || ! -f "$1" ]]; then
    echo "Usage: $0 /path/to/DISSCO-<version>-Darwin.dmg" >&2
    exit 2
fi

dmg_path="$1"
startup_seconds="${DISSCO_SMOKE_TEST_SECONDS:-10}"
temp_root="${TMPDIR:-/tmp}"
mount_dir="$(mktemp -d "${temp_root%/}/dissco-dmg.XXXXXX")"
work_dir="$(mktemp -d "${temp_root%/}/dissco-smoke.XXXXXX")"
mounted=0
lassie_pid=""
cleanup() {
    if [[ -n "${lassie_pid}" ]]; then
        kill "${lassie_pid}" >/dev/null 2>&1 || true
        wait "${lassie_pid}" >/dev/null 2>&1 || true
    fi
    if [[ ${mounted} -eq 1 ]]; then
        /usr/bin/hdiutil detach "${mount_dir}" >/dev/null 2>&1 ||
            /usr/bin/hdiutil detach -force "${mount_dir}" >/dev/null 2>&1 ||
            true
    fi
    rmdir "${mount_dir}" >/dev/null 2>&1 || true
    rm -rf "${work_dir}"
}
trap cleanup EXIT

/usr/bin/hdiutil attach \
    -readonly \
    -nobrowse \
    -mountpoint "${mount_dir}" \
    "${dmg_path}" >/dev/null
mounted=1

shopt -s nullglob
apps=("${mount_dir}"/*.app)
if [[ ${#apps[@]} -ne 1 ]]; then
    echo "Expected exactly one app in ${dmg_path}; found ${#apps[@]}." >&2
    exit 1
fi
app_path="${apps[0]}"
lassie="${app_path}/Contents/MacOS/lassie"
cmod="${app_path}/Contents/MacOS/cmod"

# The DMG is built from CPack's staging copy of the app, so check the copy
# users receive rather than trusting the bundle the fixup script validated.
/usr/bin/codesign --verify --deep --strict --verbose=2 "${app_path}"

env -i HOME="${HOME:-/tmp}" PATH="/usr/bin:/bin" \
    "${cmod}" --help | grep -Fq "Usage:"

# An invalid signature (SIGKILL, exit 137) or an unresolved library ends the
# process within moments of exec; a healthy LASSIE stays in its event loop.
cd "${work_dir}"
"${lassie}" &
lassie_pid=$!
for ((elapsed = 0; elapsed < startup_seconds; ++elapsed)); do
    sleep 1
    if ! kill -0 "${lassie_pid}" >/dev/null 2>&1; then
        set +e
        wait "${lassie_pid}"
        lassie_result=$?
        set -e
        lassie_pid=""
        echo "LASSIE exited during startup with status ${lassie_result}." >&2
        exit 1
    fi
done

echo "LASSIE from ${dmg_path} stayed running for ${startup_seconds} seconds."
