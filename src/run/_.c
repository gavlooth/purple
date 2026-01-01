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
#include <dlfcn.h>
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>

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
static u32 FFI_NAM_CST;
static u32 FFI_NAM_FIX;  // Fixed-point number: #Fix{hi, lo, scale}
static u32 FFI_NAM_PTR;  // Opaque pointer: #Ptr{hi, lo}

// Dynamic library storage
#define FFI_MAX_LIBS 64
static void* FFI_LIBRARIES[FFI_MAX_LIBS];
static u32 FFI_LIBRARY_COUNT = 0;

// FFI argument types
#define FFI_TYPE_INT    0
#define FFI_TYPE_FLOAT  1
#define FFI_TYPE_PTR    2
#define FFI_TYPE_STR    3
#define FFI_TYPE_VOID   4

// Registered external function
typedef struct {
  char name[64];
  void* ptr;
  int arg_count;
  int arg_types[16];
  int ret_type;
} FFIFunction;

#define FFI_MAX_FUNCS 256
static FFIFunction FFI_FUNCTIONS[FFI_MAX_FUNCS];
static u32 FFI_FUNCTION_COUNT = 0;

// =============================================================================
// Concurrent FFI Infrastructure
// =============================================================================

// FFI call types for async dispatch
#define FFI_CALL_INT  0   // Returns int/ptr
#define FFI_CALL_DBL  1   // Returns double

// FFI Future - represents a pending async FFI call
typedef struct {
  atomic_int ready;           // 0 = pending, 1 = complete
  Term result;                // Result term (valid when ready=1)

  // Call parameters (copied for thread safety)
  void* fn_ptr;
  int call_type;              // FFI_CALL_INT or FFI_CALL_DBL
  int argc;
  intptr_t args_int[8];       // For int/ptr args
  double args_dbl[8];         // For double args
  u32 result_scale;           // Scale for double->Fix conversion
} FFIFuture;

#define FFI_MAX_FUTURES 4096
static FFIFuture FFI_FUTURES[FFI_MAX_FUTURES];
static atomic_uint FFI_FUTURE_COUNT = 0;

// FFI Task Queue for worker threads
typedef struct {
  u32 future_id;
} FFITask;

#define FFI_QUEUE_SIZE 1024
static FFITask FFI_TASK_QUEUE[FFI_QUEUE_SIZE];
static atomic_uint FFI_QUEUE_HEAD = 0;  // Next slot to write
static atomic_uint FFI_QUEUE_TAIL = 0;  // Next slot to read
static pthread_mutex_t FFI_QUEUE_MUTEX = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t FFI_QUEUE_COND = PTHREAD_COND_INITIALIZER;
static atomic_int FFI_WORKERS_RUNNING = 0;

// Thread pool
#define FFI_NUM_WORKERS 4
static pthread_t FFI_WORKERS[FFI_NUM_WORKERS];

// Pending constructor name
static u32 FFI_NAM_PEND;  // #Pend{future_id}

fn void ffi_names_init(void) {
  FFI_NAM_PUTS = purple_nick("puts");
  FFI_NAM_PUTC = purple_nick("putc");
  FFI_NAM_GETC = purple_nick("getc");
  FFI_NAM_EXIT = purple_nick("exit");
  FFI_NAM_PRNT = purple_nick("prnt");
  FFI_NAM_FFI  = purple_nick("FFI");
  FFI_NAM_DO   = purple_nick("Do");
  FFI_NAM_CST  = purple_nick("Cst");
  FFI_NAM_FIX  = purple_nick("Fix");
  FFI_NAM_PTR  = purple_nick("Ptr");
  FFI_NAM_PEND = purple_nick("Pend");
}

// =============================================================================
// FFI Worker Thread Implementation
// =============================================================================

// Execute a single FFI future (called by worker thread)
fn void ffi_execute_future(FFIFuture* fut) {
  Term result;

  if (fut->call_type == FFI_CALL_DBL) {
    // Double-returning function
    typedef double (*fnd0_t)(void);
    typedef double (*fnd1_t)(double);
    typedef double (*fnd2_t)(double, double);
    typedef double (*fnd3_t)(double, double, double);
    typedef double (*fnd4_t)(double, double, double, double);

    double r;
    switch (fut->argc) {
      case 0: r = ((fnd0_t)fut->fn_ptr)(); break;
      case 1: r = ((fnd1_t)fut->fn_ptr)(fut->args_dbl[0]); break;
      case 2: r = ((fnd2_t)fut->fn_ptr)(fut->args_dbl[0], fut->args_dbl[1]); break;
      case 3: r = ((fnd3_t)fut->fn_ptr)(fut->args_dbl[0], fut->args_dbl[1], fut->args_dbl[2]); break;
      case 4: r = ((fnd4_t)fut->fn_ptr)(fut->args_dbl[0], fut->args_dbl[1], fut->args_dbl[2], fut->args_dbl[3]); break;
      default: r = 0.0;
    }

    // Convert to #Fix - need to be careful with heap allocation from worker thread
    // For now, store raw double bits and convert when awaited
    // Actually, let's store the scale and value directly
    double multiplier = 1.0;
    for (u32 i = 0; i < fut->result_scale; i++) multiplier *= 10.0;
    int64_t scaled = (int64_t)(r * multiplier);

    // Store as raw values - we'll build the Term when awaited (main thread)
    fut->args_int[0] = (intptr_t)((u64)scaled >> 32);      // hi
    fut->args_int[1] = (intptr_t)((u32)(scaled & 0xFFFFFFFF)); // lo
    fut->args_int[2] = (intptr_t)fut->result_scale;        // scale
    result = 0;  // Marker: build Fix when awaited
  } else {
    // Int/ptr-returning function
    typedef intptr_t (*fn0_t)(void);
    typedef intptr_t (*fn1_t)(intptr_t);
    typedef intptr_t (*fn2_t)(intptr_t, intptr_t);
    typedef intptr_t (*fn3_t)(intptr_t, intptr_t, intptr_t);
    typedef intptr_t (*fn4_t)(intptr_t, intptr_t, intptr_t, intptr_t);

    intptr_t r;
    switch (fut->argc) {
      case 0: r = ((fn0_t)fut->fn_ptr)(); break;
      case 1: r = ((fn1_t)fut->fn_ptr)(fut->args_int[0]); break;
      case 2: r = ((fn2_t)fut->fn_ptr)(fut->args_int[0], fut->args_int[1]); break;
      case 3: r = ((fn3_t)fut->fn_ptr)(fut->args_int[0], fut->args_int[1], fut->args_int[2]); break;
      case 4: r = ((fn4_t)fut->fn_ptr)(fut->args_int[0], fut->args_int[1], fut->args_int[2], fut->args_int[3]); break;
      default: r = 0;
    }

    result = term_new_num((u32)r);
  }

  fut->result = result;
  atomic_store(&fut->ready, 1);
}

// Worker thread main loop
fn void* ffi_worker_thread(void* arg) {
  (void)arg;

  while (atomic_load(&FFI_WORKERS_RUNNING)) {
    pthread_mutex_lock(&FFI_QUEUE_MUTEX);

    // Wait for work
    while (atomic_load(&FFI_QUEUE_HEAD) == atomic_load(&FFI_QUEUE_TAIL) &&
           atomic_load(&FFI_WORKERS_RUNNING)) {
      pthread_cond_wait(&FFI_QUEUE_COND, &FFI_QUEUE_MUTEX);
    }

    if (!atomic_load(&FFI_WORKERS_RUNNING)) {
      pthread_mutex_unlock(&FFI_QUEUE_MUTEX);
      break;
    }

    // Dequeue task
    u32 tail = atomic_load(&FFI_QUEUE_TAIL);
    FFITask task = FFI_TASK_QUEUE[tail % FFI_QUEUE_SIZE];
    atomic_store(&FFI_QUEUE_TAIL, tail + 1);

    pthread_mutex_unlock(&FFI_QUEUE_MUTEX);

    // Execute the future
    ffi_execute_future(&FFI_FUTURES[task.future_id]);
  }

  return NULL;
}

// Start the worker thread pool
fn void ffi_workers_start(void) {
  atomic_store(&FFI_WORKERS_RUNNING, 1);
  for (int i = 0; i < FFI_NUM_WORKERS; i++) {
    int err = pthread_create(&FFI_WORKERS[i], NULL, ffi_worker_thread, NULL);
    if (err != 0) {
      fprintf(stderr, "FFI: failed to create worker thread %d: %s\n", i, strerror(err));
      // Stop already-created workers before exiting
      atomic_store(&FFI_WORKERS_RUNNING, 0);
      pthread_mutex_lock(&FFI_QUEUE_MUTEX);
      pthread_cond_broadcast(&FFI_QUEUE_COND);
      pthread_mutex_unlock(&FFI_QUEUE_MUTEX);
      for (int j = 0; j < i; j++) {
        pthread_join(FFI_WORKERS[j], NULL);
      }
      exit(1);
    }
  }
}

// Stop the worker thread pool
fn void ffi_workers_stop(void) {
  atomic_store(&FFI_WORKERS_RUNNING, 0);

  // Wake all workers
  pthread_mutex_lock(&FFI_QUEUE_MUTEX);
  pthread_cond_broadcast(&FFI_QUEUE_COND);
  pthread_mutex_unlock(&FFI_QUEUE_MUTEX);

  // Join all workers
  for (int i = 0; i < FFI_NUM_WORKERS; i++) {
    pthread_join(FFI_WORKERS[i], NULL);
  }
}

// Spawn an async FFI call, returns future ID
fn u32 ffi_spawn_async(void* fn_ptr, int call_type, int argc,
                       intptr_t* args_int, double* args_dbl, u32 result_scale) {
  u32 fid = atomic_fetch_add(&FFI_FUTURE_COUNT, 1);
  if (fid >= FFI_MAX_FUTURES) {
    fprintf(stderr, "FFI: too many pending futures\n");
    return 0;
  }

  FFIFuture* fut = &FFI_FUTURES[fid];
  atomic_store(&fut->ready, 0);
  fut->fn_ptr = fn_ptr;
  fut->call_type = call_type;
  fut->argc = argc;
  fut->result_scale = result_scale;

  for (int i = 0; i < argc && i < 8; i++) {
    if (args_int) fut->args_int[i] = args_int[i];
    if (args_dbl) fut->args_dbl[i] = args_dbl[i];
  }

  // Enqueue task
  pthread_mutex_lock(&FFI_QUEUE_MUTEX);
  u32 head = atomic_load(&FFI_QUEUE_HEAD);
  u32 tail = atomic_load(&FFI_QUEUE_TAIL);
  if (head - tail >= FFI_QUEUE_SIZE) {
    pthread_mutex_unlock(&FFI_QUEUE_MUTEX);
    fprintf(stderr, "FFI: task queue overflow\n");
    return 0;
  }
  FFI_TASK_QUEUE[head % FFI_QUEUE_SIZE].future_id = fid;
  atomic_store(&FFI_QUEUE_HEAD, head + 1);
  pthread_cond_signal(&FFI_QUEUE_COND);
  pthread_mutex_unlock(&FFI_QUEUE_MUTEX);

  return fid;
}

// Check if a future is ready (non-blocking)
fn int ffi_future_ready(u32 fid) {
  if (fid >= FFI_MAX_FUTURES) return 1;
  return atomic_load(&FFI_FUTURES[fid].ready);
}

// Await a future (blocking if necessary), returns result Term
fn Term ffi_await(u32 fid) {
  if (fid >= FFI_MAX_FUTURES) return term_new_num(0);

  FFIFuture* fut = &FFI_FUTURES[fid];

  // Spin-wait (could use condition variable for better efficiency)
  while (!atomic_load(&fut->ready)) {
    // Yield to other threads
    sched_yield();
  }

  // Build result Term if needed (for double results)
  if (fut->call_type == FFI_CALL_DBL) {
    u32 hi = (u32)fut->args_int[0];
    u32 lo = (u32)fut->args_int[1];
    u32 scale = (u32)fut->args_int[2];

    Term args[3];
    args[0] = term_new_num(hi);
    args[1] = term_new_num(lo);
    args[2] = term_new_num(scale);
    return term_new_ctr(FFI_NAM_FIX, 3, args);
  }

  return fut->result;
}

// Create a #Pend{future_id} term
fn Term ffi_make_pending(u32 fid) {
  Term args[1];
  args[0] = term_new_num(fid);
  return term_new_ctr(FFI_NAM_PEND, 1, args);
}

// Load a dynamic library
// Returns library index or -1 on failure
fn int ffi_load_library(const char* path) {
  if (FFI_LIBRARY_COUNT >= FFI_MAX_LIBS) {
    fprintf(stderr, "FFI: too many libraries\n");
    return -1;
  }

  void* handle = dlopen(path, RTLD_LAZY);
  if (!handle) {
    fprintf(stderr, "FFI: failed to load '%s': %s\n", path, dlerror());
    return -1;
  }

  FFI_LIBRARIES[FFI_LIBRARY_COUNT] = handle;
  return (int)FFI_LIBRARY_COUNT++;
}

// Register an external function
// Returns function index or -1 on failure
fn int ffi_register_function(const char* name, int arg_count, int* arg_types, int ret_type) {
  if (FFI_FUNCTION_COUNT >= FFI_MAX_FUNCS) {
    fprintf(stderr, "FFI: too many functions\n");
    return -1;
  }

  // Search for symbol in all loaded libraries
  void* ptr = NULL;
  for (u32 i = 0; i < FFI_LIBRARY_COUNT && !ptr; i++) {
    ptr = dlsym(FFI_LIBRARIES[i], name);
  }

  if (!ptr) {
    // Also check global symbols
    ptr = dlsym(RTLD_DEFAULT, name);
  }

  if (!ptr) {
    fprintf(stderr, "FFI: symbol '%s' not found\n", name);
    return -1;
  }

  FFIFunction* func = &FFI_FUNCTIONS[FFI_FUNCTION_COUNT];
  strncpy(func->name, name, sizeof(func->name) - 1);
  func->name[sizeof(func->name) - 1] = '\0';
  func->ptr = ptr;
  func->arg_count = arg_count;
  func->ret_type = ret_type;
  for (int i = 0; i < arg_count && i < 16; i++) {
    func->arg_types[i] = arg_types[i];
  }

  return (int)FFI_FUNCTION_COUNT++;
}

// Find a registered function by name
fn FFIFunction* ffi_find_function(const char* name) {
  for (u32 i = 0; i < FFI_FUNCTION_COUNT; i++) {
    if (strcmp(FFI_FUNCTIONS[i].name, name) == 0) {
      return &FFI_FUNCTIONS[i];
    }
  }
  return NULL;
}

// Extract string from HVM4 cons list of CHR
// Properly encodes Unicode codepoints as UTF-8
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
      // Encode codepoint as UTF-8
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

// Extract integer from value
// Handles both raw NUM and #Cst{n} wrapper
fn int ffi_extract_int(Term v) {
  if (term_tag(v) == NUM) {
    return (int)term_val(v);
  }
  // Check for #Cst{n} wrapper
  if (term_tag(v) >= C00 && term_tag(v) <= C16 && term_ext(v) == FFI_NAM_CST) {
    u32 loc = term_val(v);
    Term inner = HEAP[loc];
    if (term_tag(inner) == NUM) {
      return (int)term_val(inner);
    }
  }
  return 0;
}

// Extract double from value
// Handles #Fix{hi, lo, scale} and #Cst{n}
fn double ffi_extract_double(Term v) {
  u8 tag = term_tag(v);

  // Raw number
  if (tag == NUM) {
    return (double)term_val(v);
  }

  if (tag >= C00 && tag <= C16) {
    u32 nam = term_ext(v);
    u32 loc = term_val(v);

    // #Cst{n} - treat as integer
    if (nam == FFI_NAM_CST) {
      Term inner = HEAP[loc];
      if (term_tag(inner) == NUM) {
        return (double)term_val(inner);
      }
    }

    // #Fix{hi, lo, scale} - convert to double
    if (nam == FFI_NAM_FIX) {
      Term hi_term = wnf(HEAP[loc]);
      Term lo_term = wnf(HEAP[loc + 1]);
      Term scale_term = wnf(HEAP[loc + 2]);

      u32 hi = (term_tag(hi_term) == NUM) ? term_val(hi_term) : 0;
      u32 lo = (term_tag(lo_term) == NUM) ? term_val(lo_term) : 0;
      u32 scale = (term_tag(scale_term) == NUM) ? term_val(scale_term) : 0;

      // Reconstruct 64-bit value and convert to double
      int64_t value = ((int64_t)hi << 32) | lo;

      // Divide by 10^scale
      double divisor = 1.0;
      for (u32 i = 0; i < scale; i++) {
        divisor *= 10.0;
      }

      return (double)value / divisor;
    }
  }

  return 0.0;
}

// Wrap a double as #Fix{hi, lo, scale}
// Uses default scale of 6 (millionths precision)
fn Term ffi_wrap_double(double d, u32 scale) {
  // Scale up by 10^scale
  double multiplier = 1.0;
  for (u32 i = 0; i < scale; i++) {
    multiplier *= 10.0;
  }

  int64_t value = (int64_t)(d * multiplier);
  u32 hi = (u32)((u64)value >> 32);
  u32 lo = (u32)(value & 0xFFFFFFFF);

  // Build #Fix{hi, lo, scale}
  Term args[3];
  args[0] = term_new_num(hi);
  args[1] = term_new_num(lo);
  args[2] = term_new_num(scale);

  return term_new_ctr(FFI_NAM_FIX, 3, args);
}

// Extract pointer from #Ptr{hi, lo}
fn void* ffi_extract_pointer(Term v) {
  if (term_tag(v) >= C00 && term_tag(v) <= C16 && term_ext(v) == FFI_NAM_PTR) {
    u32 loc = term_val(v);
    Term hi_term = HEAP[loc];
    Term lo_term = HEAP[loc + 1];

    u32 hi = (term_tag(hi_term) == NUM) ? term_val(hi_term) : 0;
    u32 lo = (term_tag(lo_term) == NUM) ? term_val(lo_term) : 0;

    // Reconstruct 64-bit pointer
    uintptr_t ptr = ((uintptr_t)hi << 32) | lo;
    return (void*)ptr;
  }
  return NULL;
}

// Wrap a pointer as #Ptr{hi, lo}
fn Term ffi_wrap_pointer(void* ptr) {
  uintptr_t p = (uintptr_t)ptr;
  u32 hi = (u32)(p >> 32);
  u32 lo = (u32)(p & 0xFFFFFFFF);

  // Build #Ptr{hi, lo}
  Term args[2];
  args[0] = term_new_num(hi);
  args[1] = term_new_num(lo);

  return term_new_ctr(FFI_NAM_PTR, 2, args);
}

// Execute FFI call and return result
fn Term ffi_execute(Term name_term, Term args) {
  // Get function name from table
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

  // ffi-load: Load a dynamic library
  // (ffi "ffi-load" (list "path/to/lib.so"))
  if (strcmp(name, "ffi-load") == 0) {
    char *path = ffi_extract_string(arg0);
    int idx = ffi_load_library(path);
    return term_new_num(idx >= 0 ? (u32)idx : 0);
  }

  // ffi-sym: Look up a symbol in loaded libraries
  // (ffi "ffi-sym" (list "symbol_name"))
  // Returns a #Ptr to the symbol
  if (strcmp(name, "ffi-sym") == 0) {
    char *sym_name = ffi_extract_string(arg0);
    void* ptr = NULL;

    // Search in loaded libraries
    for (u32 i = 0; i < FFI_LIBRARY_COUNT && !ptr; i++) {
      ptr = dlsym(FFI_LIBRARIES[i], sym_name);
    }
    if (!ptr) {
      ptr = dlsym(RTLD_DEFAULT, sym_name);
    }

    return ffi_wrap_pointer(ptr);
  }

  // ffi-call-ptr: Call a function pointer with arguments
  // (ffi "ffi-call-ptr" (list #Ptr{...} arg1 arg2 ...))
  // For now, supports up to 4 int/ptr arguments, returns int
  if (strcmp(name, "ffi-call-ptr") == 0) {
    void* fn_ptr = ffi_extract_pointer(arg0);
    if (!fn_ptr) {
      fprintf(stderr, "FFI: null function pointer\n");
      return term_new_num(0);
    }

    // Get remaining args from the list
    Term rest = args;
    if (term_tag(rest) >= C00 && term_tag(rest) <= C16 && term_ext(rest) == NAM_CON) {
      rest = HEAP[term_val(rest) + 1];  // tail
    }

    // Extract up to 4 arguments
    intptr_t a[4] = {0, 0, 0, 0};
    int argc = 0;
    while (argc < 4 && term_tag(rest) >= C00 && term_tag(rest) <= C16 && term_ext(rest) == NAM_CON) {
      u32 loc = term_val(rest);
      Term arg = HEAP[loc];

      // Check argument type
      if (term_tag(arg) >= C00 && term_tag(arg) <= C16 && term_ext(arg) == FFI_NAM_PTR) {
        a[argc] = (intptr_t)ffi_extract_pointer(arg);
      } else if (term_tag(arg) >= C00 && term_tag(arg) <= C16 && term_ext(arg) == FFI_NAM_FIX) {
        // Fixed-point: pass as double bits (for float functions)
        double d = ffi_extract_double(arg);
        a[argc] = *(intptr_t*)&d;
      } else {
        a[argc] = ffi_extract_int(arg);
      }

      argc++;
      rest = HEAP[loc + 1];
    }

    // Call with appropriate signature
    typedef intptr_t (*fn0_t)(void);
    typedef intptr_t (*fn1_t)(intptr_t);
    typedef intptr_t (*fn2_t)(intptr_t, intptr_t);
    typedef intptr_t (*fn3_t)(intptr_t, intptr_t, intptr_t);
    typedef intptr_t (*fn4_t)(intptr_t, intptr_t, intptr_t, intptr_t);

    intptr_t result;
    switch (argc) {
      case 0: result = ((fn0_t)fn_ptr)(); break;
      case 1: result = ((fn1_t)fn_ptr)(a[0]); break;
      case 2: result = ((fn2_t)fn_ptr)(a[0], a[1]); break;
      case 3: result = ((fn3_t)fn_ptr)(a[0], a[1], a[2]); break;
      case 4: result = ((fn4_t)fn_ptr)(a[0], a[1], a[2], a[3]); break;
      default: result = 0;
    }

    return term_new_num((u32)result);
  }

  // ffi-call-ptr-d: Call returning double, wrap as #Fix
  if (strcmp(name, "ffi-call-ptr-d") == 0) {
    void* fn_ptr = ffi_extract_pointer(arg0);
    if (!fn_ptr) {
      fprintf(stderr, "FFI: null function pointer\n");
      return ffi_wrap_double(0.0, 6);
    }

    // Get remaining args
    Term rest = args;
    if (term_tag(rest) >= C00 && term_tag(rest) <= C16 && term_ext(rest) == NAM_CON) {
      rest = HEAP[term_val(rest) + 1];
    }

    double a[4] = {0, 0, 0, 0};
    int argc = 0;
    while (argc < 4 && term_tag(rest) >= C00 && term_tag(rest) <= C16 && term_ext(rest) == NAM_CON) {
      u32 loc = term_val(rest);
      Term arg = HEAP[loc];
      a[argc] = ffi_extract_double(arg);
      argc++;
      rest = HEAP[loc + 1];
    }

    typedef double (*fnd0_t)(void);
    typedef double (*fnd1_t)(double);
    typedef double (*fnd2_t)(double, double);
    typedef double (*fnd3_t)(double, double, double);
    typedef double (*fnd4_t)(double, double, double, double);

    double result;
    switch (argc) {
      case 0: result = ((fnd0_t)fn_ptr)(); break;
      case 1: result = ((fnd1_t)fn_ptr)(a[0]); break;
      case 2: result = ((fnd2_t)fn_ptr)(a[0], a[1]); break;
      case 3: result = ((fnd3_t)fn_ptr)(a[0], a[1], a[2]); break;
      case 4: result = ((fnd4_t)fn_ptr)(a[0], a[1], a[2], a[3]); break;
      default: result = 0.0;
    }

    return ffi_wrap_double(result, 6);
  }

  // ffi-call-async: Non-blocking call returning int, returns #Pend{future_id}
  if (strcmp(name, "ffi-call-async") == 0) {
    void* fn_ptr = ffi_extract_pointer(arg0);
    if (!fn_ptr) {
      fprintf(stderr, "FFI: null function pointer\n");
      return term_new_num(0);
    }

    // Get remaining args
    Term rest = args;
    if (term_tag(rest) >= C00 && term_tag(rest) <= C16 && term_ext(rest) == NAM_CON) {
      rest = HEAP[term_val(rest) + 1];
    }

    intptr_t a[8] = {0};
    int argc = 0;
    while (argc < 8 && term_tag(rest) >= C00 && term_tag(rest) <= C16 && term_ext(rest) == NAM_CON) {
      u32 loc = term_val(rest);
      Term arg = HEAP[loc];
      if (term_tag(arg) >= C00 && term_tag(arg) <= C16 && term_ext(arg) == FFI_NAM_PTR) {
        a[argc] = (intptr_t)ffi_extract_pointer(arg);
      } else {
        a[argc] = ffi_extract_int(arg);
      }
      argc++;
      rest = HEAP[loc + 1];
    }

    u32 fid = ffi_spawn_async(fn_ptr, FFI_CALL_INT, argc, a, NULL, 0);
    return ffi_make_pending(fid);
  }

  // ffi-call-async-d: Non-blocking call returning double, returns #Pend{future_id}
  if (strcmp(name, "ffi-call-async-d") == 0) {
    void* fn_ptr = ffi_extract_pointer(arg0);
    if (!fn_ptr) {
      fprintf(stderr, "FFI: null function pointer\n");
      return ffi_make_pending(0);
    }

    // Get remaining args
    Term rest = args;
    if (term_tag(rest) >= C00 && term_tag(rest) <= C16 && term_ext(rest) == NAM_CON) {
      rest = HEAP[term_val(rest) + 1];
    }

    double a[8] = {0};
    int argc = 0;
    while (argc < 8 && term_tag(rest) >= C00 && term_tag(rest) <= C16 && term_ext(rest) == NAM_CON) {
      u32 loc = term_val(rest);
      Term arg = HEAP[loc];
      a[argc] = ffi_extract_double(arg);
      argc++;
      rest = HEAP[loc + 1];
    }

    u32 fid = ffi_spawn_async(fn_ptr, FFI_CALL_DBL, argc, NULL, a, 6);
    return ffi_make_pending(fid);
  }

  // ffi-await: Block until future completes and return result
  if (strcmp(name, "ffi-await") == 0) {
    // arg0 should be #Pend{future_id}
    if (term_tag(arg0) >= C00 && term_tag(arg0) <= C16 && term_ext(arg0) == FFI_NAM_PEND) {
      u32 loc = term_val(arg0);
      Term fid_term = HEAP[loc];
      if (term_tag(fid_term) == NUM) {
        return ffi_await(term_val(fid_term));
      }
    }
    // If passed a future_id directly
    if (term_tag(arg0) == NUM) {
      return ffi_await(term_val(arg0));
    }
    return term_new_num(0);
  }

  // ffi-poll: Non-blocking check if future is ready
  // Returns 1 if ready, 0 if still pending
  if (strcmp(name, "ffi-poll") == 0) {
    u32 fid = 0;
    if (term_tag(arg0) >= C00 && term_tag(arg0) <= C16 && term_ext(arg0) == FFI_NAM_PEND) {
      u32 loc = term_val(arg0);
      Term fid_term = HEAP[loc];
      if (term_tag(fid_term) == NUM) {
        fid = term_val(fid_term);
      }
    } else if (term_tag(arg0) == NUM) {
      fid = term_val(arg0);
    }
    return term_new_num(ffi_future_ready(fid) ? 1 : 0);
  }

  // Try to find in registered functions
  FFIFunction* func = ffi_find_function(name);
  if (func) {
    // TODO: implement generic dispatch based on registered types
    fprintf(stderr, "FFI: registered function '%s' not yet callable\n", name);
    return term_new_num(0);
  }

  fprintf(stderr, "FFI: unimplemented function '%s'\n", name);
  return term_new_num(0);
}

// Forward declaration
fn Term ffi_eval_code(Term code);
fn Term ffi_eval_with_menv(Term menv, Term expr);

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

    // #Pend{future_id} - await the pending FFI call
    if (nam == FFI_NAM_PEND) {
      u32 loc = term_val(result);
      Term fid_term = HEAP[loc];
      if (term_tag(fid_term) == NUM) {
        u32 fid = term_val(fid_term);
        result = ffi_await(fid);
        continue;
      }
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

    // #WMnv{menv, body} - evaluate body in preserved environment
    if (nam == PURPLE_NAM_WMENV || nam == purple_nick("WMnv")) {
      u32 loc = term_val(result);
      Term menv = wnf(HEAP[loc]);
      Term body = wnf(HEAP[loc + 1]);
      result = ffi_eval_with_menv(menv, body);
      continue;
    }

    // Not an FFI marker, return as-is
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

// Evaluate a Purple code term by building and running HVM4
fn Term ffi_eval_code(Term code) {
  // Build HVM4 expression: @purple_unwrap(@purple_eval(@purple_menv_new(0)(#NIL)(#NIL))(code))
  // For simplicity, we just evaluate through SNF if it's a simple value

  // Check if code is already a simple value
  u8 tag = term_tag(code);
  if (tag == NUM) {
    return code;
  }

  // For code nodes like #Lit{n}, extract and return
  if (tag >= C00 && tag <= C16) {
    u32 nam = term_ext(code);
    if (nam == PURPLE_NAM_WMENV || nam == purple_nick("WMnv")) {
      u32 loc = term_val(code);
      Term menv = wnf(HEAP[loc]);
      Term body = wnf(HEAP[loc + 1]);
      return ffi_eval_with_menv(menv, body);
    }
    // #Lit{n} -> extract n
    if (nam == purple_nick("Lit")) {
      u32 loc = term_val(code);
      return HEAP[loc];  // return the number inside
    }
  }

  // For complex code, run SNF
  Term result = wnf(code);
  return ffi_process(result);
}

// Pretty-print a Purple result value
// Recursively normalizes thunks and prints in a readable format
fn void purple_print_result(Term t) {
  // Normalize to weak head normal form first
  t = wnf(t);
  u8 tag = term_tag(t);

  // Raw numbers
  if (tag == NUM) {
    printf("%u", term_val(t));
    return;
  }

  // Constructors
  if (tag >= C00 && tag <= C16) {
    u32 nam = term_ext(t);
    u32 loc = term_val(t);

    // #Cst{n} - unwrap to number
    if (nam == FFI_NAM_CST) {
      Term inner = wnf(HEAP[loc]);
      if (term_tag(inner) == NUM) {
        printf("%u", term_val(inner));
      } else {
        print_term(inner);
      }
      return;
    }

    // #Fix{hi, lo, scale} - print as decimal
    if (nam == FFI_NAM_FIX) {
      double d = ffi_extract_double(t);
      printf("%g", d);
      return;
    }

    // #Ptr{hi, lo} - print as hex pointer
    if (nam == FFI_NAM_PTR) {
      void* p = ffi_extract_pointer(t);
      printf("#<ptr %p>", p);
      return;
    }

    // #Sym{s} - print as symbol
    if (nam == purple_nick("Sym")) {
      Term inner = wnf(HEAP[loc]);
      if (term_tag(inner) == NUM) {
        u32 sym_id = term_val(inner);
        // Convert nick back to string if possible
        if (sym_id < 10000000) {
          // It's a nick-encoded symbol
          char buf[5] = {0};
          u32 v = sym_id;
          for (int i = 3; i >= 0; i--) {
            u32 c = v % 64;
            v /= 64;
            if (c == 0) buf[i] = 0;
            else if (c < 27) buf[i] = 'a' + c - 1;
            else if (c < 53) buf[i] = 'A' + c - 27;
            else if (c < 63) buf[i] = '0' + c - 53;
            else buf[i] = '_';
          }
          // Skip leading nulls
          char* s = buf;
          while (*s == 0 && s < buf + 4) s++;
          printf("'%s", s);
        } else {
          // Gensym
          printf("'#g%u", sym_id - 10000000);
        }
      } else {
        printf("#<sym>");
      }
      return;
    }

    // #CHR{c} - print as character
    if (nam == NAM_CHR) {
      Term inner = wnf(HEAP[loc]);
      if (term_tag(inner) == NUM) {
        u32 c = term_val(inner);
        if (c >= 32 && c < 127) {
          printf("#\\%c", (char)c);
        } else {
          printf("#\\x%x", c);
        }
      } else {
        printf("#<chr>");
      }
      return;
    }

    // #NIL - print as empty list
    if (nam == NAM_NIL) {
      printf("[]");
      return;
    }

    // #CON{a, b} - print as list if it looks like one
    if (nam == NAM_CON) {
      Term head = HEAP[loc];
      Term tail = HEAP[loc + 1];

      // Check if this looks like a list (tail is NIL or another CON)
      Term nt = wnf(tail);
      u8 ttag = term_tag(nt);
      int is_list = (ttag >= C00 && ttag <= C16 &&
                     (term_ext(nt) == NAM_NIL || term_ext(nt) == NAM_CON));

      // Also check if it looks like a string (head is CHR)
      Term nh = wnf(head);
      int is_string = (term_tag(nh) >= C00 && term_tag(nh) <= C16 &&
                       term_ext(nh) == NAM_CHR);

      if (is_string && is_list) {
        // Print as string
        printf("\"");
        Term cur = t;
        while (term_tag(cur) >= C00 && term_tag(cur) <= C16 && term_ext(cur) == NAM_CON) {
          u32 cloc = term_val(cur);
          Term ch = wnf(HEAP[cloc]);
          if (term_tag(ch) >= C00 && term_tag(ch) <= C16 && term_ext(ch) == NAM_CHR) {
            Term cv = wnf(HEAP[term_val(ch)]);
            if (term_tag(cv) == NUM) {
              u32 c = term_val(cv);
              if (c == '"') printf("\\\"");
              else if (c == '\\') printf("\\\\");
              else if (c == '\n') printf("\\n");
              else if (c >= 32 && c < 127) printf("%c", (char)c);
              else printf("\\x%02x", c);
            }
          }
          cur = wnf(HEAP[cloc + 1]);
        }
        printf("\"");
        return;
      }

      if (is_list) {
        // Print as list
        printf("[");
        Term cur = t;
        int first = 1;
        while (term_tag(cur) >= C00 && term_tag(cur) <= C16 && term_ext(cur) == NAM_CON) {
          u32 cloc = term_val(cur);
          if (!first) printf(", ");
          first = 0;
          purple_print_result(HEAP[cloc]);
          cur = wnf(HEAP[cloc + 1]);
        }
        printf("]");
        return;
      }

      // Print as pair
      printf("(");
      purple_print_result(head);
      printf(" . ");
      purple_print_result(tail);
      printf(")");
      return;
    }

    // #Clo, #CloR - closures
    if (nam == purple_nick("Clo") || nam == purple_nick("CloR")) {
      printf("#<closure>");
      return;
    }

    // #Cod{e} - code
    if (nam == purple_nick("Cod")) {
      printf("#<code>");
      return;
    }

    // #Err{msg}
    if (nam == purple_nick("Err")) {
      printf("#<error: ");
      purple_print_result(HEAP[loc]);
      printf(">");
      return;
    }

    // #Traced{v}
    if (nam == purple_nick("Trcd")) {
      printf("#<traced: ");
      purple_print_result(HEAP[loc]);
      printf(">");
      return;
    }

    // Default: fall through to print_term
  }

  // Fallback: use HVM4's print_term
  print_term(t);
}

// Read runtime and emit it
// Returns 0 on success, 1 on failure
fn int emit_runtime(FILE *out, const char *argv0) {
  char runtime_path[4096];
  char *abs = realpath(argv0, NULL);
#ifdef __linux__
  // On Linux, use /proc/self/exe as fallback when realpath(argv0) fails
  if (!abs) {
    abs = realpath("/proc/self/exe", NULL);
  }
#endif
  const char *base = abs ? abs : argv0;
  sys_path_join(runtime_path, sizeof(runtime_path), base, "lib/runtime.hvm4");
  if (abs) free(abs);

  char *runtime = sys_file_read(runtime_path);
  if (!runtime) {
    fprintf(stderr, "Error: could not read runtime '%s'\n", runtime_path);
    return 1;
  }
  fputs(runtime, out);
  free(runtime);
  return 0;
}

int main(int argc, char **argv) {
  int show_stats = 0;
  int do_collapse = 0;
  int collapse_limit = -1;  // -1 = no limit
  int num_threads = 0;      // 0 = auto (all cores)
  char *in_path = NULL;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-s") == 0) {
      show_stats = 1;
    } else if (strncmp(argv[i], "-C", 2) == 0) {
      // Collapse mode: -C or -C<limit>
      do_collapse = 1;
      if (argv[i][2] != '\0') {
        collapse_limit = atoi(&argv[i][2]);
      }
    } else if (strncmp(argv[i], "-T", 2) == 0) {
      // Thread count: -T<n>
      if (argv[i][2] != '\0') {
        num_threads = atoi(&argv[i][2]);
      } else {
        num_threads = 0;  // auto
      }
    } else if (argv[i][0] != '-') {
      in_path = argv[i];
    }
  }

  if (!in_path) {
    fprintf(stderr, "Usage: %s <file.purple> [-s] [-C[limit]] [-T<threads>]\n", argv[0]);
    fprintf(stderr, "  -s         Show statistics\n");
    fprintf(stderr, "  -C         Collapse mode (enumerate all superposition branches)\n");
    fprintf(stderr, "  -C<n>      Collapse with limit of n branches\n");
    fprintf(stderr, "  -T<n>      Use n threads (0 = auto, uses all cores)\n");
    return 1;
  }

  // Configure threads
  u32 threads = (num_threads > 0) ? (u32)num_threads : 1;
  if (do_collapse && num_threads == 0) {
    // Auto-detect cores for collapse mode
    long ncpus = sysconf(_SC_NPROCESSORS_ONLN);
    threads = (ncpus > 0) ? (u32)ncpus : 1;
  }
  thread_set_count(threads);
  wnf_set_tid(0);

  // Allocate memory
  BOOK  = calloc(BOOK_CAP, sizeof(u32));
  HEAP  = calloc(HEAP_CAP, sizeof(Term));
  TABLE = calloc(BOOK_CAP, sizeof(char*));

  if (!BOOK || !HEAP || !TABLE) {
    free(HEAP);
    free(BOOK);
    free(TABLE);
    sys_error("Memory allocation failed");
  }
  heap_init_slices();

  // Initialize names
  purple_names_init();
  ffi_names_init();

  // Start FFI worker threads
  ffi_workers_start();

  // Read Purple source
  char *src = sys_file_read(in_path);
  if (!src) {
    fprintf(stderr, "Error: could not open '%s'\n", in_path);
    ffi_workers_stop();
    free(HEAP);
    free(BOOK);
    free(TABLE);
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
    ffi_workers_stop();
    free(HEAP);
    free(BOOK);
    free(TABLE);
    return 1;
  }

  if (emit_runtime(tmp, argv[0]) != 0) {
    fclose(tmp);
    ffi_workers_stop();
    free(HEAP);
    free(BOOK);
    free(TABLE);
    return 1;
  }
  fputs("\n@main = @purple_unwrap(@purple_eval(@purple_menv_new(0)(#NIL)(#NIL))(", tmp);
  purple_compile_emit(tmp, ast);
  fputs("))\n", tmp);

  // Rewind and read as string
  if (fseek(tmp, 0, SEEK_END) != 0) {
    fclose(tmp);
    fprintf(stderr, "Error: failed to seek to end of temp file\n");
    ffi_workers_stop();
    free(HEAP);
    free(BOOK);
    free(TABLE);
    return 1;
  }
  long size = ftell(tmp);
  if (size < 0) {
    fclose(tmp);
    fprintf(stderr, "Error: failed to get temp file size\n");
    ffi_workers_stop();
    free(HEAP);
    free(BOOK);
    free(TABLE);
    return 1;
  }
  if (fseek(tmp, 0, SEEK_SET) != 0) {
    fclose(tmp);
    fprintf(stderr, "Error: failed to seek in temp file\n");
    ffi_workers_stop();
    free(HEAP);
    free(BOOK);
    free(TABLE);
    return 1;
  }
  char *hvm4_src = malloc(size + 1);
  if (!hvm4_src) {
    fclose(tmp);
    ffi_workers_stop();
    free(HEAP);
    free(BOOK);
    free(TABLE);
    sys_error("out of memory allocating HVM4 source buffer");
  }
  size_t bytes_read = fread(hvm4_src, 1, size, tmp);
  fclose(tmp);
  if ((long)bytes_read != size) {
    free(hvm4_src);
    fprintf(stderr, "Error: failed to read temp file\n");
    ffi_workers_stop();
    free(HEAP);
    free(BOOK);
    free(TABLE);
    return 1;
  }
  hvm4_src[size] = '\0';

  // Reset heap for HVM4
  memset(HEAP, 0, HEAP_CAP * sizeof(Term));
  memset(BOOK, 0, BOOK_CAP * sizeof(u32));
  ITRS = 0;
  heap_init_slices();

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
    ffi_workers_stop();
    free(HEAP);
    free(BOOK);
    free(TABLE);
    return 1;
  }

  // Evaluate
  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC, &start);

  Term main_ref = term_new_ref(main_id);

  if (do_collapse) {
    // Collapse mode: enumerate all superposition branches in parallel
    // Note: FFI calls are not supported in collapse mode
    if (threads > 1) {
      fprintf(stderr, "Running with %u threads in collapse mode\n", threads);
    }
    eval_collapse(main_ref, collapse_limit, show_stats, 0);
    clock_gettime(CLOCK_MONOTONIC, &end);
  } else {
    // Normal mode: single result with FFI support
    Term result = wnf(main_ref);

    // Process FFI calls
    result = ffi_process(result);

    clock_gettime(CLOCK_MONOTONIC, &end);

    // Print final result
    purple_print_result(result);
    printf("\n");

    if (show_stats) {
      double dt = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
      printf("- Itrs: %llu interactions\n", (unsigned long long)ITRS);
      printf("- Time: %.3f seconds\n", dt);
    }
  }

  // Stop FFI worker threads
  ffi_workers_stop();

  free(HEAP);
  free(BOOK);
  free(TABLE);

  return 0;
}
