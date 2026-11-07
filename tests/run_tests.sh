#!/bin/sh
# Builds and runs the off-game unit tests (needs only a C++17 compiler).
set -e
cd "$(dirname "$0")/.."
${CXX:-g++} -std=c++17 -Wall -Wextra -Wpedantic -O1 -fsanitize=address,undefined \
    tests/test_buffer_core.cpp -o /tmp/ibuf_tests
/tmp/ibuf_tests
python3 tools/check_mod_json.py
