#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bh_read_file.h"
#include "platform_common.h"
#include "wasm_export.h"
#include "wasm_memory.h"

static char *build_module_path(const char *module_name)
{
    const char *module_search_path = ".";
#if WASM_ENABLE_AOT != 0 && WASM_ENABLE_INTERP == 0
    const char *format = "%s/%s.aot";
#else
    const char *format = "%s/%s.wasm";
#endif
    int sz = strlen(module_search_path) + strlen("/") + strlen(module_name) + strlen(".wasm") + 1;
    char *wasm_file_name = BH_MALLOC(sz);
    if (!wasm_file_name)
    {
        return NULL;
    }

    snprintf(wasm_file_name, sz, format, module_search_path, module_name);
    return wasm_file_name;
}

static bool reader(const char *module_name, uint8 **p_buffer, uint32 *p_size)
{
    char *wasm_file_path = build_module_path(module_name);
    if (!wasm_file_path)
    {
        return false;
    }

    *p_buffer = (uint8_t *)bh_read_file_to_buffer(wasm_file_path, p_size);
    BH_FREE(wasm_file_path);
    return *p_buffer != NULL;
}

void destroyer(uint8 *buffer, uint32 size)
{
    if (!buffer)
    {
        return;
    }

    BH_FREE(buffer);
    buffer = NULL;
}

#define USE_GLOBAL_HEAP_BUF 0

#if USE_GLOBAL_HEAP_BUF != 0
static char global_heap_buf[1024 * 1024 * 1024] = {0};
#endif

int main()
{
    RuntimeInitArgs init_args;
    memset(&init_args, 0, sizeof(RuntimeInitArgs));
#if USE_GLOBAL_HEAP_BUF != 0
    init_args.mem_alloc_type = Alloc_With_Pool;
    init_args.mem_alloc_option.pool.heap_buf = global_heap_buf;
    init_args.mem_alloc_option.pool.heap_size = sizeof(global_heap_buf);
#else
    init_args.mem_alloc_type = Alloc_With_Allocator;
    init_args.mem_alloc_option.allocator.malloc_func = malloc;
    init_args.mem_alloc_option.allocator.realloc_func = realloc;
    init_args.mem_alloc_option.allocator.free_func = free;
#endif

    if (!wasm_runtime_full_init(&init_args))
    {
        printf("Init runtime environment failed.\n");
        return -1;
    }

    uint8 *buffer = NULL;
    uint32 buffer_size = 0;
    if (!reader("noop", &buffer, &buffer_size))
    {
        return -1;
    }

    wasm_module_t module = NULL;
    char error_buffer[128];
    module = wasm_runtime_load(buffer, buffer_size, error_buffer, sizeof(error_buffer));
    if (!module)
    {
        printf("Load wasm module failed. error: %s\n", error_buffer);
        return -1;
    }

        const uint32 stack_size = 8192, heap_size = 65536;
        wasm_module_inst_t module_inst = wasm_runtime_instantiate(module, stack_size, heap_size, error_buffer, sizeof(error_buffer));
        if (!module_inst)
        {
            printf("Instantiate wasm module failed. error: %s\n", error_buffer);
            return -1;
        }

        wasm_exec_env_t exec_env = wasm_runtime_create_exec_env(module_inst, stack_size);
        if (!exec_env)
        {
            printf("Create wasm execution environment failed.\n");
            return -1;
        }

        // static int app_argc;
        // static char **app_argv;
        // wasm_application_execute_main(module_inst, app_argc, app_argv);

        wasm_function_inst_t func = wasm_runtime_lookup_function(module_inst, "foo", NULL);
        if (!func)
        {
            printf("The generate_float wasm function is not found.\n");
            return -1;
        }

#define TIME_NOW() clock_gettime(CLOCK_MONOTONIC, &ts)
    struct timespec ts;
    TIME_NOW();
    long long start = ts.tv_sec * 1000000000LL + ts.tv_nsec;

    wasm_val_t results[1] = {{.kind = WASM_I32}};
    wasm_val_t arguments[1] = {{.kind = WASM_I32, .of.i32 = 0}};
    // wasm_val_t results[1] = {{.kind = WASM_I32, .of.i32 = 0}};
    // wasm_val_t arguments[0] = {};

    // if (!wasm_runtime_call_wasm_a(exec_env, func, 1, results, 0, arguments))
    if (!wasm_runtime_call_wasm_a(exec_env, func, 1, results, 1, arguments))
    {
        printf("call wasm function A1 failed. %s\n", wasm_runtime_get_exception(module_inst));
        return -1;
    }

    int i = results[0].of.i32;
    while (i < 100000000)
    {
        wasm_val_t arguments[1] = {{.kind = WASM_I32, .of.i32 = i}};
        // wasm_val_t results[1] = {{.kind = WASM_I32, .of.i32 = 0}};
        // wasm_val_t arguments[0] = {};

        // if (!wasm_runtime_call_wasm_a(exec_env, func, 1, results, 0, arguments))
        if (!wasm_runtime_call_wasm_a(exec_env, func, 1, results, 1, arguments))
        {
            printf("call wasm function A1 failed. %s\n", wasm_runtime_get_exception(module_inst));
            return -1;
        }

        i = results[0].of.i32;
    }

    TIME_NOW();
    long long end = ts.tv_sec * 1000000000LL + ts.tv_nsec;
    printf("time: %lld ns\n", end - start);

    wasm_runtime_deinstantiate(module_inst);
    wasm_runtime_unload(module);
    wasm_runtime_destroy_exec_env(exec_env);
    wasm_runtime_destroy();

    printf("\n ret value: %d\n", i);
    // wasm_runtime_dump_mem_consumption(exec_env);
    return EXIT_SUCCESS;
}
