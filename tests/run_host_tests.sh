#!/bin/sh
# Requires a C++11 compiler and curl. Downloads two pinned upstream API headers.
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_deps=${ICCARD_TEST_DEPS:-${TMPDIR:-/tmp}/iccard-host-test-deps}
mkdir -p "$test_deps"

fetch_header() {
  if [ ! -s "$test_deps/$1" ]; then
    curl -fsSL --connect-timeout 10 --max-time 60 "$2" -o "$test_deps/$1.download"
    mv "$test_deps/$1.download" "$test_deps/$1"
  fi
}
fetch_header MFRC522.h https://raw.githubusercontent.com/miguelbalboa/rfid/1.4.12/src/MFRC522.h
fetch_header Preferences.h https://raw.githubusercontent.com/espressif/arduino-esp32/3.3.8/libraries/Preferences/src/Preferences.h

# Some macOS Command Line Tools installations keep libc++ headers in the SDK.
set --
if [ "$(uname -s)" = Darwin ]; then
  cpp_headers="$(xcrun --show-sdk-path)/usr/include/c++/v1"
  if [ -d "$cpp_headers" ]; then set -- -isystem "$cpp_headers"; fi
fi
"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror "$@" \
  "$project_dir/tests/backup_format_test.cpp" -o "$test_deps/format-test"
"$test_deps/format-test"
"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror "$@" \
  -I "$project_dir/tests/host" -I "$test_deps" \
  "$project_dir/tests/workflow_test.cpp" -o "$test_deps/workflow-test"
"$test_deps/workflow-test"
