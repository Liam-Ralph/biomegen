// Includes

#include <math.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>


// Main Function

int main() {

    printf("\n");

    FILE *fptr;
    fptr = fopen("autorun_tasks.txt", "r");
    char raw_line[100]; // Max task length is 100 chars (99 if save_png)

    while (fgets(raw_line, 100, fptr)) {

        // Getting Task Parameters

        char line_copy[100];
        strcpy(line_copy, raw_line);

        char *token = strtok(line_copy, ":");
        int reps = atoi(line_copy); // Repetitions

        token = strtok(NULL, ":");
        bool show_rep_times = false; // Whether to show rep times
        if (token == "y") {
            show_rep_times = true;
        }

        token = strtok(NULL, ":");
        bool save_png = false; // Whether to save png outputs
        if (token == "y") {
            save_png = true;
        }

        token = strtok(NULL, "");
        char *inputs = token; // Inputs for main program

        printf("Running task %s for %d reps.\n", inputs, reps);

        // Prepping Save Path

        if (save_png) {
            const int len_inputs = strlen(inputs);
            strncpy(inputs, inputs, len_inputs - 5);
            strcat(inputs, "0.png");
        }
        // "1920 1080 100 120 50 5 8 file.png" --> "1920 1080 100 120 50 5 8 file0.png"

        // Running Repetitions

        if (!show_rep_times) {
            // Progress Bar Scale
            printf("       10|       20|       30|       40|       50|       60|");
            printf("       70|       80|       90|      100|\n");
        }

        int rep_times[reps];
        for (int i = 0; i < reps; i++) {

            if (save_png) {

                // Preparing Rep's Save Path

                const int len_inputs = strlen(inputs);
                strncpy(inputs, inputs, len_inputs - 6);
                sprintf(inputs, "%d", i);
                strcat(inputs, ".png");
                // Rep 1 is saved in file1.png, rep2 in file2.png, etc.

            }

            // Choosing Python Command

            char python_command[100] = "python3"; // Command for running python program

            #ifdef _WIN32
                python_command = "py";
            #endif

            // Running Program and Collecting Output

            FILE *fp;
            char output[50] = {0};
            char buffer[50];

            strcat(python_command, " main.py ");
            strcat(python_command, inputs);
            fp = popen(python_command, "r");
            while (fgets(buffer, sizeof(buffer), fp) != NULL) {
                strcat(output, buffer);
            }
            pclose(fp);

            float time = atof(output);
            rep_times[i] = time;

            if (show_rep_times) {
                printf("Rep %d Time: %f\n", i, time);
            } else {
                const int bars = round((i + 1) / reps * 100);
                printf("\r\033[K\033[48;5;2m");
                for (int ii = 0; ii < bars; ii++) {
                    printf(" ");
                }
                printf("\033[0m");
            }

        }

        if (!show_rep_times) {
            printf("\n");
        }

    }

    fclose(fptr);

    return 0;

}