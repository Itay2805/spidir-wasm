#include <errno.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <wasm/wasm.h>
#include <wasm/jit.h>
#include <wasm/debug_elf.h>
#include <wasm/host.h>
#include <wasm/error.h>

#include <util/except.h>
#include <spidir/log.h>

#include "gdb_jit.h"
#include "runtime.h"
#include "spidir/x64.h"

// --- spidir logging glue -------------------------------------------------

static const char* spidir_log_level_to_string(spidir_log_level_t level) {
    switch (level) {
        case SPIDIR_LOG_LEVEL_ERROR: return "ERROR";
        case SPIDIR_LOG_LEVEL_WARN: return "WARN";
        case SPIDIR_LOG_LEVEL_INFO: return "INFO";
        case SPIDIR_LOG_LEVEL_DEBUG: return "DEBUG";
        case SPIDIR_LOG_LEVEL_TRACE: return "TRACE";
        default: return "LOG";
    }
}

static void stdout_log_callback(spidir_log_level_t level, const char* module, size_t module_len, const char* message, size_t message_len) {
    printf("[%s %.*s] %.*s\n", spidir_log_level_to_string(level), (int)module_len, module, (int)message_len, message);
}

static spidir_dump_status_t spidir_dump_callback(const char* data, size_t size, void* ctx) {
    FILE* file = (FILE*)ctx;
    fprintf(file, "%.*s", (int)size, data);
    fflush(file);
    return SPIDIR_DUMP_CONTINUE;
}

// --- Command-line options ------------------------------------------------

typedef enum option_type {
    // options with a short form
    OPTION_HELP = 'h',
    OPTION_MODULE = 'm',
    OPTION_DEBUG = 'd',

    // long-only options, numbered past the ASCII range so they don't collide
    // with any short option character
    OPTION_BASE = 0xFF,
    OPTION_LOG_LEVEL,
    OPTION_SPIDIR_DUMP,
    OPTION_EMIT_DEBUG_ELF,
    OPTION_GDB_JIT,
    OPTION_JIT_ONLY,
} option_type_t;

static struct option long_options[] = {
    { "help", no_argument, 0, OPTION_HELP },
    { "module", required_argument, 0, OPTION_MODULE },
    { "debug", no_argument, 0, OPTION_DEBUG },
    { "log-level", required_argument, 0, OPTION_LOG_LEVEL },
    { "spidir-dump", optional_argument, 0, OPTION_SPIDIR_DUMP },
    { "jit-only", no_argument, 0, OPTION_JIT_ONLY },
    { "emit-debug-elf", required_argument, 0, OPTION_EMIT_DEBUG_ELF },
    { "gdb-jit", no_argument, 0, OPTION_GDB_JIT },
    { 0, 0, 0, 0 },
};

/**
 * Parsed command-line options. The owned pointers (module_path, debug_elf_path)
 * and the dump_file handle are released by main during cleanup.
 */
typedef struct options {
    char* module_path;       // -m: module to compile (owned)
    bool optimize;           // cleared by -d
    bool jit_only;           // --jit-only: compile but don't run
    char* debug_elf_path;    // --emit-debug-elf: where to write the debug ELF (owned)
    bool gdb_jit;            // --gdb-jit: publish the debug ELF to GDB
    spidir_dump_callback_t dump_callback;   // --spidir-dump sink, or NULL
    void* dump_arg;
    FILE* dump_file;         // owned dump target, or NULL when dumping to stdout
    bool help;               // -h: usage was printed, nothing left to do
} options_t;

static void print_usage(void) {
    TRACE(" -h | --help                   print this help text");
    TRACE(" -m | --module <file>          the wasm module file to compile");
    TRACE(" -d | --debug                  don't perform jit optimizations");
    TRACE("      --jit-only               compile the module but don't run it");
    TRACE("      --log-level <level>      set the spidir log level (0=none .. 5=trace)");
    TRACE("      --spidir-dump[=<file>]   dump the spidir output (omit the file for stdout)");
    TRACE("      --emit-debug-elf <file>  write a debug ELF reflecting the JIT'd binary");
    TRACE("      --gdb-jit                register the debug ELF with GDB via the JIT interface");
}

/**
 * Parse argv into `opts`. `opts` must already hold the defaults (notably
 * optimize = true). On a usage request opts->help is set and parsing stops.
 */
static wasm_err_t parse_args(int argc, char** argv, options_t* opts) {
    wasm_err_t err = WASM_NO_ERROR;

    int c;
    while ((c = getopt_long(argc, argv, "hm:d", long_options, nullptr)) != -1) {
        switch (c) {
            case OPTION_MODULE: {
                CHECK(opts->module_path == nullptr, "Module already specified");
                opts->module_path = strdup(optarg);
                CHECK(opts->module_path != nullptr);
            } break;

            case OPTION_DEBUG: {
                opts->optimize = false;
            } break;

            case OPTION_JIT_ONLY: {
                opts->jit_only = true;
            } break;

            case OPTION_SPIDIR_DUMP: {
                opts->dump_callback = spidir_dump_callback;
                if (optarg == nullptr) {
                    opts->dump_arg = stdout;
                } else {
                    opts->dump_file = fopen(optarg, "wb");
                    CHECK(opts->dump_file != nullptr, "%s: %s", strerror(errno), optarg);
                    opts->dump_arg = opts->dump_file;
                }
            } break;

            case OPTION_LOG_LEVEL: {
                errno = 0;
                unsigned long level = strtoul(optarg, nullptr, 0);
                CHECK(errno == 0, "invalid --log-level: %s", optarg);
                // TODO: also set the wasm log level from here
                spidir_log_set_max_level(level);
            } break;

            case OPTION_EMIT_DEBUG_ELF: {
                CHECK(opts->debug_elf_path == nullptr, "Debug ELF path already specified");
                opts->debug_elf_path = strdup(optarg);
                CHECK(opts->debug_elf_path != nullptr);
            } break;

            case OPTION_GDB_JIT: {
                opts->gdb_jit = true;
            } break;

            case OPTION_HELP: {
                print_usage();
                opts->help = true;
            } break;

            default: {
                CHECK_FAIL();
            } break;
        }
    }

cleanup:
    return err;
}

// --- File I/O ------------------------------------------------------------

/**
 * Read an entire file into a freshly allocated buffer. On success ownership of
 * *out_data transfers to the caller (free with wasm_host_free).
 */
static wasm_err_t read_file(const char* path, void** out_data, size_t* out_size) {
    wasm_err_t err = WASM_NO_ERROR;
    FILE* file = nullptr;
    void* data = nullptr;

    file = fopen(path, "rb");
    CHECK(file != nullptr, "%s: %s", strerror(errno), path);

    CHECK(fseek(file, 0, SEEK_END) == 0, "%s", strerror(errno));
    long size = ftell(file);
    CHECK(size > 0, "%s", strerror(errno));
    CHECK(fseek(file, 0, SEEK_SET) == 0, "%s", strerror(errno));

    data = wasm_host_calloc(1, (size_t)size);
    CHECK(data != nullptr);
    CHECK(fread(data, (size_t)size, 1, file) == 1, "%s", strerror(errno));

    *out_data = data;
    *out_size = (size_t)size;
    data = nullptr;  // ownership transferred to the caller

cleanup:
    if (file != nullptr) fclose(file);
    wasm_host_free(data);  // frees only on the error path (NULL on success)
    return err;
}

/**
 * Write the debug ELF buffer to disk.
 */
static wasm_err_t write_debug_elf(const char* path, const void* data, size_t size) {
    wasm_err_t err = WASM_NO_ERROR;

    FILE* file = fopen(path, "wb");
    CHECK(file != nullptr, "%s: %s", strerror(errno), path);
    CHECK(fwrite(data, size, 1, file) == 1, "%s", strerror(errno));

cleanup:
    if (file != nullptr) fclose(file);
    return err;
}

// --- Execution -----------------------------------------------------------

/**
 * Run the module: its start function (if any) followed by the exported
 * `_start`. Returns the exit status `_start` produces, or EXIT_FAILURE if the
 * module exports no `_start`.
 */
static int run_module(wasm_module_t* module, wasm_module_jit_t* jit, void* state) {
    void* memory = runtime_memory_base();

    if (module->start_func >= 0) {
        jit->start_func(memory, state);
    }

    int64_t index = wasm_find_export(module, "_start");
    if (index < 0) {
        ERROR("module has no _start export");
        return EXIT_FAILURE;
    }

    int (*entry)(void* memory, void* state) = jit->exports[index].func.address;
    return entry(memory, state);
}

int main(int argc, char** argv) {
    wasm_err_t err = WASM_NO_ERROR;
    int status = EXIT_SUCCESS;

    options_t opts = { .optimize = true };
    wasm_module_t module = {};
    wasm_module_jit_t jit = {};
    void* module_binary = nullptr;
    size_t module_size = 0;
    void* state = nullptr;
    void* debug_elf_data = nullptr;
    size_t debug_elf_size = 0;
    gdb_jit_entry_t* gdb_entry = nullptr;

    // Enable spidir logging at warn level by default.
    spidir_log_init(stdout_log_callback);
    spidir_log_set_max_level(SPIDIR_LOG_LEVEL_WARN);

    RETHROW(parse_args(argc, argv, &opts));
    if (opts.help) {
        goto cleanup;
    }
    CHECK(opts.module_path != nullptr, "Missing module (-m <file>)");

    wasm_jit_config_t config = {
        .optimize = opts.optimize,
        .resolve_import = opts.jit_only ? runtime_resolve_import_dummy : runtime_resolve_import,
        .dump_callback = opts.dump_callback,
        .dump_arg = opts.dump_arg,
        // The debug ELF / GDB JIT paths need per-function layout and resolved
        // relocations recorded during the JIT; a normal run pays nothing.
        .emit_debug_info = opts.debug_elf_path != nullptr || opts.gdb_jit,
    };

    // Load and compile the module.
    RETHROW(read_file(opts.module_path, &module_binary, &module_size));
    RETHROW(wasm_load_module(&module, module_binary, module_size));
    wasm_host_free(module_binary);
    module_binary = nullptr;
    RETHROW(wasm_module_jit(&module, &jit, &config));

    // Emit the debug ELF up front so it reflects the live JIT image (the bytes
    // don't change after this point). One buffer feeds both the file dump and
    // the GDB registration since they want identical contents.
    if (config.emit_debug_info) {
        RETHROW(wasm_jit_emit_debug_elf(&module, &jit, &debug_elf_data, &debug_elf_size));
        if (opts.debug_elf_path != nullptr) {
            RETHROW(write_debug_elf(opts.debug_elf_path, debug_elf_data, debug_elf_size));
        }
        if (opts.gdb_jit) {
            gdb_entry = gdb_jit_register(debug_elf_data, debug_elf_size);
            CHECK(gdb_entry != nullptr);
        }
    }

    // Set up the linear memory and the main instance's state buffer, then run.
    RETHROW(runtime_init(&module, &jit));
    CHECK(runtime_alloc_state(&state));

    if (!opts.jit_only) {
        status = run_module(&module, &jit, state);
    }

cleanup:
    gdb_jit_unregister(gdb_entry);
    wasm_host_free(debug_elf_data);
    runtime_destroy();
    wasm_module_jit_free(&jit);
    wasm_module_free(&module);
    wasm_host_free(module_binary);
    free(state);
    free(opts.module_path);
    free(opts.debug_elf_path);
    if (opts.dump_file != nullptr) fclose(opts.dump_file);

    return IS_ERROR(err) ? EXIT_FAILURE : status;
}
