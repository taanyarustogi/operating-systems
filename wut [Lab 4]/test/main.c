#include "wut.h"

#include <stdio.h>
int x = 0;

/* Do not modify this function, you should call this to check for any value
   you want to inspect from the solution. */
void check(int value, const char* message) {
    printf("Check: %d (%s)\n", value, message);
}

void run(void) {
    x = wut_id();
    return;
}

int main() {
    /*
    You may write any of your own testing code here.

    You can execute it using `build/test/wut`. However, without your own
    implementation, it shouldn't do much. We'll run this code with the solution
    so you can clarify anything you want, or write a tricky test case.

    Place at least one call to `check` with a value (probably a return from a
    library call) that you'd like to see the output of. For example, here's
    how to convert `tests/main-thread-is-0.c` into this format:
    
    wut_init();
    check(wut_id(), "wut_id of the main thread is should be 0");

    */

    wut_init();
    int main_id = wut_id();
    check(main_id, "wrong main thread ID, expected: 0");

    int first_id = wut_create(run);
    check(first_id, "wrong first thread ID, expected: 1");

    int second_id = wut_create(run);
    check(second_id, "wrong second thread ID, expected: 2");

    int cancel_suc = wut_cancel(first_id);
    int yield_suc = wut_yield();

    check(cancel_suc, "cancel should be successful, expected: 0");
    check(yield_suc, "yield should be successful, expected: 0");
    check(x, "should be second thread ID, expected: 2");

    /* Insert your code here, and remove this comment. */
    return 0;
}
