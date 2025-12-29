# Purple

Purple is a reflective language extending Pink with:
- **Conceptually Infinite Tower**: Meta-levels interpreted by higher meta-levels
- **EM (Execute-at-Metalevel)**: Jump to meta-level, access/modify the evaluator
- **clambda**: Compile under current (potentially modified) semantics
- **Mutable Evaluator Functions**: User code can modify eval-var, eval-app, etc.
- **Pattern Matching**: Match on constructors, literals, and wildcards
- **FFI**: Call C functions directly

Based on "Collapsing Towers of Interpreters" (POPL 2018).

## Build

```bash
# Initialize submodules
git submodule update --init --recursive

# Build HVM4 runtime
cd hvm4/clang && clang -O2 -o main main.c && cd ../..

# Build Purple compiler
cd src && clang -O2 -o ../purple main.c && cd ..

# Build Purple runner (with FFI support)
cd src/run && clang -O2 -o ../../purple-run _.c && cd ../..
```

## Usage

```bash
# Compile Purple to HVM4 and run
./purple program.purple > out.hvm4
./hvm4/clang/main out.hvm4

# Or use purple-run for FFI support
./purple-run program.purple
```

## Language Features

### Basic Forms

```lisp
;; Literals
42                          ; Numbers
"hello"                     ; Strings
#\a                         ; Characters
#\newline                   ; Named characters

;; Variables and Bindings
(lambda (x) body)           ; Lambda
(lambda self (x) body)      ; Recursive lambda
(let (x val) body)          ; Let binding
(letrec ((f (lambda (x) ...))) body)  ; Recursive binding

;; Control Flow
(if cond then else)         ; Conditional

;; Arithmetic
(+ a b) (- a b) (* a b)     ; Basic arithmetic
(/ a b) (% a b)             ; Division, modulo
(< a b) (> a b)             ; Comparisons
(<= a b) (>= a b) (= a b)

;; Logic
(and a b) (or a b) (not a)  ; Short-circuit logic

;; Pairs and Lists
(cons a b)                  ; Construct pair
(fst p) (snd p)             ; Access pair elements
nil                         ; Empty list
```

### Pattern Matching

```lisp
(match expr
  (0 "zero")                    ; Match literal
  ((CON head tail) head)        ; Match constructor
  (x x))                        ; Match variable (binds x)

;; Matching on characters
(match #\a
  ((CHR c) c)                   ; Extract character code
  (_ 0))
```

### Staging (Code Generation)

```lisp
(lift expr)                 ; Lift value to code
(run base code)             ; Run code at base level
(code expr)                 ; Quote expression as code
```

### Reflective Features

```lisp
(EM expr)                   ; Execute at meta-level
(clambda (x) body)          ; Compiled lambda
(meta-level)                ; Get current meta-level number
(shift n expr)              ; Execute n levels up

;; Handler customization
(get-meta 'key)             ; Get handler
(set-meta! 'key handler)    ; Set handler
```

### FFI (Foreign Function Interface)

```lisp
;; Call C functions
(ffi "puts" "Hello, World!")

;; Sequence IO operations
(do
  (ffi "puts" "Line 1")
  (ffi "puts" "Line 2")
  42)                       ; Returns 42

;; Supported functions:
;; puts, putchar, getchar, print, newline, exit
```

### Include/Module System

```lisp
;; Include another Purple file
(include "lib/helpers.purple")

;; Example: shared math library
;; lib/math.purple:
(lambda (x y) (+ x y))

;; main.purple:
(let (add (include "lib/math.purple"))
  (add 10 32))  ; => 42
```

## Examples

### Factorial

```lisp
(letrec ((fact (lambda (n)
  (if (<= n 1)
    1
    (* n (fact (- n 1)))))))
  (fact 5))  ; => 120
```

### Tower Demo (meta-level programming)

```lisp
;; Compute 2^3 using meta-level
(+ (EM (+ 1 2 3))   ; Execute at meta-level: 6
   (meta-level))    ; Current level: 0
; Base level: 6 + 0 = 6

(shift 1            ; Go up 1 level
  (+ (EM (+ 1 1))   ; At level 2: 2
     (meta-level))) ; At level 1: 1
; Level 1: 2 + 1 = 3, returned to level 0
```

### Custom Handler (traced addition)

```lisp
;; Create a handler that traces additions
(let (traced-add
  (lambda (menv self val)
    ;; val is #Cst{result} after standard eval
    (ffi "puts" "add called")
    val))

  (with-handler 'add traced-add
    (+ 1 2)))  ; Prints "add called", returns 3
```

## Project Structure

```
purple/
├── src/
│   ├── main.c              # Compiler entry point
│   ├── parse/_.c           # Purple parser
│   ├── compile/_.c         # Compiler to HVM4
│   └── run/_.c             # Purple runner with FFI
├── lib/
│   └── runtime.hvm4        # Purple runtime
├── test/
│   ├── run.sh              # Test runner
│   └── cases/              # Test files (.purple)
├── docs/                   # Documentation
└── hvm4/                   # HVM4 submodule
```

## Running Tests

```bash
./test/run.sh
# Passed: 73, Failed: 0
```

## References

- Amin & Rompf. "Collapsing Towers of Interpreters." POPL 2018.
- [lms-black](https://github.com/namin/lms-black) - Original Purple implementation
- [HVM4](https://github.com/HigherOrderCO/hvm4) - Optimal runtime
