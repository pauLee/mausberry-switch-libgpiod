#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

static int test_exit_non_zero_without_gpio(void) {
    int exitstatus = system("../src/mausberry-switch");
    if (WIFEXITED(exitstatus) && WEXITSTATUS(exitstatus) == 1) {
        printf("PASS: exits with code 1 when GPIO unavailable\n");
        return 0;
    }
    printf("FAIL: expected exit code 1, got %d\n", WEXITSTATUS(exitstatus));
    return 1;
}

int main(void) {
    int failures = 0;

    failures += test_exit_non_zero_without_gpio();

    if (failures > 0) {
        printf("\n%d test(s) FAILED\n", failures);
        return 1;
    }

    printf("\nAll tests passed\n");
    return 0;
}
