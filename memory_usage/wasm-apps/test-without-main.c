#include <stdio.h>

__attribute__((export_name("entry"))) int entry(int argc, char **argv)
{
    printf("%d", 3);
    return 0;
}