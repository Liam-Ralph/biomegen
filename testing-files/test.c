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
    FILE *fptr = fopen("record5.txt", "w");
    struct timespec st, tn;
    clock_gettime(CLOCK_REALTIME, &st);

    for (int _ = 0; _ < 1000 * 1000 * 1000; _++) {

        int new_num = rand();

        if (new_num < max && new_num != 0) {

            // int pos_max = 0;
            // for (int i = 1; i < 10; i++) {
            //     if (list[i] > list[pos_max]) {
            //         pos_max = i;
            //         if (list[pos_max] == max) {
            //             break;
            //         }
            //     }
            // }
            // list[pos_max] = new_num;
            // pos_max = 0;
            // for (int i = 1; i < 10; i++) {
            //     if (list[i] > list[pos_max]) {
            //         pos_max = i;
            //         if (list[pos_max] == max) {
            //             break;
            //         }
            //     }
            // }
            // max = list[pos_max];

            int low = 0;
            int high = 9;
            int pos = 9;
            while (low <= high) {
                // if (new_num == 135497281) fprintf(fptr, "*%d\n", pos);
                int mid = low + (high - low) / 2;
                if ((mid == 0 || list[mid - 1] <= new_num) && (mid == 9 || list[mid] >= new_num)) {
                    pos = mid;
                    break;
                } else if (list[mid] > new_num) {
                    high = mid - 1;
                } else {
                    low = mid + 1;
                }
                // if (new_num == 135497281) fprintf(fptr, "low%dmid%dhigh%dpos%d\n", low, mid, high,pos);
            }

            // int pos = 0;
            // while (new_num > list[pos]) pos++;
            for (int i = 9; i > pos; i--) {
                list[i] = list[i - 1];
            }
            list[pos] = new_num;
            max = list[9];

            // fprintf(fptr, "*%d\n", pos);

        }

// for (int i = 0; i < 10; i++) {
//     fprintf(fptr, "%d\n", list[i]);
// }
// fprintf(fptr, "^%d\n*%d\n\n", max, new_num);

// for (int i = 0; i < 9; i++) {
//     int swapped = 0;
//     for (int ii = 0; ii < 9 - i; ii++) {
//         if (list[ii] > list[ii + 1]) {
//             int temp = list[ii];
//             list[ii] = list[ii + 1];
//             list[ii + 1] = temp;
//             swapped = 1;
//         }
//     }
//     if (swapped == 0) {
//         break;
//     }
// }


    }

    clock_gettime(CLOCK_REALTIME, &tn);
    printf("%f\n", (float)(tn.tv_sec - st.tv_sec) + (tn.tv_nsec - st.tv_nsec) / 1000000000.0);

    for (int i = 0; i < 9; i++) {
        int swapped = 0;
        for (int ii = 0; ii < 9 - i; ii++) {
            if (list[ii] > list[ii + 1]) {
                int temp = list[ii];
                list[ii] = list[ii + 1];
                list[ii + 1] = temp;
                swapped = 1;
            }
        }
        if (swapped == 0) {
            break;
        }
    }

    for (int i = 0; i < 10; i++) {
        fprintf(fptr, "%d\n", list[i]);
    }

    fclose(fptr);

    return 0;

}