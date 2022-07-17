#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bh_read_file.h"
#include "platform_common.h"
#include "wasm_export.h"

static char *build_module_path(const char *module_name)
{
    const char *module_search_path = ".";
    const char *format = "%s/%s.wasm";
    int sz = strlen(module_search_path) + strlen("/") + strlen(module_name) + strlen(".wasm") + 1;
    char *wasm_file_name = BH_MALLOC(sz);
    if (!wasm_file_name)
    {
        return NULL;
    }

    snprintf(wasm_file_name, sz, format, module_search_path, module_name);
    return wasm_file_name;
}

static bool module_reader_cb(const char *module_name, uint8 **p_buffer, uint32 *p_size)
{
    char *wasm_file_path = build_module_path(module_name);
    if (!wasm_file_path)
    {
        return false;
    }

    printf("- bh_read_file_to_buffer %s\n", wasm_file_path);
    *p_buffer = (uint8_t *)bh_read_file_to_buffer(wasm_file_path, p_size);
    BH_FREE(wasm_file_path);
    return *p_buffer != NULL;
}

int main()
{
    static char global_heap_buf[1024 * 1024 * 1024];

    RuntimeInitArgs init_args;
    memset(&init_args, 0, sizeof(RuntimeInitArgs));

    init_args.mem_alloc_type = Alloc_With_Pool;
    init_args.mem_alloc_option.pool.heap_buf = global_heap_buf;
    init_args.mem_alloc_option.pool.heap_size = sizeof(global_heap_buf);

    if (!wasm_runtime_full_init(&init_args))
    {
        printf("Init runtime environment failed.\n");
        return -1;
    }

    uint8 *buffer = NULL;
    uint32 buffer_size = 0;
    if (!module_reader_cb("hello-world", &buffer, &buffer_size))
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

    for (int i = 0; i < 1; i++)
    {
        const uint32 stack_size = 512, heap_size = 64;
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

        wasm_function_inst_t func = NULL;
        if (!(func = wasm_runtime_lookup_function(module_inst, "dummy", NULL)))
        {
            printf("The generate_float wasm function is not found.\n");
            return -1;
        }

        wasm_val_t results[1] = {{.kind = WASM_I32, .of.i32 = 0}};
        wasm_val_t arguments[0] = {};
        if (!wasm_runtime_call_wasm_a(exec_env, func, 1, results, 0, arguments))
        {
            printf("call wasm function A1 failed. %s\n", wasm_runtime_get_exception(module_inst));
            return -1;
        }

        int ret_val;
        ret_val = results[0].of.i32;
        printf("Native finished calling wasm function generate_float(), returned a "
               "float value: %d\n",
               ret_val + i);
    }
    return EXIT_SUCCESS;
}
