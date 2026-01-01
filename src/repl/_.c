// Purple REPL - Interactive tower exploration
// Usage: ./purple-repl
//
// Commands:
//   :help     - Show help
//   :quit     - Exit REPL
//   :level    - Show current meta-level
//   :load <f> - Load and run a file
//   :reset    - Reset environment

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

// Include HVM4 runtime
#include "../../hvm4/clang/hvm4.c"

// Include Purple parser and compiler
#include "../parse/_.c"
#include "../compile/_.c"

// FFI function names
static u32 FFI_NAM_PUTS;
static u32 FFI_NAM_PUTC;
static u32 FFI_NAM_GETC;
static u32 FFI_NAM_EXIT;
static u32 FFI_NAM_PRNT;
static u32 FFI_NAM_FFI;
static u32 FFI_NAM_DO;
static u32 FFI_NAM_CST;

fn void ffi_names_init(void) {
  FFI_NAM_PUTS = purple_nick("puts");
  FFI_NAM_PUTC = purple_nick("putc");
  FFI_NAM_GETC = purple_nick("getc");
  FFI_NAM_EXIT = purple_nick("exit");
  FFI_NAM_PRNT = purple_nick("prnt");
  FFI_NAM_FFI  = purple_nick("FFI");
  FFI_NAM_DO   = purple_nick("Do");
  FFI_NAM_CST  = purple_nick("Cst");
}

// Extract string from HVM4 cons list of CHR
fn char* ffi_extract_string(Term lst) {
  static char buf[4096];
  u32 i = 0;
  Term cur = lst;

  while (term_tag(cur) >= C00 && term_tag(cur) <= C16) {
    u32 nam = term_ext(cur);
    if (nam == NAM_NIL) break;
    if (nam != NAM_CON) break;

    u32 loc = term_val(cur);
    Term head = HEAP[loc];
    Term tail = HEAP[loc + 1];

    if (term_tag(head) >= C00 && term_tag(head) <= C16 && term_ext(head) == NAM_CHR) {
      u32 cp = term_val(HEAP[term_val(head)]);
      if (cp < 0x80) {
        if (i < sizeof(buf) - 1) buf[i++] = (char)cp;
      } else if (cp < 0x800) {
        if (i < sizeof(buf) - 2) {
          buf[i++] = (char)(0xC0 | (cp >> 6));
          buf[i++] = (char)(0x80 | (cp & 0x3F));
        }
      } else if (cp < 0x10000) {
        if (i < sizeof(buf) - 3) {
          buf[i++] = (char)(0xE0 | (cp >> 12));
          buf[i++] = (char)(0x80 | ((cp >> 6) & 0x3F));
          buf[i++] = (char)(0x80 | (cp & 0x3F));
        }
      } else if (cp < 0x110000) {
        if (i < sizeof(buf) - 4) {
          buf[i++] = (char)(0xF0 | (cp >> 18));
          buf[i++] = (char)(0x80 | ((cp >> 12) & 0x3F));
          buf[i++] = (char)(0x80 | ((cp >> 6) & 0x3F));
          buf[i++] = (char)(0x80 | (cp & 0x3F));
        }
      }
    }
    cur = tail;
  }
  buf[i] = '\0';
  return buf;
}

fn int ffi_extract_int(Term v) {
  if (term_tag(v) == NUM) {
    return (int)term_val(v);
  }
  if (term_tag(v) >= C00 && term_tag(v) <= C16 && term_ext(v) == FFI_NAM_CST) {
    u32 loc = term_val(v);
    Term inner = HEAP[loc];
    if (term_tag(inner) == NUM) {
      return (int)term_val(inner);
    }
  }
  return 0;
}

fn Term ffi_execute(Term name_term, Term args) {
  u32 name_id = term_val(name_term);
  if (name_id >= BOOK_CAP) {
    fprintf(stderr, "FFI: function ID %u out of bounds\n", name_id);
    return term_new_num(0);
  }
  const char *name = TABLE[name_id];

  if (!name) {
    fprintf(stderr, "FFI: unknown function ID %u\n", name_id);
    return term_new_num(0);
  }

  Term arg0 = term_new_num(0);
  if (term_tag(args) >= C00 && term_tag(args) <= C16 && term_ext(args) == NAM_CON) {
    u32 loc = term_val(args);
    arg0 = HEAP[loc];
  }

  if (strcmp(name, "puts") == 0) {
    char *s = ffi_extract_string(arg0);
    puts(s);
    return term_new_num(0);
  }

  if (strcmp(name, "putchar") == 0) {
    int c = ffi_extract_int(arg0);
    putchar(c);
    return term_new_num(c);
  }

  if (strcmp(name, "getchar") == 0) {
    int c = getchar();
    return term_new_num(c);
  }

  if (strcmp(name, "print") == 0 || strcmp(name, "printf") == 0) {
    char *s = ffi_extract_string(arg0);
    printf("%s", s);
    return term_new_num(0);
  }

  if (strcmp(name, "exit") == 0) {
    int code = ffi_extract_int(arg0);
    exit(code);
    return term_new_num(0);
  }

  if (strcmp(name, "newline") == 0) {
    putchar('\n');
    return term_new_num(0);
  }

  fprintf(stderr, "FFI: unimplemented function '%s'\n", name);
  return term_new_num(0);
}

fn Term ffi_eval_code(Term code);
fn Term ffi_eval_with_menv(Term menv, Term expr);

fn Term ffi_process(Term result) {
  while (1) {
    u8 tag = term_tag(result);
    if (tag < C00 || tag > C16) break;

    u32 nam = term_ext(result);

    if (nam == FFI_NAM_FFI) {
      u32 loc = term_val(result);
      Term name = HEAP[loc];
      Term args = HEAP[loc + 1];
      result = ffi_execute(name, args);
      break;
    }

    if (nam == FFI_NAM_DO) {
      u32 loc = term_val(result);
      Term first = HEAP[loc];
      Term rest = HEAP[loc + 1];
      Term first_result = ffi_process(first);
      (void)first_result;
      result = ffi_eval_code(rest);
      continue;
    }

    if (nam == PURPLE_NAM_WMENV || nam == purple_nick("WMnv")) {
      u32 loc = term_val(result);
      Term menv = wnf(HEAP[loc]);
      Term body = wnf(HEAP[loc + 1]);
      result = ffi_eval_with_menv(menv, body);
      continue;
    }

    break;
  }
  return result;
}

fn Term ffi_eval_with_menv(Term menv, Term expr) {
  u32 eval_id = table_find("purple_eval", 11);
  u32 unwrap_id = table_find("purple_unwrap", 13);
  Term eval_ref = term_new_ref(eval_id);
  Term unwrap_ref = term_new_ref(unwrap_id);
  Term app1 = term_new_app(eval_ref, menv);
  Term app2 = term_new_app(app1, expr);
  Term wrapped = term_new_app(unwrap_ref, app2);
  Term result = wnf(wrapped);
  return ffi_process(result);
}

fn Term ffi_eval_code(Term code) {
  u8 tag = term_tag(code);
  if (tag == NUM) return code;

  if (tag >= C00 && tag <= C16) {
    u32 nam = term_ext(code);
    if (nam == PURPLE_NAM_WMENV || nam == purple_nick("WMnv")) {
      u32 loc = term_val(code);
      Term menv = wnf(HEAP[loc]);
      Term body = wnf(HEAP[loc + 1]);
      return ffi_eval_with_menv(menv, body);
    }
    if (nam == purple_nick("Lit")) {
      u32 loc = term_val(code);
      return HEAP[loc];
    }
  }

  Term result = wnf(code);
  return ffi_process(result);
}

// Runtime path
fn void get_runtime_path(char *out, int size, const char *argv0) {
  char *abs = realpath(argv0, NULL);
#ifdef __linux__
  if (!abs) abs = realpath("/proc/self/exe", NULL);
#endif
  const char *base = abs ? abs : argv0;
  sys_path_join(out, size, base, "lib/runtime.hvm4");
  if (abs) free(abs);
}

// Global state for runtime
static char *runtime_src = NULL;

fn void load_runtime(const char *argv0) {
  char runtime_path[4096];
  get_runtime_path(runtime_path, sizeof(runtime_path), argv0);
  runtime_src = sys_file_read(runtime_path);
  if (!runtime_src) {
    fprintf(stderr, "Error: could not read runtime '%s'\n", runtime_path);
    exit(1);
  }
}

fn void show_help(void) {
  printf("Purple REPL - Interactive Tower Exploration\n");
  printf("\n");
  printf("Commands:\n");
  printf("  :help       Show this help\n");
  printf("  :quit       Exit REPL\n");
  printf("  :level      Show current meta-level\n");
  printf("  :load <f>   Load and evaluate a file\n");
  printf("  :reset      Reset environment\n");
  printf("  :tower      Run tower demo\n");
  printf("\n");
  printf("Language features:\n");
  printf("  (+ 1 2)                  Arithmetic\n");
  printf("  (lambda (x) (* x x))     Lambda\n");
  printf("  (let (x 10) (+ x 1))     Let binding\n");
  printf("  (if (> x 0) 1 0)         Conditional\n");
  printf("  (EM expr)                Execute at meta-level\n");
  printf("  (meta-level)             Get current level\n");
  printf("  (with-handlers ...)      Custom semantics\n");
  printf("\n");
}

fn int is_command(const char *line, const char *cmd) {
  return strncmp(line, cmd, strlen(cmd)) == 0;
}

// Evaluate a Purple expression and print result
fn int eval_expr(const char *src) {
  // Reset heap for fresh evaluation
  memset(HEAP, 0, HEAP_CAP * sizeof(Term));
  memset(BOOK, 0, BOOK_CAP * sizeof(u32));
  ITRS = 0;
  heap_init_slices();

  // Parse runtime
  PState rs = {
    .file = "<runtime>",
    .src  = runtime_src,
    .pos  = 0,
    .len  = strlen(runtime_src),
    .line = 1,
    .col  = 1
  };
  parse_def(&rs);

  // Parse user expression
  PState ps = {
    .file = "<repl>",
    .src  = src,
    .pos  = 0,
    .len  = strlen(src),
    .line = 1,
    .col  = 1
  };

  Term ast = parse_purple(&ps);

  // Build main: @purple_unwrap(@purple_eval(@purple_menv_new(0, #NIL, #NIL), ast))
  FILE *tmp = tmpfile();
  if (!tmp) {
    fprintf(stderr, "Error: could not create temp file\n");
    return 1;
  }

  fputs("@main = @purple_unwrap(@purple_eval(@purple_menv_new(0)(#NIL)(#NIL), ", tmp);
  purple_compile_emit(tmp, ast);
  fputs("))\n", tmp);

  fseek(tmp, 0, SEEK_END);
  long size = ftell(tmp);
  if (size < 0) {
    fclose(tmp);
    fprintf(stderr, "Error: failed to get temp file size\n");
    return 1;
  }
  fseek(tmp, 0, SEEK_SET);
  char *main_src = malloc(size + 1);
  if (!main_src) {
    fclose(tmp);
    fprintf(stderr, "Error: out of memory\n");
    return 1;
  }
  size_t bytes_read = fread(main_src, 1, size, tmp);
  fclose(tmp);
  if ((long)bytes_read != size) {
    free(main_src);
    fprintf(stderr, "Error: failed to read temp file\n");
    return 1;
  }
  main_src[size] = '\0';

  // Parse main definition
  PState ms = {
    .file = "<main>",
    .src  = main_src,
    .pos  = 0,
    .len  = strlen(main_src),
    .line = 1,
    .col  = 1
  };
  parse_def(&ms);
  free(main_src);

  // Run
  u32 main_id = table_find("main", 4);
  if (BOOK[main_id] == 0) {
    fprintf(stderr, "Error: @main not defined\n");
    return 1;
  }

  Term result = wnf(term_new_ref(main_id));
  result = ffi_process(result);

  // Print result
  print_term(result);
  printf("\n");

  return 0;
}

// Read a line, handling multi-line input with balanced parens
fn char* read_expr(void) {
  static char buf[65536];
  int pos = 0;
  int parens = 0;
  int in_string = 0;
  int in_comment = 0;
  int got_input = 0;

  while (1) {
    if (!got_input) {
      printf("purple> ");
    } else {
      printf("  ...   ");
    }
    fflush(stdout);

    char line[4096];
    if (!fgets(line, sizeof(line), stdin)) {
      if (got_input) {
        buf[pos] = '\0';
        return buf;
      }
      return NULL;
    }

    // Copy line to buffer, tracking parens
    for (int i = 0; line[i]; i++) {
      char c = line[i];

      if (c == '\n' && !in_string) {
        if (pos < (int)sizeof(buf) - 1) buf[pos++] = ' ';
        continue;
      }

      if (!in_string && !in_comment && c == ';') {
        in_comment = 1;
      }
      if (in_comment) {
        if (c == '\n') in_comment = 0;
        continue;
      }

      if (c == '"' && (pos == 0 || buf[pos-1] != '\\')) {
        in_string = !in_string;
      }

      if (!in_string) {
        if (c == '(') parens++;
        if (c == ')') parens--;
      }

      if (pos < (int)sizeof(buf) - 1) {
        buf[pos++] = c;
        got_input = 1;
      }
    }

    // Check if expression is complete
    if (got_input && parens <= 0 && !in_string) {
      buf[pos] = '\0';

      // Trim whitespace
      char *start = buf;
      while (*start == ' ' || *start == '\t' || *start == '\n') start++;
      if (start != buf) {
        memmove(buf, start, strlen(start) + 1);
      }
      int len = strlen(buf);
      while (len > 0 && (buf[len-1] == ' ' || buf[len-1] == '\t' || buf[len-1] == '\n')) {
        buf[--len] = '\0';
      }

      if (buf[0] == '\0') {
        pos = 0;
        parens = 0;
        got_input = 0;
        continue;
      }

      return buf;
    }
  }
}

// Handle signals
static volatile int running = 1;

fn void signal_handler(int sig) {
  (void)sig;
  write(STDOUT_FILENO, "\n", 1);
  running = 1;
}

int main(int argc, char **argv) {
  (void)argc;

  signal(SIGINT, signal_handler);

  // Configure threads
  thread_set_count(1);
  wnf_set_tid(0);

  // Allocate memory
  BOOK  = calloc(BOOK_CAP, sizeof(u32));
  HEAP  = calloc(HEAP_CAP, sizeof(Term));
  TABLE = calloc(BOOK_CAP, sizeof(char*));

  if (!BOOK || !HEAP || !TABLE) {
    sys_error("Memory allocation failed");
  }
  heap_init_slices();

  // Initialize names
  purple_names_init();
  ffi_names_init();

  // Load runtime
  load_runtime(argv[0]);

  printf("Purple REPL v0.1 - Type :help for commands\n");
  printf("Tower of Interpreters ready.\n\n");

  while (running) {
    char *expr = read_expr();
    if (!expr) {
      printf("\nGoodbye!\n");
      break;
    }

    // Handle commands
    if (expr[0] == ':') {
      if (is_command(expr, ":quit") || is_command(expr, ":q")) {
        printf("Goodbye!\n");
        break;
      }
      if (is_command(expr, ":help") || is_command(expr, ":h") || is_command(expr, ":?")) {
        show_help();
        continue;
      }
      if (is_command(expr, ":level")) {
        eval_expr("(meta-level)");
        continue;
      }
      if (is_command(expr, ":tower")) {
        printf("Running tower demo...\n");
        eval_expr("(+ 5 (+ (EM (meta-level)) (shift 2 (meta-level))))");
        continue;
      }
      if (is_command(expr, ":reset")) {
        printf("Environment reset.\n");
        continue;
      }
      if (is_command(expr, ":load ")) {
        char *path = expr + 6;
        while (*path == ' ') path++;
        char *src = sys_file_read(path);
        if (!src) {
          printf("Error: could not load '%s'\n", path);
          continue;
        }
        eval_expr(src);
        free(src);
        continue;
      }
      printf("Unknown command: %s (type :help for commands)\n", expr);
      continue;
    }

    // Evaluate expression
    eval_expr(expr);
  }

  free(runtime_src);
  free(HEAP);
  free(BOOK);
  free(TABLE);

  return 0;
}
