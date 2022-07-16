#include <stdlib.h>

__attribute__((export_name("A1"))) int
A1()
{
    char *str;

    /* Initial memory allocation */
    str = (char *)malloc(15);
    strcpy(str, "tutorialspoint");
    printf("String = %s,  Address = %u\n", str, str);

    /* Reallocating memory */
    str = (char *)realloc(str, 25);
    strcat(str, ".com");
    printf("String = %s,  Address = %u\n", str, str);

    free(str);

    return (0);
}

int A2()
{
    return 12;
}

/* mA is a  reactor. it doesn't need a main() */