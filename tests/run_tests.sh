#!/usr/bin/env bash

cd "$(dirname "$0")/.."

GOON="./goon"
PASS=0
FAIL=0

red='\033[0;31m'
green='\033[0;32m'
reset='\033[0m'

for test in tests/valid/*.goon; do
    name=$(basename "$test" .goon)
    expected="tests/valid/${name}.expected"

    if [ ! -f "$expected" ]; then
        echo -e "${red}SKIP${reset} $name (no .expected file)"
        continue
    fi

    output=$("$GOON" eval "$test" 2>&1)
    expected_content=$(cat "$expected")

    if [ "$output" = "$expected_content" ]; then
        echo -e "${green}PASS${reset} $name"
        ((PASS++))
    else
        echo -e "${red}FAIL${reset} $name"
        echo "  expected: $expected_content"
        echo "  got:      $output"
        ((FAIL++))
    fi
done

for test in tests/invalid/*.goon; do
    name=$(basename "$test" .goon)
    expected="tests/invalid/${name}.expected"

    if [ ! -f "$expected" ]; then
        echo -e "${red}SKIP${reset} $name (no .expected file)"
        continue
    fi

    output=$("$GOON" check "$test" 2>&1)
    exit_code=$?
    expected_content=$(cat "$expected")

    if [ $exit_code -eq 0 ]; then
        echo -e "${red}FAIL${reset} $name (should have failed)"
        ((FAIL++))
    elif echo "$output" | grep -q "$expected_content"; then
        echo -e "${green}PASS${reset} $name"
        ((PASS++))
    else
        echo -e "${red}FAIL${reset} $name"
        echo "  expected error containing: $expected_content"
        echo "  got: $output"
        ((FAIL++))
    fi
done

echo ""
echo "Results: $PASS passed, $FAIL failed"

if [ $FAIL -gt 0 ]; then
    exit 1
fi
