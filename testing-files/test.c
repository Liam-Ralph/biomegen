#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <time.h>

int main() {

    int list[10];
    srand(0);
    for (int i = 0; i < 10; i++) list[i] = INT_MAX;
    int max = INT_MAX;
    FILE *fptr = fopen("record.txt", "w");
    struct timespec st, tn;
    clock_gettime(CLOCK_REALTIME, &st);

    for (int _ = 0; _ < 1000000000; _++) {

        int new_num = rand();

        if (new_num < max && new_num != 0) {
            int pos_max = 0;
            for (int i = 1; i < 10; i++) {
                if (list[i] > list[pos_max]) {
                    pos_max = i;
                    if (list[pos_max] == max) {
                        break;
                    }
                }
            }
            list[pos_max] = new_num;
            pos_max = 0;
            for (int i = 1; i < 10; i++) {
                if (list[i] > list[pos_max]) {
                    pos_max = i;
                    if (list[pos_max] == max) {
                        break;
                    }
                }
            }
            max = list[pos_max];
        }

    }

    for (int i = 0; i < 10; i++) {
        fprintf(fptr, "%d %d\n", i, list[i]);
    }
    fprintf(fptr, "\n");

    fclose(fptr);

    clock_gettime(CLOCK_REALTIME, &tn);
    printf("%f\n", (float)(tn.tv_sec - st.tv_sec) + (tn.tv_nsec - st.tv_nsec) / 1000000000.0);

    return 0;

}