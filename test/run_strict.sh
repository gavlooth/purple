#!/bin/bash
# Purple strict test runner (enforces EXPECT:/ERROR: markers)

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

PASS=0
FAIL=0

for test in "$PURPLE_DIR"/test/cases/*.purple; do
  name=$(basename "$test" .purple)
  expected_file="$PURPLE_DIR/test/cases/$name.expected"
  expected_kind=""
  expected=""

  if [ -f "$expected_file" ]; then
    line=$(grep -m 1 -v '^[[:space:]]*$' "$expected_file")
    line="${line%$'\r'}"
    if [[ "$line" == EXPECT:* ]]; then
      expected_kind="expect"
      expected="${line#EXPECT:}"
      expected="${expected#"${expected%%[![:space:]]*}"}"
    elif [[ "$line" == ERROR:* ]]; then
      expected_kind="error"
    else
      # Treat raw expected value (without EXPECT: prefix) as expected output
      expected_kind="expect"
      expected="$line"
    fi
  fi

  # Use purple-run only when explicitly requested in the test file.
  use_runner="hvm4"
  if grep -q "RUNNER:[[:space:]]*purple-run" "$test"; then
    use_runner="purple-run"
  fi

  if [ "$use_runner" = "purple-run" ]; then
    if [ ! -f "$PURPLE_RUN" ]; then
      echo "Building purple-run..."
      (cd "$PURPLE_DIR/src/run" && clang -O2 -o ../../purple-run _.c)
    fi
    output=$("$PURPLE_RUN" "$test" 2>&1)
    status=$?
    if [ "$expected_kind" = "error" ]; then
      if [ $status -ne 0 ]; then
        echo "PASS: $name -> expected error"
        PASS=$((PASS + 1))
      else
        echo "FAIL: $name -> expected error, got success"
        FAIL=$((FAIL + 1))
      fi
      continue
    fi
  else
    rm -f /tmp/purple_test.hvm4 /tmp/purple_test.err
    "$PURPLE" "$test" > /tmp/purple_test.hvm4 2>/tmp/purple_test.err
    if [ $? -ne 0 ]; then
      if [ "$expected_kind" = "error" ]; then
        echo "PASS: $name -> expected error"
        PASS=$((PASS + 1))
      else
        echo "FAIL: $name -> $(cat /tmp/purple_test.err | head -1)"
        FAIL=$((FAIL + 1))
      fi
      continue
    fi
    if [ "$expected_kind" = "error" ]; then
      echo "FAIL: $name -> expected error, got success"
      FAIL=$((FAIL + 1))
      continue
    fi
    output=$("$HVM4" /tmp/purple_test.hvm4 2>&1)
    status=$?
  fi

  if [ $status -ne 0 ]; then
    echo "FAIL: $name -> runtime error"
    FAIL=$((FAIL + 1))
    continue
  fi

  # Use last non-empty line as the result to allow for FFI output.
  result=$(printf "%s\n" "$output" | awk 'NF{line=$0} END{print line}')

  if [ "$expected_kind" = "expect" ]; then
    if [[ "$result" == "$expected"* ]]; then
      echo "PASS: $name -> $result"
      PASS=$((PASS + 1))
    else
      echo "FAIL: $name -> $result (expected: $expected)"
      FAIL=$((FAIL + 1))
    fi
  else
    # Accept: numbers, constructors (#...), strings ("..."), chars ('...'), lists ([...])
    if [[ "$result" =~ ^[0-9]+$ ]] || [[ "$result" =~ ^#[A-Za-z] ]] || [[ "$result" =~ ^\".* ]] || [[ "$result" =~ ^\'.\' ]] || [[ "$result" =~ ^\[.* ]]; then
      echo "PASS: $name -> $result"
      PASS=$((PASS + 1))
    else
      echo "FAIL: $name -> $result"
      FAIL=$((FAIL + 1))
    fi
  fi
done

echo "---"
echo "Passed: $PASS, Failed: $FAIL"
exit $FAIL
