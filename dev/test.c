
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ANSI_GREEN "\033[38;5;2m"
#define ANSI_BLUE "\033[38;5;4m"
#define ANSI_RESET "\033[0m"

int main(int argc, char *argv[]) {

    // Still needs to print current progress/time/whatever after bar

    int prev_bars = 0;
    const long TOT = 10000000000;
    printf("%s", ANSI_BLUE);
    for (int i = 0; i < 100; i++) printf("█");
    printf("\r%s", ANSI_GREEN);
    fflush(stdout);

    for (long i = 0; i < TOT; i++) {
        int bars = (i + 1) * 100 / TOT;
        if (bars > prev_bars) {
            for (int ii = 0; ii < bars - prev_bars; ii++)
                printf("█");
            prev_bars = bars;
            fflush(stdout);
        }
    }

    printf("%s\n", ANSI_RESET);

    return 0;
}