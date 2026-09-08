/* A block comment, and a keyword inside it: return. */
#include <stdio.h>
#define GREETING "hello, %s\n"

static const char *name = "world";

int
main (void)
{
    int count = 0x2a;          // a line comment
    double ratio = 1.5e-3;

    while (count-- > 0)
        printf (GREETING, name);

    return ratio > 0 ? 0 : 1;
}
