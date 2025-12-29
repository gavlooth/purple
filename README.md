# Purple

Purple is a reflective language extending Pink with:
- **Conceptually Infinite Tower**: Meta-levels interpreted by higher meta-levels
- **EM (Execute-at-Metalevel)**: Jump to meta-level, access/modify the evaluator
- **clambda**: Compile under current (potentially modified) semantics
- **Mutable Evaluator Functions**: User code can modify eval-var, eval-app, etc.

Based on "Collapsing Towers of Interpreters" (POPL 2018).

## Build

```bash
# Initialize submodules
git submodule update --init --recursive

# Build HVM4 runtime first
cd hvm4/clang && clang -O2 -o main main.c && cd ../..

# Build Purple compiler
cd src && clang -O2 -o purplec main.c && cd ..
```

## Usage

```bash
# Compile Purple to HVM4
./src/purplec examples/hello.purple out.hvm4

# Run with HVM4
./hvm4/clang/main out.hvm4
```

## Syntax

```lisp
;; Basic Pink forms (inherited)
(lambda (x) body)           ; Lambda
(let (x val) body)          ; Let binding
(if cond then else)         ; Conditional
(lift expr)                 ; Lift to code
(run base expr)             ; Run code at base level

;; Purple extensions
(EM expr)                   ; Execute at meta-level
(clambda (x) body)          ; Compiled lambda
(get-meta 'key)             ; Get meta field
(set-meta! 'key val)        ; Set meta field
```

## Project Structure

```
purple/
├── src/                    # Purple compiler source
│   └── main.c              # Compiler entry point
├── lib/                    # Runtime libraries
│   └── runtime.hvm4        # Purple runtime (extends Pink)
├── test/                   # Test cases
│   └── cases/              # .purple test files
├── docs/                   # Documentation
│   └── IMPLEMENTATION_PLAN.md
└── hvm4/                   # HVM4 submodule
```

## References

- Amin & Rompf. "Collapsing Towers of Interpreters." POPL 2018.
- [lms-black](https://github.com/namin/lms-black) - Original Purple implementation
