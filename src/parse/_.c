// Purple S-expression parser
// Fully self-contained parser for Purple language

// =============================================================================
// Parser State and Helpers (from Pink)
// =============================================================================

// Bind stack for de Bruijn indexing
#define PURPLE_BIND_MAX 16384
static u32 PURPLE_BINDS[PURPLE_BIND_MAX];
static u32 PURPLE_BINDS_LEN = 0;

// Constructor names (nick-encoded, <= 4 chars)
static int PURPLE_NAMES_READY = 0;

// Pink AST constructor names
static u32 PURPLE_NAM_LIT;
static u32 PURPLE_NAM_SYM;
static u32 PURPLE_NAM_VAR;
static u32 PURPLE_NAM_LAM;
static u32 PURPLE_NAM_LAMR;
static u32 PURPLE_NAM_APP;
static u32 PURPLE_NAM_LET;
static u32 PURPLE_NAM_IF;
static u32 PURPLE_NAM_LFT;
static u32 PURPLE_NAM_RUN;
static u32 PURPLE_NAM_COD;
static u32 PURPLE_NAM_ADD;
static u32 PURPLE_NAM_SUB;
static u32 PURPLE_NAM_MUL;
static u32 PURPLE_NAM_EQL;
static u32 PURPLE_NAM_LT;
static u32 PURPLE_NAM_GT;
static u32 PURPLE_NAM_LE;
static u32 PURPLE_NAM_GE;
static u32 PURPLE_NAM_DIV;
static u32 PURPLE_NAM_MOD;
static u32 PURPLE_NAM_AND;
static u32 PURPLE_NAM_OR;
static u32 PURPLE_NAM_NOT;
static u32 PURPLE_NAM_CON;
static u32 PURPLE_NAM_NIL;
static u32 PURPLE_NAM_FST;
static u32 PURPLE_NAM_SND;
static u32 PURPLE_NAM_CHR;  // Character wrapper

// Purple-specific constructor names
static u32 PURPLE_NAM_EM;
static u32 PURPLE_NAM_CLAM;
static u32 PURPLE_NAM_GMTA;
static u32 PURPLE_NAM_SMTA;
static u32 PURPLE_NAM_MENV;
static u32 PURPLE_NAM_SEQ;   // Symbol equality
static u32 PURPLE_NAM_WMENV; // With-menv (evaluate with custom menv)
static u32 PURPLE_NAM_CARG;  // Constructor argument extraction
static u32 PURPLE_NAM_EVAL; // Evaluate code expression
static u32 PURPLE_NAM_MLVL; // Meta-level introspection
static u32 PURPLE_NAM_SHFT; // Shift n levels up
static u32 PURPLE_NAM_CTAG; // Constructor tag
static u32 PURPLE_NAM_MAT;  // Match expression
static u32 PURPLE_NAM_NMAT; // Native match (compile-time optimized)
static u32 PURPLE_NAM_CASE; // Match case
static u32 PURPLE_NAM_PCTR; // Pattern: constructor
static u32 PURPLE_NAM_PLIT; // Pattern: literal
static u32 PURPLE_NAM_PWLD; // Pattern: wildcard
static u32 PURPLE_NAM_PVAR; // Pattern: variable (for nested)
static u32 PURPLE_NAM_POR;  // Pattern: or-pattern (alternatives)
static u32 PURPLE_NAM_PAS;  // Pattern: as-pattern (bind name and subpattern)
static u32 PURPLE_NAM_FFI;  // Foreign function call
static u32 PURPLE_NAM_DO;   // IO sequencing
static u32 PURPLE_NAM_MOV;  // Move binding (multi-use without dup)
static u32 PURPLE_NAM_QQ;   // Quasiquote
static u32 PURPLE_NAM_UQ;   // Unquote
static u32 PURPLE_NAM_UQS;  // Unquote-splicing
// List operations
static u32 PURPLE_NAM_MAP;    // map
static u32 PURPLE_NAM_FLTR;   // filter
static u32 PURPLE_NAM_FOLD;   // fold
static u32 PURPLE_NAM_FOLDL;  // foldl (left fold)
static u32 PURPLE_NAM_LEN;    // length
static u32 PURPLE_NAM_APND;   // append
static u32 PURPLE_NAM_REV;    // reverse

// Higher-order function utilities
static u32 PURPLE_NAM_COMP;   // compose
static u32 PURPLE_NAM_FLIP;   // flip
static u32 PURPLE_NAM_APPL;   // apply (to list of args)

// Macro system
static u32 PURPLE_NAM_DEFMAC; // defmacro
static u32 PURPLE_NAM_MACEXP; // macroexpand

// Handler delegation
static u32 PURPLE_NAM_DEFH;   // DefH (default-handler)

// Error handling
static u32 PURPLE_NAM_ERRR;   // ErrR (error - raise error)
static u32 PURPLE_NAM_TRY;    // Try (try expr handler)
static u32 PURPLE_NAM_ASRT;   // Asrt (assert condition msg)
static u32 PURPLE_NAM_TRCE;   // Trce (trace expr)

// Reflection
static u32 PURPLE_NAM_RENV;   // REnv (reify-env)
static u32 PURPLE_NAM_GSYM;   // GSym (gensym n)

fn u32 purple_nick(const char *name) {
  u32 k = 0;
  for (u32 i = 0; name[i] != '\0'; i++) {
    k = ((k << 6) + nick_letter_to_b64(name[i])) & EXT_MASK;
  }
  return k;
}

fn void purple_names_init(void) {
  if (PURPLE_NAMES_READY) {
    return;
  }
  // Pink names
  PURPLE_NAM_LIT  = purple_nick("Lit");
  PURPLE_NAM_SYM  = purple_nick("Sym");
  PURPLE_NAM_VAR  = purple_nick("Var");
  PURPLE_NAM_LAM  = purple_nick("Lam");
  PURPLE_NAM_LAMR = purple_nick("LamR");
  PURPLE_NAM_APP  = purple_nick("App");
  PURPLE_NAM_LET  = purple_nick("Let");
  PURPLE_NAM_IF   = purple_nick("If");
  PURPLE_NAM_LFT  = purple_nick("Lft");
  PURPLE_NAM_RUN  = purple_nick("Run");
  PURPLE_NAM_COD  = purple_nick("Cod");
  PURPLE_NAM_ADD  = purple_nick("Add");
  PURPLE_NAM_SUB  = purple_nick("Sub");
  PURPLE_NAM_MUL  = purple_nick("Mul");
  PURPLE_NAM_EQL  = purple_nick("Eql");
  PURPLE_NAM_LT   = purple_nick("Lt");
  PURPLE_NAM_GT   = purple_nick("Gt");
  PURPLE_NAM_LE   = purple_nick("Le");
  PURPLE_NAM_GE   = purple_nick("Ge");
  PURPLE_NAM_DIV  = purple_nick("Div");
  PURPLE_NAM_MOD  = purple_nick("Mod");
  PURPLE_NAM_AND  = purple_nick("And");
  PURPLE_NAM_OR   = purple_nick("Or");
  PURPLE_NAM_NOT  = purple_nick("Not");
  PURPLE_NAM_CON  = NAM_CON;
  PURPLE_NAM_NIL  = NAM_NIL;
  PURPLE_NAM_FST  = purple_nick("Fst");
  PURPLE_NAM_SND  = purple_nick("Snd");
  PURPLE_NAM_CHR  = NAM_CHR;  // Use HVM4's NAM_CHR constant
  // Purple names
  PURPLE_NAM_EM   = purple_nick("EM");
  PURPLE_NAM_CLAM = purple_nick("CLam");
  PURPLE_NAM_GMTA = purple_nick("GMta");
  PURPLE_NAM_SMTA = purple_nick("SMta");
  PURPLE_NAM_MENV = purple_nick("MEnv");
  PURPLE_NAM_SEQ   = purple_nick("SEq");
  PURPLE_NAM_WMENV = purple_nick("WMnv");
  PURPLE_NAM_CARG  = purple_nick("CArg");
  PURPLE_NAM_EVAL  = purple_nick("Eval");
  PURPLE_NAM_MLVL  = purple_nick("MLvl");
  PURPLE_NAM_SHFT  = purple_nick("Shft");
  PURPLE_NAM_CTAG  = purple_nick("CTag");
  PURPLE_NAM_MAT   = purple_nick("Mat");
  PURPLE_NAM_NMAT  = purple_nick("NMat");
  PURPLE_NAM_CASE  = purple_nick("Case");
  PURPLE_NAM_PCTR  = purple_nick("PCtr");
  PURPLE_NAM_PLIT  = purple_nick("PLit");
  PURPLE_NAM_PWLD  = purple_nick("PWld");
  PURPLE_NAM_PVAR  = purple_nick("PVar");
  PURPLE_NAM_POR   = purple_nick("POr");
  PURPLE_NAM_PAS   = purple_nick("PAs");
  PURPLE_NAM_FFI   = purple_nick("FFI");
  PURPLE_NAM_DO    = purple_nick("Do");
  PURPLE_NAM_MOV   = purple_nick("Mov");
  PURPLE_NAM_QQ    = purple_nick("QQ");
  PURPLE_NAM_UQ    = purple_nick("UQ");
  PURPLE_NAM_UQS   = purple_nick("UQS");
  // List operations
  PURPLE_NAM_MAP   = purple_nick("Map");
  PURPLE_NAM_FLTR  = purple_nick("Fltr");
  PURPLE_NAM_FOLD  = purple_nick("Fold");
  PURPLE_NAM_FOLDL = purple_nick("FdLf");
  PURPLE_NAM_LEN   = purple_nick("Len");
  PURPLE_NAM_APND  = purple_nick("Apnd");
  PURPLE_NAM_REV   = purple_nick("Rev");
  // Higher-order function utilities
  PURPLE_NAM_COMP  = purple_nick("Comp");
  PURPLE_NAM_FLIP  = purple_nick("Flip");
  PURPLE_NAM_APPL  = purple_nick("Appl");
  // Macro system
  PURPLE_NAM_DEFMAC = purple_nick("DMac");
  PURPLE_NAM_MACEXP = purple_nick("MExp");
  // Handler delegation
  PURPLE_NAM_DEFH   = purple_nick("DefH");
  // Error handling
  PURPLE_NAM_ERRR   = purple_nick("ErrR");
  PURPLE_NAM_TRY    = purple_nick("Try");
  PURPLE_NAM_ASRT   = purple_nick("Asrt");
  PURPLE_NAM_TRCE   = purple_nick("Trce");
  PURPLE_NAM_RENV   = purple_nick("REnv");
  PURPLE_NAM_GSYM   = purple_nick("GSym");
  PURPLE_NAMES_READY = 1;
}

fn void purple_bind_push(u32 sym) {
  if (PURPLE_BINDS_LEN >= PURPLE_BIND_MAX) {
    fprintf(stderr, "PURPLE_ERROR: too many binders\n");
    exit(1);
  }
  PURPLE_BINDS[PURPLE_BINDS_LEN++] = sym;
}

fn void purple_bind_pop(u32 count) {
  while (count > 0) {
    if (PURPLE_BINDS_LEN == 0) {
      fprintf(stderr, "PURPLE_ERROR: bind stack underflow\n");
      exit(1);
    }
    PURPLE_BINDS_LEN--;
    count--;
  }
}

fn int purple_bind_lookup(u32 sym, u32 *out_idx) {
  for (int i = (int)PURPLE_BINDS_LEN - 1; i >= 0; i--) {
    if (PURPLE_BINDS[i] == sym) {
      *out_idx = (u32)(PURPLE_BINDS_LEN - 1 - (u32)i);
      return 1;
    }
  }
  return 0;
}

// Skip semicolon-style comments
fn void purple_skip_comment(PState *s) {
  while (!parse_at_end(s) && parse_peek(s) != '\n') {
    parse_advance(s);
  }
}

// Skip whitespace and comments (Lisp style with ;)
fn void purple_skip(PState *s) {
  while (!parse_at_end(s)) {
    if (parse_is_space(parse_peek(s))) {
      parse_advance(s);
      continue;
    }
    if (parse_peek(s) == ';') {
      purple_skip_comment(s);
      continue;
    }
    if (parse_starts_with(s, "//")) {
      purple_skip_comment(s);
      continue;
    }
    break;
  }
}

fn int purple_is_delim(char c) {
  if (c == '(' || c == ')') {
    return 1;
  }
  if (parse_is_space(c)) {
    return 1;
  }
  return 0;
}

fn int purple_parse_symbol_token(PState *s, u32 *out_start, u32 *out_len) {
  purple_skip(s);
  char c = parse_peek(s);
  if (c == '\0' || c == '(' || c == ')') {
    return 0;
  }
  if (isdigit(c)) {
    return 0;
  }
  u32 start = s->pos;
  while (!parse_at_end(s)) {
    c = parse_peek(s);
    if (purple_is_delim(c)) {
      break;
    }
    parse_advance(s);
  }
  u32 len = s->pos - start;
  purple_skip(s);
  if (len == 0) {
    return 0;
  }
  *out_start = start;
  *out_len   = len;
  return 1;
}

fn u32 purple_parse_number(PState *s) {
  purple_skip(s);
  u32 n = 0;
  char c = parse_peek(s);
  if (!isdigit(c)) {
    parse_error(s, "number", c);
  }
  while (!parse_at_end(s) && isdigit(parse_peek(s))) {
    c = parse_peek(s);
    n = (u32)(n * 10 + (u32)(c - '0'));
    parse_advance(s);
  }
  purple_skip(s);
  return n;
}

fn int purple_symbol_is(PState *s, u32 start, u32 len, const char *lit) {
  u32 lit_len = (u32)strlen(lit);
  if (len != lit_len) {
    return 0;
  }
  if (memcmp(s->src + start, lit, len) != 0) {
    return 0;
  }
  return 1;
}

// =============================================================================
// Term Constructors
// =============================================================================

fn Term purple_ctr0(u32 nam) {
  return term_new_ctr(nam, 0, NULL);
}

fn Term purple_ctr1(u32 nam, Term a) {
  Term args[1];
  args[0] = a;
  return term_new_ctr(nam, 1, args);
}

fn Term purple_ctr2(u32 nam, Term a, Term b) {
  Term args[2];
  args[0] = a;
  args[1] = b;
  return term_new_ctr(nam, 2, args);
}

fn Term purple_ctr3(u32 nam, Term a, Term b, Term c) {
  Term args[3];
  args[0] = a;
  args[1] = b;
  args[2] = c;
  return term_new_ctr(nam, 3, args);
}

// Pink AST constructors
fn Term purple_term_lit(u32 n) {
  return purple_ctr1(PURPLE_NAM_LIT, term_new_num(n));
}

fn Term purple_term_sym(u32 sym_id) {
  // Store symbol as a number (nick-encoded ID), not a reference
  return purple_ctr1(PURPLE_NAM_SYM, term_new_num(sym_id));
}

fn Term purple_term_var(u32 idx) {
  return purple_ctr1(PURPLE_NAM_VAR, term_new_num(idx));
}

fn Term purple_term_lam(Term body) {
  return purple_ctr1(PURPLE_NAM_LAM, body);
}

fn Term purple_term_lamr(Term body) {
  return purple_ctr1(PURPLE_NAM_LAMR, body);
}

fn Term purple_term_app(Term fun, Term arg) {
  return purple_ctr2(PURPLE_NAM_APP, fun, arg);
}

fn Term purple_term_let(Term val, Term body) {
  return purple_ctr2(PURPLE_NAM_LET, val, body);
}

fn Term purple_term_mov(Term val, Term body) {
  return purple_ctr2(PURPLE_NAM_MOV, val, body);
}

fn Term purple_term_if(Term c, Term t, Term e) {
  return purple_ctr3(PURPLE_NAM_IF, c, t, e);
}

fn Term purple_term_lft(Term e) {
  return purple_ctr1(PURPLE_NAM_LFT, e);
}

fn Term purple_term_run(Term b, Term e) {
  return purple_ctr2(PURPLE_NAM_RUN, b, e);
}

fn Term purple_term_cod(Term e) {
  return purple_ctr1(PURPLE_NAM_COD, e);
}

fn Term purple_term_add(Term a, Term b) {
  return purple_ctr2(PURPLE_NAM_ADD, a, b);
}

fn Term purple_term_sub(Term a, Term b) {
  return purple_ctr2(PURPLE_NAM_SUB, a, b);
}

fn Term purple_term_mul(Term a, Term b) {
  return purple_ctr2(PURPLE_NAM_MUL, a, b);
}

fn Term purple_term_eql(Term a, Term b) {
  return purple_ctr2(PURPLE_NAM_EQL, a, b);
}

fn Term purple_term_lt(Term a, Term b) {
  return purple_ctr2(PURPLE_NAM_LT, a, b);
}

fn Term purple_term_gt(Term a, Term b) {
  return purple_ctr2(PURPLE_NAM_GT, a, b);
}

fn Term purple_term_le(Term a, Term b) {
  return purple_ctr2(PURPLE_NAM_LE, a, b);
}

fn Term purple_term_ge(Term a, Term b) {
  return purple_ctr2(PURPLE_NAM_GE, a, b);
}

fn Term purple_term_div(Term a, Term b) {
  return purple_ctr2(PURPLE_NAM_DIV, a, b);
}

fn Term purple_term_mod(Term a, Term b) {
  return purple_ctr2(PURPLE_NAM_MOD, a, b);
}

fn Term purple_term_and(Term a, Term b) {
  return purple_ctr2(PURPLE_NAM_AND, a, b);
}

fn Term purple_term_or(Term a, Term b) {
  return purple_ctr2(PURPLE_NAM_OR, a, b);
}

fn Term purple_term_not(Term a) {
  return purple_ctr1(PURPLE_NAM_NOT, a);
}

fn Term purple_term_con(Term a, Term b) {
  return purple_ctr2(PURPLE_NAM_CON, a, b);
}

fn Term purple_term_nil(void) {
  return purple_ctr0(PURPLE_NAM_NIL);
}

fn Term purple_term_fst(Term p) {
  return purple_ctr1(PURPLE_NAM_FST, p);
}

fn Term purple_term_snd(Term p) {
  return purple_ctr1(PURPLE_NAM_SND, p);
}

fn Term purple_term_chr(u32 codepoint) {
  return purple_ctr1(PURPLE_NAM_CHR, term_new_num(codepoint));
}

// Purple-specific constructors
fn Term purple_term_em(Term e) {
  return purple_ctr1(PURPLE_NAM_EM, e);
}

fn Term purple_term_clam(Term body) {
  return purple_ctr1(PURPLE_NAM_CLAM, body);
}

fn Term purple_term_gmta(Term key) {
  return purple_ctr1(PURPLE_NAM_GMTA, key);
}

fn Term purple_term_smta(Term key, Term val) {
  return purple_ctr2(PURPLE_NAM_SMTA, key, val);
}

// Quasiquote: `expr
fn Term purple_term_qq(Term expr) {
  return purple_ctr1(PURPLE_NAM_QQ, expr);
}

// Unquote: ,expr
fn Term purple_term_uq(Term expr) {
  return purple_ctr1(PURPLE_NAM_UQ, expr);
}

// Unquote-splicing: ,@expr
fn Term purple_term_uqs(Term expr) {
  return purple_ctr1(PURPLE_NAM_UQS, expr);
}

// List operations
fn Term purple_term_map(Term func, Term lst) {
  return purple_ctr2(PURPLE_NAM_MAP, func, lst);
}

fn Term purple_term_filter(Term func, Term lst) {
  return purple_ctr2(PURPLE_NAM_FLTR, func, lst);
}

fn Term purple_term_fold(Term func, Term init, Term lst) {
  return purple_ctr3(PURPLE_NAM_FOLD, func, init, lst);
}

fn Term purple_term_foldl(Term func, Term init, Term lst) {
  return purple_ctr3(PURPLE_NAM_FOLDL, func, init, lst);
}

fn Term purple_term_length(Term lst) {
  return purple_ctr1(PURPLE_NAM_LEN, lst);
}

fn Term purple_term_append(Term a, Term b) {
  return purple_ctr2(PURPLE_NAM_APND, a, b);
}

fn Term purple_term_reverse(Term lst) {
  return purple_ctr1(PURPLE_NAM_REV, lst);
}

// Higher-order function utilities
fn Term purple_term_compose(Term f, Term g) {
  return purple_ctr2(PURPLE_NAM_COMP, f, g);
}

fn Term purple_term_flip(Term f) {
  return purple_ctr1(PURPLE_NAM_FLIP, f);
}

fn Term purple_term_apply_list(Term func, Term args) {
  return purple_ctr2(PURPLE_NAM_APPL, func, args);
}

// Macro system
fn Term purple_term_defmacro(Term name, Term params, Term body) {
  return purple_ctr3(PURPLE_NAM_DEFMAC, name, params, body);
}

fn Term purple_term_macroexpand(Term expr) {
  return purple_ctr1(PURPLE_NAM_MACEXP, expr);
}

// Handler delegation: (default-handler 'name arg) -> calls default handler
fn Term purple_term_defh(Term name, Term arg) {
  return purple_ctr2(PURPLE_NAM_DEFH, name, arg);
}

// Error: (error msg) -> raises an error
fn Term purple_term_err(Term msg) {
  return purple_ctr1(PURPLE_NAM_ERRR, msg);
}

// Try: (try expr handler) -> catch errors with handler lambda
fn Term purple_term_try(Term expr, Term handler) {
  return purple_ctr2(PURPLE_NAM_TRY, expr, handler);
}

// Assert: (assert cond msg) -> error if cond is false
fn Term purple_term_assert(Term cond, Term msg) {
  return purple_ctr2(PURPLE_NAM_ASRT, cond, msg);
}

// Trace: (trace expr) -> evaluates and returns #Traced{value}
fn Term purple_term_trace(Term expr) {
  return purple_ctr1(PURPLE_NAM_TRCE, expr);
}

// Reify-env: (reify-env) -> returns current environment
fn Term purple_term_renv(void) {
  return purple_ctr0(PURPLE_NAM_RENV);
}

// Gensym: (gensym n) -> generate unique symbol
fn Term purple_term_gensym(Term n) {
  return purple_ctr1(PURPLE_NAM_GSYM, n);
}

fn Term purple_term_seq(Term a, Term b) {
  return purple_ctr2(PURPLE_NAM_SEQ, a, b);
}

fn Term purple_term_wmenv(Term menv_expr, Term body) {
  return purple_ctr2(PURPLE_NAM_WMENV, menv_expr, body);
}

fn Term purple_term_carg(Term ctr, Term idx) {
  return purple_ctr2(PURPLE_NAM_CARG, ctr, idx);
}

fn Term purple_term_eval(Term e) {
  return purple_ctr1(PURPLE_NAM_EVAL, e);
}

fn Term purple_term_mlvl(void) {
  return purple_ctr0(PURPLE_NAM_MLVL);
}

fn Term purple_term_shft(Term n, Term e) {
  return purple_ctr2(PURPLE_NAM_SHFT, n, e);
}

fn Term purple_term_ctag(Term e) {
  return purple_ctr1(PURPLE_NAM_CTAG, e);
}

// Pattern matching constructors
// #Mat{expr, cases} where cases is a list of #Case{pattern, body}
fn Term purple_term_mat(Term expr, Term cases) {
  return purple_ctr2(PURPLE_NAM_MAT, expr, cases);
}

// #Case{pattern, guard, body} - guard can be #NIL for no guard
fn Term purple_term_case(Term pattern, Term guard, Term body) {
  return purple_ctr3(PURPLE_NAM_CASE, pattern, guard, body);
}

// #PCtr{tag_sym, args_list} - constructor pattern
fn Term purple_term_pctr(Term tag, Term args) {
  return purple_ctr2(PURPLE_NAM_PCTR, tag, args);
}

// #PLit{n} - literal pattern
fn Term purple_term_plit(Term n) {
  return purple_ctr1(PURPLE_NAM_PLIT, n);
}

// #PWld - wildcard pattern
fn Term purple_term_pwld(void) {
  return purple_ctr0(PURPLE_NAM_PWLD);
}

// #PVar{name} - variable pattern (binds a name)
fn Term purple_term_pvar(u32 name) {
  return purple_ctr1(PURPLE_NAM_PVAR, term_new_num(name));
}

// #POr{alternatives} - or-pattern (list of alternative patterns)
fn Term purple_term_por(Term alternatives) {
  return purple_ctr1(PURPLE_NAM_POR, alternatives);
}

// #PAs{name, subpattern} - as-pattern (bind name and match subpattern)
fn Term purple_term_pas(Term name, Term subpattern) {
  return purple_ctr2(PURPLE_NAM_PAS, name, subpattern);
}

// #FFI{name, args} - foreign function call
// name is a nick-encoded symbol, args is a list
fn Term purple_term_ffi(Term name, Term args) {
  return purple_ctr2(PURPLE_NAM_FFI, name, args);
}

// #Do{first, rest} - IO sequencing
// Evaluates first, discards result (unless it's the last), evaluates rest
fn Term purple_term_do(Term first, Term rest) {
  return purple_ctr2(PURPLE_NAM_DO, first, rest);
}

// =============================================================================
// Parser Functions
// =============================================================================

fn Term purple_symbol_term(PState *s, u32 start, u32 len) {
  u32 table_id = table_find(s->src + start, len);
  u32 idx    = 0;
  if (purple_bind_lookup(table_id, &idx)) {
    return purple_term_var(idx);
  }
  if (purple_symbol_is(s, start, len, "nil")) {
    return purple_term_nil();
  }
  // For unbound symbols, use nick encoding (like quoted symbols)
  // Nick-encode the symbol (max 4 chars)
  u32 nick = 0;
  for (u32 i = 0; i < len && i < 4; i++) {
    nick = ((nick << 6) + nick_letter_to_b64(s->src[start + i])) & EXT_MASK;
  }
  return purple_term_sym(nick);
}

fn Term parse_purple_form(PState *s);

fn Term purple_parse_app_rest(PState *s, Term head) {
  while (1) {
    purple_skip(s);
    if (parse_match(s, ")")) {
      break;
    }
    Term arg = parse_purple_form(s);
    head = purple_term_app(head, arg);
  }
  return head;
}

fn Term purple_parse_lambda(PState *s) {
  purple_skip(s);
  u32 self_id = 0;
  int has_self = 0;
  if (parse_peek(s) != '(') {
    u32 start = 0;
    u32 len   = 0;
    if (!purple_parse_symbol_token(s, &start, &len)) {
      parse_error(s, "parameter list", parse_peek(s));
    }
    self_id = table_find(s->src + start, len);
    has_self = 1;
  }
  parse_consume(s, "(");
  u32 params[256];
  u32 count = 0;
  purple_skip(s);
  if (parse_match(s, ")")) {
    parse_error(s, "parameter", ')');
  }
  while (1) {
    u32 start = 0;
    u32 len   = 0;
    if (!purple_parse_symbol_token(s, &start, &len)) {
      parse_error(s, "parameter", parse_peek(s));
    }
    if (count >= 256) {
      fprintf(stderr, "PURPLE_ERROR: too many parameters\n");
      exit(1);
    }
    params[count++] = table_find(s->src + start, len);
    purple_skip(s);
    if (parse_match(s, ")")) {
      break;
    }
  }
  if (count == 0) {
    fprintf(stderr, "PURPLE_ERROR: lambda requires at least one parameter\n");
    exit(1);
  }
  if (has_self) {
    purple_bind_push(self_id);
  }
  for (u32 i = 0; i < count; i++) {
    purple_bind_push(params[i]);
  }
  Term body = parse_purple_form(s);
  purple_bind_pop(count + (has_self ? 1 : 0));
  parse_consume(s, ")");
  for (u32 i = 0; i < count; i++) {
    if (has_self && i == count - 1) {
      body = purple_term_lamr(body);
    } else {
      body = purple_term_lam(body);
    }
  }
  return body;
}

fn Term purple_parse_let(PState *s) {
  parse_consume(s, "(");
  u32 start = 0;
  u32 len   = 0;
  if (!purple_parse_symbol_token(s, &start, &len)) {
    parse_error(s, "binding name", parse_peek(s));
  }
  u32 sym_id = table_find(s->src + start, len);
  Term val = parse_purple_form(s);
  parse_consume(s, ")");
  purple_bind_push(sym_id);
  Term body = parse_purple_form(s);
  purple_bind_pop(1);
  parse_consume(s, ")");
  return purple_term_let(val, body);
}

// mov: (mov (x value) body) - multi-use binding without dup overhead
// The variable x can be used multiple times in body
fn Term purple_parse_mov(PState *s) {
  parse_consume(s, "(");
  u32 start = 0;
  u32 len   = 0;
  if (!purple_parse_symbol_token(s, &start, &len)) {
    parse_error(s, "binding name", parse_peek(s));
  }
  u32 sym_id = table_find(s->src + start, len);
  Term val = parse_purple_form(s);
  parse_consume(s, ")");
  purple_bind_push(sym_id);
  Term body = parse_purple_form(s);
  purple_bind_pop(1);
  parse_consume(s, ")");
  return purple_term_mov(val, body);
}

// letrec: (letrec ((name (lambda (x) body))) expr)
// Transforms to: (let (name (lambda name (x) body)) expr)
// The lambda becomes a recursive lambda with name as self-reference
fn Term purple_parse_letrec(PState *s) {
  parse_consume(s, "(");  // outer paren of bindings list
  parse_consume(s, "(");  // inner paren of first binding
  u32 name_start = 0;
  u32 name_len   = 0;
  if (!purple_parse_symbol_token(s, &name_start, &name_len)) {
    parse_error(s, "binding name", parse_peek(s));
  }
  u32 name_id = table_find(s->src + name_start, name_len);

  // Expect a lambda expression: (lambda (params...) body)
  purple_skip(s);
  if (!parse_match(s, "(")) {
    parse_error(s, "(lambda ...)", parse_peek(s));
  }
  purple_skip(s);
  u32 kw_start = 0;
  u32 kw_len = 0;
  if (!purple_parse_symbol_token(s, &kw_start, &kw_len)) {
    parse_error(s, "lambda", parse_peek(s));
  }
  if (!purple_symbol_is(s, kw_start, kw_len, "lambda")) {
    parse_error(s, "lambda", parse_peek(s));
  }

  // Parse the lambda with name_id as self-reference
  purple_skip(s);
  parse_consume(s, "(");
  u32 params[256];
  u32 count = 0;
  purple_skip(s);
  if (parse_match(s, ")")) {
    parse_error(s, "parameter", ')');
  }
  while (1) {
    u32 start = 0;
    u32 len   = 0;
    if (!purple_parse_symbol_token(s, &start, &len)) {
      parse_error(s, "parameter", parse_peek(s));
    }
    if (count >= 256) {
      fprintf(stderr, "PURPLE_ERROR: too many parameters\n");
      exit(1);
    }
    params[count++] = table_find(s->src + start, len);
    purple_skip(s);
    if (parse_match(s, ")")) {
      break;
    }
  }
  if (count == 0) {
    fprintf(stderr, "PURPLE_ERROR: letrec lambda requires at least one parameter\n");
    exit(1);
  }

  // Push self-reference (name_id) first, then parameters
  purple_bind_push(name_id);  // self reference at depth = count
  for (u32 i = 0; i < count; i++) {
    purple_bind_push(params[i]);
  }
  Term lam_body = parse_purple_form(s);
  purple_bind_pop(count + 1);
  parse_consume(s, ")");  // close lambda
  parse_consume(s, ")");  // close binding (inner paren)
  parse_consume(s, ")");  // close bindings list (outer paren)

  // Build recursive lambda: nested lambdas with outermost being LamR
  for (u32 i = 0; i < count; i++) {
    if (i == count - 1) {
      lam_body = purple_term_lamr(lam_body);
    } else {
      lam_body = purple_term_lam(lam_body);
    }
  }

  // Now parse the expression with name bound
  purple_bind_push(name_id);
  Term expr = parse_purple_form(s);
  purple_bind_pop(1);
  parse_consume(s, ")");

  return purple_term_let(lam_body, expr);
}

// defmacro: (defmacro name (params...) body expr)
// Defines a macro transformer and evaluates expr with it available
// The macro takes quoted syntax arguments and returns new syntax
// Usage: (eval (macro-name (quote arg1) (quote arg2)))
fn Term purple_parse_defmacro(PState *s) {
  purple_skip(s);
  // Parse macro name
  u32 name_start = 0;
  u32 name_len = 0;
  if (!purple_parse_symbol_token(s, &name_start, &name_len)) {
    parse_error(s, "macro name", parse_peek(s));
  }
  u32 name_id = table_find(s->src + name_start, name_len);

  // Parse params list
  purple_skip(s);
  parse_consume(s, "(");

  u32 params[256];
  u32 count = 0;
  while (1) {
    purple_skip(s);
    if (parse_match(s, ")")) {
      break;
    }
    u32 start = 0;
    u32 len = 0;
    if (!purple_parse_symbol_token(s, &start, &len)) {
      parse_error(s, "parameter name", parse_peek(s));
    }
    if (count >= 256) {
      fprintf(stderr, "PURPLE_ERROR: too many macro parameters\n");
      exit(1);
    }
    params[count++] = table_find(s->src + start, len);
  }

  // Push params onto binding stack (for body parsing)
  for (u32 i = 0; i < count; i++) {
    purple_bind_push(params[i]);
  }

  // Parse macro body (the transformer)
  Term body = parse_purple_form(s);
  purple_bind_pop(count);

  // Build lambda for macro transformer
  // Wrap body in nested lambdas for each param
  Term transformer = body;
  for (u32 i = count; i > 0; i--) {
    transformer = purple_term_lam(transformer);
  }

  // Now parse the expression where macro is used
  purple_bind_push(name_id);  // macro name is bound
  Term expr = parse_purple_form(s);
  purple_bind_pop(1);

  parse_consume(s, ")");

  // Return let binding: (let (name transformer) expr)
  return purple_term_let(transformer, expr);
}

// mcall: (mcall macro-name arg1 arg2 ...)
// Calls a macro by quoting args and evaluating the result
// Transforms to: (eval (mac (quote arg1) (quote arg2) ...))
fn Term purple_parse_mcall(PState *s) {
  // Parse macro expression
  Term mac = parse_purple_form(s);

  // Parse args and quote each one
  purple_skip(s);
  Term app = mac;
  while (!parse_match(s, ")")) {
    Term arg = parse_purple_form(s);
    // Quote the arg: #Cod{arg}
    Term quoted = purple_term_cod(arg);
    // Apply: (mac quoted-arg)
    app = purple_term_app(app, quoted);
    purple_skip(s);
  }

  // Wrap in eval
  return purple_term_eval(app);
}

// macroexpand: (macroexpand expr) - expand macros in expr without evaluating
fn Term purple_parse_macroexpand(PState *s) {
  Term expr = parse_purple_form(s);
  parse_consume(s, ")");
  return purple_term_macroexpand(expr);
}

fn Term purple_parse_if(PState *s) {
  Term cond = parse_purple_form(s);
  Term tval = parse_purple_form(s);
  Term fval = parse_purple_form(s);
  parse_consume(s, ")");
  return purple_term_if(cond, tval, fval);
}

fn Term purple_parse_lift(PState *s) {
  Term val = parse_purple_form(s);
  parse_consume(s, ")");
  return purple_term_lft(val);
}

fn Term purple_parse_run(PState *s) {
  Term b = parse_purple_form(s);
  Term e = parse_purple_form(s);
  parse_consume(s, ")");
  return purple_term_run(b, e);
}

fn Term purple_parse_code(PState *s) {
  Term e = parse_purple_form(s);
  parse_consume(s, ")");
  return purple_term_cod(e);
}

fn Term purple_parse_em(PState *s) {
  Term e = parse_purple_form(s);
  parse_consume(s, ")");
  return purple_term_em(e);
}

fn Term purple_parse_clambda(PState *s) {
  purple_skip(s);
  parse_consume(s, "(");
  u32 params[256];
  u32 count = 0;
  purple_skip(s);
  if (parse_match(s, ")")) {
    parse_error(s, "parameter", ')');
  }
  while (1) {
    u32 start = 0;
    u32 len   = 0;
    if (!purple_parse_symbol_token(s, &start, &len)) {
      parse_error(s, "parameter", parse_peek(s));
    }
    if (count >= 256) {
      fprintf(stderr, "PURPLE_ERROR: too many parameters\n");
      exit(1);
    }
    params[count++] = table_find(s->src + start, len);
    purple_skip(s);
    if (parse_match(s, ")")) {
      break;
    }
  }
  if (count == 0) {
    fprintf(stderr, "PURPLE_ERROR: clambda requires at least one parameter\n");
    exit(1);
  }
  for (u32 i = 0; i < count; i++) {
    purple_bind_push(params[i]);
  }
  Term body = parse_purple_form(s);
  purple_bind_pop(count);
  parse_consume(s, ")");
  // Wrap in nested lambdas, outermost is CLam
  for (u32 i = 0; i < count - 1; i++) {
    body = purple_term_lam(body);
  }
  body = purple_term_clam(body);
  return body;
}

fn Term purple_parse_getmeta(PState *s) {
  Term key = parse_purple_form(s);
  parse_consume(s, ")");
  return purple_term_gmta(key);
}

fn Term purple_parse_setmeta(PState *s) {
  Term key = parse_purple_form(s);
  Term val = parse_purple_form(s);
  parse_consume(s, ")");
  return purple_term_smta(key, val);
}

// (default-handler 'name arg) -> call default handler by name
fn Term purple_parse_defh(PState *s) {
  Term name = parse_purple_form(s);
  Term arg = parse_purple_form(s);
  parse_consume(s, ")");
  return purple_term_defh(name, arg);
}

// (error msg) -> raise an error
fn Term purple_parse_error(PState *s) {
  Term msg = parse_purple_form(s);
  parse_consume(s, ")");
  return purple_term_err(msg);
}

// (try expr handler) -> catch errors with handler
fn Term purple_parse_try(PState *s) {
  Term expr = parse_purple_form(s);
  Term handler = parse_purple_form(s);
  parse_consume(s, ")");
  return purple_term_try(expr, handler);
}

// (assert cond msg) -> raise error if cond is false
fn Term purple_parse_assert(PState *s) {
  Term cond = parse_purple_form(s);
  Term msg = parse_purple_form(s);
  parse_consume(s, ")");
  return purple_term_assert(cond, msg);
}

// (trace expr) -> evaluate and return #Traced{value}
fn Term purple_parse_trace(PState *s) {
  Term expr = parse_purple_form(s);
  parse_consume(s, ")");
  return purple_term_trace(expr);
}

// (reify-env) -> return current environment
fn Term purple_parse_renv(PState *s) {
  parse_consume(s, ")");
  return purple_term_renv();
}

// (gensym n) -> generate unique symbol
fn Term purple_parse_gensym(PState *s) {
  Term n = parse_purple_form(s);
  parse_consume(s, ")");
  return purple_term_gensym(n);
}

fn Term purple_parse_symeq(PState *s) {
  Term a = parse_purple_form(s);
  Term b = parse_purple_form(s);
  parse_consume(s, ")");
  return purple_term_seq(a, b);
}

fn Term purple_parse_withmenv(PState *s) {
  Term menv_expr = parse_purple_form(s);
  Term body = parse_purple_form(s);
  parse_consume(s, ")");
  return purple_term_wmenv(menv_expr, body);
}

// Parse with-handlers: (with-handlers ((name1 handler1) (name2 handler2) ...) body)
// Expands to nested with-menv/set-meta! calls
fn Term purple_parse_withhandlers(PState *s) {
  purple_skip(s);
  parse_consume(s, "(");  // Start of bindings list

  // Collect all handler bindings into a stack
  #define MAX_HANDLERS 32
  u32 names[MAX_HANDLERS];
  Term handlers[MAX_HANDLERS];
  u32 count = 0;

  purple_skip(s);
  while (parse_peek(s) != ')') {
    if (count >= MAX_HANDLERS) {
      fprintf(stderr, "Error: too many handlers in with-handlers\n");
      exit(1);
    }

    parse_consume(s, "(");  // Start of (name handler) pair
    purple_skip(s);

    // Parse handler name (a symbol like lit, add, etc.)
    // Use nick encoding to get the symbol value (like quoted symbols)
    u32 name_start = s->pos;
    while (!parse_at_end(s) && !purple_is_delim(parse_peek(s))) {
      parse_advance(s);
    }
    u32 name_len = s->pos - name_start;

    // Nick-encode the handler name (max 4 chars, like quoted symbols)
    u32 nick = 0;
    for (u32 i = 0; i < name_len && i < 4; i++) {
      nick = ((nick << 6) + nick_letter_to_b64(s->src[name_start + i])) & EXT_MASK;
    }
    names[count] = nick;

    // Parse handler expression
    handlers[count] = parse_purple_form(s);

    purple_skip(s);
    parse_consume(s, ")");  // End of (name handler) pair

    count++;
    purple_skip(s);
  }
  parse_consume(s, ")");  // End of bindings list

  // Parse body
  Term body = parse_purple_form(s);
  parse_consume(s, ")");  // End of with-handlers

  // Build nested with-menv/set-meta! from inside out
  // (with-handlers ((a h1) (b h2)) body) =>
  // (with-menv (set-meta! 'a h1) (with-menv (set-meta! 'b h2) body))
  Term result = body;
  for (int i = count - 1; i >= 0; i--) {
    // Build (set-meta! 'name handler) with nick-encoded symbol
    Term quoted_name = purple_term_sym(names[i]);
    Term set_meta = purple_term_smta(quoted_name, handlers[i]);
    // Wrap in with-menv
    result = purple_term_wmenv(set_meta, result);
  }

  return result;
}

fn Term purple_parse_ctrarg(PState *s) {
  Term ctr = parse_purple_form(s);
  Term idx = parse_purple_form(s);
  parse_consume(s, ")");
  return purple_term_carg(ctr, idx);
}

fn Term purple_parse_eval(PState *s) {
  Term e = parse_purple_form(s);
  parse_consume(s, ")");
  return purple_term_eval(e);
}

fn Term purple_parse_mlvl(PState *s) {
  parse_consume(s, ")");
  return purple_term_mlvl();
}

fn Term purple_parse_shift(PState *s) {
  Term n = parse_purple_form(s);
  Term e = parse_purple_form(s);
  parse_consume(s, ")");
  return purple_term_shft(n, e);
}

fn Term purple_parse_ctag(PState *s) {
  Term e = parse_purple_form(s);
  parse_consume(s, ")");
  return purple_term_ctag(e);
}

// Forward declaration
fn Term parse_purple_form(PState *s);

// Parse a pattern: (Tag args...) or _ or literal
// Returns: #PCtr{tag, args_list} or #PWld or #PLit{n}
fn Term purple_parse_pattern(PState *s) {
  purple_skip(s);
  char c = parse_peek(s);

  // Wildcard: _
  if (c == '_') {
    parse_advance(s);
    purple_skip(s);
    return purple_term_pwld();
  }

  // Literal number
  if (isdigit(c)) {
    u32 n = purple_parse_number(s);
    return purple_term_plit(term_new_num(n));
  }

  // Constructor, grouped pattern, or-pattern: (Tag arg1 arg2 ...) or (var) or (or pat1 pat2 ...)
  // If first symbol is lowercase and followed by ), treat as variable pattern
  // If first symbol is "or", treat as or-pattern
  // Otherwise, treat as constructor pattern
  if (c == '(') {
    parse_advance(s);
    purple_skip(s);

    // Get first symbol
    u32 tag_start = 0;
    u32 tag_len = 0;
    if (!purple_parse_symbol_token(s, &tag_start, &tag_len)) {
      parse_error(s, "constructor name or variable", parse_peek(s));
    }

    // Check if it's an or-pattern: (or pat1 pat2 ...)
    if (purple_symbol_is(s, tag_start, tag_len, "or")) {
      // Parse alternative patterns
      Term or_patterns[16];
      u32 or_count = 0;
      purple_skip(s);
      while (!parse_match(s, ")")) {
        if (or_count >= 16) {
          fprintf(stderr, "PURPLE_ERROR: too many or-pattern alternatives\n");
          exit(1);
        }
        or_patterns[or_count++] = purple_parse_pattern(s);
        purple_skip(s);
      }
      if (or_count == 0) {
        fprintf(stderr, "PURPLE_ERROR: or-pattern requires at least one alternative\n");
        exit(1);
      }
      // Build list of alternatives
      Term alts = purple_term_nil();
      for (int i = (int)or_count - 1; i >= 0; i--) {
        alts = purple_term_con(or_patterns[i], alts);
      }
      return purple_term_por(alts);
    }

    // Check if it's a list pattern: (list a b ...) or (list a b . rest)
    if (purple_symbol_is(s, tag_start, tag_len, "list")) {
      Term elem_patterns[16];
      u32 elem_count = 0;
      Term rest_pattern = 0; // 0 means no rest, otherwise the rest pattern
      purple_skip(s);
      while (!parse_match(s, ")")) {
        if (elem_count >= 16) {
          fprintf(stderr, "PURPLE_ERROR: too many list pattern elements\n");
          exit(1);
        }
        // Check for rest pattern: . rest
        if (parse_peek(s) == '.') {
          parse_advance(s); // consume .
          purple_skip(s);
          rest_pattern = purple_parse_pattern(s);
          purple_skip(s);
          parse_consume(s, ")");
          break;
        }
        elem_patterns[elem_count++] = purple_parse_pattern(s);
        purple_skip(s);
      }
      // Build nested CON/NIL pattern from right to left
      // If rest_pattern exists, use it as the tail; otherwise use NIL
      Term result;
      if (rest_pattern) {
        result = rest_pattern;
      } else {
        // NIL pattern (empty list terminator)
        result = purple_term_pctr(term_new_num(166118), purple_term_nil()); // 166118 = nick("NIL")
      }
      for (int i = (int)elem_count - 1; i >= 0; i--) {
        // CON pattern: #PCtr{121448, [elem, rest]}
        Term args = purple_term_con(elem_patterns[i], purple_term_con(result, purple_term_nil()));
        result = purple_term_pctr(term_new_num(121448), args); // 121448 = nick("CON")
      }
      return result;
    }

    // Check if it's a lowercase-starting symbol (potential variable)
    char first_char = s->src[tag_start];
    int is_lowercase_start = (first_char >= 'a' && first_char <= 'z');

    // Look ahead to see if this is immediately followed by )
    purple_skip(s);
    if (is_lowercase_start && parse_peek(s) == ')') {
      // It's (var) - a variable pattern with optional grouping parens
      parse_advance(s); // consume )
      u32 name = table_find(s->src + tag_start, tag_len);
      return purple_term_pvar(name);
    }

    // Check for as-pattern: (name @ subpattern)
    if (is_lowercase_start && parse_peek(s) == '@') {
      parse_advance(s); // consume @
      purple_skip(s);
      u32 name = table_find(s->src + tag_start, tag_len);
      Term subpattern = purple_parse_pattern(s);
      purple_skip(s);
      parse_consume(s, ")");
      return purple_term_pas(term_new_num(name), subpattern);
    }

    // Otherwise it's a constructor pattern: (Tag arg1 arg2 ...)
    // Nick-encode the tag (for matching against HVM4 constructors)
    u32 tag_nick = 0;
    for (u32 i = 0; i < tag_len && i < 4; i++) {
      tag_nick = ((tag_nick << 6) + nick_letter_to_b64(s->src[tag_start + i])) & EXT_MASK;
    }
    Term tag_sym = term_new_num(tag_nick);

    // Collect argument patterns (recursively parsed)
    Term arg_patterns[16];
    u32 arg_count = 0;

    while (!parse_match(s, ")")) {
      if (arg_count >= 16) {
        fprintf(stderr, "PURPLE_ERROR: too many pattern arguments\n");
        exit(1);
      }
      // Recursively parse sub-pattern
      arg_patterns[arg_count++] = purple_parse_pattern(s);
      purple_skip(s);
    }

    // Build args list as nested CON (in reverse for proper order)
    Term args = purple_term_nil();
    for (int i = (int)arg_count - 1; i >= 0; i--) {
      args = purple_term_con(arg_patterns[i], args);
    }

    return purple_term_pctr(tag_sym, args);
  }

  // Symbol - treat as variable pattern (like wildcard but named)
  u32 start = 0;
  u32 len = 0;
  if (purple_parse_symbol_token(s, &start, &len)) {
    u32 name = table_find(s->src + start, len);
    return purple_term_pvar(name);
  }

  parse_error(s, "pattern", c);
  return 0;
}

// Recursively extract and bind variable names from a pattern
// Returns the count of bound variables
fn void purple_collect_pattern_vars(Term pattern, u32 *vars, u32 *count, u32 max) {
  u8 ptag = term_tag(pattern);
  if (!(ptag >= C00 && ptag <= C16)) {
    return;
  }

  u32 pnam = term_ext(pattern);

  if (pnam == PURPLE_NAM_PVAR) {
    Term name_term = HEAP[term_val(pattern)];
    if (term_tag(name_term) == NUM) {
      if (*count >= max) {
        fprintf(stderr, "PURPLE_ERROR: too many pattern variables\n");
        exit(1);
      }
      vars[(*count)++] = term_val(name_term);
    }
  } else if (pnam == PURPLE_NAM_PCTR) {
    Term args = HEAP[term_val(pattern) + 1];
    while (term_tag(args) >= C00 && term_tag(args) <= C16 &&
           term_ext(args) == PURPLE_NAM_CON) {
      u32 loc = term_val(args);
      Term sub_pattern = HEAP[loc];
      purple_collect_pattern_vars(sub_pattern, vars, count, max);
      args = HEAP[loc + 1];
    }
  } else if (pnam == PURPLE_NAM_POR) {
    Term alts = HEAP[term_val(pattern)];
    if (term_tag(alts) >= C00 && term_tag(alts) <= C16 &&
        term_ext(alts) == PURPLE_NAM_CON) {
      u32 loc = term_val(alts);
      Term first_pattern = HEAP[loc];
      purple_collect_pattern_vars(first_pattern, vars, count, max);
    }
  } else if (pnam == PURPLE_NAM_PAS) {
    u32 loc = term_val(pattern);
    Term name_term = HEAP[loc];
    if (term_tag(name_term) == NUM) {
      if (*count >= max) {
        fprintf(stderr, "PURPLE_ERROR: too many pattern variables\n");
        exit(1);
      }
      vars[(*count)++] = term_val(name_term);
    }
    Term subpattern = HEAP[loc + 1];
    purple_collect_pattern_vars(subpattern, vars, count, max);
  }
}

fn u32 purple_bind_pattern_vars(Term pattern) {
  u8 ptag = term_tag(pattern);
  if (!(ptag >= C00 && ptag <= C16)) {
    return 0;
  }

  u32 pnam = term_ext(pattern);
  u32 count = 0;

  if (pnam == PURPLE_NAM_PVAR) {
    // Variable pattern: bind the name
    Term name_term = HEAP[term_val(pattern)];
    if (term_tag(name_term) == NUM) {
      u32 name = term_val(name_term);
      purple_bind_push(name);
      count = 1;
    }
  } else if (pnam == PURPLE_NAM_PCTR) {
    // Constructor pattern: recursively bind variables in sub-patterns
    Term args = HEAP[term_val(pattern) + 1]; // second field is args list
    while (term_tag(args) >= C00 && term_tag(args) <= C16 &&
           term_ext(args) == PURPLE_NAM_CON) {
      u32 loc = term_val(args);
      Term sub_pattern = HEAP[loc];
      count += purple_bind_pattern_vars(sub_pattern);
      args = HEAP[loc + 1];
    }
  } else if (pnam == PURPLE_NAM_POR) {
    // Or-pattern: bind variables from the first alternative
    // (all alternatives should have the same bindings)
    Term alts = HEAP[term_val(pattern)]; // first field is alternatives list
    // Validate that all alternatives bind the same variables in the same order
    u32 base_vars[256];
    u32 base_count = 0;
    int have_base = 0;
    Term cur = alts;
    while (term_tag(cur) >= C00 && term_tag(cur) <= C16 &&
           term_ext(cur) == PURPLE_NAM_CON) {
      u32 loc = term_val(cur);
      Term alt_pattern = HEAP[loc];
      u32 vars[256];
      u32 count2 = 0;
      purple_collect_pattern_vars(alt_pattern, vars, &count2, 256);
      if (!have_base) {
        memcpy(base_vars, vars, count2 * sizeof(u32));
        base_count = count2;
        have_base = 1;
      } else {
        if (count2 != base_count || memcmp(base_vars, vars, base_count * sizeof(u32)) != 0) {
          fprintf(stderr, "PURPLE_ERROR: or-pattern alternatives bind different variables\n");
          exit(1);
        }
      }
      cur = HEAP[loc + 1];
    }
    if (term_tag(alts) >= C00 && term_tag(alts) <= C16 &&
        term_ext(alts) == PURPLE_NAM_CON) {
      u32 loc = term_val(alts);
      Term first_pattern = HEAP[loc];
      count += purple_bind_pattern_vars(first_pattern);
    }
  } else if (pnam == PURPLE_NAM_PAS) {
    // As-pattern: bind the name and also the subpattern's variables
    u32 loc = term_val(pattern);
    Term name_term = HEAP[loc];
    if (term_tag(name_term) == NUM) {
      u32 name = term_val(name_term);
      purple_bind_push(name);
      count = 1;
    }
    Term subpattern = HEAP[loc + 1];
    count += purple_bind_pattern_vars(subpattern);
  }
  // PWld and PLit bind nothing

  return count;
}

// Parse a single match case: (pattern body)
// Binds pattern variables, parses body, unbinds
fn Term purple_parse_match_case(PState *s) {
  parse_consume(s, "(");

  // Parse the pattern (now supports nested patterns)
  Term pattern = purple_parse_pattern(s);

  // Recursively extract and bind all variable names from the pattern
  u32 bound_count = purple_bind_pattern_vars(pattern);

  // Check for optional :when guard
  Term guard = purple_term_nil();  // Default: no guard
  purple_skip(s);
  if (parse_peek(s) == ':') {
    parse_advance(s);  // consume ':'
    // Check for "when" keyword
    u32 start = s->pos;
    u32 len = 0;
    while (!parse_at_end(s) && !parse_is_space(parse_peek(s)) && parse_peek(s) != '(' && parse_peek(s) != ')') {
      parse_advance(s);
      len++;
    }
    if (purple_symbol_is(s, start, len, "when")) {
      // Parse the guard expression (variables are already bound)
      guard = parse_purple_form(s);
    } else {
      fprintf(stderr, "PURPLE_ERROR: expected 'when' after ':' in match case\n");
      exit(1);
    }
  }

  // Parse the body with variables bound
  Term body = parse_purple_form(s);

  // Unbind variables
  purple_bind_pop(bound_count);

  parse_consume(s, ")");

  return purple_term_case(pattern, guard, body);
}

// Parse: (match expr (pattern1 body1) (pattern2 body2) ...)
fn Term purple_parse_match(PState *s) {
  // Parse the scrutinee
  Term expr = parse_purple_form(s);

  // Collect cases into an array first
  Term case_array[64];
  u32 case_count = 0;

  purple_skip(s);
  while (!parse_match(s, ")")) {
    if (case_count >= 64) {
      fprintf(stderr, "PURPLE_ERROR: too many match cases\n");
      exit(1);
    }
    case_array[case_count++] = purple_parse_match_case(s);
    purple_skip(s);
  }

  // Build list in reverse order (from end to beginning)
  Term cases = purple_term_nil();
  for (int i = (int)case_count - 1; i >= 0; i--) {
    cases = purple_term_con(case_array[i], cases);
  }

  return purple_term_mat(expr, cases);
}

// Parse: (ffi "func-name" arg1 arg2 ...)
// Returns: #FFI{name_nick, args_list}
fn Term purple_parse_ffi(PState *s) {
  purple_skip(s);

  // Parse function name as string literal
  if (parse_peek(s) != '"') {
    parse_error(s, "function name string", parse_peek(s));
  }
  parse_advance(s);  // consume "

  // Read function name
  char func_name[256];
  u32 name_len = 0;
  while (parse_peek(s) != '"' && !parse_at_end(s)) {
    if (name_len >= 255) {
      fprintf(stderr, "PURPLE_ERROR: FFI function name too long\n");
      exit(1);
    }
    func_name[name_len++] = parse_peek(s);
    parse_advance(s);
  }
  func_name[name_len] = '\0';
  parse_advance(s);  // consume "

  // Nick-encode the function name (store as symbol)
  u32 name_nick = 0;
  for (u32 i = 0; i < name_len && i < 4; i++) {
    name_nick = ((name_nick << 6) + nick_letter_to_b64(func_name[i])) & EXT_MASK;
  }

  // Also store full name in table for later retrieval
  u32 name_id = table_find(func_name, name_len);

  // Collect arguments
  Term args_array[64];
  u32 arg_count = 0;

  purple_skip(s);
  while (!parse_match(s, ")")) {
    if (arg_count >= 64) {
      fprintf(stderr, "PURPLE_ERROR: too many FFI arguments\n");
      exit(1);
    }
    args_array[arg_count++] = parse_purple_form(s);
    purple_skip(s);
  }

  // Build args list in reverse
  Term args = purple_term_nil();
  for (int i = (int)arg_count - 1; i >= 0; i--) {
    args = purple_term_con(args_array[i], args);
  }

  // Use the full table ID for the name (so we can look it up later)
  return purple_term_ffi(term_new_num(name_id), args);
}

// Parse: (do expr1 expr2 ... exprN)
// Returns nested #Do{expr1, #Do{expr2, ... exprN}}
// The last expression is the result
fn Term purple_parse_do(PState *s) {
  // Collect all expressions
  Term exprs[256];
  u32 count = 0;

  purple_skip(s);
  while (!parse_match(s, ")")) {
    if (count >= 256) {
      fprintf(stderr, "PURPLE_ERROR: too many expressions in do block\n");
      exit(1);
    }
    exprs[count++] = parse_purple_form(s);
    purple_skip(s);
  }

  if (count == 0) {
    fprintf(stderr, "PURPLE_ERROR: do block requires at least one expression\n");
    exit(1);
  }

  // Build nested Do from end to start
  // (do e1 e2 e3) -> #Do{e1, #Do{e2, e3}}
  Term result = exprs[count - 1];
  for (int i = (int)count - 2; i >= 0; i--) {
    result = purple_term_do(exprs[i], result);
  }

  return result;
}

fn Term purple_parse_prim2(PState *s, u32 nam) {
  Term a = parse_purple_form(s);
  Term b = parse_purple_form(s);
  parse_consume(s, ")");
  if (nam == PURPLE_NAM_ADD) {
    return purple_term_add(a, b);
  } else if (nam == PURPLE_NAM_SUB) {
    return purple_term_sub(a, b);
  } else if (nam == PURPLE_NAM_MUL) {
    return purple_term_mul(a, b);
  } else if (nam == PURPLE_NAM_EQL) {
    return purple_term_eql(a, b);
  } else if (nam == PURPLE_NAM_LT) {
    return purple_term_lt(a, b);
  } else if (nam == PURPLE_NAM_GT) {
    return purple_term_gt(a, b);
  } else if (nam == PURPLE_NAM_LE) {
    return purple_term_le(a, b);
  } else if (nam == PURPLE_NAM_GE) {
    return purple_term_ge(a, b);
  } else if (nam == PURPLE_NAM_DIV) {
    return purple_term_div(a, b);
  } else if (nam == PURPLE_NAM_MOD) {
    return purple_term_mod(a, b);
  } else if (nam == PURPLE_NAM_AND) {
    return purple_term_and(a, b);
  } else if (nam == PURPLE_NAM_OR) {
    return purple_term_or(a, b);
  } else if (nam == PURPLE_NAM_CON) {
    return purple_term_con(a, b);
  } else if (nam == PURPLE_NAM_MAP) {
    return purple_term_map(a, b);
  } else if (nam == PURPLE_NAM_FLTR) {
    return purple_term_filter(a, b);
  } else if (nam == PURPLE_NAM_APND) {
    return purple_term_append(a, b);
  } else if (nam == PURPLE_NAM_COMP) {
    return purple_term_compose(a, b);
  } else if (nam == PURPLE_NAM_APPL) {
    return purple_term_apply_list(a, b);
  } else {
    fprintf(stderr, "PURPLE_ERROR: unknown binary primitive\n");
    exit(1);
  }
}

fn Term purple_parse_prim3(PState *s, u32 nam) {
  Term a = parse_purple_form(s);
  Term b = parse_purple_form(s);
  Term c = parse_purple_form(s);
  parse_consume(s, ")");
  if (nam == PURPLE_NAM_FOLD) {
    return purple_term_fold(a, b, c);
  } else if (nam == PURPLE_NAM_FOLDL) {
    return purple_term_foldl(a, b, c);
  } else {
    fprintf(stderr, "PURPLE_ERROR: unknown ternary primitive\n");
    exit(1);
  }
}

fn Term purple_parse_prim1(PState *s, u32 nam) {
  Term a = parse_purple_form(s);
  parse_consume(s, ")");
  if (nam == PURPLE_NAM_FST) {
    return purple_term_fst(a);
  } else if (nam == PURPLE_NAM_SND) {
    return purple_term_snd(a);
  } else if (nam == PURPLE_NAM_NOT) {
    return purple_term_not(a);
  } else if (nam == PURPLE_NAM_LEN) {
    return purple_term_length(a);
  } else if (nam == PURPLE_NAM_REV) {
    return purple_term_reverse(a);
  } else if (nam == PURPLE_NAM_FLIP) {
    return purple_term_flip(a);
  } else {
    fprintf(stderr, "PURPLE_ERROR: unknown unary primitive\n");
    exit(1);
  }
}

fn Term parse_purple_list(PState *s) {
  purple_skip(s);
  if (parse_match(s, ")")) {
    parse_error(s, "non-empty list", ')');
  }
  u32 start = 0;
  u32 len   = 0;
  if (purple_parse_symbol_token(s, &start, &len)) {
    // Purple-specific forms first
    if (purple_symbol_is(s, start, len, "EM")) {
      return purple_parse_em(s);
    }
    if (purple_symbol_is(s, start, len, "clambda")) {
      return purple_parse_clambda(s);
    }
    if (purple_symbol_is(s, start, len, "get-meta")) {
      return purple_parse_getmeta(s);
    }
    if (purple_symbol_is(s, start, len, "set-meta!")) {
      return purple_parse_setmeta(s);
    }
    if (purple_symbol_is(s, start, len, "default-handler")) {
      return purple_parse_defh(s);
    }
    if (purple_symbol_is(s, start, len, "error")) {
      return purple_parse_error(s);
    }
    if (purple_symbol_is(s, start, len, "try")) {
      return purple_parse_try(s);
    }
    if (purple_symbol_is(s, start, len, "assert")) {
      return purple_parse_assert(s);
    }
    if (purple_symbol_is(s, start, len, "trace")) {
      return purple_parse_trace(s);
    }
    if (purple_symbol_is(s, start, len, "reify-env")) {
      return purple_parse_renv(s);
    }
    if (purple_symbol_is(s, start, len, "gensym")) {
      return purple_parse_gensym(s);
    }
    if (purple_symbol_is(s, start, len, "sym-eq?")) {
      return purple_parse_symeq(s);
    }
    if (purple_symbol_is(s, start, len, "with-menv")) {
      return purple_parse_withmenv(s);
    }
    if (purple_symbol_is(s, start, len, "with-handlers")) {
      return purple_parse_withhandlers(s);
    }
    if (purple_symbol_is(s, start, len, "ctr-arg")) {
      return purple_parse_ctrarg(s);
    }
    if (purple_symbol_is(s, start, len, "eval")) {
      return purple_parse_eval(s);
    }
    if (purple_symbol_is(s, start, len, "meta-level")) {
      return purple_parse_mlvl(s);
    }
    if (purple_symbol_is(s, start, len, "shift")) {
      return purple_parse_shift(s);
    }
    if (purple_symbol_is(s, start, len, "ctr-tag")) {
      return purple_parse_ctag(s);
    }
    if (purple_symbol_is(s, start, len, "match")) {
      return purple_parse_match(s);
    }
    if (purple_symbol_is(s, start, len, "ffi")) {
      return purple_parse_ffi(s);
    }
    if (purple_symbol_is(s, start, len, "do")) {
      return purple_parse_do(s);
    }
    // Pink forms
    if (purple_symbol_is(s, start, len, "lambda")) {
      return purple_parse_lambda(s);
    }
    if (purple_symbol_is(s, start, len, "let")) {
      return purple_parse_let(s);
    }
    if (purple_symbol_is(s, start, len, "mov")) {
      return purple_parse_mov(s);
    }
    if (purple_symbol_is(s, start, len, "letrec")) {
      return purple_parse_letrec(s);
    }
    if (purple_symbol_is(s, start, len, "defmacro")) {
      return purple_parse_defmacro(s);
    }
    if (purple_symbol_is(s, start, len, "macroexpand")) {
      return purple_parse_macroexpand(s);
    }
    if (purple_symbol_is(s, start, len, "mcall")) {
      return purple_parse_mcall(s);
    }
    if (purple_symbol_is(s, start, len, "if")) {
      return purple_parse_if(s);
    }
    if (purple_symbol_is(s, start, len, "lift")) {
      return purple_parse_lift(s);
    }
    if (purple_symbol_is(s, start, len, "run")) {
      return purple_parse_run(s);
    }
    if (purple_symbol_is(s, start, len, "code")) {
      return purple_parse_code(s);
    }
    if (purple_symbol_is(s, start, len, "quote")) {
      return purple_parse_code(s);  // quote is alias for code
    }
    if (purple_symbol_is(s, start, len, "+")) {
      return purple_parse_prim2(s, PURPLE_NAM_ADD);
    }
    if (purple_symbol_is(s, start, len, "-")) {
      return purple_parse_prim2(s, PURPLE_NAM_SUB);
    }
    if (purple_symbol_is(s, start, len, "*")) {
      return purple_parse_prim2(s, PURPLE_NAM_MUL);
    }
    if (purple_symbol_is(s, start, len, "=")) {
      return purple_parse_prim2(s, PURPLE_NAM_EQL);
    }
    if (purple_symbol_is(s, start, len, "<")) {
      return purple_parse_prim2(s, PURPLE_NAM_LT);
    }
    if (purple_symbol_is(s, start, len, ">")) {
      return purple_parse_prim2(s, PURPLE_NAM_GT);
    }
    if (purple_symbol_is(s, start, len, "<=")) {
      return purple_parse_prim2(s, PURPLE_NAM_LE);
    }
    if (purple_symbol_is(s, start, len, ">=")) {
      return purple_parse_prim2(s, PURPLE_NAM_GE);
    }
    if (purple_symbol_is(s, start, len, "/")) {
      return purple_parse_prim2(s, PURPLE_NAM_DIV);
    }
    if (purple_symbol_is(s, start, len, "%")) {
      return purple_parse_prim2(s, PURPLE_NAM_MOD);
    }
    if (purple_symbol_is(s, start, len, "and")) {
      return purple_parse_prim2(s, PURPLE_NAM_AND);
    }
    if (purple_symbol_is(s, start, len, "or")) {
      return purple_parse_prim2(s, PURPLE_NAM_OR);
    }
    if (purple_symbol_is(s, start, len, "not")) {
      return purple_parse_prim1(s, PURPLE_NAM_NOT);
    }
    if (purple_symbol_is(s, start, len, "cons")) {
      return purple_parse_prim2(s, PURPLE_NAM_CON);
    }
    if (purple_symbol_is(s, start, len, "fst")) {
      return purple_parse_prim1(s, PURPLE_NAM_FST);
    }
    if (purple_symbol_is(s, start, len, "snd")) {
      return purple_parse_prim1(s, PURPLE_NAM_SND);
    }
    if (purple_symbol_is(s, start, len, "nil")) {
      parse_consume(s, ")");
      return purple_term_nil();
    }
    // List operations
    if (purple_symbol_is(s, start, len, "map")) {
      return purple_parse_prim2(s, PURPLE_NAM_MAP);
    }
    if (purple_symbol_is(s, start, len, "filter")) {
      return purple_parse_prim2(s, PURPLE_NAM_FLTR);
    }
    if (purple_symbol_is(s, start, len, "fold")) {
      return purple_parse_prim3(s, PURPLE_NAM_FOLD);
    }
    if (purple_symbol_is(s, start, len, "foldl")) {
      return purple_parse_prim3(s, PURPLE_NAM_FOLDL);
    }
    if (purple_symbol_is(s, start, len, "length")) {
      return purple_parse_prim1(s, PURPLE_NAM_LEN);
    }
    if (purple_symbol_is(s, start, len, "append")) {
      return purple_parse_prim2(s, PURPLE_NAM_APND);
    }
    if (purple_symbol_is(s, start, len, "reverse")) {
      return purple_parse_prim1(s, PURPLE_NAM_REV);
    }
    // Higher-order function utilities
    if (purple_symbol_is(s, start, len, "compose")) {
      return purple_parse_prim2(s, PURPLE_NAM_COMP);
    }
    if (purple_symbol_is(s, start, len, "flip")) {
      return purple_parse_prim1(s, PURPLE_NAM_FLIP);
    }
    if (purple_symbol_is(s, start, len, "apply")) {
      return purple_parse_prim2(s, PURPLE_NAM_APPL);
    }
    Term head = purple_symbol_term(s, start, len);
    return purple_parse_app_rest(s, head);
  }
  Term head = parse_purple_form(s);
  return purple_parse_app_rest(s, head);
}

// Parse escape sequence in string/char literal
fn u32 purple_parse_escape(PState *s) {
  char c = parse_peek(s);
  parse_advance(s);
  switch (c) {
    case 'n':  return '\n';
    case 't':  return '\t';
    case 'r':  return '\r';
    case '\\': return '\\';
    case '"':  return '"';
    case '0':  return '\0';
    default:   return (u32)(u8)c;
  }
}

// Parse string literal: "hello" -> #CON{#CHR{h}, #CON{#CHR{e}, ...#NIL}}
fn Term purple_parse_string(PState *s) {
  parse_advance(s);  // consume opening "
  u32 codepoints[4096];
  u32 count = 0;
  while (parse_peek(s) != '"' && !parse_at_end(s)) {
    u32 cp;
    if (parse_peek(s) == '\\') {
      parse_advance(s);
      cp = purple_parse_escape(s);
    } else {
      cp = parse_utf8(s);
    }
    if (count >= 4096) {
      fprintf(stderr, "PURPLE_ERROR: string too long\n");
      exit(1);
    }
    codepoints[count++] = cp;
  }
  if (parse_at_end(s)) {
    parse_error(s, "closing quote", '\0');
  }
  parse_advance(s);  // consume closing "
  // Build cons list in reverse
  Term result = purple_term_nil();
  for (int i = (int)count - 1; i >= 0; i--) {
    Term chr = purple_term_chr(codepoints[i]);
    result = purple_term_con(chr, result);
  }
  return result;
}

// Parse character literal: #\a, #\newline, #\space, #\tab
fn Term purple_parse_char(PState *s) {
  parse_advance(s);  // consume #
  if (parse_peek(s) != '\\') {
    parse_error(s, "backslash after #", parse_peek(s));
  }
  parse_advance(s);  // consume backslash
  // Check for named characters
  if (parse_starts_with(s, "newline")) {
    s->pos += 7;
    return purple_term_chr('\n');
  }
  if (parse_starts_with(s, "space")) {
    s->pos += 5;
    return purple_term_chr(' ');
  }
  if (parse_starts_with(s, "tab")) {
    s->pos += 3;
    return purple_term_chr('\t');
  }
  if (parse_starts_with(s, "return")) {
    s->pos += 6;
    return purple_term_chr('\r');
  }
  if (parse_starts_with(s, "null")) {
    s->pos += 4;
    return purple_term_chr('\0');
  }
  // Single character
  u32 cp = parse_utf8(s);
  return purple_term_chr(cp);
}

fn Term parse_purple_form(PState *s) {
  purple_skip(s);
  if (parse_at_end(s)) {
    parse_error(s, "form", '\0');
  }
  char c = parse_peek(s);
  // String literal: "hello"
  if (c == '"') {
    return purple_parse_string(s);
  }
  // Character literal: #\a, #\newline, etc.
  if (c == '#' && s->pos + 1 < s->len && s->src[s->pos + 1] == '\\') {
    return purple_parse_char(s);
  }
  // Quoted symbol: 'foo -> #Sym{nick_id}
  // Uses nick encoding so symbols can be matched against HVM4 constructors
  if (c == '\'') {
    parse_advance(s);
    u32 start = 0;
    u32 len   = 0;
    if (!purple_parse_symbol_token(s, &start, &len)) {
      parse_error(s, "symbol after quote", parse_peek(s));
    }
    // Nick-encode the symbol (max 4 chars)
    u32 k = 0;
    for (u32 i = 0; i < len && i < 4; i++) {
      k = ((k << 6) + nick_letter_to_b64(s->src[start + i])) & EXT_MASK;
    }
    return purple_term_sym(k);
  }
  // Quasiquote: `expr
  if (c == '`') {
    parse_advance(s);
    Term expr = parse_purple_form(s);
    return purple_term_qq(expr);
  }
  // Unquote: ,expr or ,@expr (splicing)
  if (c == ',') {
    parse_advance(s);
    if (!parse_at_end(s) && parse_peek(s) == '@') {
      // Unquote-splicing: ,@expr
      parse_advance(s);
      Term expr = parse_purple_form(s);
      return purple_term_uqs(expr);
    }
    // Regular unquote: ,expr
    Term expr = parse_purple_form(s);
    return purple_term_uq(expr);
  }
  if (c == '(') {
    parse_advance(s);
    return parse_purple_list(s);
  }
  if (isdigit(c)) {
    u32 n = purple_parse_number(s);
    return purple_term_lit(n);
  }
  u32 start = 0;
  u32 len   = 0;
  if (purple_parse_symbol_token(s, &start, &len)) {
    return purple_symbol_term(s, start, len);
  }
  parse_error(s, "form", c);
  return 0;
}

// Track included files to avoid cycles
static char *PURPLE_INCLUDED[256];
static u32 PURPLE_INCLUDED_LEN = 0;

fn int purple_already_included(const char *path) {
  for (u32 i = 0; i < PURPLE_INCLUDED_LEN; i++) {
    if (strcmp(PURPLE_INCLUDED[i], path) == 0) {
      return 1;
    }
  }
  return 0;
}

fn void purple_mark_included(const char *path) {
  if (PURPLE_INCLUDED_LEN < 256) {
    PURPLE_INCLUDED[PURPLE_INCLUDED_LEN++] = strdup(path);
  }
}

// Process includes by text substitution
// Returns a new source string with includes expanded
fn char* purple_expand_includes(const char *src, u32 len, const char *base_path) {
  // Estimate expanded size (may grow)
  u32 cap = len * 2 + 4096;
  char *out = malloc(cap);
  u32 out_len = 0;

  u32 pos = 0;
  while (pos < len) {
    // Skip include detection inside strings and comments
    if (src[pos] == '"') {
      // Copy string literal verbatim
      if (out_len + 1 >= cap) {
        cap *= 2;
        out = realloc(out, cap);
      }
      out[out_len++] = src[pos++];
      while (pos < len && src[pos] != '"') {
        if (src[pos] == '\\' && pos + 1 < len) {
          // Copy escape sequence
          if (out_len + 2 >= cap) {
            cap *= 2;
            out = realloc(out, cap);
          }
          out[out_len++] = src[pos++];
          out[out_len++] = src[pos++];
        } else {
          if (out_len + 1 >= cap) {
            cap *= 2;
            out = realloc(out, cap);
          }
          out[out_len++] = src[pos++];
        }
      }
      if (pos < len) {
        if (out_len + 1 >= cap) {
          cap *= 2;
          out = realloc(out, cap);
        }
        out[out_len++] = src[pos++]; // closing quote
      }
      continue;
    }
    // Skip line comments (; or //)
    if (src[pos] == ';' || (src[pos] == '/' && pos + 1 < len && src[pos + 1] == '/')) {
      while (pos < len && src[pos] != '\n') {
        if (out_len + 1 >= cap) {
          cap *= 2;
          out = realloc(out, cap);
        }
        out[out_len++] = src[pos++];
      }
      continue;
    }
    // Look for (include "
    if (pos + 10 < len &&
        src[pos] == '(' &&
        strncmp(src + pos + 1, "include", 7) == 0 &&
        (src[pos + 8] == ' ' || src[pos + 8] == '\t' || src[pos + 8] == '\n')) {
      // Skip "(include"
      pos += 8;
      while (pos < len && (src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\n')) {
        pos++;
      }
      // Expect "
      if (pos >= len || src[pos] != '"') {
        fprintf(stderr, "PURPLE_ERROR: include expects a string path\n");
        free(out);
        exit(1);
      }
      pos++; // skip "
      u32 path_start = pos;
      while (pos < len && src[pos] != '"') {
        pos++;
      }
      u32 path_len = pos - path_start;
      if (pos >= len) {
        fprintf(stderr, "PURPLE_ERROR: unterminated include path\n");
        free(out);
        exit(1);
      }
      pos++; // skip closing "

      // Skip closing )
      while (pos < len && (src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\n')) {
        pos++;
      }
      if (pos >= len || src[pos] != ')') {
        fprintf(stderr, "PURPLE_ERROR: include missing closing )\n");
        free(out);
        exit(1);
      }
      pos++; // skip )

      // Resolve path relative to base
      char path[512], full_path[1024];
      if (path_len >= sizeof(path)) {
        fprintf(stderr, "PURPLE_ERROR: include path too long\n");
        free(out);
        exit(1);
      }
      memcpy(path, src + path_start, path_len);
      path[path_len] = '\0';
      sys_path_join(full_path, sizeof(full_path), base_path, path);

      // Check if already included
      char *abs_path = realpath(full_path, NULL);
      if (!abs_path) {
        fprintf(stderr, "PURPLE_ERROR: cannot find '%s'\n", full_path);
        free(out);
        exit(1);
      }

      if (!purple_already_included(abs_path)) {
        purple_mark_included(abs_path);

        // Read included file
        char *inc_src = sys_file_read(abs_path);
        if (!inc_src) {
          fprintf(stderr, "PURPLE_ERROR: cannot read '%s'\n", abs_path);
          free(abs_path);
          free(out);
          exit(1);
        }
        u32 inc_len = strlen(inc_src);

        // Recursively expand includes
        char *expanded = purple_expand_includes(inc_src, inc_len, abs_path);
        free(inc_src);
        u32 exp_len = strlen(expanded);

        // Grow output buffer if needed
        while (out_len + exp_len + 1024 > cap) {
          cap *= 2;
          out = realloc(out, cap);
        }

        // Append expanded content
        memcpy(out + out_len, expanded, exp_len);
        out_len += exp_len;
        free(expanded);
      }
      free(abs_path);
    } else {
      // Regular character - copy it
      if (out_len + 1 >= cap) {
        cap *= 2;
        out = realloc(out, cap);
      }
      out[out_len++] = src[pos++];
    }
  }

  out[out_len] = '\0';
  return out;
}

fn void purple_reset_includes(void) {
  for (u32 i = 0; i < PURPLE_INCLUDED_LEN; i++) {
    free(PURPLE_INCLUDED[i]);
    PURPLE_INCLUDED[i] = NULL;
  }
  PURPLE_INCLUDED_LEN = 0;
}

fn Term parse_purple(PState *s) {
  purple_names_init();
  PURPLE_BINDS_LEN = 0;
  purple_reset_includes();

  // Expand includes before parsing
  char *expanded = purple_expand_includes(s->src, s->len, s->file);
  PState exp_s = {
    .file = s->file,
    .src = expanded,
    .pos = 0,
    .len = strlen(expanded),
    .line = 1,
    .col = 1
  };

  Term term = parse_purple_form(&exp_s);
  purple_skip(&exp_s);
  if (!parse_at_end(&exp_s)) {
    parse_error(&exp_s, "end of file", parse_peek(&exp_s));
  }

  free(expanded);
  return term;
}
