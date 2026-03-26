#include <stdio.h>
#include <stdlib.h>

extern int test_golay(void);
extern int test_ctcss(void);
extern int test_dcs(void);
extern int test_dtmf(void);
extern int test_cwid(void);
extern int test_mcw(void);
extern int test_fskcw(void);
extern int test_twotone(void);
extern int test_selcall(void);
extern int test_toneburst(void);
extern int test_mdc1200(void);
extern int test_courtesy(void);

int main(void)
{
    int failures = 0;

    printf("=== libplcode test suite ===\n\n");

    failures += test_golay();
    failures += test_ctcss();
    failures += test_dcs();
    failures += test_dtmf();
    failures += test_cwid();
    failures += test_mcw();
    failures += test_fskcw();
    failures += test_twotone();
    failures += test_selcall();
    failures += test_toneburst();
    failures += test_mdc1200();
    failures += test_courtesy();

    printf("=== %s ===\n", failures ? "SOME TESTS FAILED" : "ALL TESTS PASSED");

    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
