#!/bin/bash
# Purple test runner

PURPLE_DIR="$(dirname "$0")/.."
PURPLE="$PURPLE_DIR/purple"
PURPLE_RUN="$PURPLE_DIR/purple-run"
HVM4="${HVM4:-$PURPLE_DIR/hvm4/clang/main}"

# Build if needed
if [ ! -f "$PURPLE" ]; then
  echo "Building Purple compiler..."
  (cd "$PURPLE_DIR/src" && clang -O2 -o ../purple main.c)
fi

if [ ! -f "$HVM4" ]; then
  echo "Building HVM4..."
  (cd "$PURPLE_DIR/hvm4/clang" && clang -O2 -o main main.c)
fi

if [ ! -f "$PURPLE_RUN" ]; then
  echo "Building Purple runner..."
  (cd "$PURPLE_DIR/src/run" && clang -O2 -o ../../purple-run _.c)
fi

PASS=0
FAIL=0

for test in "$PURPLE_DIR"/test/cases/*.purple; do
  name=$(basename "$test" .purple)
  expected_file="$PURPLE_DIR/test/cases/$name.expected"

  # Check if test requires purple-run (for FFI tests)
  use_purple_run=0
  if grep -q '^; RUNNER: purple-run' "$test" 2>/dev/null; then
    use_purple_run=1
  fi

  if [ -f "$expected_file" ]; then
    expected_line=$(grep -m 1 -v '^[[:space:]]*$' "$expected_file")
    expected_line="${expected_line%$'\r'}"
    if [[ "$expected_line" == ERROR:* ]]; then
      "$PURPLE" "$test" > /tmp/purple_test.hvm4 2>/tmp/purple_test.err
      if [ $? -ne 0 ]; then
        echo "PASS: $name -> expected error"
        PASS=$((PASS + 1))
      else
        echo "FAIL: $name -> expected error, got success"
        FAIL=$((FAIL + 1))
      fi
      continue
    fi
  fi

  # Run with appropriate runner
  if [ $use_purple_run -eq 1 ]; then
    result=$("$PURPLE_RUN" "$test" 2>&1 | tail -1)
  else
    rm -f /tmp/purple_test.hvm4
    "$PURPLE" "$test" > /tmp/purple_test.hvm4 2>/tmp/purple_test.err
    if [ $? -ne 0 ]; then
      echo "FAIL: $name -> $(cat /tmp/purple_test.err | head -1)"
      FAIL=$((FAIL + 1))
      continue
    fi
    result=$("$HVM4" /tmp/purple_test.hvm4 -s 2>&1 | head -1)
  fi
  # Accept: numbers, constructors (#...), strings ("..."), chars ('...'), lists ([...])
  if [[ "$result" =~ ^[0-9]+$ ]] || [[ "$result" =~ ^#[A-Za-z] ]] || [[ "$result" =~ ^\".* ]] || [[ "$result" =~ ^\'.\' ]] || [[ "$result" =~ ^\[.* ]]; then
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
