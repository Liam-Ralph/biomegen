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
    char raw_line[99]; // Max task length is 99 chars

    while(fgets(raw_line, 100, fptr)) {

        // Getting Task Parameters

        char *line_copy = malloc(strlen(raw_line) + 1);
        strcpy(line_copy, raw_line);

        int reps = atoi(strtok(line_copy, ":")); // Repetitions
        bool show_rep_times, save_png = false;
        if (strtok(NULL, ":") == "y") {
            show_rep_times = true; // Whether to show rep times
        }
        if (strtok(NULL, ":") == "y") {
            save_png = true; // Whether to save png outputs
        }
        char *inputs = strtok(NULL, ":"); // Inputs for main program

        free(line_copy);

        printf("Running task %s for %d reps.\n", inputs, reps);

        // Prepping Save Path

        if (save_png) {
            const int len_inputs = strlen(inputs);
            char *inputs_sec[100];
            strncpy(inputs_sec, inputs, len_inputs - 5);
            inputs = strcat(inputs_sec, "0.png");
        }
        // "1920 1080 100 120 50 5 8 file.png" --> "1920 1080 100 120 50 5 8 file0.png"

        // Running Repetitions

        printf("       10|       20|       30|       40|       50|       60|");
        printf("       70|       80|       90|      100|\n");
        
        int rep_times[reps];
        for (int i = 0; i < reps; i++) {

            

        }

    }

    fclose(fptr);

    return 0;

}