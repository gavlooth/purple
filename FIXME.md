# Purple Language - Known Issues

This document tracks known bugs and issues in the Purple codebase.

---

## Critical Issues

### 1. Nick Encoding Collision Risk

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

### 2. Handler Table Limited to 9 Entries

**File:** `lib/runtime.hvm4:427-437`

Only 9 handlers are registered in `@purple_default_handlers`:
```
0: lit, 1: var, 2: lam, 3: app, 4: if, 5: lft, 6: run, 7: em, 8: clam
```

Missing handlers for: add, sub, mul, div, mod, and, or, not, lt, gt, le, ge, eql, map, filter, fold, etc.

**Impact:** Cannot customize arithmetic/logic/list operations via `set-meta!`.

**Fix:** Extend handler table or use dynamic dispatch for additional handlers.

---

### 3. Fixed Array Bounds with Hard Exit

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

### 4. Or-Pattern Only Binds First Alternative's Variables

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

### 5. Test Coverage Gaps

**Directory:** `test/cases/`

- No tests for or-patterns with divergent bindings
- Limited edge case testing for list operations

**Fix:** Add expected files and expand test coverage.

---

## Resolved Issues

### ~~Symbol Term Uses Table Index Instead of Nick~~ (FIXED)

**File:** `src/parse/_.c:677-685`

Previously `purple_symbol_term` returned table indices for unbound symbols instead of nick-encoded values.

**Status:** Fixed - now computes nick encoding from source string.

---

## False Positives (Removed)

The following issues from the original analysis were determined to be false positives:

1. **Missing Compiler Cases**: All 19 features (QQ, UQ, UQS, Map, Filter, etc.) ARE present in `src/compile/_.c` lines 905-1022.

2. **Off-by-One in List Pattern NIL Constructor**: The `purple_term_pctr(tag, purple_term_nil())` pattern is correct - `purple_term_nil()` represents an empty argument list.

3. **Include Directive Bounds Check**: The `pos + 10 < len` check is conservative but safe (provides buffer for future parsing).

4. **Compiler Name Prefix**: `PURPLEC_NAM_*` vs `PURPLE_NAM_*` is intentional modularity - both compute identical nick values from the same strings.

---

## Summary

| Severity | Count | Status |
|----------|-------|--------|
| Critical | 1 | Open |
| Medium | 2 | Open |
| Minor | 2 | Open |
| Resolved | 1 | Fixed |

---

*Last updated: 2024-12-30*
