// Includes

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <unistd.h>


// Defintions

#define PATH_README "../README.md"
#define PATH_AUTORUN_TASKS "../src/autorun_tasks.txt"
#define PATH_AUTORUN_RESULTS "../src/autorun_results.csv"

#define ANSI_GREEN "\033[38;5;2m"
#define ANSI_BLUE "\033[38;5;4m"
#define ANSI_RESET "\033[0m"


// Main Function

int main() {

    // Setting Process Title

    #ifdef __linux__
        prctl(PR_SET_NAME, "bg-autorun", 0, 0, 0);
    #elif BSD || __Apple__
        setproctitle("bg-autorun");
    #endif

    printf("\n");

    // Getting Program Version

    char file_line[29];
    FILE *fptr = fopen(PATH_README, "r");
    fgets(file_line, 29, fptr);
    fgets(file_line, 29, fptr);
    fgets(file_line, 29, fptr);
    fclose(fptr);

    char version[12];
    strncpy(version, file_line + 12, 12);
    version[strlen(version) - 2] = '\0';

    // Checking for Main Program

    if (!access("main", F_OK) == 0) {
        printf("main executable missing.");
        return 1;
    }

    // Opening Autorun Tasks

    fptr = fopen(PATH_AUTORUN_TASKS, "r");
    char line[256]; // Max task length is 256 chars

    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    uint32_t columns = (uint32_t)w.ws_col - 7;
    if (100 < columns) columns = 100;

    while (fgets(line, 256, fptr)) {

        // Getting Task Parameters

        char token[250];
        strncpy(token, strtok(line, ":"), 4);
        uint32_t reps = abs(atoi(token)); // Repetitions

        strncpy(token, strtok(NULL, ":"), 2);
        bool show_rep_times = false; // Whether to show rep times
        if (token[0] == 'y')
            show_rep_times = true;

        strncpy(token, strtok(NULL, ":"), 2);
        bool save_png = false; // Whether to save png outputs
        if (token[0] == 'y')
            save_png = true;

        char inputs[250]; // Inputs for main program
        strncpy(inputs, strtok(NULL, ":\n"), 249);

        printf("Running task \"%s\" for %u reps.\n", inputs, reps);

        // Prep Save path

        if (save_png)
            inputs[strlen(inputs) - 5] = '\0';

        // Running Repetitions

        if (!show_rep_times) {
            printf(ANSI_BLUE);
            for (uint32_t ii = 0; ii < columns; ii++)
                printf("█");
            printf(ANSI_RESET "   0%%");
            fflush(stdout);
        }

        double rep_times[reps];

        for (uint32_t i = 0; i < reps; i++) {

            // Running Program and Collecting Output

            char output[13]; // max time 99999.999999 seconds (> 27 hours)

            char command[257];
            if (save_png)
                snprintf(command, 257, "./main %s%u.png", inputs, i + 1);
            else
                snprintf(command, 257, "./main %s", inputs);

            FILE *fp = popen(command, "r");
            fgets(output, 13, fp);
            pclose(fp);

            double time = atof(output);
            rep_times[i] = time;

            // Print Rep Time or Progress Bar

            if (show_rep_times) {
                printf("Repetition ");
                if (reps < 10) printf("%u", i + 1);
                else if (reps < 100) printf("%2u", i + 1);
                else printf("%3u", i + 1);
                printf(" Time: %lfs\n", time);
            } else {
                const uint32_t bars = round((i + 1) * columns / reps);
                printf("\r" ANSI_GREEN);
                for (uint32_t ii = 0; ii < bars; ii++)
                    printf("█");
                printf(ANSI_BLUE);
                for (uint32_t ii = 0; ii < columns - bars; ii++)
                    printf("█");
                printf(ANSI_RESET);
                printf(" %3u%%", (i + 1) / reps * 100);
                fflush(stdout);
            }

        }

        uint32_t width = atoi(strtok(inputs, " "));
        uint32_t height = atoi(strtok(NULL, " "));
        for (uint32_t i = 0; i < 5; i++)
            strtok(NULL, " ");
        uint32_t processes = atoi(strtok(NULL, " "));

        // Delete Png File if not Saving

        if (!save_png) {
            char file_path[229];
            strncpy(file_path, strtok(NULL, ""), 229);
            remove(file_path);
        }

        // Move off of Progress Bar Line

        if (!show_rep_times)
            printf("\n");

        // Calculating Mean

        double mean = 0;
        for (uint32_t i = 0; i < reps; i++)
            mean += rep_times[i];
        mean /= reps;
        printf("Mean Time               %lfs\n", mean);

        // Calculating Standard Deviation and Pixels Per Second

        double std_deviation = 0;
        for (uint32_t i = 0; i < reps; i++) {
            double temp = rep_times[i] - mean;
            std_deviation += temp * temp;
        }
        std_deviation /= reps;
        std_deviation = sqrt(std_deviation);
        printf("Standard Deviation      %lfs\n", std_deviation);

        uint32_t pix_per_sec = (uint32_t)round(width * height / mean);
        printf("Pixels per Second       %d\n", pix_per_sec);

        // Saving Results to File

        FILE *fptr_results = fopen(PATH_AUTORUN_RESULTS, "a");
        fprintf(
            fptr_results, "%s, %d, %d, %d, %d, %lf, %lf, %d",
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
                if (!swapped)
                    break;
            }

            // Percentiles

            int percentiles[] = {5, 25, 50, 75, 95};
            double pctile_times[5];
            double iqr; // interquartile range (q3 - q1)

            for (uint32_t i = 0; i < 5; i++) {

                double exact_index = percentiles[i] * (reps - 1) / 100.0; // 50th index of 8 is 3.5
                uint32_t index = (uint32_t)exact_index; // floor of exact index
                double diff = exact_index - index; // diff between index and exact

                double value = rep_times[index]; // rep time at index
                if (diff > 0.0001)
                    // add diff% of next index
                    value += (rep_times[index + 1] - rep_times[index]) * diff;

                /*
                Example for 50th percentile of list {1, 2, 3, 4}
                exact_index = 50 * 3 / 100 = 1.5
                value = list[1] + (list[2] - list[1]) * 0.5 = 2 + 1 * 0.5 = 2.5
                */

                pctile_times[i] = value;

                printf("%2dth Percentile         %lfs\n", percentiles[i], value);

            }

            // Finding Outliers Using IQR

            iqr = pctile_times[3] - pctile_times[1];
            printf("Interquartile Range     %lfs\n", iqr);
            uint32_t outliers = 0;
            double *outliers_list = malloc(reps * sizeof(rep_times[0]));
            for (uint32_t i = 0; i < reps; i++)
                if (
                    rep_times[i] < pctile_times[1] - iqr * 1.5 || // Low outlier
                    rep_times[i] > pctile_times[3] + iqr * 1.5 // High outlier
                )
                    outliers_list[outliers++] = rep_times[i];
            printf("Outliers: %d\n", outliers);
            if (outliers > 0) {
                printf("    ");
                double new_mean = mean * reps;
                for (uint32_t i = 0; i < outliers; i++) {
                    printf("%lf ", outliers_list[i]);
                    new_mean -= outliers_list[i];
                }
                new_mean /= reps - outliers;
                printf("\nMean (Outliers Removed) %lfs\n", new_mean);
            }
            free(outliers_list);

            // Adding Results to File

            fprintf(
                fptr_results, ", %lf, %lf, %lf, %lf, %lf\n",
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