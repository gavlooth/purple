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
static u32 PURPLE_NAM_CASE; // Match case
static u32 PURPLE_NAM_PCTR; // Pattern: constructor
static u32 PURPLE_NAM_PLIT; // Pattern: literal
static u32 PURPLE_NAM_PWLD; // Pattern: wildcard
static u32 PURPLE_NAM_PVAR; // Pattern: variable (for nested)
static u32 PURPLE_NAM_FFI;  // Foreign function call
static u32 PURPLE_NAM_DO;   // IO sequencing

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
  PURPLE_NAM_CASE  = purple_nick("Case");
  PURPLE_NAM_PCTR  = purple_nick("PCtr");
  PURPLE_NAM_PLIT  = purple_nick("PLit");
  PURPLE_NAM_PWLD  = purple_nick("PWld");
  PURPLE_NAM_PVAR  = purple_nick("PVar");
  PURPLE_NAM_FFI   = purple_nick("FFI");
  PURPLE_NAM_DO    = purple_nick("Do");
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

fn Term purple_term_case(Term pattern, Term body) {
  return purple_ctr2(PURPLE_NAM_CASE, pattern, body);
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
  u32 sym_id = table_find(s->src + start, len);
  u32 idx    = 0;
  if (purple_bind_lookup(sym_id, &idx)) {
    return purple_term_var(idx);
  }
  if (purple_symbol_is(s, start, len, "nil")) {
    return purple_term_nil();
  }
  return purple_term_sym(sym_id);
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

  // Constructor pattern: (Tag arg1 arg2 ...)
  if (c == '(') {
    parse_advance(s);
    purple_skip(s);

    // Get constructor name
    u32 tag_start = 0;
    u32 tag_len = 0;
    if (!purple_parse_symbol_token(s, &tag_start, &tag_len)) {
      parse_error(s, "constructor name", parse_peek(s));
    }

    // Nick-encode the tag (for matching against HVM4 constructors)
    u32 tag_nick = 0;
    for (u32 i = 0; i < tag_len && i < 4; i++) {
      tag_nick = ((tag_nick << 6) + nick_letter_to_b64(s->src[tag_start + i])) & EXT_MASK;
    }
    Term tag_sym = term_new_num(tag_nick);

    // Collect argument variable names
    u32 arg_names[16];
    u32 arg_count = 0;

    purple_skip(s);
    while (!parse_match(s, ")")) {
      u32 arg_start = 0;
      u32 arg_len = 0;
      if (!purple_parse_symbol_token(s, &arg_start, &arg_len)) {
        parse_error(s, "pattern variable", parse_peek(s));
      }
      if (arg_count >= 16) {
        fprintf(stderr, "PURPLE_ERROR: too many pattern arguments\n");
        exit(1);
      }
      arg_names[arg_count++] = table_find(s->src + arg_start, arg_len);
      purple_skip(s);
    }

    // Build args list as nested CON (in reverse for proper order)
    Term args = purple_term_nil();
    for (int i = (int)arg_count - 1; i >= 0; i--) {
      args = purple_term_con(term_new_num(arg_names[i]), args);
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

// Parse a single match case: (pattern body)
// Binds pattern variables, parses body, unbinds
fn Term purple_parse_match_case(PState *s) {
  parse_consume(s, "(");

  // Parse the pattern
  Term pattern = purple_parse_pattern(s);

  // Extract variable names from pattern to bind them
  u32 bound_names[16];
  u32 bound_count = 0;

  u8 ptag = term_tag(pattern);
  if (ptag >= C00 && ptag <= C16) {
    u32 pnam = term_ext(pattern);
    if (pnam == PURPLE_NAM_PCTR) {
      // Extract args list and bind each variable
      Term args = HEAP[term_val(pattern) + 1]; // second field
      while (term_tag(args) >= C00 && term_tag(args) <= C16 &&
             term_ext(args) == PURPLE_NAM_CON) {
        u32 loc = term_val(args);
        Term name_term = HEAP[loc];
        if (term_tag(name_term) == NUM) {
          u32 name = term_val(name_term);
          if (bound_count < 16) {
            bound_names[bound_count++] = name;
            purple_bind_push(name);
          }
        }
        args = HEAP[loc + 1];
      }
    } else if (pnam == PURPLE_NAM_PVAR) {
      // Single variable pattern
      Term name_term = HEAP[term_val(pattern)];
      if (term_tag(name_term) == NUM) {
        u32 name = term_val(name_term);
        bound_names[bound_count++] = name;
        purple_bind_push(name);
      }
    }
  }

  // Parse the body with variables bound
  Term body = parse_purple_form(s);

  // Unbind variables
  purple_bind_pop(bound_count);

  parse_consume(s, ")");

  return purple_term_case(pattern, body);
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
  } else {
    fprintf(stderr, "PURPLE_ERROR: unknown binary primitive\n");
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
    if (purple_symbol_is(s, start, len, "sym-eq?")) {
      return purple_parse_symeq(s);
    }
    if (purple_symbol_is(s, start, len, "with-menv")) {
      return purple_parse_withmenv(s);
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
    if (purple_symbol_is(s, start, len, "letrec")) {
      return purple_parse_letrec(s);
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

fn Term parse_purple(PState *s) {
  purple_names_init();
  PURPLE_BINDS_LEN = 0;
  Term term = parse_purple_form(s);
  purple_skip(s);
  if (!parse_at_end(s)) {
    parse_error(s, "end of file", parse_peek(s));
  }
  return term;
}
