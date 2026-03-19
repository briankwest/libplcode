#include <stdio.h>
#include <stdlib.h>

extern int test_golay(void);
extern int test_ctcss(void);
extern int test_dcs(void);
extern int test_dtmf(void);
extern int test_cwid(void);

int main(void)
{
    int failures = 0;

    printf("=== libplcode test suite ===\n\n");

    failures += test_golay();
    failures += test_ctcss();
    failures += test_dcs();
    failures += test_dtmf();
    failures += test_cwid();

    printf("=== %s ===\n", failures ? "SOME TESTS FAILED" : "ALL TESTS PASSED");

    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
