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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

void clear_screen() {
    char *command = "clear";
    #ifdef _Win32
        command = "clear";
    #endif
    system(command);
}

int get_int(const int min, const int max) {

    char raw_result[100];
    long result;

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


// Multiprocessing Functins
//(Order of use)


// Main Function

int main(int argc, char *argv[]) {

    // Setting Main Process Title

    #ifdef __linux__

        #include <sys/prctl.h>
        prctl(PR_SET_NAME, "BiomeGen Main", 0, 0, 0);

    #elif BSD

        #include <unistd.h>
        setproctitle("BiomeGen Main");

    #elif __Apple__

        #include <unistd.h>
        setproctitle("BiomeGen Main");
    
    #endif

    // Clearing Errors File

    FILE *fptr;
    fptr = fopen("errors.txt", "w");
    fclose(fptr);

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

    struct timespec start_time_ts;
    float start_time;
    clock_gettime(CLOCK_REALTIME, &start_time_ts);
    start_time = (float)start_time_ts.tv_sec + ((float)start_time_ts.tv_nsec / 1000000000.0);
    printf("%f\n", start_time);
    sleep(2);
    struct timespec end_time_ts;
    float end_time, timediff;
    clock_gettime(CLOCK_REALTIME, &end_time_ts);
    end_time = (float)end_time_ts.tv_sec + ((float)end_time_ts.tv_nsec / 1000000000.0);
    timediff = end_time - start_time;
    printf("%f\n", end_time);
    printf("%f\n", timediff);

    printf("vsec\n%ld\n%ld\n", start_time_ts.tv_sec, end_time_ts.tv_sec);
    printf("nsec\n%ld\n%ld\n", start_time_ts.tv_nsec, end_time_ts.tv_nsec);

    return 0;

}