#include <util/except.h>
#include <wasm/error.h>
#include <wasm/host.h>

#include "internal.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Value type
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// to avoid excessive allocations we are going to have all the possible types
// pre-defined and only support these kinds
static wasm_valtype_t m_wasm_i32 = { .kind = WASM_I32 };
static wasm_valtype_t m_wasm_i64 = { .kind = WASM_I64 };
static wasm_valtype_t m_wasm_f32 = { .kind = WASM_F32 };
static wasm_valtype_t m_wasm_f64 = { .kind = WASM_F64 };
static wasm_valtype_t m_wasm_externref = { .kind = WASM_EXTERNREF };
static wasm_valtype_t m_wasm_funcref = { .kind = WASM_FUNCREF };

WASM_API_EXTERN wasm_valtype_t* wasm_valtype_new(wasm_valkind_t kind) {
    switch (kind) {
        case WASM_I32: return &m_wasm_i32;
        case WASM_I64: return &m_wasm_i64;
        case WASM_F32: return &m_wasm_f32;
        case WASM_F64: return &m_wasm_f64;
        case WASM_EXTERNREF: return &m_wasm_externref;
        case WASM_FUNCREF: return &m_wasm_funcref;
        default: return NULL;
    }
}

WASM_API_EXTERN wasm_valtype_t* wasm_valtype_copy(const wasm_valtype_t* valtype) {
    return (wasm_valtype_t*)valtype;
}

WASM_API_EXTERN void wasm_valtype_delete(wasm_valtype_t* valtype) {
    (void)valtype;
}

WASM_API_EXTERN wasm_valkind_t wasm_valtype_kind(const wasm_valtype_t* type) {
    return type->kind;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Function types
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

WASM_API_EXTERN wasm_functype_t* wasm_functype_new(wasm_valtype_vec_t* params, wasm_valtype_vec_t* results) {
    wasm_functype_t* functype = wasm_host_calloc(1, sizeof(wasm_functype_t));
    if (functype == NULL) return NULL;
    if (params != NULL) functype->params = *params;
    if (results != NULL) functype->results = *results;
    return functype;
}

WASM_API_EXTERN wasm_functype_t* wasm_functype_copy(const wasm_functype_t* functype) {
    wasm_err_t err = WASM_NO_ERROR;

    CHECK(functype != NULL);

    wasm_functype_t* new_functype = wasm_host_calloc(1, sizeof(wasm_functype_t));
    CHECK(new_functype != NULL);

    wasm_valtype_vec_copy(&new_functype->params, &functype->params);
    CHECK(new_functype->params.data != NULL);

    wasm_valtype_vec_copy(&new_functype->results, &functype->results);
    CHECK(new_functype->results.data != NULL);

cleanup:
    if (IS_ERROR(err)) {
        wasm_functype_delete(new_functype);
        new_functype = NULL;
    }

    return new_functype;
}

WASM_API_EXTERN const wasm_valtype_vec_t* wasm_functype_params(const wasm_functype_t* functype) {
    return &functype->params;
}

WASM_API_EXTERN const wasm_valtype_vec_t* wasm_functype_results(const wasm_functype_t* functype) {
    return &functype->results;
}

WASM_API_EXTERN void wasm_functype_delete(wasm_functype_t* functype) {
    if (functype != NULL) {
        wasm_valtype_vec_delete(&functype->params);
        wasm_valtype_vec_delete(&functype->results);
        wasm_host_free(functype);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Value
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

WASM_API_EXTERN void wasm_val_copy(wasm_val_t* out, const wasm_val_t* val) {
    // TODO: handle references
    *out = *val;
}

WASM_API_EXTERN void wasm_val_delete(wasm_val_t* val) {
    (void)val;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Memory types
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

WASM_API_EXTERN wasm_memorytype_t* wasm_memorytype_new(const wasm_limits_t* limits) {
    wasm_memorytype_t* memorytype = wasm_host_calloc(1, sizeof(wasm_memorytype_t));
    if (memorytype == NULL) return NULL;
    if (limits != NULL) memorytype->limits = *limits;
    return memorytype;
}

WASM_API_EXTERN const wasm_limits_t* wasm_memorytype_limits(const wasm_memorytype_t* memorytype) {
    return &memorytype->limits;
}

WASM_API_EXTERN wasm_memorytype_t* wasm_memorytype_copy(const wasm_memorytype_t* memorytype) {
    return wasm_memorytype_new(&memorytype->limits);
}

WASM_API_EXTERN void wasm_memorytype_delete(wasm_memorytype_t* memorytype) {
    if (memorytype != NULL) {
        wasm_host_free(memorytype);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Global types
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

WASM_API_EXTERN wasm_globaltype_t* wasm_globaltype_new(wasm_valtype_t* valtype, wasm_mutability_t mutability) {
    wasm_globaltype_t* globaltype = wasm_host_calloc(1, sizeof(wasm_globaltype_t));
    if (globaltype == NULL) return NULL;
    globaltype->content = valtype;
    globaltype->mutability = mutability;
    return globaltype;
}

WASM_API_EXTERN const wasm_valtype_t* wasm_globaltype_content(const wasm_globaltype_t* globaltype) {
    return globaltype->content;
}

WASM_API_EXTERN wasm_mutability_t wasm_globaltype_mutability(const wasm_globaltype_t* globaltype) {
    return globaltype->mutability;
}

WASM_API_EXTERN wasm_globaltype_t* wasm_globaltype_copy(const wasm_globaltype_t* globaltype) {
    wasm_err_t err = WASM_NO_ERROR;

    wasm_globaltype_t* new_globaltype = wasm_host_calloc(1, sizeof(wasm_globaltype_t));
    CHECK(new_globaltype != NULL);

    new_globaltype->mutability = globaltype->mutability;

    new_globaltype->content = wasm_valtype_copy(globaltype->content);
    CHECK(new_globaltype->content != NULL);

    wasm_val_copy(&new_globaltype->init, &globaltype->init);
    // TODO: check the copy went well

cleanup:
    if (IS_ERROR(err)) {
        wasm_globaltype_delete(new_globaltype);
        new_globaltype = NULL;
    }

    return new_globaltype;
}

WASM_API_EXTERN void wasm_globaltype_delete(wasm_globaltype_t* globaltype) {
    if (globaltype != NULL) {
        wasm_valtype_delete(globaltype->content);
        wasm_host_free(globaltype);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Export types
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

WASM_API_EXTERN wasm_exporttype_t* wasm_exporttype_new(wasm_name_t* name, wasm_externtype_t* externtype) {
    wasm_exporttype_t* exporttype = wasm_host_calloc(1, sizeof(wasm_globaltype_t));
    if (exporttype == NULL) return NULL;
    if (name != NULL) exporttype->name = *name;
    exporttype->externtype = externtype;
    return exporttype;
}

WASM_API_EXTERN const wasm_name_t* wasm_exporttype_name(const wasm_exporttype_t* exporttype) {
    return &exporttype->name;
}

WASM_API_EXTERN const wasm_externtype_t* wasm_exporttype_type(const wasm_exporttype_t* exporttype) {
    return exporttype->externtype;
}

WASM_API_EXTERN wasm_exporttype_t* wasm_exporttype_copy(const wasm_exporttype_t* exporttype) {
    wasm_err_t err = WASM_NO_ERROR;

    wasm_exporttype_t* new_exporttype = wasm_host_calloc(1, sizeof(wasm_exporttype_t));
    CHECK(new_exporttype != NULL);

    wasm_byte_vec_copy(&new_exporttype->name, &exporttype->name);
    CHECK(new_exporttype->name.data != NULL);

    new_exporttype->externtype = wasm_externtype_copy(exporttype->externtype);
    CHECK(new_exporttype->externtype != NULL);

cleanup:
    if (IS_ERROR(err)) {
        wasm_exporttype_delete(new_exporttype);
        new_exporttype = NULL;
    }

    return new_exporttype;
}

WASM_API_EXTERN void wasm_exporttype_delete(wasm_exporttype_t* exporttype) {
    if (exporttype != NULL) {
        wasm_byte_vec_delete(&exporttype->name);
        wasm_externtype_delete(exporttype->externtype);
        wasm_host_free(exporttype);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Extern types
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

WASM_API_EXTERN wasm_externkind_t wasm_externtype_kind(const wasm_externtype_t* externtype) {
    return externtype->kind;
}

WASM_API_EXTERN wasm_externtype_t* wasm_functype_as_externtype(wasm_functype_t* functype) {
    wasm_externtype_t* externtype = wasm_host_calloc(1, sizeof(wasm_externtype_t));
    if (externtype == NULL) return NULL;
    externtype->kind = WASM_EXTERN_FUNC;
    externtype->functype = functype;
    return externtype;
}

WASM_API_EXTERN wasm_externtype_t* wasm_globaltype_as_externtype(wasm_globaltype_t* globaltype) {
    wasm_externtype_t* externtype = wasm_host_calloc(1, sizeof(wasm_externtype_t));
    if (externtype == NULL) return NULL;
    externtype->kind = WASM_EXTERN_GLOBAL;
    externtype->globaltype = globaltype;
    return externtype;
}

WASM_API_EXTERN wasm_externtype_t* wasm_tabletype_as_externtype(wasm_tabletype_t* tabletype) {
    wasm_externtype_t* externtype = wasm_host_calloc(1, sizeof(wasm_externtype_t));
    if (externtype == NULL) return NULL;
    externtype->kind = WASM_EXTERN_TABLE;
    externtype->tabletype = tabletype;
    return externtype;
}

WASM_API_EXTERN wasm_externtype_t* wasm_memorytype_as_externtype(wasm_memorytype_t* memorytype) {
    wasm_externtype_t* externtype = wasm_host_calloc(1, sizeof(wasm_externtype_t));
    if (externtype == NULL) return NULL;
    externtype->kind = WASM_EXTERN_MEMORY;
    externtype->memorytype = memorytype;
    return externtype;
}

WASM_API_EXTERN wasm_functype_t* wasm_externtype_as_functype(wasm_externtype_t* externtype) {
    if (externtype->kind != WASM_EXTERN_FUNC) return NULL;
    return externtype->functype;
}

WASM_API_EXTERN wasm_globaltype_t* wasm_externtype_as_globaltype(wasm_externtype_t* externtype) {
    if (externtype->kind != WASM_EXTERN_GLOBAL) return NULL;
    return externtype->globaltype;
}

WASM_API_EXTERN wasm_tabletype_t* wasm_externtype_as_tabletype(wasm_externtype_t* externtype) {
    if (externtype->kind != WASM_EXTERN_TABLE) return NULL;
    return externtype->tabletype;
}

WASM_API_EXTERN wasm_memorytype_t* wasm_externtype_as_memorytype(wasm_externtype_t* externtype) {
    if (externtype->kind != WASM_EXTERN_MEMORY) return NULL;
    return externtype->memorytype;
}

WASM_API_EXTERN const wasm_externtype_t* wasm_functype_as_externtype_const(const wasm_functype_t* functype) {
    wasm_externtype_t* externtype = wasm_host_calloc(1, sizeof(wasm_externtype_t));
    if (externtype == NULL) return NULL;
    externtype->kind = WASM_EXTERN_FUNC;
    externtype->functype = (wasm_functype_t*)functype;
    return externtype;
}

WASM_API_EXTERN const wasm_externtype_t* wasm_globaltype_as_externtype_const(const wasm_globaltype_t* globaltype) {
    wasm_externtype_t* externtype = wasm_host_calloc(1, sizeof(wasm_externtype_t));
    if (externtype == NULL) return NULL;
    externtype->kind = WASM_EXTERN_GLOBAL;
    externtype->globaltype = (wasm_globaltype_t*)globaltype;
    return externtype;
}

WASM_API_EXTERN const wasm_externtype_t* wasm_tabletype_as_externtype_const(const wasm_tabletype_t* tabletype) {
    wasm_externtype_t* externtype = wasm_host_calloc(1, sizeof(wasm_externtype_t));
    if (externtype == NULL) return NULL;
    externtype->kind = WASM_EXTERN_TABLE;
    externtype->tabletype = (wasm_tabletype_t*)tabletype;
    return externtype;
}

WASM_API_EXTERN const wasm_externtype_t* wasm_memorytype_as_externtype_const(const wasm_memorytype_t* memorytype) {
    wasm_externtype_t* externtype = wasm_host_calloc(1, sizeof(wasm_externtype_t));
    if (externtype == NULL) return NULL;
    externtype->kind = WASM_EXTERN_MEMORY;
    externtype->memorytype = (wasm_memorytype_t*)memorytype;
    return externtype;
}

WASM_API_EXTERN const wasm_functype_t* wasm_externtype_as_functype_const(const wasm_externtype_t* externtype) {
    if (externtype->kind != WASM_EXTERN_FUNC) return NULL;
    return externtype->functype;
}

WASM_API_EXTERN const wasm_globaltype_t* wasm_externtype_as_globaltype_const(const wasm_externtype_t* externtype) {
    if (externtype->kind != WASM_EXTERN_GLOBAL) return NULL;
    return externtype->globaltype;
}

WASM_API_EXTERN const wasm_tabletype_t* wasm_externtype_as_tabletype_const(const wasm_externtype_t* externtype) {
    if (externtype->kind != WASM_EXTERN_TABLE) return NULL;
    return externtype->tabletype;
}

WASM_API_EXTERN const wasm_memorytype_t* wasm_externtype_as_memorytype_const(const wasm_externtype_t* externtype) {
    if (externtype->kind != WASM_EXTERN_MEMORY) return NULL;
    return externtype->memorytype;
}

WASM_API_EXTERN wasm_externtype_t* wasm_externtype_copy(const wasm_externtype_t* externtype) {
    wasm_externtype_t* new_externtype = wasm_host_calloc(1, sizeof(wasm_externtype_t));
    if (new_externtype == NULL) return NULL;
    *new_externtype = *externtype;
    return new_externtype;
}

WASM_API_EXTERN void wasm_externtype_delete(wasm_externtype_t* externtype) {
    if (externtype != NULL) {
        wasm_host_free(externtype);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Function
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

WASM_API_EXTERN wasm_functype_t* wasm_func_type(const wasm_func_t* func) {
    return wasm_functype_copy(func->functype);
}

WASM_API_EXTERN size_t wasm_func_param_arity(const wasm_func_t* func) {
    return func->functype->params.size;
}

WASM_API_EXTERN size_t wasm_func_result_arity(const wasm_func_t* func) {
    return func->functype->results.size;
}

WASM_API_EXTERN void wasm_func_delete(wasm_func_t* func) {
    if (func != NULL) {
        wasm_host_free(func);
    }
}
