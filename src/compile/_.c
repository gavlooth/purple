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
  e->env_names[e->env_len++] = strdup(buf);
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
    const char *name = TABLE[id];
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

  // Emit main using Purple evaluator
  fputs("\n@main = @purple_unwrap(@purple_eval(@purple_menv_new(#NIL, #NIL), ", out);
  purple_compile_emit(out, ast);
  fputs("))\n", out);
}
