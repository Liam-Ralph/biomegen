/*
Copyright (C) 2025 Liam Ralph
https://github.com/liam-ralph

This program, including this file, is licensed under the
GNU General Public License v3.0 (GNU GPLv3), with one exception.
See LICENSE or this project's source for more information.
Project Source: https://github.com/liam-ralph/biomegen

result.png, the output of this program, is licensed under The Unlicense.
See LICENSE_PNG or this project's source for more information.

BiomeGen, a terminal application for generating png maps.
*/


// Includes

#define _POSIX_C_SOURCE 199309L

#include <math.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>


// Definitions

#define ANSI_GREEN "\033[38;5;2m"
#define ANSI_BLUE "\033[38;5;4m"
#define ANSI_RESET "\033[0m"


// Structs

struct Dot {
    int x;
    int y;
    char type[13];
};
typedef struct Dot Dot;


// Functions
// (Alphabetical order)

/**
 * Check if a 2D int array[outer_size][inner_size] contains another int array of
 * size inner_size
 */
// bool array_contains_int_array(int outer_size, int inner_size, int array[outer_size][inner_size], int value_array[inner_size]) {
//     for (int i = 0; i < outer_size; i++) {
//         bool found_value = true;
//         for (int ii = 0; ii < inner_size; ii++) {
//             if (array[i][ii] != value_array[ii]) {
//                 found_value = false;
//                 break;
//             }
//         }
//         if (found_value) {
//             return true;
//         }
//     }
//     return false;
// }

/**
 * Calculates the difference in seconds between two timespecs, start and end.
*/
float calc_time_diff(struct timespec start, struct timespec end) {
    int diff_sec = end.tv_sec - start.tv_sec;
    int diff_nsec = end.tv_nsec - start.tv_nsec;
    return (float)diff_sec + diff_nsec / 1000000000.0;
}

/**
 * Clears all output from the terminal.
 */
void clear_screen() {
    char command[6] = "clear";
    system(command);
}

/**
 * Formats a time in seconds into a string, placing the output in buffer.
 * Output string has the format "MM:SS.SSS", e.g. 12:34.567.
 */
void format_time(char *buffer, size_t buffer_size, float time_seconds) {

    int minutes = (int)time_seconds / 60;
    float seconds = fmod(time_seconds, 60.0f);

    snprintf(buffer, buffer_size, "%2d:%06.3f", minutes, seconds);

}

/**
 * Get a sanitized integer input from the user between min and max,
 * both inclusive.
 */
int get_int(const int min, const int max) {

    int result;

    while (true) {

        int scanf_return = scanf("%d", &result);
        while (getchar() != '\n');

        if (scanf_return != 1 || result < min || result > max) {
            printf("Input must be an integer between %d and %d (both inclusive).\n", min, max);
        } else {
            break;
        }

    }

    return result;

}

/**
 * Return the sum of a list of integers.
 */
int sum_list_int(int *list, int list_len) {
    int sum = 0;
    for (int i = 0; i < list_len; i++) {
        sum += list[i];
    }
    return sum;
}

/**
 * Return the sum of a list of _Atomic int.
 */
int sum_list_int_atomic(_Atomic int *list, int list_len) {
    int sum = 0;
    for (int i = 0; i < list_len; i++) {
        sum += atomic_load(&list[i]);
    }
    return sum;
}

/**
 * Return the sum of a list of floats.
 */
float sum_list_float(float *list, int list_len) {
    float sum = 0;
    for (int i = 0; i < list_len; i++) {
        sum += list[i];
    }
    return sum;
}


// Multiprocessing Functions
// (Order of use)

/**
 * Track the progress of map generation, and show the progress in the terminal.
 * start_time is the time at the start of map generation in main(), and the
 * other inputs are all shared memory used to track section progress and times.
 */
void track_progress(
    struct timespec start_time,
    _Atomic int *section_progress, int *section_progress_total, float *section_times
) {

    // Setting Tracker Process Title

    #ifdef __linux__

        prctl(PR_SET_NAME, "BiomeGenTracker", 0, 0, 0);

    #elif BSD || __Apple__

        setproctitle("BiomeGen Tracker");
    
    #endif

    char section_names[7][20] = {
        "Setup", "Section Generation", "Section Assignment", "Coastline Smoothing",
        "Biome Generation", "Image Generation", "Finish"
    };
    float section_weights[7] = {0.02, 0.01, 0.11, 0.38, 0.29, 0.17, 0.02};
    // Used for overall progress bar (e.g. Setup takes ~2% of total time)

    while (true) {

        clear_screen();

        // Section Progress

        struct timespec time_now;
        clock_gettime(CLOCK_REALTIME, &time_now);
        float time_diff = calc_time_diff(start_time, time_now);

        float total_progress = 0.0;

        for (int i = 0; i < 7; i++) {

            // Calculating Section Progress

            float progress_section =
                atomic_load(&section_progress[i]) / (float)section_progress_total[i];
            total_progress += progress_section * section_weights[i];

            // Checking if Section Complete

            char *color;
            float section_time;
            if (atomic_load(&section_progress[i]) == section_progress_total[i]) {
                color = ANSI_GREEN;
                section_time = section_times[i]; // Section complete
            } else {
                color = ANSI_BLUE;
                if (i == 0 || section_times[i - 1] != 0.0) {
                    section_time = time_diff - sum_list_float(section_times, 7);
                    // Section in progress
                } else {
                    section_time = 0.0; // Section hasn't started
                }
            }

            // Printing Output for Section Progress

            printf(
                "%s[%d/7] %-20s%8.2f%% %s",
                color, i + 1, section_names[i], progress_section * 100, ANSI_GREEN
            ); // "[1/7] Setup               100.00% "
            int green_bars = (int)round(20 * progress_section);
            for (int ii = 0; ii < green_bars; ii++) {
                printf("█");
            }
            printf("%s", ANSI_BLUE);
            for (int ii = 0; ii < 20 - green_bars; ii++) {
                printf("█");
            }
            char formatted_time[10];
            format_time(formatted_time, sizeof(formatted_time), section_time);
            printf("%s%s\n", ANSI_RESET, formatted_time);

        }

        // Total Progress

        char *color;
        if (sum_list_int_atomic(section_progress, 7) == sum_list_int(section_progress_total, 7)) {
            color = ANSI_GREEN;
        } else {
            color = ANSI_BLUE;
        }

        printf("%s      TOTAL PROGRESS      %8.2f%% %s", color, total_progress * 100, ANSI_GREEN);
        int green_bars = (int)round(20 * total_progress);
        for (int i = 0; i < green_bars; i++) {
            printf("█");
        }
        printf("%s", ANSI_BLUE);
        for (int i = 0; i < 20 - green_bars; i++) {
            printf("█");
        }
        char formatted_time[10];
        format_time(formatted_time, sizeof(formatted_time), calc_time_diff(start_time, time_now));
        printf("%s%s\n", ANSI_RESET, formatted_time);

        // Checking Exit Status

        if (strcmp(color, ANSI_GREEN) == 0) {
            break; // All sections done, exit tracking process
        } else {
            // Sleep 0.5 seconds
            struct timespec sleep_time;
            sleep_time.tv_sec = 0;
            sleep_time.tv_nsec = 500000000;
            nanosleep(&sleep_time, &sleep_time);
        }

    }

}

void assign_sections(
    const int map_resolution, const float island_size, const int start_index, const int end_index,
    const struct Dot *origin_dots, const int num_origin_dots, struct Dot *dots, _Atomic int *section_progress
) {

    for (int i = start_index; i < end_index; i++) {

        struct Dot dot = dots[i];

        if (strcmp(dot.type, "Water") == 0) { // Ignore "Water Forced" and "Land Origin"

            int min = -1; // min and dist are squared, sqrt is not dont until later
            for (int ii = 0; ii < num_origin_dots; ii++) {
                const struct Dot origin_dot = origin_dots[ii];
                const int dist = pow(origin_dot.x - dot.x, 2) + pow(origin_dot.y - dot.y, 2);
                if (ii == 0) {
                    min = dist;
                } else {
                    if (dist < min) {
                        min = dist;
                    }
                }
            }

            srand(time(NULL));

            int chance;
            if (sqrt(min) <= ((float)(rand() % 20) / 19 * 1.5 + 0.25) * island_size) {
                chance = 9;
            } else {
                chance = 1;
            }

            if (rand() % 10 < chance) {
                strcpy(dot.type, "Land");
            }

        }

        atomic_fetch_add(&section_progress[2], 1);

    }

}


// Main Function

/**
 * Main Function. argc and argv used for manual inputs mode.
 */
int main(int argc, char *argv[]) {

    // Setting Main Process Title

    #ifdef __linux__

        prctl(PR_SET_NAME, "BiomeGen Main", 0, 0, 0);

    #elif BSD

        setproctitle("BiomeGen Main");

    #elif __Apple__

        setproctitle("BiomeGen Main");
    
    #endif

    // Getting Inputs

    bool auto_mode;
    char *output_file;
    int width, height, map_resolution, island_abundance, coastline_smoothing, processes;
    float island_size;

    if (argc == 1) {
        
        // Manual Inputs Mode

        auto_mode = false;

        output_file = "result.png";

        // Getting Program Version

        char file_line[29];
        FILE *fptr = fopen("README.md", "r");
        fgets(file_line, 29, fptr);
        fgets(file_line, 29, fptr);
        fgets(file_line, 29, fptr);
        fclose(fptr);

        char version[12];
        strncpy(version, file_line + 12, 12);
        version[strlen(version) - 1] = '\0';

        // Copyright, license notice, etc.
        clear_screen();
        printf(
            "Welcome to BiomeGen v%s\n"
            "Copyright (C) 2025 Liam Ralph\n"
            "https://github.com/liam-ralph\n"
            "This project is licensed under the GNU General Public License v3.0,\n"
            "except for result.png, this program's output, licensed under The Unlicense.\n"
            "\033[38;5;1mWARNING: In some terminals, the refreshing progress screen\n"
            "may flash, which could cause problems for people with epilepsy.%s\n"
            "Press ENTER to begin.\n", version, ANSI_RESET
        );
        /*
        I do not know if the flashing lights this program sometimes makes could reasonably cause
        epilepsy or not, but I put this just in case
        */
        getchar();
        clear_screen();

        printf("Map Width (pixels):\n");
        width = get_int(500, 10000);

        printf("\nMap Height:\n");
        height = get_int(500, 10000);

        printf(
            "\nMap resolution controls the section size of the map.\n"
            "Choose a number between 50 and 500. 100 is the default.\n"
            "Larger numbers produce lower resolutions, with larger pieces\n"
            "while lower numbers take longer to generate.\n"
            "Map Resolution:\n"
        );
        map_resolution = get_int(50, 500);

        printf(
            "\nIsland abundance control how many islands there are,\n"
            "and the ration of land to water.\n"
            "Choose a number between 10 and 1000. 120 is the default.\n"
            "Larger numbers produces less land.\n"
            "Island Abundance:\n"
        );
        island_abundance = get_int(10, 1000);

        printf(
            "\nIsland size controls average island size.\n"
            "Choose a number between 10 and 100. 50 is the default.\n"
            "Larger numbers produce larger islands.\n"
            "Island Size:\n"
        );
        island_size = get_int(10, 100) / 10.0;

        printf(
            "\nCoastline smoothing controls how smooth or rough coastlines look.\n"
            "Choose a number between 0 and 100. Larger numbers cause more smoothing.\n"
            "A value of 0 causes no smoothing. A value of 5 causes some smoothing,\n"
            "and is the default value.\n"
            "Coastline Smoothing:\n"
        );
        coastline_smoothing = get_int(0, 100);

        printf(
            "\nNow you must choose how many of your CPU's threads to use for map generation.\n"
            "Values exceeding your CPU's number of threads will slow map generation.\n"
            "The most efficient number of threads to use varies by hardware, OS,\n"
            "and CPU load. Values less than 4 threads are usually very inefficient.\n"
            "Ensure you monitor your CPU for overheating, and halt the program if\n"
            "high temperatures occur. Using fewer threads may reduce temperatures.\n"
            "Number of Threads:\n"
        );
        processes = get_int(1, 64); // Change this for CPUs with >64 threads

        clear_screen();

    } else {
        
        // Automated Inputs Mode

        auto_mode = true;

        width = atoi(argv[1]);
        height = atoi(argv[2]);
        map_resolution = atoi(argv[3]);
        island_abundance = atoi(argv[4]);
        island_size = atoi(argv[5]) / 10.0;
        coastline_smoothing = atoi(argv[6]);
        processes = atoi(argv[7]);
        output_file = argv[8];

    }

    struct timespec start_time;
    clock_gettime(CLOCK_REALTIME, &start_time);

    // Shared Memory

    _Atomic int *section_progress = mmap(
        NULL, sizeof(int) * 7, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0
    );

    int *section_progress_total = mmap(
        NULL, sizeof(int) * 7, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0
    );

    float *section_times = mmap(
        NULL, sizeof(float) * 7, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0
    );

    for (int i = 0; i < 7; i++) {
        atomic_init(&section_progress[i], 0);
        section_progress_total[i] = 1;
        section_times[i] = 0;
    }

    const int num_dots = width * height / map_resolution;

    struct Dot *dots = mmap(
        NULL, sizeof(struct Dot) * num_dots, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0
    );

    int tracker_process_pid = -1;

    if (!auto_mode) {

        // Progress Tracking

        tracker_process_pid = fork();

        if (tracker_process_pid == 0) {
            track_progress(start_time, section_progress, section_progress_total, section_times);
            exit(0);
        }

    }

    struct timespec time_now;
    clock_gettime(CLOCK_REALTIME, &time_now);
    section_times[0] = calc_time_diff(start_time, time_now);
    atomic_store(&section_progress[0], 1);

    // Section Generation
    // Creating the initial list of dots

    section_progress_total[1] = num_dots;

    srand(time(NULL));

    int num_special_dots = num_dots / island_abundance;

    bool *used_coords = calloc(width * height, sizeof(bool));

    for (int i = 0; i < num_dots; i++) {

        // Find unused coordinate

        int ii;
        do {
            ii = rand() % (width * height);
        } while (used_coords[ii]);

        // Create dot at rand_index[x, y]

        used_coords[ii] = true;

        Dot new_dot;
        new_dot.x = ii % width;
        new_dot.y = ii / width;
        if (i < num_special_dots) {
            strcpy(new_dot.type, "Land Origin"); // Origin points for islands
        } else if (i < num_special_dots * 2) {
            strcpy(new_dot.type, "Water Forced"); // Forced water (good for making lakes)
        } else {
            strcpy(new_dot.type, "Water"); // Water (default)
        }

        dots[i] = new_dot;

        atomic_fetch_add(&section_progress[1], 1);

    }

    free(used_coords);

    clock_gettime(CLOCK_REALTIME, &time_now);
    section_times[1] = calc_time_diff(start_time, time_now) - sum_list_float(section_times, 7);

    // Section Assignment
    // Assigning dots as "Land", "Land Origin", "Water", or "Water Forced"

    section_progress_total[2] = num_dots;

    struct Dot origin_dots[num_special_dots];
    int i = 0;
    for (int ii = 0; ii < num_dots; ii++) {
        if (strcmp(dots[ii].type, "Land Origin") == 0) {
            origin_dots[i + 1] = dots[i];
            i++;
        }
    }

    const int piece_length = num_dots / processes;
    int piece_ends[processes];
    for (int i = 0; i < processes - 1; i++) {
        piece_ends[i] = (i + 1) * piece_length;
    }
    piece_ends[processes - 1] = num_dots;
    // used to create x pieces of size num_dots / x, where x = processes
    // last piece may be larger if num_dots / x != (float)num_dots / x

    int fork_pids[processes];
    for (int i = 0; i < processes; i++) {
        fork_pids[i] = fork();
        if (fork_pids[i] == 0) {
            assign_sections(
                map_resolution, island_size, piece_length * i, piece_ends[i],
                origin_dots, num_special_dots, dots, section_progress
            );
            exit(0);
        }
    }
    for (int i = 0; i < processes; i++) {
        waitpid(fork_pids[i], NULL, 0);
    }

    clock_gettime(CLOCK_REALTIME, &time_now);
    section_times[2] = calc_time_diff(start_time, time_now) - sum_list_float(section_times, 7);

    // Finish

    clock_gettime(CLOCK_REALTIME, &time_now);
    section_times[6] = calc_time_diff(start_time, time_now) - sum_list_float(section_times, 7);
    atomic_store(&section_progress[6], 1);

    if (!auto_mode) {
        // Wait for Tracker Process
        waitpid(tracker_process_pid, NULL, 0);
    }

    // Shared Memory Cleanup

    munmap(section_progress, sizeof(int) * 7);
    munmap(section_progress_total, sizeof(int) * 7);
    munmap(section_times, sizeof(float) * 7);
    munmap(dots, sizeof(struct Dot) * num_dots);

    // Completion

    struct timespec end_time;
    clock_gettime(CLOCK_REALTIME, &end_time);
    float completion_time = calc_time_diff(start_time, end_time);

    if (!auto_mode) {

        // Print Result Statistics

    } else {

        printf("%f\n", completion_time);

    }

    return 0;

}