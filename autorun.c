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
    char raw_line[100]; // Max task length is 100 chars

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

    }

    fclose(fptr);

    return 0;

}