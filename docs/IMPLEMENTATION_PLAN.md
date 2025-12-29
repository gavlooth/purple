# Purple Language Implementation Plan for HVM4

## Overview

Purple is a reflective language extending Pink with:
1. **Conceptually Infinite Tower**: Meta-levels interpreted by higher meta-levels
2. **EM (Execute-at-Metalevel)**: Jump to meta-level, access/modify the evaluator
3. **clambda**: Compile under current (potentially modified) semantics
4. **Mutable Evaluator Functions**: User code can modify eval-var, eval-app, etc.

Reference: "Collapsing Towers of Interpreters" POPL 2018, Sections 5-9.

---

## Phase 1: Parser Extensions

**File**: `/home/heefoo/code/hvm4/clang/parse/pink/_.c`

### Step 1.1: Add Purple constructor name constants

After line 29, add:
```c
static u32 PINK_NAM_EM;    // Execute-at-metalevel
static u32 PINK_NAM_CLAM;  // Compiled lambda (clambda)
static u32 PINK_NAM_GMTA;  // Get meta field
static u32 PINK_NAM_SMTA;  // Set meta field
```

### Step 1.2: Initialize constants in pink_names_init()

After line 61, add:
```c
PINK_NAM_EM   = pink_nick("EM");
PINK_NAM_CLAM = pink_nick("CLam");
PINK_NAM_GMTA = pink_nick("GMta");
PINK_NAM_SMTA = pink_nick("SMta");
```

### Step 1.3: Add term constructor helpers

After line 257, add:
```c
fn Term pink_term_em(Term e) {
  return pink_ctr1(PINK_NAM_EM, e);
}

fn Term pink_term_clam(Term body) {
  return pink_ctr1(PINK_NAM_CLAM, body);
}

fn Term pink_term_gmta(Term key) {
  return pink_ctr1(PINK_NAM_GMTA, key);
}

fn Term pink_term_smta(Term key, Term val) {
  return pink_ctr2(PINK_NAM_SMTA, key, val);
}
```

### Step 1.4: Add pink_parse_em()

After pink_parse_code(), add:
```c
fn Term pink_parse_em(PState *s) {
  Term e = parse_pink_form(s);
  parse_consume(s, ")");
  return pink_term_em(e);
}
```

### Step 1.5: Add pink_parse_clambda()

```c
fn Term pink_parse_clambda(PState *s) {
  // Reuse lambda parsing logic but produce #CLam instead
  parse_skip(s);
  parse_consume(s, "(");
  u32 params[256];
  u32 count = 0;
  parse_skip(s);
  if (parse_match(s, ")")) {
    parse_error(s, "parameter", ')');
  }
  while (1) {
    u32 start = 0;
    u32 len   = 0;
    if (!pink_parse_symbol_token(s, &start, &len)) {
      parse_error(s, "parameter", parse_peek(s));
    }
    if (count >= 256) {
      fprintf(stderr, "PINK_ERROR: too many parameters\n");
      exit(1);
    }
    params[count++] = table_find(s->src + start, len);
    parse_skip(s);
    if (parse_match(s, ")")) {
      break;
    }
  }
  if (count == 0) {
    fprintf(stderr, "PINK_ERROR: clambda requires at least one parameter\n");
    exit(1);
  }
  for (u32 i = 0; i < count; i++) {
    pink_bind_push(params[i]);
  }
  Term body = parse_pink_form(s);
  pink_bind_pop(count);
  parse_consume(s, ")");
  // Wrap in nested lambdas, outermost is CLam
  for (u32 i = 0; i < count - 1; i++) {
    body = pink_term_lam(body);
  }
  body = pink_term_clam(body);
  return body;
}
```

### Step 1.6: Add pink_parse_getmeta() and pink_parse_setmeta()

```c
fn Term pink_parse_getmeta(PState *s) {
  Term key = parse_pink_form(s);
  parse_consume(s, ")");
  return pink_term_gmta(key);
}

fn Term pink_parse_setmeta(PState *s) {
  Term key = parse_pink_form(s);
  Term val = parse_pink_form(s);
  parse_consume(s, ")");
  return pink_term_smta(key, val);
}
```

### Step 1.7: Wire new forms into parse_pink_list()

In parse_pink_list(), after the "code" check (line 446), add:
```c
if (pink_symbol_is(s, start, len, "EM")) {
  return pink_parse_em(s);
}
if (pink_symbol_is(s, start, len, "clambda")) {
  return pink_parse_clambda(s);
}
if (pink_symbol_is(s, start, len, "get-meta")) {
  return pink_parse_getmeta(s);
}
if (pink_symbol_is(s, start, len, "set-meta!")) {
  return pink_parse_setmeta(s);
}
```

---

## Phase 2: Runtime Foundation

**File**: `/home/heefoo/Documents/code/hvm4/pink/lib/runtime.hvm4`

### Step 2.1: Add MEnv constructor and field accessors

After @pink_unwrap (end of file), add:
```hvm4
// =============================================================================
// Purple Runtime Extensions
// =============================================================================

// Meta-environment: #MEnv{env, handlers, parent}
// - env: variable bindings (same as Pink)
// - handlers: #HTab{...} of eval handlers
// - parent: parent meta-environment (lazy, #NIL if not yet created)

@purple_menv_env = λ&m. λ{#MEnv: λ&e. λ&h. λ&p. e; _ : @pink_err(#menv)}(m)
@purple_menv_handlers = λ&m. λ{#MEnv: λ&e. λ&h. λ&p. h; _ : @pink_err(#menv)}(m)
@purple_menv_parent = λ&m. λ{#MEnv: λ&e. λ&h. λ&p. p; _ : @pink_err(#menv)}(m)
```

### Step 2.2: Add handler table constructor and accessors

```hvm4
// Handler table: #HTab{lit, var, lam, app, if, lft, run, em, clam}
// Each handler is: λmenv. λexp. <result>

@purple_htab_lit  = λ&t. λ{#HTab: λ&a.λ&b.λ&c.λ&d.λ&e.λ&f.λ&g.λ&h.λ&i. a; _ : @pink_err(#htab)}(t)
@purple_htab_var  = λ&t. λ{#HTab: λ&a.λ&b.λ&c.λ&d.λ&e.λ&f.λ&g.λ&h.λ&i. b; _ : @pink_err(#htab)}(t)
@purple_htab_lam  = λ&t. λ{#HTab: λ&a.λ&b.λ&c.λ&d.λ&e.λ&f.λ&g.λ&h.λ&i. c; _ : @pink_err(#htab)}(t)
@purple_htab_app  = λ&t. λ{#HTab: λ&a.λ&b.λ&c.λ&d.λ&e.λ&f.λ&g.λ&h.λ&i. d; _ : @pink_err(#htab)}(t)
@purple_htab_if   = λ&t. λ{#HTab: λ&a.λ&b.λ&c.λ&d.λ&e.λ&f.λ&g.λ&h.λ&i. e; _ : @pink_err(#htab)}(t)
@purple_htab_lft  = λ&t. λ{#HTab: λ&a.λ&b.λ&c.λ&d.λ&e.λ&f.λ&g.λ&h.λ&i. f; _ : @pink_err(#htab)}(t)
@purple_htab_run  = λ&t. λ{#HTab: λ&a.λ&b.λ&c.λ&d.λ&e.λ&f.λ&g.λ&h.λ&i. g; _ : @pink_err(#htab)}(t)
@purple_htab_em   = λ&t. λ{#HTab: λ&a.λ&b.λ&c.λ&d.λ&e.λ&f.λ&g.λ&h.λ&i. h; _ : @pink_err(#htab)}(t)
@purple_htab_clam = λ&t. λ{#HTab: λ&a.λ&b.λ&c.λ&d.λ&e.λ&f.λ&g.λ&h.λ&i. i; _ : @pink_err(#htab)}(t)
```

### Step 2.3: Add menv factory and default handlers

```hvm4
// Create new meta-environment with default handlers
@purple_menv_new = λ&parent. λ&env.
  #MEnv{env, @purple_default_handlers, parent}

// Default handler implementations (delegate to standard behavior)
@purple_h_lit_default = λ&menv. λ&self. λ&exp.
  !!&n = λ{#Lit: λ&x. x; _ : @pink_err(#lit)}(exp);
  #Cst{n}

@purple_h_var_default = λ&menv. λ&self. λ&exp.
  !!&idx = λ{#Var: λ&x. x; _ : @pink_err(#var)}(exp);
  !!&env = @purple_menv_env(menv);
  @pink_env_get(env, idx)

@purple_h_lam_default = λ&menv. λ&self. λ&exp.
  !!&body = λ{#Lam: λ&x. x; _ : @pink_err(#lam)}(exp);
  !!&env = @purple_menv_env(menv);
  #Clo{env, body}

@purple_h_app_default = λ&menv. λ&self. λ&exp.
  !!&f = λ{#App: λ&x. λ&y. x; _ : @pink_err(#app)}(exp);
  !!&x = λ{#App: λ&a. λ&b. b; _ : @pink_err(#app)}(exp);
  !!&vf = self(menv, f);
  !!&vx = self(menv, x);
  @purple_apply(menv, self, vf, vx)

@purple_h_if_default = λ&menv. λ&self. λ&exp.
  !!&c = λ{#If: λ&a.λ&b.λ&c. a; _ : @pink_err(#if)}(exp);
  !!&t = λ{#If: λ&a.λ&b.λ&c. b; _ : @pink_err(#if)}(exp);
  !!&e = λ{#If: λ&a.λ&b.λ&c. c; _ : @pink_err(#if)}(exp);
  !!&vc = self(menv, c);
  λ{#Cst: λ&n. λ{0: self(menv, e); λ&m. self(menv, t)}(n); _ : @pink_err(#if)}(vc)

@purple_h_lft_default = λ&menv. λ&self. λ&exp.
  !!&e = λ{#Lft: λ&x. x; _ : @pink_err(#lft)}(exp);
  !!&v = self(menv, e);
  @pink_lift(v)

@purple_h_run_default = λ&menv. λ&self. λ&exp.
  !!&b = λ{#Run: λ&x.λ&y. x; _ : @pink_err(#run)}(exp);
  !!&e = λ{#Run: λ&x.λ&y. y; _ : @pink_err(#run)}(exp);
  !!&vb = self(menv, b);
  λ{
    #Cst: λ&n.
      !!&vc = @purple_evalc(menv, e);
      λ{#Cod: λ&ec. self(menv, ec); _ : @pink_err(#run)}(vc)
    _ : @pink_err(#run)
  }(vb)

@purple_h_em_default = λ&menv. λ&self. λ&exp.
  !!&e = λ{#EM: λ&x. x; _ : @pink_err(#em)}(exp);
  !!&parent = @purple_ensure_parent(menv);
  self(parent, e)

@purple_h_clam_default = λ&menv. λ&self. λ&exp.
  !!&body = λ{#CLam: λ&x. x; _ : @pink_err(#clam)}(exp);
  !!&env = @purple_menv_env(menv);
  !!&compiled = @purple_evalc(menv, #Lam{body});
  λ{#Cod: λ&code. self(menv, code); _ : @pink_err(#clam)}(compiled)

@purple_default_handlers = #HTab{
  @purple_h_lit_default,
  @purple_h_var_default,
  @purple_h_lam_default,
  @purple_h_app_default,
  @purple_h_if_default,
  @purple_h_lft_default,
  @purple_h_run_default,
  @purple_h_em_default,
  @purple_h_clam_default
}
```

### Step 2.4: Add lazy tower creation

```hvm4
// Ensure parent meta-environment exists (lazy creation)
@purple_ensure_parent = λ&menv.
  !!&parent = @purple_menv_parent(menv);
  λ{
    #NIL: @purple_menv_new(#NIL, #NIL)
    _ : parent
  }(parent)
```

---

## Phase 3: Purple Evaluator

**File**: `/home/heefoo/Documents/code/hvm4/pink/lib/runtime.hvm4` (continued)

### Step 3.1: Add Purple application helper

```hvm4
@purple_apply = λ&menv. λ&self. λ&vf. λ&vx.
  λ{
    #Clo: λ&cenv. λ&body.
      !!&new_menv = #MEnv{#CON{vx, cenv}, @purple_menv_handlers(menv), @purple_menv_parent(menv)};
      self(new_menv, body)
    #CloR: λ&cenv. λ&body.
      !!&new_menv = #MEnv{#CON{vx, #CON{vf, cenv}}, @purple_menv_handlers(menv), @purple_menv_parent(menv)};
      self(new_menv, body)
    #Cod: λ&fcode. λ{#Cod: λ&acode. #Cod{#App{fcode, acode}}; _ : @pink_err(#app)}(vx)
    _ : @pink_err(#app)
  }(vf)
```

### Step 3.2: Add stage-polymorphic Purple evaluator

```hvm4
@purple_eval_poly = λ&maybe_lift. λ&self. λ&menv. λ&exp.
  !!&handlers = @purple_menv_handlers(menv);
  λ{
    #Lit: λ&n. maybe_lift(@purple_htab_lit(handlers)(menv, self, exp))
    #Sym: λ&s. maybe_lift(#Sym{s})
    #Var: λ&n. maybe_lift(@purple_htab_var(handlers)(menv, self, exp))
    #Lam: λ&body. maybe_lift(@purple_htab_lam(handlers)(menv, self, exp))
    #LamR: λ&body.
      !!&env = @purple_menv_env(menv);
      maybe_lift(#CloR{env, body})
    #App: λ&f. λ&x. @purple_htab_app(handlers)(menv, self, exp)
    #Let: λ&val. λ&body.
      !!&vv = self(menv, val);
      !!&env = @purple_menv_env(menv);
      !!&new_menv = #MEnv{#CON{vv, env}, handlers, @purple_menv_parent(menv)};
      self(new_menv, body)
    #If: λ&c. λ&t. λ&e. @purple_htab_if(handlers)(menv, self, exp)
    #Lft: λ&e. @purple_htab_lft(handlers)(menv, self, exp)
    #Run: λ&b. λ&e. @purple_htab_run(handlers)(menv, self, exp)
    #Cod: λ&e. #Cod{e}
    #Add: λ&a. λ&b.
      !!&va = self(menv, a);
      !!&vb = self(menv, b);
      λ{
        #Cst: λ&na. λ{#Cst: λ&nb. maybe_lift(#Cst{(na + nb)}); _ : @pink_err(#add)}(vb)
        #Cod: λ&ca. λ{#Cod: λ&cb. #Cod{#Add{ca, cb}}; _ : @pink_err(#add)}(vb)
        _ : @pink_err(#add)
      }(va)
    #Sub: λ&a. λ&b.
      !!&va = self(menv, a);
      !!&vb = self(menv, b);
      λ{
        #Cst: λ&na. λ{#Cst: λ&nb. maybe_lift(#Cst{(na - nb)}); _ : @pink_err(#sub)}(vb)
        #Cod: λ&ca. λ{#Cod: λ&cb. #Cod{#Sub{ca, cb}}; _ : @pink_err(#sub)}(vb)
        _ : @pink_err(#sub)
      }(va)
    #Mul: λ&a. λ&b.
      !!&va = self(menv, a);
      !!&vb = self(menv, b);
      λ{
        #Cst: λ&na. λ{#Cst: λ&nb. maybe_lift(#Cst{(na * nb)}); _ : @pink_err(#mul)}(vb)
        #Cod: λ&ca. λ{#Cod: λ&cb. #Cod{#Mul{ca, cb}}; _ : @pink_err(#mul)}(vb)
        _ : @pink_err(#mul)
      }(va)
    #Eql: λ&a. λ&b.
      !!&va = self(menv, a);
      !!&vb = self(menv, b);
      λ{
        #Cst: λ&na. λ{#Cst: λ&nb. maybe_lift(#Cst{(na == nb)}); _ : @pink_err(#eql)}(vb)
        #Cod: λ&ca. λ{#Cod: λ&cb. #Cod{#Eql{ca, cb}}; _ : @pink_err(#eql)}(vb)
        _ : @pink_err(#eql)
      }(va)
    #CON: λ&a. λ&b.
      !!&va = self(menv, a);
      !!&vb = self(menv, b);
      λ{
        #Cod: λ&ca. λ{#Cod: λ&cb. #Cod{#CON{ca, cb}}; _ : @pink_err(#con)}(vb)
        _ : λ&u. maybe_lift(#CON{va, vb})
      }(va)
    #NIL: maybe_lift(#NIL)
    #Fst: λ&p.
      !!&vp = self(menv, p);
      λ{
        #CON: λ&a. λ&b. a
        #Cod: λ&pc. #Cod{#Fst{pc}}
        _ : @pink_err(#fst)
      }(vp)
    #Snd: λ&p.
      !!&vp = self(menv, p);
      λ{
        #CON: λ&a. λ&b. b
        #Cod: λ&pc. #Cod{#Snd{pc}}
        _ : @pink_err(#snd)
      }(vp)
    #EM: λ&e. @purple_htab_em(handlers)(menv, self, exp)
    #CLam: λ&body. @purple_htab_clam(handlers)(menv, self, exp)
    #GMta: λ&key.
      !!&vk = self(menv, key);
      @purple_menv_get_by_key(menv, vk)
    #SMta: λ&key. λ&val.
      !!&vk = self(menv, key);
      !!&vv = self(menv, val);
      @purple_menv_set_by_key(menv, vk, vv)
    _ : @pink_err(#pexp)
  }(exp)
```

### Step 3.3: Add eval/evalc instantiations

```hvm4
@purple_eval  = @purple_eval_poly(λv.v, @purple_eval)
@purple_evalc = @purple_eval_poly(@pink_lift, @purple_evalc)
```

### Step 3.4: Add get-meta/set-meta helpers

```hvm4
// Get handler by symbol key
@purple_menv_get_by_key = λ&menv. λ&key.
  !!&handlers = @purple_menv_handlers(menv);
  λ{
    #Sym: λ&s.
      // Match on known handler names
      // (simplified - would need proper symbol comparison)
      handlers
    _ : @pink_err(#gmta)
  }(key)

// Set handler by symbol key (returns updated menv)
@purple_menv_set_by_key = λ&menv. λ&key. λ&val.
  // For now, return menv unchanged (proper impl needs mutable update)
  menv
```

### Step 3.5: Add Purple unwrap

```hvm4
@purple_unwrap = λ&v.
  λ{
    #Cst: λ&n. n
    #Sym: λ&s. #Sym{s}
    #CON: λ&a. λ&b. #CON{@purple_unwrap(a), @purple_unwrap(b)}
    #NIL: #NIL
    #Cod: λ&e. #Cod{e}
    #Clo: λ&env. λ&body. #Clo{env, body}
    #CloR: λ&env. λ&body. #CloR{env, body}
    _ : @pink_err(#punw)
  }(v)
```

---

## Phase 4: Compiler Extensions

**File**: `/home/heefoo/Documents/code/hvm4/pink/compile/_.c`

### Step 4.1: Add Purple constructor name constants

After existing PINKC_NAM_* declarations, add:
```c
static u32 PINKC_NAM_EM;
static u32 PINKC_NAM_CLAM;
static u32 PINKC_NAM_GMTA;
static u32 PINKC_NAM_SMTA;
```

In pinkc_names_init(), add:
```c
PINKC_NAM_EM   = pinkc_nick("EM");
PINKC_NAM_CLAM = pinkc_nick("CLam");
PINKC_NAM_GMTA = pinkc_nick("GMta");
PINKC_NAM_SMTA = pinkc_nick("SMta");
```

### Step 4.2: Add pink_ast_has_purple() detection

```c
fn int pink_ast_has_purple(Term t) {
  pinkc_names_init();
  u8 tag = term_tag(t);
  if (tag >= C00 && tag <= C16) {
    u32 nam = term_ext(t);
    if (nam == PINKC_NAM_EM || nam == PINKC_NAM_CLAM ||
        nam == PINKC_NAM_GMTA || nam == PINKC_NAM_SMTA) {
      return 1;
    }
    u32 ari = pink_ctr_arity(t);
    for (u32 i = 0; i < ari; i++) {
      if (pink_ast_has_purple(pink_ctr_arg(t, i))) {
        return 1;
      }
    }
  }
  return 0;
}
```

### Step 4.3: Update pink_ast_needs_runtime()

Modify to include Purple:
```c
fn int pink_ast_needs_runtime(Term t) {
  return pink_ast_has_staging(t) || pink_ast_has_purple(t);
}
```

**File**: `/home/heefoo/Documents/code/hvm4/pink/main.c`

### Step 4.4: Update main.c for Purple runtime

Modify pink_compile_emit_runtime() call to use Purple evaluator when Purple forms present:
```c
if (pink_ast_needs_runtime(ast)) {
  char runtime_path[4096];
  pink_runtime_path(runtime_path, sizeof(runtime_path), argv[0]);
  if (pink_ast_has_purple(ast)) {
    pink_compile_emit_purple_runtime(out, ast, runtime_path);
  } else {
    pink_compile_emit_runtime(out, ast, runtime_path);
  }
}
```

Add new function:
```c
fn void pink_compile_emit_purple_runtime(FILE *out, Term ast, const char *runtime_path) {
  // Include Pink runtime first
  char *runtime = sys_file_read(runtime_path);
  if (!runtime) {
    fprintf(stderr, "Error: could not read runtime '%s'\n", runtime_path);
    exit(1);
  }
  fputs(runtime, out);
  free(runtime);

  // Emit main using Purple evaluator
  fputs("@main = @purple_unwrap(@purple_eval(@purple_menv_new(#NIL, #NIL), ", out);
  pink_compile_emit(out, ast);
  fputs("))\n", out);
}
```

---

## Phase 5: C Interpreter Extensions

**File**: `/home/heefoo/Documents/code/hvm4/pink/interpreter.c`

### Step 5.1: Add Purple frame types

After existing PF_* definitions, add:
```c
#define PF_EM       14
#define PF_CLAM     15
#define PF_GMTA     16
#define PF_SMTA_KEY 17
#define PF_SMTA_VAL 18
```

### Step 5.2: Add menv to evaluation state

Modify PinkState or add new field:
```c
typedef struct {
  Term exp;
  Term env;
  Term menv;   // NEW: meta-environment for Purple
  Term val;
  u32  fresh;
  u32  sp;
  PinkFrame *stack;
  u32  cap;
  int  phase;
} PinkState;
```

### Step 5.3: Add menv construction helpers

```c
fn Term purple_menv_new(Term parent, Term env) {
  Term args[3];
  args[0] = env;
  args[1] = purple_default_handlers();
  args[2] = parent;
  return pink_val_ctr(PINK_NAM_MENV, 3, args);
}

fn Term purple_menv_env(Term menv) {
  return pink_ctr_arg(menv, 0);
}

fn Term purple_menv_handlers(Term menv) {
  return pink_ctr_arg(menv, 1);
}

fn Term purple_menv_parent(Term menv) {
  return pink_ctr_arg(menv, 2);
}
```

### Step 5.4: Handle EM in trampoline loop

In the EVAL phase switch:
```c
case PINK_NAM_EM: {
  Term e = pink_ctr_arg(exp, 0);
  // Get or create parent menv
  Term parent = purple_menv_parent(state.menv);
  if (pink_is_nil(parent)) {
    parent = purple_menv_new(pink_term_nil(), pink_term_nil());
  }
  // Push frame to restore current menv
  PinkFrame fr = { .tag = PF_EM, .a = state.menv };
  pink_stack_push(&state, fr);
  // Switch to parent menv
  state.menv = parent;
  state.exp = e;
  continue;
}
```

In the APPLY phase:
```c
case PF_EM: {
  // Restore original menv
  state.menv = fr.a;
  // val already contains result from parent eval
  continue;
}
```

### Step 5.5: Handle clambda in trampoline loop

```c
case PINK_NAM_CLAM: {
  Term body = pink_ctr_arg(exp, 0);
  // Force compilation mode for this lambda
  PinkFrame fr = { .tag = PF_CLAM, .a = state.menv };
  pink_stack_push(&state, fr);
  // Evaluate as Lam in compile mode
  state.exp = pink_term_lam(body);
  state.phase = PINK_PHASE_EVALC;
  continue;
}
```

---

## Phase 6: Test Cases

**Directory**: `/home/heefoo/Documents/code/hvm4/pink/test/cases/`

### Step 6.1: em_basic.pink

```lisp
;; Simple EM: returns value from meta-level
(EM 42)
```
Expected: `42`

### Step 6.2: em_env.pink

```lisp
;; EM accessing outer environment
(let (x 10)
  (+ x (EM 5)))
```
Expected: `15`

### Step 6.3: clambda_basic.pink

```lisp
;; Basic clambda: compiled at definition
(let (f (clambda (x) (+ x 1)))
  (f 5))
```
Expected: `6`

### Step 6.4: clambda_capture.pink

```lisp
;; clambda capturing environment
(let (y 10)
  (let (f (clambda (x) (+ x y)))
    (f 5)))
```
Expected: `15`

### Step 6.5: Add test entries to test runner

Update test script to include new Purple tests.

---

## Phase 7: Documentation

**File**: `/home/heefoo/Documents/code/hvm4/docs/hvm4/pink.md`

### Step 7.1: Add Purple section

Add after Pink implementation status:
```markdown
---

# Purple on HVM4 - Reflective Extension

Purple extends Pink with reflective capabilities:

## New Forms

### EM (Execute-at-Metalevel)

```lisp
(EM expr)
```

Evaluates `expr` one level up in the tower. Allows accessing and modifying
the meta-level's environment and handlers.

### clambda

```lisp
(clambda (x y) body)
```

Compiles `body` at definition time using current (potentially modified)
semantics. Unlike regular `lambda`, the body is compiled immediately.

### get-meta / set-meta!

```lisp
(get-meta 'handler-name)
(set-meta! 'handler-name new-handler)
```

Access and modify evaluator handlers at runtime.

## Meta-Environment

Purple uses a meta-environment (`menv`) containing:
- `env`: variable bindings
- `handlers`: evaluation handlers (lit, var, lam, app, if, etc.)
- `parent`: link to meta-level (lazy)

## Tower Structure

The tower is conceptually infinite but lazily materialized. Each `EM` call
accesses or creates the parent level.
```

---

## Summary

| Phase | Steps | Files Modified |
|-------|-------|----------------|
| 1. Parser | 7 | `clang/parse/pink/_.c` |
| 2. Runtime Foundation | 4 | `pink/lib/runtime.hvm4` |
| 3. Purple Evaluator | 5 | `pink/lib/runtime.hvm4` |
| 4. Compiler | 4 | `pink/compile/_.c`, `pink/main.c` |
| 5. C Interpreter | 5 | `pink/interpreter.c` |
| 6. Tests | 5 | `pink/test/cases/*.pink` |
| 7. Documentation | 1 | `docs/hvm4/pink.md` |

**Total: 31 steps**
