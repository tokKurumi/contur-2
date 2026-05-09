/* native_hello.c — minimal Linux x86 hello-world used by NativeEngine integration tests.
 *
 * Compiled by tests/CMakeLists.txt on POSIX hosts only. The resulting binary
 * is launched by NativeEngine through fork/exec; the integration test
 * verifies that stdout is captured and the exit code reaches the kernel.
 */

#include <stdio.h>

int main(void)
{
    puts("hello, contur");
    return 0;
}
