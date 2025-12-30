# Purple Full Implementation TODO

## Current Status (Dec 2024)

**Working Features (96 tests pass):**
- [x] EM (execute-at-metalevel)
- [x] clambda (compile under current semantics)
- [x] `meta-level` returns current tower level
- [x] `shift n expr` - go up n levels
- [x] `with-menv` / `set-meta!` - custom handlers work!
- [x] lift/run staging (Pink compatibility)
- [x] Basic pattern matching (literals, wildcards, flat constructors, variables)
- [x] Strings, chars, symbols
- [x] letrec (recursion via Y-combinator style)
- [x] FFI hooks (`ffi-call`)
- [x] `do` blocks for sequencing
- [x] Custom handlers for lit, var, app, if, lft, run, em, clam
- [x] `quote` for AST reification
- [x] `ctr-tag` and `ctr-arg` for AST introspection
- [x] Comparison operators (`<`, `>`, `<=`, `>=`)
- [x] Logic operators (`and`, `or`, `not`)
- [x] Arithmetic (`+`, `-`, `*`, `/`, `mod`)

---

## Phase 1: Complete Pattern Matching

**Current limitations:**
- Only flat patterns: `(CON a b)` works, but `(CON (CON a b) c)` fails
- No guards/conditions
- No or-patterns
- No as-patterns

### Step 1.1: Nested Constructor Patterns
**File:** `src/parse/_.c` - `purple_parse_pattern()`

```
Current: (CON a b) where a, b must be variable names
Target:  (CON (CON a b) c) with arbitrary nesting
```

- [ ] Modify `purple_parse_pattern()` to recursively parse sub-patterns
- [ ] Change pattern argument parsing from symbol-only to `purple_parse_pattern()`
- [ ] Update `#PCtr{tag, args}` to allow nested `#PCtr` in args list
- [ ] Track all bound variables across nested patterns

**File:** `lib/runtime.hvm4` - `@purple_pattern_match`

- [ ] Modify `@purple_match_ctr` to recursively match nested patterns
- [ ] Handle binding collection from nested matches
- [ ] Ensure proper ordering of bindings (outermost to innermost)

**Test cases to add:**
```lisp
(match (cons (cons 1 2) 3)
  ((CON (CON a b) c) (+ a (+ b c)))  ; => 6
  (_ 0))

(match (quote (+ (+ 1 2) 3))
  ((Add (Add a b) c) 'nested-add)
  (_ 'other))
```

### Step 1.2: Guard Expressions
```lisp
(match x
  ((n) :when (> n 0) 'positive)
  ((n) :when (< n 0) 'negative)
  (_ 'zero))
```

- [ ] Parser: Add `:when` keyword after pattern, before body
- [ ] New AST: `#Case{pattern, guard, body}` (guard can be #NIL)
- [ ] Runtime: Evaluate guard after pattern match, before executing body
- [ ] If guard fails, continue to next case

### Step 1.3: Or-Patterns
```lisp
(match x
  ((or 1 2 3) 'small)
  ((or 10 20) 'medium)
  (_ 'other))
```

- [ ] Parser: Add `or` pattern form
- [ ] New AST: `#POr{patterns_list}`
- [ ] Runtime: Try each sub-pattern, succeed if any matches
- [ ] Ensure all branches bind same variables

### Step 1.4: As-Patterns
```lisp
(match expr
  ((x @ (CON a b)) (list x a b))  ; bind both whole and parts
  (_ nil))
```

- [ ] Parser: Add `@` syntax for as-patterns
- [ ] New AST: `#PAs{name, sub_pattern}`
- [ ] Runtime: Bind name to whole value, then match sub-pattern

### Step 1.5: List Patterns (Sugar)
```lisp
(match lst
  ((list) 'empty)
  ((list a) 'singleton)
  ((list a b . rest) 'many))
```

- [ ] Parser: Desugar list patterns to nested CON/NIL patterns
- [ ] Support `...` or `.` for rest patterns

---

## Phase 2: Symbol System Enhancement

### Step 2.1: Symbol Equality
**File:** `lib/runtime.hvm4`

- [ ] Add `@purple_sym_eq` function comparing two `#Sym{n}` values
- [ ] Returns `#Cst{1}` if equal, `#Cst{0}` if not

**File:** `src/parse/_.c`

- [ ] Add `sym-eq?` or `eq?` form: `(eq? 'foo 'foo)` => 1
- [ ] Parse to `#SymEq{a, b}`

**File:** `lib/runtime.hvm4`

- [ ] Handle `#SymEq` in `@purple_eval`

### Step 2.2: Symbol Interning (Optional Optimization)
- [ ] Build symbol table at parse time
- [ ] Ensure same string always gets same nick encoding
- [ ] Useful for large programs with many symbols

### Step 2.3: Quoted Symbol Shorthand
```lisp
;; Current:
(sym-eq? (quote foo) (quote bar))
;; Shorthand:
(sym-eq? 'foo 'bar)
```

- [ ] Parser: `'symbol` desugars to `(quote symbol)`
- [ ] Handle in `parse_purple_form()` when seeing `'`

---

## Phase 3: Handler System Improvements

### Step 3.1: Get Handler by Name
```lisp
(get-meta 'add)  ; => returns the add handler closure
```

**Currently:** Partially implemented but may not return usable closure

- [ ] Verify `@purple_get_handler` returns callable value
- [ ] Add test: `((get-meta 'lit) 42)` should behave like lit handler
- [ ] Ensure returned handler can be passed to `set-meta!`

### Step 3.2: Handler Delegation (Super Calls)
```lisp
(with-menv
  (set-meta! 'add
    (lambda (x)
      (do (print "adding!")
          (default-add x))))  ; call original
  (+ 1 2))
```

- [ ] Capture default handlers at menv creation
- [ ] Provide `default-<handler>` or `(super)` mechanism
- [ ] Store original in menv: `#MEnv{env, handlers, parent, level, defaults}`

### Step 3.3: Handler Composition
```lisp
(compose-handler 'add
  (lambda (x next)
    (do (trace x) (next x))))
```

- [ ] Allow wrapping handlers without replacing
- [ ] Chain of responsibility pattern

### Step 3.4: Bulk Handler Update
```lisp
(with-handlers
  ((lit my-lit) (add my-add) (if my-if))
  body)
```

- [ ] Sugar for multiple `set-meta!` calls
- [ ] Single menv update with multiple handlers changed

---

## Phase 4: Quote and Eval System

### Step 4.1: Quasiquote and Unquote
```lisp
`(+ 1 ,(compute-value))     ; quasiquote
`(list ,@(get-items))        ; unquote-splicing
```

**Parser:**
- [ ] Add `` ` `` as quasiquote prefix
- [ ] Add `,` as unquote prefix
- [ ] Add `,@` as unquote-splicing prefix

**AST:**
- [ ] `#QQuote{expr}` - quasiquote
- [ ] `#Unquote{expr}` - unquote (must be inside quasiquote)
- [ ] `#UnquoteSplice{expr}` - splice list

**Runtime:**
- [ ] Evaluate quasiquote: traverse, evaluate unquotes, build AST
- [ ] Handle splicing into lists

### Step 4.2: Explicit Eval
```lisp
(eval env expr)        ; evaluate expr in given environment
(eval-here expr)       ; evaluate in current environment
```

- [ ] `#Eval{env_expr, body_expr}` AST node
- [ ] Runtime: build env, then evaluate body
- [ ] Security consideration: controlled evaluation

### Step 4.3: Apply
```lisp
(apply f (list 1 2 3))  ; apply f to argument list
```

- [ ] Parse `apply` form
- [ ] Runtime: unfold list and apply function iteratively

---

## Phase 5: Reflection and Reification

### Step 5.1: Environment Reification
```lisp
(reify-env)  ; => get current env as data structure
```

- [ ] Return env as inspectable list: `((name . value) ...)`
- [ ] Or return raw `#CON` chain with indices

### Step 5.2: Continuation Reification
```lisp
(call/cc (lambda (k) ...))  ; capture current continuation
```

- [ ] Requires continuation-passing transform or VM support
- [ ] Complex - may need HVM4 extensions
- [ ] Alternative: delimited continuations (shift/reset)

### Step 5.3: Handler Reification
```lisp
(reify-handler 'add)  ; => get handler source code as AST
```

- [ ] Store handler bodies in inspectable form
- [ ] Return `#Lam{...}` or similar

---

## Phase 6: Macro System

### Step 6.1: Basic Defmacro
```lisp
(defmacro when (cond . body)
  `(if ,cond (do ,@body) nil))

(when (> x 0)
  (print "positive")
  x)
```

**Parser:**
- [ ] Add `defmacro` form
- [ ] Store macro definitions separately from runtime definitions
- [ ] Macro expansion phase before evaluation

**Expansion:**
- [ ] Walk AST, find macro calls
- [ ] Apply macro function to get expanded AST
- [ ] Replace call with expansion
- [ ] Recursively expand until fixed point

### Step 6.2: Hygiene
- [ ] Option 1: Explicit gensym - `(gensym "tmp")`
- [ ] Option 2: Automatic hygiene with syntax objects
- [ ] Start with explicit gensym (simpler)

### Step 6.3: Macroexpand for Debugging
```lisp
(macroexpand '(when (> x 0) x))
; => (if (> x 0) (do x) nil)
```

- [ ] `macroexpand-1` - expand once
- [ ] `macroexpand` - expand fully

### Step 6.4: Local Macros
```lisp
(let-syntax ((my-macro (syntax-rules () ...)))
  body)
```

- [ ] Scoped macro definitions
- [ ] Shadow outer macros

---

## Phase 7: Standard Library

### Step 7.1: List Operations
```lisp
(car lst)           ; => first element (alias for fst)
(cdr lst)           ; => rest of list (alias for snd)
(null? lst)         ; => 1 if nil, 0 otherwise
(list a b c)        ; => (cons a (cons b (cons c nil)))
(length lst)        ; => number of elements
(append l1 l2)      ; => concatenate lists
(reverse lst)       ; => reverse list
(nth n lst)         ; => nth element (0-indexed)
```

- [ ] Implement as Purple definitions in `lib/stdlib.purple`
- [ ] Auto-include in compilation

### Step 7.2: Higher-Order Functions
```lisp
(map f lst)         ; => apply f to each element
(filter pred lst)   ; => keep elements where pred is true
(fold f init lst)   ; => left fold
(foldr f init lst)  ; => right fold
(zip l1 l2)         ; => pair up elements
(flatmap f lst)     ; => map then flatten
```

### Step 7.3: Utility Functions
```lisp
(identity x)        ; => x
(const a b)         ; => a
(flip f a b)        ; => (f b a)
(compose f g)       ; => (lambda (x) (f (g x)))
(curry f)           ; => curried version
(partial f . args)  ; => partially applied
```

### Step 7.4: Boolean and Comparison
```lisp
(not x)             ; DONE
(and a b)           ; DONE
(or a b)            ; DONE
(xor a b)           ; => exclusive or
(implies a b)       ; => (or (not a) b)
```

### Step 7.5: Numeric
```lisp
(abs n)             ; => absolute value
(min a b)           ; => smaller value
(max a b)           ; => larger value
(clamp lo hi n)     ; => constrain to range
(even? n)           ; => 1 if even
(odd? n)            ; => 1 if odd
(zero? n)           ; => 1 if zero
(positive? n)       ; => 1 if > 0
(negative? n)       ; => 1 if < 0
```

---

## Phase 8: Error Handling

### Step 8.1: Source Location Tracking
- [ ] Store file/line/column in AST nodes
- [ ] Propagate through compilation
- [ ] Include in error messages

**Parser changes:**
- [ ] Track position during parsing
- [ ] Add location field to term nodes or separate location map

### Step 8.2: Error Values
```lisp
(try expr
  (catch e (handle-error e)))

(throw 'my-error "message")
```

- [ ] `#Err{tag, message, location}` as first-class value
- [ ] Error propagation through evaluation
- [ ] Try/catch for recovery

### Step 8.3: Stack Traces
- [ ] Track call stack during evaluation
- [ ] Include tower level in traces
- [ ] Print readable traces on error

### Step 8.4: Debugging Forms
```lisp
(trace expr)        ; print value and return it
(assert cond msg)   ; error if cond is false
(debug expr)        ; breakpoint (if debugger connected)
(time expr)         ; measure and print execution time
```

---

## Phase 9: Example Languages

### Step 9.1: Traced Language
All operations logged:
```lisp
(define-language traced
  (handler add (lambda (x)
    (do (print (list 'add x))
        (default-add x))))
  (handler app (lambda (x)
    (do (print (list 'app x))
        (default-app x)))))
```

### Step 9.2: Typed Language
Runtime type checking:
```lisp
(define-language typed
  (handler app (lambda (args)
    (let ((f (fst args)) (x (snd args)))
      (if (type-check? f x)
          (default-app args)
          (throw 'type-error (list f x)))))))
```

### Step 9.3: Lazy Language
Lazy evaluation:
```lisp
(define-language lazy
  (handler app (lambda (args)
    ;; Don't evaluate argument until needed
    (let ((f (fst args)) (thunk (snd args)))
      (default-app (cons f (delay thunk)))))))
```

### Step 9.4: Logic Language
Backtracking:
```lisp
(define-language logic
  ;; amb, fail, require
  (handler amb ...))
```

---

## Phase 10: Tooling

### Step 10.1: REPL
```bash
$ purple repl
Purple> (+ 1 2)
3
Purple> (define x 10)
Purple> x
10
```

- [ ] Read-eval-print loop
- [ ] Readline support (history, editing)
- [ ] Multi-line input
- [ ] Special commands: `:help`, `:load`, `:quit`

### Step 10.2: Pretty Printer
- [ ] Format Purple code with proper indentation
- [ ] AST to source code conversion
- [ ] Used for macroexpand output

### Step 10.3: Syntax Highlighting
- [ ] VSCode extension
- [ ] Vim syntax file
- [ ] Emacs mode

---

## Phase 11: Performance

### Step 11.1: Handler Fast Path
- [ ] Detect when no custom handlers are used
- [ ] Use simpler evaluator without handler dispatch
- [ ] Significant speedup for non-reflective code

### Step 11.2: Compile-Time Handler Resolution
- [ ] If handler is known at compile time, inline it
- [ ] Avoid runtime lookup for static programs

### Step 11.3: Memoization
- [ ] Cache handler lookup results
- [ ] Memoize pure computations where safe

### Step 11.4: Benchmark Suite
- [ ] Factorial, fibonacci, sorting algorithms
- [ ] Compare with Pink, direct HVM4
- [ ] Track performance over time

---

## Implementation Priority

### Tier 1: Core Language Completion
1. **Phase 1.1**: Nested patterns (enables better AST manipulation)
2. **Phase 2.1**: Symbol equality (needed for many things)
3. **Phase 4.1**: Quasiquote (essential for metaprogramming)
4. **Phase 7.1-7.2**: List ops & HOFs (basic utility)

### Tier 2: Metaprogramming Power
5. **Phase 6.1-6.3**: Basic macro system
6. **Phase 3.2**: Handler delegation
7. **Phase 5.1**: Environment reification

### Tier 3: Production Quality
8. **Phase 8.1-8.3**: Error handling
9. **Phase 10.1**: REPL
10. **Phase 11.1-11.2**: Performance

### Tier 4: Showcase
11. **Phase 9**: Example languages
12. **Phase 10.2-10.3**: Tooling

---

## Notes

### Reference
- "Collapsing Towers of Interpreters" (POPL 2018)
- Sections 5-9 cover Purple specifically

### HVM4 Constraints
- Pure language - "mutation" returns new values
- MOV bindings for efficiency
- Pattern matching compiles to native HVM4 match

### Testing Strategy
- Add test case for each new feature
- Naming: `<feature>_<variant>.purple`
- Expected output in `.expected` file or comment

### File Locations
- Parser: `src/parse/_.c`
- Compiler: `src/compile/_.c`
- Runtime: `lib/runtime.hvm4`
- Tests: `test/cases/`
