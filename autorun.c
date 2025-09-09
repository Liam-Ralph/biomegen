// Includes

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

        char *line_copy = malloc(strlen(raw_line) + 1);
        strcpy(line_copy, raw_line);

        int reps = atoi(strtok(line_copy, ":")); // Repetitions
        bool show_rep_times = false; // Whether to show rep times
        bool save_png = false; // Whether to save png outputs
        if (strtok(NULL, ":") == "y") {
            show_rep_times = true;
        }
        if (strtok(NULL, ":") == "y") {
            save_png = true;
        }
        char *inputs = strtok(NULL, ":"); // Inputs for main program

        free(line_copy);

        printf("Running task %s for %d reps.\n", inputs, reps);

        // Prepping Save Path

        if (save_png) {
            const int len_inputs = strlen(inputs);
            strncpy(inputs, inputs, len_inputs - 5);
            strcat(inputs, "0.png");
        }
        // "1920 1080 100 120 50 5 8 file.png" --> "1920 1080 100 120 50 5 8 file0.png"

        // Running Repetitions

        if (show_rep_times) {
            // Progress Bar Scale
            printf("       10|       20|       30|       40|       50|       60|");
            printf("       70|       80|       90|      100|\n");
        }

        int rep_times[reps];
        for (int i = 0; i < reps; i++) {

            if (save_png) {

                // Preparing Rep's Save Path

                const int len_inputs = strlen(inputs);
                char inputs_sec[100];
                strncpy(inputs_sec, inputs, len_inputs - 6);
                strcat(inputs, snprintf(inputs_sec, 5, "%d", i));
                strcat(inputs, ".png");
                // Rep 1 is saved in file1.png, rep2 in file2.png, etc.

            }

            // Choosing Python Command

            char *python_command = "python3";

            #ifdef _WIN32
                python_command = "py";
            #endif

            // Running Program and Collecting Output

            FILE *fp;
            char output[50] = {0};
            char buffer[50];

            fp = popen(strcat(strcat(python_command, " main.py "), inputs));
            while (fgets(buffer, sizeof(buffer), fp) != NULL) {
                strcat(output, buffer);
            }
            pclose(fp);

            printf(output);

        }

    }

    fclose(fptr);

    return 0;

}