// Purple compiler
// Emits HVM4 source from Purple AST

// Compiler state
typedef struct {
  FILE *out;
  char *env_names[256];
  u32   env_len;
  u32   fresh;
} PurpleEmit;

fn void purple_env_init(PurpleEmit *e) {
  e->env_len = 0;
  e->fresh = 0;
}

fn const char *purple_env_push(PurpleEmit *e) {
  static char buf[64];
  snprintf(buf, sizeof(buf), "p%u", e->fresh++);
  if (e->env_len >= 256) {
    fprintf(stderr, "PURPLE_ERROR: too many bindings\n");
    exit(1);
  }
  char *dup = strdup(buf);
  if (!dup) {
    fprintf(stderr, "PURPLE_ERROR: out of memory in env_push\n");
    exit(1);
  }
  e->env_names[e->env_len++] = dup;
  return e->env_names[e->env_len - 1];
}

fn void purple_env_pop(PurpleEmit *e, u32 count) {
  while (count > 0 && e->env_len > 0) {
    free(e->env_names[--e->env_len]);
    count--;
  }
}

fn const char *purple_env_get(PurpleEmit *e, u32 idx) {
  if (idx >= e->env_len) {
    return "?";
  }
  return e->env_names[e->env_len - 1 - idx];
}

fn u32 purple_ctr_arity(Term t) {
  u8 tag = term_tag(t);
  if (tag >= C00 && tag <= C16) {
    return (u32)(tag - C00);
  }
  return 0;
}

fn Term purple_ctr_arg(Term t, u32 idx) {
  u32 loc = term_val(t);
  return HEAP[loc + idx];
}

// Forward declaration
fn void purple_compile_emit_term(PurpleEmit *e, Term t);

// Nick-encoded constructor names for patterns
static u32 PURPLEC_NAM_CON;
static u32 PURPLEC_NAM_NIL;
static u32 PURPLEC_NAM_CHR;
static u32 PURPLEC_NAM_CST;
static u32 PURPLEC_NAM_PCTR;
static u32 PURPLEC_NAM_PLIT;
static u32 PURPLEC_NAM_PWLD;
static u32 PURPLEC_NAM_PVAR;
static u32 PURPLEC_NAM_POR;
static u32 PURPLEC_NAM_PAS;
static u32 PURPLEC_NAM_CASE;
// Lowercase variants (source syntax)
static u32 PURPLEC_NAM_con;
static u32 PURPLEC_NAM_nil;
static u32 PURPLEC_NAM_chr;
// AST constructors
static u32 PURPLEC_NAM_Add;
static u32 PURPLEC_NAM_Sub;
static u32 PURPLEC_NAM_Mul;
static u32 PURPLEC_NAM_Eql;
static u32 PURPLEC_NAM_Lit;
static u32 PURPLEC_NAM_Var;
static u32 PURPLEC_NAM_Lam;
static u32 PURPLEC_NAM_App;
static u32 PURPLEC_NAM_Let;
static u32 PURPLEC_NAM_If;
static u32 PURPLEC_NAM_Cod;
static u32 PURPLEC_NAM_Clo;
static u32 PURPLEC_NAM_QQ;
static u32 PURPLEC_NAM_UQ;
static u32 PURPLEC_NAM_UQS;
// List operations
static u32 PURPLEC_NAM_MAP;
static u32 PURPLEC_NAM_FLTR;
static u32 PURPLEC_NAM_FOLD;
static u32 PURPLEC_NAM_FOLDL;
static u32 PURPLEC_NAM_LEN;
static u32 PURPLEC_NAM_APND;
static u32 PURPLEC_NAM_REV;
// Higher-order function utilities
static u32 PURPLEC_NAM_COMP;
static u32 PURPLEC_NAM_FLIP;
static u32 PURPLEC_NAM_APPL;
// Macro system
static u32 PURPLEC_NAM_DEFMAC;
static u32 PURPLEC_NAM_MACEXP;
static int PURPLEC_NAMES_READY = 0;

fn void purplec_names_init(void) {
  if (PURPLEC_NAMES_READY) return;
  PURPLEC_NAM_CON  = purple_nick("CON");
  PURPLEC_NAM_NIL  = purple_nick("NIL");
  PURPLEC_NAM_CHR  = purple_nick("CHR");
  PURPLEC_NAM_CST  = purple_nick("Cst");
  PURPLEC_NAM_PCTR = purple_nick("PCtr");
  PURPLEC_NAM_PLIT = purple_nick("PLit");
  PURPLEC_NAM_PWLD = purple_nick("PWld");
  PURPLEC_NAM_PVAR = purple_nick("PVar");
  PURPLEC_NAM_POR  = purple_nick("POr");
  PURPLEC_NAM_PAS  = purple_nick("PAs");
  PURPLEC_NAM_CASE = purple_nick("Case");
  // Lowercase variants
  PURPLEC_NAM_con  = purple_nick("cons");
  PURPLEC_NAM_nil  = purple_nick("nil");
  PURPLEC_NAM_chr  = purple_nick("chr");
  // AST constructors
  PURPLEC_NAM_Add  = purple_nick("Add");
  PURPLEC_NAM_Sub  = purple_nick("Sub");
  PURPLEC_NAM_Mul  = purple_nick("Mul");
  PURPLEC_NAM_Eql  = purple_nick("Eql");
  PURPLEC_NAM_Lit  = purple_nick("Lit");
  PURPLEC_NAM_Var  = purple_nick("Var");
  PURPLEC_NAM_Lam  = purple_nick("Lam");
  PURPLEC_NAM_App  = purple_nick("App");
  PURPLEC_NAM_Let  = purple_nick("Let");
  PURPLEC_NAM_If   = purple_nick("If");
  PURPLEC_NAM_Cod  = purple_nick("Cod");
  PURPLEC_NAM_Clo  = purple_nick("Clo");
  PURPLEC_NAM_QQ   = purple_nick("QQ");
  PURPLEC_NAM_UQ   = purple_nick("UQ");
  PURPLEC_NAM_UQS  = purple_nick("UQS");
  // List operations
  PURPLEC_NAM_MAP   = purple_nick("Map");
  PURPLEC_NAM_FLTR  = purple_nick("Fltr");
  PURPLEC_NAM_FOLD  = purple_nick("Fold");
  PURPLEC_NAM_FOLDL = purple_nick("FdLf");
  PURPLEC_NAM_LEN   = purple_nick("Len");
  PURPLEC_NAM_APND  = purple_nick("Apnd");
  PURPLEC_NAM_REV   = purple_nick("Rev");
  // Higher-order function utilities
  PURPLEC_NAM_COMP  = purple_nick("Comp");
  PURPLEC_NAM_FLIP  = purple_nick("Flip");
  PURPLEC_NAM_APPL  = purple_nick("Appl");
  // Macro system
  PURPLEC_NAM_DEFMAC = purple_nick("DMac");
  PURPLEC_NAM_MACEXP = purple_nick("MExp");
  PURPLEC_NAMES_READY = 1;
}

// Get constructor name from nick value (for pattern matching)
// Maps source syntax to HVM4 constructor names
fn const char *purplec_nick_to_name(u32 nick) {
  // Data constructors
  if (nick == PURPLEC_NAM_CON) return "CON";
  if (nick == PURPLEC_NAM_NIL) return "NIL";
  if (nick == PURPLEC_NAM_CHR) return "CHR";
  if (nick == PURPLEC_NAM_CST) return "Cst";
  // Lowercase source syntax -> uppercase HVM4
  if (nick == PURPLEC_NAM_con) return "CON";
  if (nick == PURPLEC_NAM_nil) return "NIL";
  if (nick == PURPLEC_NAM_chr) return "CHR";
  // AST constructors
  if (nick == PURPLEC_NAM_Add) return "Add";
  if (nick == PURPLEC_NAM_Sub) return "Sub";
  if (nick == PURPLEC_NAM_Mul) return "Mul";
  if (nick == PURPLEC_NAM_Eql) return "Eql";
  if (nick == PURPLEC_NAM_Lit) return "Lit";
  if (nick == PURPLEC_NAM_Var) return "Var";
  if (nick == PURPLEC_NAM_Lam) return "Lam";
  if (nick == PURPLEC_NAM_App) return "App";
  if (nick == PURPLEC_NAM_Let) return "Let";
  if (nick == PURPLEC_NAM_If)  return "If";
  if (nick == PURPLEC_NAM_Cod) return "Cod";
  if (nick == PURPLEC_NAM_Clo) return "Clo";
  // For unknown nicks, decode using the table or generate
  return NULL;
}

// Count variables in a pattern's arg list
fn u32 purple_count_pattern_args(Term args) {
  u8 tag = term_tag(args);
  if (tag >= C00 && tag <= C16) {
    u32 nam = term_ext(args);
    if (nam == PURPLEC_NAM_NIL) return 0;
    if (nam == PURPLEC_NAM_CON && purple_ctr_arity(args) == 2) {
      return 1 + purple_count_pattern_args(purple_ctr_arg(args, 1));
    }
  }
  return 0;
}

// Check if a constructor pattern has raw inner values that need wrapping
// Returns 1 for CHR, Lit, Var, Cst patterns that extract raw numbers
fn int purple_pattern_needs_raw_wrap(u32 tag_nick) {
  purplec_names_init();
  // These constructors have raw number arguments
  if (tag_nick == PURPLEC_NAM_CHR) return 1;
  if (tag_nick == PURPLEC_NAM_Lit) return 1;
  if (tag_nick == PURPLEC_NAM_Var) return 1;
  if (tag_nick == PURPLEC_NAM_CST) return 1;
  if (tag_nick == purple_nick("chr")) return 1;  // lowercase
  return 0;
}

// Native match only supports constructor arguments that are plain variables.
fn int purple_pattern_arg_is_var(Term pat) {
  u8 tag = term_tag(pat);
  if (tag >= C00 && tag <= C16) {
    return term_ext(pat) == PURPLEC_NAM_PVAR;
  }
  return 0;
}

fn int purple_pattern_args_need_runtime(Term args) {
  Term curr = args;
  while (1) {
    u8 tag = term_tag(curr);
    if (tag >= C00 && tag <= C16) {
      u32 nam = term_ext(curr);
      if (nam == PURPLEC_NAM_NIL) break;
      if (nam == PURPLEC_NAM_CON && purple_ctr_arity(curr) == 2) {
        Term pat = purple_ctr_arg(curr, 0);
        if (!purple_pattern_arg_is_var(pat)) {
          return 1;
        }
        curr = purple_ctr_arg(curr, 1);
        continue;
      }
    }
    break;
  }
  return 0;
}

// Emit native HVM4 pattern match case for a constructor pattern
// Pattern: #PCtr{tag_nick, args_list}
// Generates: #TAG: λ&a. λ&b. #CON{body, #CON{a, #CON{b, #NIL}}}
// The result is a pair of (body_ast, bindings_list) for the runtime
fn void purple_emit_native_ctr_case(PurpleEmit *e, Term pattern, Term body) {
  purplec_names_init();
  u32 ari = purple_ctr_arity(pattern);
  if (ari != 2) return;

  Term tag_term = purple_ctr_arg(pattern, 0);  // NUM with nick value
  Term args = purple_ctr_arg(pattern, 1);      // List of arg name IDs

  // Get the tag nick value
  u32 tag_nick = term_val(tag_term);

  // Emit constructor tag
  const char *ctr_name = purplec_nick_to_name(tag_nick);
  if (ctr_name) {
    fprintf(e->out, "#%s: ", ctr_name);
  } else {
    // Fallback: decode the nick
    char buf[8];
    // Decode nick to string (reverse of encoding)
    u32 k = tag_nick;
    int len = 0;
    char tmp[5] = {0};
    while (k > 0 && len < 4) {
      u32 b = k & 0x3F;
      if (b == 0) tmp[len++] = '_';
      else if (b <= 26) tmp[len++] = 'a' + b - 1;
      else if (b <= 52) tmp[len++] = 'A' + b - 27;
      else if (b <= 62) tmp[len++] = '0' + b - 53;
      else tmp[len++] = '$';
      k >>= 6;
    }
    // Reverse
    for (int i = 0; i < len; i++) buf[i] = tmp[len - 1 - i];
    buf[len] = '\0';
    fprintf(e->out, "#%s: ", buf);
  }

  // Count args and collect variable names
  u32 n_args = purple_count_pattern_args(args);
  if (n_args > 64) {
    fprintf(stderr, "PURPLE_ERROR: too many pattern arguments in compiler\n");
    exit(1);
  }
  const char *var_names[64];  // Max 64 args

  // Emit lambdas and save variable names
  for (u32 i = 0; i < n_args; i++) {
    var_names[i] = purple_env_push(e);
    fprintf(e->out, "λ&%s. ", var_names[i]);
  }

  // Check if this constructor has raw values that need wrapping
  int needs_wrap = purple_pattern_needs_raw_wrap(tag_nick);

  // Emit result as #CON{body, bindings_list}
  // bindings_list is #CON{var0, #CON{var1, ... #NIL}}
  // For constructors with raw values (CHR, Lit, etc.), wrap as #Cst{var}
  fputs("#CON{", e->out);
  purple_compile_emit_term(e, body);
  fputs(", ", e->out);

  // Emit bindings list from the variables
  for (u32 i = 0; i < n_args; i++) {
    fputs("#CON{", e->out);
    if (needs_wrap) {
      // Wrap raw number in #Cst
      fprintf(e->out, "#Cst{%s}", var_names[i]);
    } else {
      // Direct value (already a Purple value)
      fputs(var_names[i], e->out);
    }
    fputs(", ", e->out);
  }
  fputs("#NIL", e->out);
  for (u32 i = 0; i < n_args; i++) {
    fputc('}', e->out);
  }

  fputc('}', e->out);  // Close outer #CON

  // Pop variable names
  purple_env_pop(e, n_args);
}

// Check if match cases require runtime handling
// Returns 1 for patterns that can't be handled by native HVM4 matching
// NOTE: HVM4's native matching has a bug with _ : wildcards after patterns
// with lambda arguments, so we use runtime matching for most cases
fn int purple_match_needs_runtime(Term cases) {
  purplec_names_init();
  Term curr = cases;
  int has_wildcard = 0;
  int has_ctr_with_args = 0;

  while (1) {
    u8 tag = term_tag(curr);
    if (tag >= C00 && tag <= C16) {
      u32 nam = term_ext(curr);
      if (nam == PURPLEC_NAM_NIL) break;
      if (nam == PURPLEC_NAM_CON && purple_ctr_arity(curr) == 2) {
        Term case_term = purple_ctr_arg(curr, 0);
        // Case now has 3 args: pattern, guard, body
        if (term_tag(case_term) >= C00 && term_tag(case_term) <= C16 && term_ext(case_term) == PURPLEC_NAM_CASE && purple_ctr_arity(case_term) == 3) {
          // Check if case has a guard (guard is not #NIL)
          Term guard = purple_ctr_arg(case_term, 1);
          if (term_tag(guard) >= C00 && term_tag(guard) <= C16) {
            u32 guard_nam = term_ext(guard);
            if (guard_nam != PURPLEC_NAM_NIL) {
              // Has a guard - needs runtime for guard evaluation
              return 1;
            }
          }
          Term pattern = purple_ctr_arg(case_term, 0);
          u8 ptag = term_tag(pattern);
          if (ptag >= C00 && ptag <= C16) {
            u32 pnam = term_ext(pattern);
            // Literal patterns need runtime
            if (pnam == PURPLEC_NAM_PLIT) return 1;
            // Variable patterns need runtime (for proper binding)
            if (pnam == PURPLEC_NAM_PVAR) return 1;
            // Or-patterns need runtime
            if (pnam == PURPLEC_NAM_POR) return 1;
            // As-patterns need runtime
            if (pnam == PURPLEC_NAM_PAS) return 1;
            // Wildcard pattern
            if (pnam == PURPLEC_NAM_PWLD) has_wildcard = 1;
            // Constructor patterns
            if (pnam == PURPLEC_NAM_PCTR) {
              // Check if has arguments
              Term args = purple_ctr_arg(pattern, 1);
              if (term_tag(args) >= C00 && term_tag(args) <= C16) {
                u32 anam = term_ext(args);
                if (anam == PURPLEC_NAM_CON) has_ctr_with_args = 1;
              }
              // Native match can't handle nested patterns or wildcards in args
              if (purple_pattern_args_need_runtime(args)) return 1;
              // Constructor patterns with raw values need runtime
              Term tag_term = purple_ctr_arg(pattern, 0);
              if (term_tag(tag_term) == NUM) {
                u32 tag_nick = term_val(tag_term);
                if (purple_pattern_needs_raw_wrap(tag_nick)) return 1;
              }
            }
          }
        }
        curr = purple_ctr_arg(curr, 1);
        continue;
      }
    }
    break;
  }
  // HVM4 bug: _ : wildcard doesn't work after patterns with lambda arguments
  // Use runtime matching when both conditions are present
  if (has_wildcard && has_ctr_with_args) return 1;
  return 0;
}

// Emit a native HVM4 pattern match wrapped in #NMat for runtime evaluation
// Generates: #NMat{scrutinee, λ{case1; case2; ...}}
// The runtime will evaluate scrutinee first, then apply the native matcher
fn void purple_emit_native_match(PurpleEmit *e, Term scrutinee, Term cases) {
  purplec_names_init();

  // Start NMat constructor
  fputs("#NMat{", e->out);

  // Emit scrutinee (will be evaluated by runtime)
  purple_compile_emit_term(e, scrutinee);
  fputs(", ", e->out);

  // Emit native pattern match lambda
  fputs("λ{", e->out);

  // Iterate through cases
  Term curr = cases;
  int first = 1;
  int has_catchall = 0;  // Track if we have a wildcard or variable pattern
  while (1) {
    u8 tag = term_tag(curr);
    if (tag >= C00 && tag <= C16) {
      u32 nam = term_ext(curr);
      if (nam == PURPLEC_NAM_NIL) break;  // End of cases
      if (nam == PURPLEC_NAM_CON && purple_ctr_arity(curr) == 2) {
        Term case_term = purple_ctr_arg(curr, 0);
        Term rest = purple_ctr_arg(curr, 1);

        if (!first) fputs("; ", e->out);
        first = 0;

        // Each case is #Case{pattern, guard, body}
        if (term_tag(case_term) >= C00 && term_tag(case_term) <= C16 && term_ext(case_term) == PURPLEC_NAM_CASE) {
          Term pattern = purple_ctr_arg(case_term, 0);
          Term body = purple_ctr_arg(case_term, 2);

          u8 ptag = term_tag(pattern);
          if (ptag >= C00 && ptag <= C16) {
            u32 pnam = term_ext(pattern);

            // Wildcard: _ : λ&u. #CON{body, #NIL}
            // NOTE: Must wrap in lambda because HVM4 applies the default case
            // body to the unmatched value, which fails for non-function bodies
            if (pnam == PURPLEC_NAM_PWLD) {
              fputs("_ : λ&u. #CON{", e->out);
              purple_compile_emit_term(e, body);
              fputs(", #NIL}", e->out);
              has_catchall = 1;
            }
            // Variable: x (binds value) - convert to default with binding
            // Use _ : pattern to avoid HVM4 parsing issues with λ&x. as match arm
            else if (pnam == PURPLEC_NAM_PVAR) {
              // Emit as default case with lambda to capture the value
              // NOTE: Must wrap in lambda because HVM4 applies the default case
              fputs("_ : λ&u. #CON{", e->out);
              purple_compile_emit_term(e, body);
              fputs(", #NIL}", e->out);
              has_catchall = 1;
            }
            // Literal: n : #CON{body, #NIL}
            else if (pnam == PURPLEC_NAM_PLIT) {
              Term lit = purple_ctr_arg(pattern, 0);
              fprintf(e->out, "%u: #CON{", term_val(lit));
              purple_compile_emit_term(e, body);
              fputs(", #NIL}", e->out);
            }
            // Constructor: (TAG args...) - handled by purple_emit_native_ctr_case
            else if (pnam == PURPLEC_NAM_PCTR) {
              purple_emit_native_ctr_case(e, pattern, body);
            }
          }
        }
        curr = rest;
        continue;
      }
    }
    break;
  }

  // Add default error case only if no catch-all pattern exists
  // NOTE: Must wrap in lambda because HVM4 applies the default body to the unmatched value
  if (!has_catchall) {
    if (!first) fputs("; ", e->out);
    fputs("_ : λ&u. #CON{@pink_err(#nmch), #NIL}", e->out);
  }

  // Close pattern match and NMat
  fputs("}}", e->out);
}

fn void purple_emit_lambda(PurpleEmit *e, Term body, int is_rec) {
  const char *name = purple_env_push(e);
  fputs("λ&", e->out);
  fputs(name, e->out);
  fputc('.', e->out);

  // Check if body is another lambda
  u8 tag = term_tag(body);
  if (tag >= C00 && tag <= C16) {
    u32 nam = term_ext(body);
    if (nam == PURPLE_NAM_LAM && purple_ctr_arity(body) == 1) {
      purple_emit_lambda(e, purple_ctr_arg(body, 0), 0);
      purple_env_pop(e, 1);
      return;
    }
    if (nam == PURPLE_NAM_LAMR && purple_ctr_arity(body) == 1) {
      purple_emit_lambda(e, purple_ctr_arg(body, 0), 1);
      purple_env_pop(e, 1);
      return;
    }
  }

  purple_compile_emit_term(e, body);
  purple_env_pop(e, 1);
}

fn void purple_compile_emit_term(PurpleEmit *e, Term t) {
  u8 tag = term_tag(t);

  // Numbers
  if (tag == NUM) {
    fprintf(e->out, "%u", term_val(t));
    return;
  }

  // References
  if (tag == REF) {
    u32 id = term_val(t);
    const char *name = (id < BOOK_CAP) ? TABLE[id] : NULL;
    if (name) {
      fprintf(e->out, "@%s", name);
    } else {
      fprintf(e->out, "@ref_%u", id);
    }
    return;
  }

  // Constructors
  if (tag >= C00 && tag <= C16) {
    u32 nam = term_ext(t);
    u32 ari = purple_ctr_arity(t);

    // Lit
    if (nam == PURPLE_NAM_LIT && ari == 1) {
      Term n = purple_ctr_arg(t, 0);
      fputs("#Lit{", e->out);
      purple_compile_emit_term(e, n);
      fputc('}', e->out);
      return;
    }

    // Fixed-point: #Fix{hi, lo, scale}
    if (nam == PURPLE_NAM_FIX && ari == 3) {
      fputs("#Fix{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));  // hi
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));  // lo
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 2));  // scale
      fputc('}', e->out);
      return;
    }

    // Sym
    if (nam == PURPLE_NAM_SYM && ari == 1) {
      Term s = purple_ctr_arg(t, 0);
      fputs("#Sym{", e->out);
      purple_compile_emit_term(e, s);
      fputc('}', e->out);
      return;
    }

    // Var
    if (nam == PURPLE_NAM_VAR && ari == 1) {
      Term n = purple_ctr_arg(t, 0);
      fputs("#Var{", e->out);
      purple_compile_emit_term(e, n);
      fputc('}', e->out);
      return;
    }

    // Lam
    if (nam == PURPLE_NAM_LAM && ari == 1) {
      fputs("#Lam{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputc('}', e->out);
      return;
    }

    // LamR
    if (nam == PURPLE_NAM_LAMR && ari == 1) {
      fputs("#LamR{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputc('}', e->out);
      return;
    }

    // App
    if (nam == PURPLE_NAM_APP && ari == 2) {
      fputs("#App{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));
      fputc('}', e->out);
      return;
    }

    // Let
    if (nam == PURPLE_NAM_LET && ari == 2) {
      fputs("#Let{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));
      fputc('}', e->out);
      return;
    }

    // Mov (multi-use binding without dup overhead)
    if (nam == PURPLE_NAM_MOV && ari == 2) {
      fputs("#Mov{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));
      fputc('}', e->out);
      return;
    }

    // If
    if (nam == PURPLE_NAM_IF && ari == 3) {
      fputs("#If{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 2));
      fputc('}', e->out);
      return;
    }

    // Lft
    if (nam == PURPLE_NAM_LFT && ari == 1) {
      fputs("#Lft{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputc('}', e->out);
      return;
    }

    // Run
    if (nam == PURPLE_NAM_RUN && ari == 2) {
      fputs("#Run{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));
      fputc('}', e->out);
      return;
    }

    // Cod
    if (nam == PURPLE_NAM_COD && ari == 1) {
      fputs("#Cod{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputc('}', e->out);
      return;
    }

    // Arithmetic
    if (nam == PURPLE_NAM_ADD && ari == 2) {
      fputs("#Add{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));
      fputc('}', e->out);
      return;
    }
    if (nam == PURPLE_NAM_SUB && ari == 2) {
      fputs("#Sub{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));
      fputc('}', e->out);
      return;
    }
    if (nam == PURPLE_NAM_MUL && ari == 2) {
      fputs("#Mul{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));
      fputc('}', e->out);
      return;
    }
    if (nam == PURPLE_NAM_EQL && ari == 2) {
      fputs("#Eql{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));
      fputc('}', e->out);
      return;
    }
    if (nam == PURPLE_NAM_LT && ari == 2) {
      fputs("#Lt{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));
      fputc('}', e->out);
      return;
    }
    if (nam == PURPLE_NAM_GT && ari == 2) {
      fputs("#Gt{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));
      fputc('}', e->out);
      return;
    }
    if (nam == PURPLE_NAM_LE && ari == 2) {
      fputs("#Le{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));
      fputc('}', e->out);
      return;
    }
    if (nam == PURPLE_NAM_GE && ari == 2) {
      fputs("#Ge{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));
      fputc('}', e->out);
      return;
    }
    if (nam == PURPLE_NAM_DIV && ari == 2) {
      fputs("#Div{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));
      fputc('}', e->out);
      return;
    }
    if (nam == PURPLE_NAM_MOD && ari == 2) {
      fputs("#Mod{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));
      fputc('}', e->out);
      return;
    }
    if (nam == PURPLE_NAM_AND && ari == 2) {
      fputs("#And{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));
      fputc('}', e->out);
      return;
    }
    if (nam == PURPLE_NAM_OR && ari == 2) {
      fputs("#Or{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));
      fputc('}', e->out);
      return;
    }
    if (nam == PURPLE_NAM_NOT && ari == 1) {
      fputs("#Not{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputc('}', e->out);
      return;
    }

    // Pairs
    if (nam == PURPLE_NAM_CON && ari == 2) {
      fputs("#CON{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));
      fputc('}', e->out);
      return;
    }
    if (nam == PURPLE_NAM_NIL && ari == 0) {
      fputs("#NIL", e->out);
      return;
    }
    if (nam == PURPLE_NAM_FST && ari == 1) {
      fputs("#Fst{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputc('}', e->out);
      return;
    }
    if (nam == PURPLE_NAM_SND && ari == 1) {
      fputs("#Snd{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputc('}', e->out);
      return;
    }

    // Character (use HVM4's built-in #CHR)
    if (nam == PURPLE_NAM_CHR && ari == 1) {
      fputs("#CHR{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputc('}', e->out);
      return;
    }

    // Purple-specific constructors
    if (nam == PURPLE_NAM_EM && ari == 1) {
      fputs("#EM{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputc('}', e->out);
      return;
    }
    if (nam == PURPLE_NAM_CLAM && ari == 1) {
      fputs("#CLam{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputc('}', e->out);
      return;
    }
    if (nam == PURPLE_NAM_GMTA && ari == 1) {
      fputs("#GMta{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputc('}', e->out);
      return;
    }
    if (nam == PURPLE_NAM_SMTA && ari == 2) {
      fputs("#SMta{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));
      fputc('}', e->out);
      return;
    }
    if (nam == PURPLE_NAM_SEQ && ari == 2) {
      fputs("#SEq{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));
      fputc('}', e->out);
      return;
    }
    if (nam == PURPLE_NAM_WMENV && ari == 2) {
      fputs("#WMnv{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));
      fputc('}', e->out);
      return;
    }
    if (nam == PURPLE_NAM_CARG && ari == 2) {
      fputs("#CArg{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));
      fputc('}', e->out);
      return;
    }
    if (nam == PURPLE_NAM_EVAL && ari == 1) {
      fputs("#Eval{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputc('}', e->out);
      return;
    }
    if (nam == PURPLE_NAM_MLVL && ari == 0) {
      fputs("#MLvl", e->out);
      return;
    }
    if (nam == PURPLE_NAM_SHFT && ari == 2) {
      fputs("#Shft{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));
      fputc('}', e->out);
      return;
    }
    // Delimited continuations
    if (nam == PURPLE_NAM_PRMT && ari == 1) {
      fputs("#Prmt{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputc('}', e->out);
      return;
    }
    if (nam == PURPLE_NAM_CTRL && ari == 2) {
      fputs("#Ctrl{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));
      fputc('}', e->out);
      return;
    }
    if (nam == PURPLE_NAM_CTAG && ari == 1) {
      fputs("#CTag{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputc('}', e->out);
      return;
    }

    // Pattern matching - use native HVM4 match for simple constructor patterns
    // Fall back to runtime interpretation for patterns that need special handling
    if (nam == PURPLE_NAM_MAT && ari == 2) {
      Term scrutinee = purple_ctr_arg(t, 0);
      Term cases = purple_ctr_arg(t, 1);
      if (purple_match_needs_runtime(cases)) {
        // Fall back to runtime interpretation
        fputs("#Mat{", e->out);
        purple_compile_emit_term(e, scrutinee);
        fputs(", ", e->out);
        purple_compile_emit_term(e, cases);
        fputc('}', e->out);
      } else {
        // Use native HVM4 pattern matching for simple constructor patterns
        purple_emit_native_match(e, scrutinee, cases);
      }
      return;
    }
    // Keep these for potential runtime use (not commonly needed now)
    // Case now has 3 args: pattern, guard, body
    if (nam == PURPLE_NAM_CASE && ari == 3) {
      fputs("#Case{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 2));
      fputc('}', e->out);
      return;
    }
    if (nam == PURPLE_NAM_PCTR && ari == 2) {
      fputs("#PCtr{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));
      fputc('}', e->out);
      return;
    }
    if (nam == PURPLE_NAM_PLIT && ari == 1) {
      fputs("#PLit{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputc('}', e->out);
      return;
    }
    if (nam == PURPLE_NAM_PWLD && ari == 0) {
      fputs("#PWld", e->out);
      return;
    }
    if (nam == PURPLE_NAM_PVAR && ari == 1) {
      fputs("#PVar{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputc('}', e->out);
      return;
    }
    if (nam == PURPLEC_NAM_POR && ari == 1) {
      fputs("#POr{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputc('}', e->out);
      return;
    }
    if (nam == PURPLEC_NAM_PAS && ari == 2) {
      fputs("#PAs{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));
      fputc('}', e->out);
      return;
    }
    // Quasiquote: #QQ{expr}
    if (nam == PURPLEC_NAM_QQ && ari == 1) {
      fputs("#QQ{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputc('}', e->out);
      return;
    }
    // Unquote: #UQ{expr}
    if (nam == PURPLEC_NAM_UQ && ari == 1) {
      fputs("#UQ{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputc('}', e->out);
      return;
    }
    // Unquote-splicing: #UQS{expr}
    if (nam == PURPLEC_NAM_UQS && ari == 1) {
      fputs("#UQS{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputc('}', e->out);
      return;
    }

    // List operations
    if (nam == PURPLEC_NAM_MAP && ari == 2) {
      fputs("#Map{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));
      fputc('}', e->out);
      return;
    }
    if (nam == PURPLEC_NAM_FLTR && ari == 2) {
      fputs("#Fltr{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));
      fputc('}', e->out);
      return;
    }
    if (nam == PURPLEC_NAM_FOLD && ari == 3) {
      fputs("#Fold{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 2));
      fputc('}', e->out);
      return;
    }
    if (nam == PURPLEC_NAM_FOLDL && ari == 3) {
      fputs("#FdLf{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 2));
      fputc('}', e->out);
      return;
    }
    if (nam == PURPLEC_NAM_LEN && ari == 1) {
      fputs("#Len{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputc('}', e->out);
      return;
    }
    if (nam == PURPLEC_NAM_APND && ari == 2) {
      fputs("#Apnd{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));
      fputc('}', e->out);
      return;
    }
    if (nam == PURPLEC_NAM_REV && ari == 1) {
      fputs("#Rev{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputc('}', e->out);
      return;
    }
    // Higher-order function utilities
    if (nam == PURPLEC_NAM_COMP && ari == 2) {
      fputs("#Comp{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));
      fputc('}', e->out);
      return;
    }
    if (nam == PURPLEC_NAM_FLIP && ari == 1) {
      fputs("#Flip{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputc('}', e->out);
      return;
    }
    if (nam == PURPLEC_NAM_APPL && ari == 2) {
      fputs("#Appl{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));
      fputc('}', e->out);
      return;
    }
    // Macro system
    if (nam == PURPLEC_NAM_DEFMAC && ari == 3) {
      fputs("#DMac{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));  // name
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));  // params
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 2));  // body
      fputc('}', e->out);
      return;
    }
    if (nam == PURPLEC_NAM_MACEXP && ari == 1) {
      fputs("#MExp{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputc('}', e->out);
      return;
    }

    // FFI call: #FFI{name, args}
    if (nam == PURPLE_NAM_FFI && ari == 2) {
      fputs("#FFI{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));
      fputc('}', e->out);
      return;
    }

    // Do sequencing: #Do{first, rest}
    if (nam == PURPLE_NAM_DO && ari == 2) {
      fputs("#Do{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));
      fputc('}', e->out);
      return;
    }

    // Default handler: #DefH{name, arg} -> call default handler by symbol name
    if (nam == PURPLE_NAM_DEFH && ari == 2) {
      fputs("#DefH{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));
      fputc('}', e->out);
      return;
    }

    // Error: #ErrR{msg} -> raise an error
    if (nam == PURPLE_NAM_ERRR && ari == 1) {
      fputs("#ErrR{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputc('}', e->out);
      return;
    }

    // Try: #Try{expr, handler} -> try/catch error handling
    if (nam == PURPLE_NAM_TRY && ari == 2) {
      fputs("#Try{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));
      fputc('}', e->out);
      return;
    }

    // Assert: #Asrt{cond, msg} -> assert condition or raise error
    if (nam == PURPLE_NAM_ASRT && ari == 2) {
      fputs("#Asrt{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));
      fputc('}', e->out);
      return;
    }

    // Trace: #Trce{expr} -> trace and return value
    if (nam == PURPLE_NAM_TRCE && ari == 1) {
      fputs("#Trce{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputc('}', e->out);
      return;
    }

    // Reify-env: #REnv -> return current environment
    if (nam == PURPLE_NAM_RENV && ari == 0) {
      fputs("#REnv", e->out);
      return;
    }

    // Gensym: #GSym{n} -> generate unique symbol
    if (nam == PURPLE_NAM_GSYM && ari == 1) {
      fputs("#GSym{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputc('}', e->out);
      return;
    }

    // Parallel primitives (HVM4 native)
    // Amb: #Amb{a, b} -> nondeterministic choice (HVM4 superposition, label 0)
    if (nam == PURPLE_NAM_AMB && ari == 2) {
      fputs("#Amb{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));
      fputc('}', e->out);
      return;
    }
    // Fork: #Fork{label, a, b} -> labeled superposition (different labels = cross product)
    if (nam == PURPLE_NAM_FORK && ari == 3) {
      fputs("#Fork{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 2));
      fputc('}', e->out);
      return;
    }
    // ESeq: #ESeq{a, b} -> sequential evaluation (a then b, return b)
    if (nam == PURPLE_NAM_ESEQ && ari == 2) {
      fputs("#ESeq{", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 0));
      fputs(", ", e->out);
      purple_compile_emit_term(e, purple_ctr_arg(t, 1));
      fputc('}', e->out);
      return;
    }

    // Unknown constructor - emit generically
    fprintf(e->out, "#?%u{", nam);
    for (u32 i = 0; i < ari; i++) {
      if (i > 0) {
        fputs(", ", e->out);
      }
      purple_compile_emit_term(e, purple_ctr_arg(t, i));
    }
    fputc('}', e->out);
    return;
  }

  // Unknown
  fprintf(e->out, "<?tag=%u>", tag);
}

fn void purple_compile_emit(FILE *out, Term ast) {
  purplec_names_init();
  PurpleEmit e = { .out = out };
  purple_env_init(&e);
  purple_compile_emit_term(&e, ast);
}

fn void purple_compile_emit_runtime(FILE *out, Term ast, const char *runtime_path) {
  // Read and emit the runtime library
  char *runtime = sys_file_read(runtime_path);
  if (!runtime) {
    fprintf(stderr, "Error: could not read runtime '%s'\n", runtime_path);
    exit(1);
  }
  fputs(runtime, out);
  free(runtime);

  // Emit main using Purple evaluator (curried calling convention)
  fputs("\n@main = @purple_unwrap(@purple_eval(@purple_menv_new(0)(#NIL)(#NIL))(", out);
  purple_compile_emit(out, ast);
  fputs("))\n", out);
}
