# Potential Bugs Found (excluding hvm4)

1. FFI integer args ignored: `ffi_extract_int` only handles raw `NUM`, but evaluated Purple numbers are `#Cst{n}`. As a result, FFI calls like `(ffi "putchar" 65)` or `(ffi "exit" 1)` see `0` instead of the provided value. (src/run/_.c:70-125)
   - Fix idea: accept `#Cst{n}` by unwrapping the constructor payload before returning an int.

2. FFI `do` continuation loses the evaluation environment: when a `#Do{#FFI{...}, rest}` is returned, `ffi_eval_code` runs `snf(code, 0, 0)` without the current menv/handlers. Any variables or meta-level state in `rest` are evaluated in the wrong environment (or left unbound). (src/run/_.c:158-203)
   - Example: `(let (x 1) (do (ffi "puts" "hi") (+ x 1)))` should evaluate to `2` but likely won’t.
   - Fix idea: carry the current menv in the `#Do` marker or reconstruct `@purple_eval(@purple_menv_new(...), rest)` in the runner.

3. Include expansion is a raw text scan and does not skip strings or comments. A literal like `"(include \"x.purple\")"` or a commented-out include will still be expanded, corrupting source before parsing. (src/parse/_.c:2070-2169)

4. Include cycle tracking is capped at 256 paths; after that, new paths are not recorded, so large include graphs can re-include earlier files and potentially recurse indefinitely. (src/parse/_.c:2050-2065, 2132-2147)

5. Or-pattern bindings aren’t validated across alternatives: `purple_bind_pattern_vars` only inspects the first alternative. If later alternatives bind different names, the match body can reference unbound vars at runtime. (src/parse/_.c:1429-1438)

6. `purple-run` never calls `heap_init_slices()`, so HVM4 heap allocation uses zeroed slice bounds and will fail on first allocation (often as immediate OOM). (src/run/_.c:240-299)
   - Fix idea: call `heap_init_slices()` after allocating HEAP/BOOK/TABLE (same as `hvm4/clang/main.c`).

7. Native match optimization ignores nested constructor patterns: the parser allows nested patterns, but `purple_match_needs_runtime` only checks the top-level `#PCtr` and then emits a native matcher that binds only positional args. Nested constraints are silently dropped, producing wrong matches/bindings. (src/parse/_.c:1239-1391, src/compile/_.c:301-356)
   - Fix idea: detect non-trivial subpatterns in constructor args (nested `#PCtr`, `#PLit`, `#POr`, `#PAs`, etc.) and force runtime matching.

8. Native match compilation treats `#Case` as 2-arg and uses arg1 as the body, but the parser emits `#Case{pattern, guard, body}`. For unguarded cases, the body is read as `#NIL`, so compiled matches return the wrong result or always fall through. (src/compile/_.c:404-442, src/parse/_.c:625-628)
   - Fix idea: read case body from arg2 and ignore arg1 when guard is not used, or plumb guard support into native emission and `purple_match_needs_runtime`.

9. Include expansion grows buffers with `malloc/realloc` without checking for failure and uses `u32 cap` arithmetic that can overflow on large inputs, leading to NULL deref or buffer mis-sizing. (src/parse/_.c:2076-2179)
   - Fix idea: check allocation results and use `size_t` for `cap`/`out_len` with overflow checks.

10. Include path tracking leaks memory across parses: `purple_mark_included` uses `strdup` and the pointers are never freed even when `PURPLE_INCLUDED_LEN` resets. (src/parse/_.c:2068-2199)
   - Fix idea: free `PURPLE_INCLUDED[i]` before resetting, or store paths in a transient arena.

11. FFI string extraction truncates silently at 4096 bytes and casts codepoints to `char`, so Unicode or NUL characters are corrupted and long strings are cut off. (src/run/_.c:42-67)
   - Fix idea: return a heap buffer with length, or accept UTF-8 bytes and stop only at list end.

12. Runtime path resolution depends on `argv0`; if `realpath(argv0)` fails (e.g., invoked via PATH), `sys_path_join` uses the current working directory and can’t locate `lib/runtime.hvm4`. (src/main.c:17-23, src/run/_.c:206-213)
   - Fix idea: use `/proc/self/exe` (Linux) or `realpath` of the executable via platform-specific APIs, or embed/require an absolute runtime path.

# Fixed Bugs

- Include path buffer overflow risk in `src/parse/_.c`: Fixed by adding a check to ensure `path_len` fits in the buffer before `memcpy`.
- Compiler pattern match buffer overflow in `src/compile/_.c`: Fixed by increasing `var_names` buffer to 64 and adding a bounds check.
