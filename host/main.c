#include <errno.h>
#include <stdio.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <wasm/wasm.h>
#include <util/except.h>
#include <spidir/log.h>

typedef enum option_type {
    // options with short version
    OPTION_HELP = 'h',
    OPTION_MODULE = 'm',
    OPTION_OPTIMIZE = 'o',

    // options without short version
    OPTION_BASE = 0xFF,

    OPTION_LOG_LEVEL,
    OPTION_SPIDIR_DUMP,
} option_type_t;

static struct option long_options[] = {
    { "help", no_argument, 0, OPTION_HELP },
    { "module", required_argument, 0, OPTION_MODULE },
    { "optimize", no_argument, 0, OPTION_OPTIMIZE },
    { "log-level", required_argument, 0, OPTION_LOG_LEVEL },

    { "spidir-dump", required_argument, 0, OPTION_SPIDIR_DUMP },
};

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
    printf("[%s %.*s] %.*s\n", spidir_log_level_to_string(level), (int) module_len, module, (int) message_len, message);
}


static spidir_dump_status_t spidir_dump_callback(const char* data, size_t size, void* ctx) {
    FILE* file = (FILE*)ctx;
    fprintf(file, "%.*s", (int)size, data);
    return SPIDIR_DUMP_CONTINUE;
}

int main(int argc, char** argv) {
    wasm_err_t err = WASM_NO_ERROR;
    wasm_engine_t* engine = NULL;
    wasm_store_t* store = NULL;
    wasm_module_t* module = NULL;
    FILE* module_file = NULL;
    wasm_byte_vec_t binary = {};

    char* module_path = NULL;
    FILE* spidir_output_file = NULL;


    wasm_config_t* config = wasm_config_new();
    CHECK(config != NULL);

    // enable logging and set them to warn by default
    spidir_log_init(stdout_log_callback);
    spidir_log_set_max_level(SPIDIR_LOG_LEVEL_WARN);

    while (1) {
        int option_index = 0;

        int c = getopt_long(argc, argv, "hm:o", long_options, &option_index);
        if (c == -1) {
            break;
        }

        switch (c) {
            case OPTION_MODULE: {
                module_path = strdup(optarg);
                CHECK(module_path != NULL);
            } break;

            case OPTION_OPTIMIZE: {
                wasm_config_optimize(config, true);
            } break;

            case OPTION_SPIDIR_DUMP: {
                if (strcmp(optarg, "-") == 0) {
                    wasm_config_spidir_dump(config, spidir_dump_callback, stdout);
                } else {
                    spidir_output_file = fopen(optarg, "wb");
                    CHECK(spidir_output_file != NULL);
                    wasm_config_spidir_dump(config, spidir_dump_callback, spidir_output_file);
                }
            } break;

            case OPTION_LOG_LEVEL: {
                errno = 0;
                unsigned long level = strtoul(optarg, NULL, 0);
                CHECK(errno == 0);

                // TODO: also set the wasm log level from here
                spidir_log_set_max_level(level);
            } break;

            case OPTION_HELP: {
                TRACE(" -h | --help                        print this help text");
                TRACE(" -m | --module <file>               the wasm module file to compile");
                TRACE(" -o | --optimize                    perform optimizations on the spidir");
                TRACE("      --log-level <level>           set the log level (0=none, 1=error, 2=warn, 3=info, debug=4, trace=5)");
                TRACE("      --spidir-dump <file>          dump the spidir output into a file");
            } break;

            default: {
                CHECK_FAIL();
            } break;
        }
    }

    // check we have the module path
    CHECK(module_path != NULL);

    // open the module file
    module_file = fopen(module_path, "rb");
    CHECK(module_file != NULL);

    free(module_path);
    module_path = NULL;

    // get the module size
    CHECK(fseek(module_file, 0, SEEK_END) == 0);
    size_t module_size = ftell(module_file);
    CHECK(module_size)
    CHECK(fseek(module_file, 0, SEEK_SET) == 0);

    // read the file
    wasm_byte_vec_new_uninitialized(&binary, module_size);
    CHECK(binary.data != NULL && binary.size == module_size);
    CHECK(fread(binary.data, 1, module_size, module_file) == module_size);

    fclose(module_file);
    module_file = NULL;

    // create the engine and store
    engine = wasm_engine_new_with_config(config);
    CHECK(engine != NULL);
    store = wasm_store_new(engine);
    CHECK(store != NULL);

    // create the module
    module = wasm_module_new(store, &binary);
    CHECK(module != NULL);
    wasm_byte_vec_delete(&binary);

cleanup:
    free(module_path);
    wasm_byte_vec_delete(&binary);

    if (spidir_output_file != NULL) {
        fclose(spidir_output_file);
    }

    if (module_file != NULL) {
        fclose(module_file);
    }

    if (module != NULL) {
        wasm_module_delete(module);
    }

    if (store != NULL) {
        wasm_store_delete(store);
    }

    if (engine != NULL) {
        wasm_engine_delete(engine);
    } else {
        wasm_config_delete(config);
    }

    return IS_ERROR(err) ? EXIT_SUCCESS : EXIT_FAILURE;
}

void wasm_host_log(wasm_host_log_level_t log_level, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    switch (log_level) {
        case WASM_HOST_LOG_RAW: break;
        case WASM_HOST_LOG_DEBUG: printf("[?] "); break;
        case WASM_HOST_LOG_TRACE: printf("[*] "); break;
        case WASM_HOST_LOG_WARN: printf("[!] "); break;
        case WASM_HOST_LOG_ERROR: printf("[-] "); break;
    }
    vprintf(fmt, args);
    if (log_level != WASM_HOST_LOG_RAW) {
        printf("\n");
    }
    va_end(args);
}

void* wasm_host_calloc(size_t nmemb, size_t size) {
    return calloc(nmemb, size);
}

void* wasm_host_realloc(void* ptr, size_t new_size) {
    return realloc(ptr, new_size);
}

void wasm_host_free(void* ptr) {
    free(ptr);
}
