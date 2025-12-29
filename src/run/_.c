// Purple Runner - Executes Purple programs with FFI support
// Usage: ./purple-run <file.purple> [-s]
//
// This driver:
// 1. Compiles Purple source to HVM4
// 2. Runs HVM4 evaluation
// 3. Interprets FFI markers and executes C functions
// 4. Sequences Do blocks with FFI calls

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Include HVM4 runtime
#include "../../hvm4/clang/hvm4.c"

// Include Purple parser and compiler
#include "../parse/_.c"
#include "../compile/_.c"

// FFI function name constants (nick-encoded)
static u32 FFI_NAM_PUTS;
static u32 FFI_NAM_PUTC;
static u32 FFI_NAM_GETC;
static u32 FFI_NAM_EXIT;
static u32 FFI_NAM_PRNT;

// FFI constructor names
static u32 FFI_NAM_FFI;
static u32 FFI_NAM_DO;

fn void ffi_names_init(void) {
  FFI_NAM_PUTS = purple_nick("puts");
  FFI_NAM_PUTC = purple_nick("putc");
  FFI_NAM_GETC = purple_nick("getc");
  FFI_NAM_EXIT = purple_nick("exit");
  FFI_NAM_PRNT = purple_nick("prnt");
  FFI_NAM_FFI  = purple_nick("FFI");
  FFI_NAM_DO   = purple_nick("Do");
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

    // head should be #CHR{codepoint}
    if (term_tag(head) >= C00 && term_tag(head) <= C16 && term_ext(head) == NAM_CHR) {
      u32 cp = term_val(HEAP[term_val(head)]);
      if (i < sizeof(buf) - 1) {
        buf[i++] = (char)cp;
      }
    }
    cur = tail;
  }
  buf[i] = '\0';
  return buf;
}

// Extract integer from value
fn int ffi_extract_int(Term v) {
  if (term_tag(v) == NUM) {
    return (int)term_val(v);
  }
  return 0;
}

// Execute FFI call and return result
fn Term ffi_execute(Term name_term, Term args) {
  // Get function name from table
  u32 name_id = term_val(name_term);
  const char *name = TABLE[name_id];

  if (!name) {
    fprintf(stderr, "FFI: unknown function ID %u\n", name_id);
    return term_new_num(0);
  }

  // Get first argument
  Term arg0 = term_new_num(0);
  if (term_tag(args) >= C00 && term_tag(args) <= C16 && term_ext(args) == NAM_CON) {
    u32 loc = term_val(args);
    arg0 = HEAP[loc];
  }

  // Dispatch based on function name
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
    // Simple print: just print the first argument as string
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

// Forward declaration
fn Term ffi_eval_code(Term code);

// Process result, executing FFI calls and Do sequences
fn Term ffi_process(Term result) {
  while (1) {
    u8 tag = term_tag(result);
    if (tag < C00 || tag > C16) {
      break;
    }

    u32 nam = term_ext(result);

    // #FFI{name, args} - execute and return result
    if (nam == FFI_NAM_FFI) {
      u32 loc = term_val(result);
      Term name = HEAP[loc];
      Term args = HEAP[loc + 1];
      result = ffi_execute(name, args);
      break;
    }

    // #Do{first, rest} - execute first (if FFI), then evaluate rest
    if (nam == FFI_NAM_DO) {
      u32 loc = term_val(result);
      Term first = HEAP[loc];
      Term rest = HEAP[loc + 1];

      // Process first
      Term first_result = ffi_process(first);
      (void)first_result;  // discard

      // rest is unevaluated code - need to evaluate it
      result = ffi_eval_code(rest);
      continue;
    }

    // Not an FFI marker, return as-is
    break;
  }

  return result;
}

// Evaluate a Purple code term by building and running HVM4
fn Term ffi_eval_code(Term code) {
  // Build HVM4 expression: @purple_unwrap(@purple_eval(@purple_menv_new(0, #NIL, #NIL), code))
  // For simplicity, we just evaluate through SNF if it's a simple value

  // Check if code is already a simple value
  u8 tag = term_tag(code);
  if (tag == NUM) {
    return code;
  }

  // For code nodes like #Lit{n}, extract and return
  if (tag >= C00 && tag <= C16) {
    u32 nam = term_ext(code);
    // #Lit{n} -> extract n
    if (nam == purple_nick("Lit")) {
      u32 loc = term_val(code);
      return HEAP[loc];  // return the number inside
    }
  }

  // For complex code, run SNF
  Term snf_result = snf(code, 0, 0);
  return ffi_process(snf_result);
}

// Read runtime and emit it
fn void emit_runtime(FILE *out, const char *argv0) {
  char runtime_path[4096];
  char *abs = realpath(argv0, NULL);
  const char *base = abs ? abs : argv0;
  sys_path_join(runtime_path, sizeof(runtime_path), base, "lib/runtime.hvm4");
  if (abs) free(abs);

  char *runtime = sys_file_read(runtime_path);
  if (!runtime) {
    fprintf(stderr, "Error: could not read runtime '%s'\n", runtime_path);
    exit(1);
  }
  fputs(runtime, out);
  free(runtime);
}

int main(int argc, char **argv) {
  int show_stats = 0;
  char *in_path = NULL;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-s") == 0) {
      show_stats = 1;
    } else if (argv[i][0] != '-') {
      in_path = argv[i];
    }
  }

  if (!in_path) {
    fprintf(stderr, "Usage: %s <file.purple> [-s]\n", argv[0]);
    return 1;
  }

  // Allocate memory
  BOOK  = calloc(BOOK_CAP, sizeof(u32));
  HEAP  = calloc(HEAP_CAP, sizeof(Term));
  STACK = calloc(WNF_CAP, sizeof(Term));
  TABLE = calloc(BOOK_CAP, sizeof(char*));

  if (!BOOK || !HEAP || !STACK || !TABLE) {
    sys_error("Memory allocation failed");
  }

  // Initialize names
  purple_names_init();
  ffi_names_init();

  // Read Purple source
  char *src = sys_file_read(in_path);
  if (!src) {
    fprintf(stderr, "Error: could not open '%s'\n", in_path);
    return 1;
  }

  // Parse Purple
  PState ps = {
    .file = in_path,
    .src  = src,
    .pos  = 0,
    .len  = strlen(src),
    .line = 1,
    .col  = 1
  };
  Term ast = parse_purple(&ps);
  free(src);

  // Emit to temp file
  FILE *tmp = tmpfile();
  if (!tmp) {
    fprintf(stderr, "Error: could not create temp file\n");
    return 1;
  }

  emit_runtime(tmp, argv[0]);
  fputs("\n@main = @purple_unwrap(@purple_eval(@purple_menv_new(0, #NIL, #NIL), ", tmp);
  purple_compile_emit(tmp, ast);
  fputs("))\n", tmp);

  // Rewind and read as string
  fseek(tmp, 0, SEEK_END);
  long size = ftell(tmp);
  fseek(tmp, 0, SEEK_SET);
  char *hvm4_src = malloc(size + 1);
  fread(hvm4_src, 1, size, tmp);
  hvm4_src[size] = '\0';
  fclose(tmp);

  // Reset heap for HVM4
  memset(HEAP, 0, HEAP_CAP * sizeof(Term));
  memset(BOOK, 0, BOOK_CAP * sizeof(u32));
  ALLOC = 0;
  ITRS = 0;

  // Parse HVM4
  PState hs = {
    .file = "<generated>",
    .src  = hvm4_src,
    .pos  = 0,
    .len  = strlen(hvm4_src),
    .line = 1,
    .col  = 1
  };
  parse_def(&hs);
  free(hvm4_src);

  // Get @main
  u32 main_id = table_find("main", 4);
  if (BOOK[main_id] == 0) {
    fprintf(stderr, "Error: @main not defined\n");
    return 1;
  }

  // Evaluate
  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC, &start);

  Term result = snf(term_new_ref(main_id), 0, 0);

  // Process FFI calls
  result = ffi_process(result);

  clock_gettime(CLOCK_MONOTONIC, &end);

  // Print final result (if not unit)
  if (term_tag(result) != NUM || term_val(result) != 0) {
    print_term(result);
    printf("\n");
  }

  if (show_stats) {
    double dt = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("- Itrs: %llu interactions\n", (unsigned long long)ITRS);
    printf("- Time: %.3f seconds\n", dt);
  }

  free(HEAP);
  free(BOOK);
  free(STACK);
  free(TABLE);

  return 0;
}
