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


// Definitions

#define ANSI_GREEN "\003[38;5;2m"
#define ANSI_BLUE "\003[38;5;4m"
#define ANSI_RESET "\003[0m"


// Structs

struct Dot {
    int x;
    int y;
    char type[13];
};
typedef struct Dot Dot;


// Functions
// (Alphabetical order)

int clear_screen(){
    char command[] = "clear";
    #ifdef _WIN32
        command = "cls";
    #endif
    system(command);
}


// Multiprocessing Functins
//(Order of use)


// Main Function

int main(int argc, char *argv[]) {

    // Clearing Errors File

    FILE *fptr;
    fptr = fopen("errors.txt", "w");
    fclose(fptr);

    bool auto_mode;
    char *output_file;

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
            "\003[38;5;1mWARNING: In some terminals, the refreshing progress screen\n"
            "may flash, which could cause problems for people with epilepsy.%s\n"
            "Press ENTER to begin.\n", version, ANSI_RESET
        );
        /*
        I do not know if the flashing lights this program sometimes makes could reasonably cause
        epilepsy or not, but I put this just in case
        */
        getchar();
        clear_screen();

        printf("Map Width (pixels):");

    } else {
        
        // Automated Inputs Mode

        auto_mode = true;

    }

    return 0;

}