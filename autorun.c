// Includes

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <unistd.h>


// Main Function

int main() {

    // Setting Process Title

    #ifdef __linux__

        prctl(PR_SET_NAME, "biomegenautorun", 0, 0, 0);

    #elif BSD

        setproctitle("biomegen-autorun");

    #elif __Apple__

        setproctitle("biomegen-autorun");
    
    #endif

    printf("\n");

    // Getting Program Version

    char file_line[29];
    FILE *fptr = fopen("README.md", "r");
    fgets(file_line, 29, fptr);
    fgets(file_line, 29, fptr);
    fgets(file_line, 29, fptr);
    fclose(fptr);

    char version[12];
    strncpy(version, file_line + 12, 12);
    version[strlen(version) - 2] = '\0';

    // Compiling Main Program

    #ifdef _WIN32
        char exec[9] = "main.exe";
    #else
        char exec[5] = "main";
    #endif

    if (!access(exec, F_OK) == 0) {
        system("gcc main.c -o main -lm");
    } 

    // Opening Autorun Tasks

    fptr = fopen("autorun_tasks.txt", "r");
    char raw_line[256]; // Max task length is 255 chars

    while (fgets(raw_line, 256, fptr)) {

        // Getting Task Parameters

        char line_copy[256];
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
            char inputs_copy[257];
            strncpy(inputs_copy, inputs, len_inputs - 4);
            snprintf(inputs, len_inputs + 3, "%s0.png", inputs_copy);
        }
        // "1920 1080 100 120 50 5 8 file.png" --> "1920 1080 100 120 50 5 8 file0.png"

        // Running Repetitions

        float rep_times[reps];
        for (int i = 0; i < reps; i++) {

            if (save_png) {

                // Preparing Rep's Save Path

                const int len_inputs = strlen(inputs);
                char inputs_copy[257] = "";
                strncpy(inputs_copy, inputs, len_inputs - 5);
                snprintf(inputs, len_inputs + 4, "%s%d.png", inputs_copy, i + 1);
                // Rep 1 is saved in file1.png, rep2 in file2.png, etc.

            }

            // Running Program and Collecting Output

            FILE *fp;
            char output[13] = {0};
            char buffer[13]; // max time 99999.999999 seconds (> 27 hours)

            #ifdef _WIN32
                char command[265] = "main.exe ";
            #else
                char command[263] = "./main ";
            #endif

            strcat(command, inputs);
            fp = popen(command, "r");
            while (fgets(buffer, 13, fp) != NULL) {
                strcat(output, buffer);
            }
            pclose(fp);

            float time = atof(output);
            rep_times[i] = time;

            // Print Rep Time or Progress Bar

            if (show_rep_times) {
                printf("Repetition ");
                if (reps < 10) {
                    printf("%d Time %16lfs\n", (i + 1), time);
                } else if (reps < 100) {
                    printf("%2d Time %15lfs\n", (i + 1), time);
                } else {
                    printf("%3d Time %14lfs\n", (i + 1), time);
                }
            } else {
                const int bars = round((i + 1) * 100 / reps);
                printf("\r\033[K\033[48;5;2m");
                for (int ii = 0; ii < bars; ii++) {
                    printf(" ");
                    fflush(stdout);
                }
                printf("\033[48;5;1m");
                for (int ii = 0; ii < 100 - bars; ii++) {
                    printf(" ");
                    fflush(stdout);
                }
                printf("\033[0m %3d%%", bars);
                fflush(stdout);
            }

        }

        int width = atoi(strtok(inputs, " "));
        int height = atoi(strtok(NULL, " "));
        strtok(NULL, " ");
        strtok(NULL, " ");
        strtok(NULL, " ");
        strtok(NULL, " ");
        int processes = atoi(strtok(NULL, " "));

        // Delete Png File if not Saving

        if (!save_png) {
            char *file_path = strtok(NULL, "");
            remove(file_path);
        }

        // Move off of Progress Bar Line

        if (!show_rep_times) {
            printf("\n");
        }

        // Calculating Mean

        float mean = 0;
        for (int i = 0; i < reps; i++) {
            mean += rep_times[i];
        }
        mean /= reps;
        printf("Mean Time %24lfs\n", mean);

        // Calculating Standard Deviation and Pixels Per Second

        float std_deviation = 0;
        for (int i = 0; i < reps; i++) {
            std_deviation += pow((rep_times[i] - mean), 2);
        }
        std_deviation /= reps;
        std_deviation = sqrt(std_deviation);
        printf("Standard Deviation %15lfs\n", std_deviation);

        int pix_per_sec = (int)round(width * height / mean);
        printf("Pixels per Second: %d\n", pix_per_sec);

        // Saving Results to File

        FILE *fptr_results = fopen("autorun_results.csv", "a");
        fprintf(
            fptr_results, "%s, %d, %d, %d, %d, %f, %f, %d",
            version, width, height, processes, reps, mean, std_deviation, pix_per_sec
        );

        if (reps >= 10) {

            // Sorting Rep Times

            bool swapped;
            float temp;
            for (int i = 0; i < reps - 1; i++) {
                swapped = false;
                for (int ii = 0; ii < reps - i - 1; ii++) {
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

            int percentiles[] = {5, 25, 50, 75, 95};
            float pctile_times[] = {-1.0, -1.0, -1.0, -1.0, -1.0};
            float iqr; // interquartile range (q3 - q1)

            for (int i = 0; i < sizeof(percentiles) / sizeof(percentiles[0]); i++) {

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

                pctile_times[i] = value;

                printf("%2dth Percentile %18lfs\n", percentiles[i], value);

            }

            // Finding Outliers Using IQR

            iqr = pctile_times[3] - pctile_times[1];
            printf("Interquartile Range %14lfs\n", iqr);
            int outliers = 0;
            float *outliers_list = malloc(reps * sizeof(rep_times[0]));
            int outliers_cap = 0;
            for (int i = 0; i < reps; i++) {
                if (
                    rep_times[i] < pctile_times[1] - iqr * 1.5 || // Low outlier
                    rep_times[i] > pctile_times[3] + iqr * 1.5 // High outlier
                ) {
                    if (outliers > outliers_cap) {
                        outliers_cap = (outliers_cap == 0) ? 1 : outliers_cap * 2;
                        outliers_list = realloc(outliers_list, outliers_cap * sizeof(rep_times[i]));
                    }
                    outliers_list[outliers] = rep_times[i];
                    outliers++;
                }
            }
            printf("Outliers: %d\n", outliers);
            if (outliers > 0) {
                printf("    ");
                float new_mean = mean * reps;
                for (int i = 0; i < outliers; i++) {
                    printf("%f ", outliers_list[i]);
                    new_mean -= outliers_list[i];
                }
                new_mean /= reps - outliers;
                printf("\nMean (Outliers Removed) %10lfs\n", new_mean);
            }
            free(outliers_list);

            // Adding Results to File

            fprintf(
                fptr_results, ", %f, %f, %f, %f, %f\n",
                pctile_times[0], pctile_times[1], pctile_times[2], pctile_times[3], pctile_times[4]
            );

        } else {

            fprintf(fptr_results, "\n");

        }

        printf("\n");

        fclose(fptr_results);

    }

    fclose(fptr);

    return 0;

}