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
static u32 PURPLE_NAM_CON;
static u32 PURPLE_NAM_NIL;
static u32 PURPLE_NAM_FST;
static u32 PURPLE_NAM_SND;

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
  PURPLE_NAM_CON  = NAM_CON;
  PURPLE_NAM_NIL  = NAM_NIL;
  PURPLE_NAM_FST  = purple_nick("Fst");
  PURPLE_NAM_SND  = purple_nick("Snd");
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
    // Pink forms
    if (purple_symbol_is(s, start, len, "lambda")) {
      return purple_parse_lambda(s);
    }
    if (purple_symbol_is(s, start, len, "let")) {
      return purple_parse_let(s);
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

fn Term parse_purple_form(PState *s) {
  purple_skip(s);
  if (parse_at_end(s)) {
    parse_error(s, "form", '\0');
  }
  char c = parse_peek(s);
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
