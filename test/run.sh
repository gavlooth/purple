#!/bin/bash
# Purple test runner

PURPLE_DIR="$(dirname "$0")/.."
PURPLE="$PURPLE_DIR/src/main"
HVM4="${HVM4:-$PURPLE_DIR/hvm4/clang/main}"

# Build if needed
if [ ! -f "$PURPLE" ]; then
  echo "Building Purple compiler..."
  clang -O2 -o "$PURPLE" "$PURPLE_DIR/src/main.c"
fi

if [ ! -f "$HVM4" ]; then
  echo "Building HVM4..."
  (cd "$PURPLE_DIR/hvm4/clang" && clang -O2 -o main main.c)
fi

PASS=0
FAIL=0

for test in "$PURPLE_DIR"/test/cases/*.purple; do
  name=$(basename "$test" .purple)
  rm -f /tmp/purple_test.hvm4
  "$PURPLE" "$test" > /tmp/purple_test.hvm4 2>&1
  if [ $? -ne 0 ]; then
    echo "FAIL: $name (compile error)"
    FAIL=$((FAIL + 1))
    continue
  fi

  result=$("$HVM4" /tmp/purple_test.hvm4 -s 2>&1 | head -1)
  if [[ "$result" =~ ^[0-9]+$ ]] || [[ "$result" =~ ^#[A-Za-z] ]]; then
    echo "PASS: $name -> $result"
    PASS=$((PASS + 1))
  else
    echo "FAIL: $name -> $result"
    FAIL=$((FAIL + 1))
  fi
done

echo "---"
echo "Passed: $PASS, Failed: $FAIL"
exit $FAIL
