#include <stdio.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <wasm/wasm.h>
#include <util/except.h>

typedef enum option_type {
    // options with short version
    OPTION_MODULE = 'm',

    // options without short version
    OPTION_DUMP_SPIDIR = 256,
} option_type_t;

static struct option long_options[] = {
    { "module", required_argument, 0, OPTION_MODULE },
    { "dump-spidir", required_argument, 0, OPTION_DUMP_SPIDIR },
};

int main(int argc, char** argv) {
    wasm_err_t err = WASM_NO_ERROR;
    wasm_engine_t* engine = NULL;
    wasm_store_t* store = NULL;
    wasm_module_t* module = NULL;
    FILE* module_file = NULL;
    wasm_byte_vec_t binary = {};

    char* module_path = NULL;
    char* spidir_output_path = NULL;

    while (1) {
        int option_index = 0;

        int c = getopt_long(argc, argv, "m:", long_options, &option_index);
        if (c == -1) {
            break;
        }

        switch (c) {
            case OPTION_MODULE: {
                module_path = strdup(optarg);
                CHECK(module_path != NULL);
            } break;

            case OPTION_DUMP_SPIDIR: {
                spidir_output_path = strdup(optarg);
                CHECK(spidir_output_path != NULL);
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
    engine = wasm_engine_new();
    CHECK(engine != NULL);
    store = wasm_store_new(engine);
    CHECK(store != NULL);

    // create the module
    module = wasm_module_new(store, &binary);
    CHECK(module != NULL);
    wasm_byte_vec_delete(&binary);

cleanup:
    free(module_path);
    free(spidir_output_path);
    wasm_byte_vec_delete(&binary);

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
    }

    return IS_ERROR(err) ? EXIT_SUCCESS : EXIT_FAILURE;
}

void wasm_host_log(wasm_host_log_level_t log_level, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    switch (log_level) {
        case WASM_HOST_LOG_DEBUG: printf("[?] "); break;
        case WASM_HOST_LOG_TRACE: printf("[*] "); break;
        case WASM_HOST_LOG_WARN: printf("[!] "); break;
        case WASM_HOST_LOG_ERROR: printf("[-] "); break;
    }
    vprintf(fmt, args);
    printf("\n");
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
