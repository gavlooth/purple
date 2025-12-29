# Purple Full Implementation TODO

## Phase 1: Symbol System
- [ ] Implement symbol equality (`eq?` or `sym-eq?`)
- [ ] Add symbol table for interning (avoid duplicate symbols)
- [ ] Parser support for quoted symbols: `'add`, `'if`, `'lam`
- [ ] Runtime symbol comparison in pattern matching

## Phase 2: Handler Modification
- [ ] Implement `get-meta` to return handler by symbol key
  - `(get-meta 'add)` → returns the add handler function
  - Need to map symbols to handler indices
- [ ] Implement `set-meta!` to create new menv with modified handler
  - `(set-meta! 'add new-handler)` → returns updated menv
  - Since HVM is pure, this returns a new menv (not mutation)
- [ ] Add `with-meta` form for scoped handler changes
  - `(with-meta ((add my-add) (if my-if)) body)`
- [ ] Store original handlers for delegation
  - Handlers should be able to call the "super" implementation

## Phase 3: Efficient Handler Dispatch
- [ ] Redesign handler table to avoid closure explosion
  - Current `#HTab{h0,h1,...h8}` with index lookup is expensive
  - Consider: direct field access, or compile-time handler resolution
- [ ] Add handler caching / memoization
- [ ] Profile and optimize critical paths
- [ ] Consider splitting: fast path (no custom handlers) vs slow path (custom handlers)

## Phase 4: Complete Tower Mechanics
- [ ] Multi-level EM: `(EM (EM expr))` should go up 2 levels
- [ ] Tower introspection: `(meta-level)` returns current level number
- [ ] `(base expr)` - evaluate at base level (level 0)
- [ ] `(shift n expr)` - evaluate n levels up
- [ ] Proper tower collapse for efficiency (as per paper)

## Phase 5: Reification & Reflection
- [ ] `(reify-env)` - get current environment as data
- [ ] `(reify-cont)` - get current continuation as data
- [ ] `(reflect-env env expr)` - evaluate with given environment
- [ ] `(reify-handler name)` - get handler as inspectable code
- [ ] Full closure reification (not just code, but captured env)

## Phase 6: New Expression Forms
- [ ] `quote` - prevent evaluation: `(quote (+ 1 2))` → `#App{#Add, ...}`
- [ ] `quasiquote` / `unquote` - template with holes
- [ ] `eval` - explicit evaluation: `(eval env expr)`
- [ ] `apply` - apply function to argument list
- [ ] `match` - pattern matching on values/constructors
- [ ] `define` - top-level definitions within Purple

## Phase 7: Macro System
- [ ] Design macro representation
- [ ] `defmacro` - define syntax transformers
- [ ] Macro expansion phase before evaluation
- [ ] Hygiene (or explicit capture)
- [ ] `macroexpand` for debugging

## Phase 8: Parser Extensibility
- [ ] Reader macros or syntax extension points
- [ ] Custom literal syntax (e.g., `#vec[1 2 3]`)
- [ ] Operator definition with precedence
- [ ] Or: keep S-expr syntax, extend via macros only

## Phase 9: Error Handling & Debugging
- [ ] Source location tracking through compilation
- [ ] Stack traces that show tower levels
- [ ] `(trace expr)` - trace evaluation steps
- [ ] `(break)` - debugger breakpoint
- [ ] Better error messages with context

## Phase 10: Standard Library
- [ ] List operations: `cons`, `car`, `cdr`, `null?`, `list`
- [ ] Higher-order: `map`, `filter`, `fold`
- [ ] Boolean operations: `and`, `or`, `not`
- [ ] Comparison: `<`, `>`, `<=`, `>=`
- [ ] Arithmetic: `mod`, `div`, `abs`, `min`, `max`
- [ ] String operations (if supported by HVM4)

## Phase 11: Example Languages
- [ ] **Traced**: Language where all operations are logged
- [ ] **Typed**: Add runtime type checking via handlers
- [ ] **Lazy**: Change evaluation strategy to lazy
- [ ] **Logic**: Add backtracking/unification
- [ ] **Stack**: Forth-like stack language
- [ ] Document each as example of Purple's power

## Phase 12: Documentation
- [ ] Language reference (all forms, semantics)
- [ ] Tutorial: "Your First Purple Program"
- [ ] Guide: "Creating a New Language with Purple"
- [ ] Paper-style explanation of tower mechanics
- [ ] API reference for runtime functions

## Phase 13: Testing & CI
- [ ] Unit tests for each expression form
- [ ] Integration tests for tower mechanics
- [ ] Tests for handler modification
- [ ] Regression tests for performance
- [ ] Fuzzing / property-based testing
- [ ] CI pipeline (GitHub Actions)

## Phase 14: Tooling
- [ ] REPL with readline support
- [ ] Pretty printer for Purple AST
- [ ] Syntax highlighting (VSCode, vim, emacs)
- [ ] LSP server for IDE integration
- [ ] Debugger UI

## Phase 15: Advanced Features (Future)
- [ ] First-class continuations (`call/cc`)
- [ ] Delimited continuations (`shift`/`reset`)
- [ ] Effect handlers
- [ ] Module system
- [ ] Separate compilation
- [ ] JIT compilation via HVM4 optimization

---

## Priority Order (Suggested)

### MVP for Language Creation:
1. Phase 1 (Symbols) - needed for handler lookup
2. Phase 2 (Handler Modification) - core feature
3. Phase 6 (quote, eval, match) - essential forms
4. Phase 11 (One example language) - proof it works

### Production Ready:
5. Phase 3 (Efficient dispatch)
6. Phase 9 (Error handling)
7. Phase 10 (Standard library)
8. Phase 12 (Documentation)

### Full Vision:
9. Remaining phases as needed

---

## Notes

- The paper "Collapsing Towers of Interpreters" (POPL 2018) is the reference
- Current implementation has working: EM, clambda, lift/run staging
- Main gap: handler modification is stubbed
- Performance concern: handler dispatch caused memory issues, solved by inlining
- HVM4 is pure, so "mutation" returns new values
