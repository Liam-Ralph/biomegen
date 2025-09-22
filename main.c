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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

// OS Specific Includes

#ifdef __linux__
    #include <sys/prctl.h>
#elif _WIN32
    #include <windows.h>
#endif

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
    char *command = "clear";
    #ifdef _Win32
        command = "clear";
    #endif
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

    char raw_result[100];
    int result;

    while (true) {

        fgets(raw_result, 100, stdin);
        result = atoi(raw_result);

        if (result < min || result > max || (result == 0  && strncmp("0", raw_result, 1) != 0)) {
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
int sum_list_int(int *list) {
    int sum = 0;
    for (int i = 0; i < sizeof(list) / sizeof(int); i++) {
        sum += list[i];
    }
    return sum;
}

/**
 * Return the sum of a list of floats.
 */
float sum_list_float(float *list) {
    float sum = 0;
    for (int i = 0; i < sizeof(list) / sizeof(float); i++) {
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
    int *section_progress, int *section_progress_total, float *section_times
) {

    // Setting Tracker Process Title

    #ifdef __linux__

        prctl(PR_SET_NAME, "BiomeGenTracker", 0, 0, 0);

    #elif BSD

        setproctitle("BiomeGen Tracker");

    #elif __Apple__

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

            // Calculatin Section Progress

            float progress_section = section_progress[i] / (float)section_progress_total[i];
            total_progress += progress_section * section_weights[i];

            // Checking if Section Complete

            char *color;
            float section_time;
            if (section_progress[i] == section_progress_total[i]) {
                color = ANSI_GREEN;
                section_time = section_times[i]; // Section complete
            } else {
                color = ANSI_BLUE;
                if (i == 0 || section_times[i - 1] != 0.0) {
                    section_time = time_diff - sum_list_float(section_times); // Section in progress
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
        if (sum_list_int(section_progress) == sum_list_int(section_progress_total)) {
            color = ANSI_GREEN;
        } else {
            color = ANSI_BLUE;
        }

        printf("%s      TOTAL PROGRESS      %8.2f%% %s", color, total_progress * 100.0, ANSI_GREEN);
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
            #ifdef _WIN32
                Sleep(500);
            #else
                struct timespec sleep_time;
                sleep_time.tv_sec = 0;
                sleep_time.tv_nsec = 500000000;
                nanosleep(&sleep_time, &sleep_time);
            #endif
        }

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

        char file_line[100];
        FILE *fptr;
        fptr = fopen("README.md", "r");
        fgets(file_line, 100, fptr);
        fgets(file_line, 100, fptr);
        fgets(file_line, 100, fptr);
        fclose(fptr);

        char version[16];
        strncpy(version, file_line + 12, 12);
        version[strlen(version) - 1] = '\0';

        // Copyright, license notice, etc.
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

    int *section_progress = mmap(
        NULL, sizeof(int) * 7, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0
    );

    int *section_progress_total = mmap(
        NULL, sizeof(int) * 7, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0
    );

    float *section_times = mmap(
        NULL, sizeof(float) * 7, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0
    );

    for (int i = 0; i < 7; i++) {
        section_progress[i] = 0;
        section_progress_total[i] = 1;
        section_times[i] = 0.0;
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

        }

    }

    struct timespec time_now;
    clock_gettime(CLOCK_REALTIME, &time_now);
    section_times[0] = calc_time_diff(start_time, time_now);
    section_progress[0] = 1;

    // Section Generation

    section_progress_total[1] = num_dots;

    // Finish

    clock_gettime(CLOCK_REALTIME, &time_now);
    section_times[6] = calc_time_diff(start_time, time_now) - sum_list_float(section_times);
    section_progress[6] = 1;

    if (!auto_mode) {
        // Wait for Tracker Process
        int junk;
        waitpid(tracker_process_pid, &junk, 0);
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