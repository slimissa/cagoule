#include <stdio.h>

// Declare the function from the library
int _check_avx2_cpuid(void);

int main() {
    printf("Direct call: %d\n", _check_avx2_cpuid());
    return 0;
}
