# Purple Language - Known Issues

This document tracks known bugs and issues in the Purple codebase.

---

## Critical Issues

### 1. Missing Compiler Cases for Parsed Features

**File:** `src/compile/_.c`

19 features are parsed and have runtime support but lack compiler emission handlers:

| Feature | Parser | Compiler | Runtime |
|---------|--------|----------|---------|
| Quasiquote (`QQ`) | Line 173 | Missing | Line 1201 |
| Unquote (`UQ`) | Line 174 | Missing | Line 1205 |
| Unquote-splicing (`UQS`) | Line 175 | Missing | Line 1208 |
| Map | Line 76 | Missing | Line 1210 |
| Filter | Line 77 | Missing | Line 1214 |
| Fold | Line 78 | Missing | Line 1218 |
| Foldl | Line 79 | Missing | Line 1222 |
| Length | Line 80 | Missing | Line 1228 |
| Append | Line 81 | Missing | Line 1231 |
| Reverse | Line 82 | Missing | Line 1234 |
| Compose | Line 85 | Missing | Line 1239 |
| Flip | Line 86 | Missing | Line 1243 |
| Apply | Line 87 | Missing | Line 1246 |
| Defmacro | Line 90 | Missing | None |
| Macroexpand | Line 91 | Missing | None |

**Impact:** Programs using these features compile to invalid HVM4 code.

**Fix:** Add cases to `purple_compile_emit_term()` switch statement for each missing feature.

---

### 2. Nick Encoding Collision Risk

**File:** `lib/runtime.hvm4`

The 4-character nick encoding limits symbols to ~16.7M unique values (64^4). Abbreviations may collide with user-defined symbols.

Examples of current abbreviations:
- `"FdLf"` for foldl
- `"Fltr"` for filter
- `"Apnd"` for append

**Impact:** Symbol collisions can cause incorrect program behavior.

**Fix:** Consider longer encoding or namespace separation for built-ins.

---

## Medium Issues

### 3. Handler Table Limited to 9 Entries

**File:** `lib/runtime.hvm4:427-437`

Only 9 handlers are registered in `@purple_default_handlers`:
```
0: lit, 1: var, 2: lam, 3: app, 4: if, 5: lft, 6: run, 7: em, 8: clam
```

Missing handlers for: add, sub, mul, div, mod, and, or, not, lt, gt, le, ge, eql, map, filter, fold, etc.

**Impact:** Cannot customize arithmetic/logic/list operations via `set-meta!`.

**Fix:** Extend handler table or use dynamic dispatch for additional handlers.

---

### 4. Off-by-One in List Pattern NIL Constructor

**File:** `src/parse/_.c:1293-1330`

```c
result = purple_term_pctr(term_new_num(166118), purple_term_nil()); // NIL pattern
```

NIL is a 0-arity constructor but pattern is created with an argument list.

**Impact:** List patterns like `(list a b)` may fail to match empty list terminators.

**Fix:** Create NIL pattern without arguments: `purple_term_pctr(term_new_num(166118), purple_term_nil())` should be just the tag with empty args.

---

### 5. Include Directive Bounds Check Off-by-One

**File:** `src/parse/_.c:2078-2086`

```c
if (pos + 10 < len &&
    src[pos] == '(' &&
    strncmp(src + pos + 1, "include", 7) == 0 &&
    (src[pos + 8] == ' ' || src[pos + 8] == '\t' || src[pos + 8] == '\n'))
```

Uses `pos + 10 < len` but only needs to access `src[pos + 8]`.

**Fix:** Change to `pos + 9 <= len` or `pos + 8 < len`.

---

### 6. Fixed Array Bounds with Hard Exit

**File:** `src/parse/_.c`

| Location | Limit | Array |
|----------|-------|-------|
| Line 1270 | 16 | or-pattern alternatives |
| Line 1295 | 16 | list pattern elements |
| Line 1366 | 16 | pattern arguments |
| Line 1505 | 64 | match cases |
| Line 1562 | 64 | FFI arguments |
| Line 716 | 256 | lambda parameters |

**Impact:** Exceeding limits causes `exit(1)` with no recovery.

**Fix:** Use dynamic allocation or increase limits with proper error messages.

---

## Minor Issues

### 7. Compiler Uses Wrong Name Prefix for Patterns

**File:** `src/compile/_.c:890-902`

```c
if (nam == PURPLEC_NAM_POR && ari == 1) {  // Should be PURPLE_NAM_POR
```

Uses `PURPLEC_NAM_*` instead of `PURPLE_NAM_*` for pattern constructors.

**Fix:** Use consistent `PURPLE_NAM_*` naming from parser.

---

### 8. Or-Pattern Only Binds First Alternative's Variables

**File:** `src/parse/_.c:1433-1438`

```c
case PURPLE_NAM_POR:
  // Only processes first alternative
  purple_bind_pattern_vars(term_get_arg(pattern, 0));
  break;
```

**Impact:** If or-pattern alternatives have different variable sets, binding is incomplete.

**Fix:** Validate all alternatives bind same variables, or collect union of bindings.

---

### 9. Test Coverage Gaps

**Directory:** `test/cases/`

- 64 of 134 test files lack `.expected` output verification
- No tests for macro system (`defmacro`, `macroexpand`)
- No tests for or-patterns with divergent bindings
- Limited edge case testing for list operations

**Fix:** Add expected files and expand test coverage.

---

### 10. Symbol Term Uses Table Index Instead of Nick

**File:** `src/parse/_.c:677-685`

```c
fn Term purple_symbol_term(PState *s, u32 start, u32 len) {
  u32 sym_id = table_find(s->src + start, len);  // Returns table index
  ...
  return purple_term_sym(sym_id);  // But purple_term_sym expects nick-encoded value
}
```

Comment says "nick-encoded ID" but `table_find` returns a table index.

**Impact:** Unbound symbols get table indices instead of nick values, causing runtime mismatches.

**Fix:** Use nick encoding for unbound symbols: compute nick from source string.

---

## Summary

| Severity | Count | Status |
|----------|-------|--------|
| Critical | 2 | Open |
| Medium | 4 | Open |
| Minor | 4 | Open |

---

*Last updated: 2024-12-30*
