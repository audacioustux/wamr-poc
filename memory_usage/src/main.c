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
    char error_buf[128];
    wasm_module_t module = NULL;
    wasm_module_inst_t module_inst;
    wasm_function_inst_t func;
    wasm_exec_env_t exec_env;
    uint32 size, stack_size = 8092, heap_size = 8092;

    /* initialize the wasm runtime by default configurations */
    wasm_runtime_init();

    /* read WASM file into a memory buffer */
    uint8 *file_buf = NULL;
    uint32 file_buf_size = 0;

    if (!module_reader_cb("mA", &file_buf, &file_buf_size))
    {
        return -1;
    }

    /* add line below if we want to export native functions to WASM app */
    // wasm_runtime_register_natives(...);

    /* parse the WASM file from buffer and create a WASM module */
    if (!(module = wasm_runtime_load(file_buf, file_buf_size, error_buf, sizeof(error_buf))))
    {
        printf("%s\n", error_buf);
        return -1;
    }
    /* create an instance of the WASM module (WASM linear memory is ready) */
    module_inst = wasm_runtime_instantiate(module, stack_size, heap_size,
                                           error_buf, sizeof(error_buf));
}
