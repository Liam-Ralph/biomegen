// Includes

#include <math.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>


// Main Function

int main() {

    int i, ii;

    printf("\n");

    FILE *fptr;
    fptr = fopen("autorun_tasks.txt", "r");
    char raw_line[100]; // Max task length is 100 chars

    while (fgets(raw_line, 100, fptr)) {

        // Getting Task Parameters

        char line_copy[100];
        strcpy(line_copy, raw_line);

        char *token = strtok(line_copy, ":");
        int reps = atoi(line_copy); // Repetitions

        token = strtok(NULL, ":");
        bool show_rep_times = false; // Whether to show rep times
        if (strcmp(token, "y") == 0) {
            show_rep_times = true;
        }

        token = strtok(NULL, ":");
        bool save_png = false; // Whether to save png outputs
        if (strcmp(token, "y") == 0) {
            save_png = true;
        }

        token = strtok(NULL, "\n");
        char *inputs = token; // Inputs for main program

        printf("Running task \"%s\" for %d reps.\n", inputs, reps);

        // Prepping Save Path

        if (save_png) {
            const int len_inputs = strlen(inputs);
            char inputs_copy[100];
            strncpy(inputs_copy, inputs, len_inputs - 4);
            snprintf(inputs, len_inputs + 3, "%s0.png", inputs_copy);
        }
        // "1920 1080 100 120 50 5 8 file.png" --> "1920 1080 100 120 50 5 8 file0.png"

        // Running Repetitions

        float rep_times[reps];
        for (i = 0; i < reps; i++) {

            if (save_png) {

                // Preparing Rep's Save Path

                const int len_inputs = strlen(inputs);
                char inputs_copy[100] = "";
                strncpy(inputs_copy, inputs, len_inputs - 5);
                snprintf(inputs, len_inputs + 4, "%s%d.png", inputs_copy, i + 1);
                // Rep 1 is saved in file1.png, rep2 in file2.png, etc.

            }

            // Compiling Main Program

            system("gcc main.c -o main -lm");

            // Running Program and Collecting Output

            FILE *fp;
            char output[20] = {0};
            char buffer[20];

            char command[107] = "./main ";
            strcat(command, inputs);
            fp = popen(command, "r");
            while (fgets(buffer, 20, fp) != NULL) {
                strcat(output, buffer);
            }
            pclose(fp);

            float time = atof(output);
            rep_times[i] = time;

            // Print Rep Time or Progress Bar

            if (show_rep_times) {
                printf("Repetition ");
                if (reps < 10) {
                    printf("%d Time: %15lfs\n", (i + 1), time);
                } else if (reps < 100) {
                    printf("%2d Time: %14lfs\n", (i + 1), time);
                } else {
                    printf("%3d Time: %15lfs\n", (i + 1), time);
                }
            } else {
                const int bars = round((i + 1) * 100 / reps);
                printf("\r\033[K\033[48;5;2m");
                for (ii = 0; ii < bars; ii++) {
                    printf(" ");
                    fflush(stdout);
                }
                printf("\033[48;5;1m");
                for (ii = 0; ii < 100 - bars; ii++) {
                    printf(" ");
                    fflush(stdout);
                }
                printf("\033[0m %3d%%", bars);
                fflush(stdout);
            }

        }

        // Delete Png File if not Saving

        if (!save_png) {
            strtok(inputs, " ");
            strtok(NULL, " ");
            strtok(NULL, " ");
            strtok(NULL, " ");
            strtok(NULL, " ");
            strtok(NULL, " ");
            strtok(NULL, " ");
            char *file_path = strtok(NULL, "");
            remove(file_path);
        }

        // Move off of Progress Bar Line

        if (!show_rep_times) {
            printf("\n");
        }

        // Calculating Mean

        float mean = 0;
        for (i = 0; i < reps; i++) {
            mean += rep_times[i];
        }
        mean /= reps;
        printf("Mean Time: %23lfs\n", mean);

        // Calculating Standard Deviation

        float std_deviation = 0;
        for (i = 0; i < reps; i++) {
            std_deviation += pow((rep_times[i] - mean), 2);
        }
        std_deviation /= reps;
        std_deviation = sqrt(std_deviation);
        printf("Standard Deviation: %14lfs\n", std_deviation);

        if (reps >= 10) {

            // Sorting Rep Times

            bool swapped;
            float temp;
            for (i = 0; i < reps - 1; i++) {
                swapped = false;
                for (ii = 0; ii < reps - i - 1; ii++) {
                    if (rep_times[ii] > rep_times[ii + 1]) {
                        temp = rep_times[ii];
                        rep_times[ii] = rep_times[ii + 1];
                        rep_times[ii + 1] = temp;
                        swapped = true;
                    }
                }
                if (!swapped) {
                    break;
                }
            }

            // Percentiles

            int percentiles[] = {5, 25, 50, 75, 90};
            for (i = 0; i < sizeof(percentiles) / sizeof(percentiles[0]); i++) {

                float exact_index = percentiles[i] * (reps - 1) / 100.0; // 50th index of 8 is 3.5
                int index = (int)exact_index; // floor of exact index
                float diff = exact_index - index; // diff between index and exact

                float value = rep_times[index]; // rep time at index
                if (diff > 0.0001) {
                    // add diff% of next index
                    value += (rep_times[index + 1] - rep_times[index]) * diff;
                }

                /*
                Example for 50th percentile of list {1, 2, 3, 4}
                exact_index = 50 * 3 / 100 = 1.5
                value = list[1] + (list[2] - list[1]) * 0.5 = 2 + 1 * 0.5 = 2.5
                */

                printf("%2dth Percentile: %17lfs\n", percentiles[i], value);

            }

        }

        printf("\n");

    }

    fclose(fptr);

    return 0;

}