# Potential Bugs Found (excluding hvm4)

1. ~~FFI integer args ignored~~ (FIXED): `ffi_extract_int` now unwraps `#Cst{n}` wrappers using the `FFI_NAM_CST` constant (src/run/_.c:79-85).

2. ~~FFI `do` continuation loses the evaluation environment~~ (FIXED): The runtime now wraps the continuation in `#WMnv{menv, rest}` when FFI is detected (lib/runtime.hvm4:913), and the C driver handles `#WMnv` to preserve the evaluation context (src/run/_.c:186-191, 227-231).

3. ~~Include expansion is a raw text scan and does not skip strings or comments~~ (FIXED): `purple_expand_includes` now handles string literals (with escape sequences) and line comments (`;` and `//`) verbatim, only looking for `(include ...)` directives outside of these contexts. (src/parse/_.c:2157-2200)

4. ~~Include cycle tracking is capped at 256 paths~~ (FIXED): `purple_mark_included` now errors out if the 256 path limit is exceeded, preventing silent failure that could allow infinite include recursion. (src/parse/_.c:2141-2147)

5. ~~Or-pattern bindings aren't validated across alternatives~~ (FIXED): `purple_bind_pattern_vars` now validates that all alternatives bind the same variables in the same order (src/parse/_.c:1488-1510), then binds from the first alternative. Mismatched bindings cause a compile error.

6. ~~`purple-run` never calls `heap_init_slices()`~~ (FIXED): `heap_init_slices()` is now called at line 291 after allocating HEAP/BOOK/TABLE (src/run/_.c:291).

7. Native match optimization ignores nested constructor patterns: the parser allows nested patterns, but `purple_match_needs_runtime` only checks the top-level `#PCtr` and then emits a native matcher that binds only positional args. Nested constraints are silently dropped, producing wrong matches/bindings. (src/parse/_.c:1239-1391, src/compile/_.c:301-356)
   - Fix idea: detect non-trivial subpatterns in constructor args (nested `#PCtr`, `#PLit`, `#POr`, `#PAs`, etc.) and force runtime matching.

8. ~~Native match compilation treats `#Case` as 2-arg~~ (FIXED): The native match emission now correctly reads the body from arg2 of the 3-arg `#Case{pattern, guard, body}` (src/compile/_.c:439).

9. Include expansion grows buffers with `malloc/realloc` without checking for failure and uses `u32 cap` arithmetic that can overflow on large inputs, leading to NULL deref or buffer mis-sizing. (src/parse/_.c:2076-2179)
   - Fix idea: check allocation results and use `size_t` for `cap`/`out_len` with overflow checks.

10. ~~Include path tracking leaks memory across parses~~ (FIXED): `purple_reset_includes` now frees all `strdup`'d paths before resetting the counter (src/parse/_.c:2305-2311).

11. FFI string extraction truncates silently at 4096 bytes and casts codepoints to `char`, so Unicode or NUL characters are corrupted and long strings are cut off. (src/run/_.c:42-67)
   - Fix idea: return a heap buffer with length, or accept UTF-8 bytes and stop only at list end.

12. Runtime path resolution depends on `argv0`; if `realpath(argv0)` fails (e.g., invoked via PATH), `sys_path_join` uses the current working directory and can’t locate `lib/runtime.hvm4`. (src/main.c:17-23, src/run/_.c:206-213)
   - Fix idea: use `/proc/self/exe` (Linux) or `realpath` of the executable via platform-specific APIs, or embed/require an absolute runtime path.

# Fixed Bugs

- #1 FFI integer args: Fixed `ffi_extract_int` to unwrap `#Cst{n}` using `FFI_NAM_CST` constant.
- #2 FFI do continuation: Runtime now wraps continuation in `#WMnv{menv, rest}` for environment preservation.
- #5 Or-pattern bindings: `purple_bind_pattern_vars` now validates all alternatives bind the same variables.
- #6 heap_init_slices: Now called in purple-run after allocating HEAP/BOOK/TABLE.
- #8 Native match Case body: Now correctly reads body from arg2 of 3-arg `#Case{pattern, guard, body}`.
- Include path buffer overflow risk in `src/parse/_.c`: Fixed by adding a check to ensure `path_len` fits in the buffer before `memcpy`.
- Compiler pattern match buffer overflow in `src/compile/_.c`: Fixed by increasing `var_names` buffer to 64 and adding a bounds check.
- #3 Include expansion in strings/comments: `purple_expand_includes` now skips string literals and line comments when looking for `(include ...)` directives.
- #10 Include path memory leak: `purple_reset_includes` now frees all `strdup`'d paths before resetting the counter.
- #4 Include cycle tracking cap: `purple_mark_included` now errors out if the 256 path limit is exceeded.
