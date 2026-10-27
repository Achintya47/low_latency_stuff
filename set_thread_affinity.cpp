#include <pthread.h>
#include <sched.h>

int main() {
    cpu_set_t set;

    CPU_ZERO(&set);      // Clear CPU set
    CPU_SET(0, &set);    // Allow CPU 0

    pthread_setaffinity_np(
        pthread_self(),
        sizeof(cpu_set_t),
        &set
    );

    return 0;
}