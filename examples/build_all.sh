#!/usr/bin/env bash
# Build every example with the appropriate standard. C++23/26 examples are
# skipped automatically if your compiler rejects the standard flag.
set -u
CXX="${CXX:-clang++}"
DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$DIR"

WARN="-Wall -Wextra"
PASS=0; FAIL=0; SKIP=0

# Map each example to the minimum standard it needs.
declare -A STD=(
  [ch01_for_each.cpp]=c++20
  [ch02_owned.cpp]=c++20
  [ch03_ratio.cpp]=c++20
  [ch04_deduction_probe.cpp]=c++20
  [ch05_serialize.cpp]=c++20
  [ch06_fold_print.cpp]=c++20
  [ch06_tuple.cpp]=c++20
  [ch07_sfinae.cpp]=c++20
  [ch08_traits.cpp]=c++20
  [ch09_forwarding.cpp]=c++20
  [ch10_crtp.cpp]=c++20
  [ch10_type_erasure.cpp]=c++20
  [ch11_tuple_for_each.cpp]=c++20
  [ch12_constexpr_table.cpp]=c++20
  [ch12_if_constexpr.cpp]=c++20
  [ch13_concepts_print.cpp]=c++20
  [ch14_deducing_this.cpp]=c++23
  [ch15_pack_indexing.cpp]=c++2c
)

std_supported() {
  echo 'int main(){}' | "$CXX" -std="$1" -x c++ - -o /dev/null 2>/dev/null
}

for f in $(printf '%s\n' "${!STD[@]}" | sort); do
  std="${STD[$f]}"
  if ! std_supported "$std"; then
    echo "SKIP  $f (compiler lacks -std=$std)"; SKIP=$((SKIP+1)); continue
  fi
  if "$CXX" -std="$std" $WARN "$f" -o "/tmp/$(basename "$f" .cpp)" 2>/tmp/err.txt; then
    echo "OK    $f (-std=$std)"; PASS=$((PASS+1))
  else
    echo "FAIL  $f (-std=$std)"; sed 's/^/        /' /tmp/err.txt; FAIL=$((FAIL+1))
  fi
done

echo "-----------------------------------------"
echo "passed=$PASS failed=$FAIL skipped=$SKIP"
