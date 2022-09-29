#include <stdio.h>
#include <stdlib.h>

__attribute__((export_name("_main"))) int entry()
{
    char *buf;

    printf("Hello world!\n");

    buf = malloc(6);
    if (!buf)
    {
        printf("malloc buf failed\n");
        return -1;
    }

    printf("buf ptr: %p\n", buf);

    snprintf(buf, 6, "%s", "1234\n");
    printf("buf: %s", buf);

    free(buf);

    return 0;
}