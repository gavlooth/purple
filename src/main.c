// Purple compiler entry point
// Usage: ./purplec <file.purple> [out.hvm4]

// Include HVM4 runtime from submodule
#include "../hvm4/clang/hvm4.c"

// Purple parser (includes Pink functionality)
#include "parse/_.c"

// Purple compiler
#include "compile/_.c"

fn void purple_usage(const char *prog) {
  fprintf(stderr, "Usage: %s <file.purple> [out.hvm4]\n", prog);
}

fn void purple_runtime_path(char *out, int size, const char *argv0) {
  char *abs = realpath(argv0, NULL);
  const char *base = abs ? abs : argv0;
  sys_path_join(out, size, base, "lib/runtime.hvm4");
  if (abs) {
    free(abs);
  }
}

int main(int argc, char **argv) {
  if (argc < 2 || argc > 3) {
    purple_usage(argv[0]);
    return 1;
  }

  char *in_path = argv[1];
  char *out_path = NULL;
  if (argc == 3) {
    out_path = argv[2];
  }

  // Allocate runtime storage
  BOOK  = calloc(BOOK_CAP, sizeof(u32));
  HEAP  = calloc(HEAP_CAP, sizeof(Term));
  TABLE = calloc(BOOK_CAP, sizeof(char*));

  if (!BOOK || !HEAP || !TABLE) {
    sys_error("Memory allocation failed");
  }
  heap_init_slices();

  // Read Purple source
  char *src = sys_file_read(in_path);
  if (!src) {
    fprintf(stderr, "Error: could not open '%s'\n", in_path);
    return 1;
  }

  PState s = {
    .file = in_path,
    .src  = src,
    .pos  = 0,
    .len  = strlen(src),
    .line = 1,
    .col  = 1
  };

  // Parse using Purple parser (extends Pink)
  Term ast = parse_purple(&s);
  free(src);

  FILE *out = stdout;
  if (out_path != NULL) {
    out = fopen(out_path, "w");
    if (!out) {
      fprintf(stderr, "Error: could not open '%s' for writing\n", out_path);
      return 1;
    }
  }

  // Always use Purple runtime (includes Pink runtime + Purple extensions)
  char runtime_path[4096];
  purple_runtime_path(runtime_path, sizeof(runtime_path), argv[0]);
  purple_compile_emit_runtime(out, ast, runtime_path);

  if (out != stdout) {
    fclose(out);
  }

  free(HEAP);
  free(BOOK);
  free(TABLE);

  return 0;
}
