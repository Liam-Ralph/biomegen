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

#define _POSIX_C_SOURCE 199309L // Needed for CLOCK_MONOTONIC

#include <math.h>
#include <png.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>


// Definitions

#define PATH_README "../README.md"

#define ANSI_GREEN "\033[38;5;2m"
#define ANSI_BLUE "\033[38;5;4m"
#define ANSI_RESET "\033[0m"

#define NUM_SECTIONS 7
#define NUM_DOTTYPES_IMAGE 11

#define DIM_MIN 500 // Width and height
#define DIM_MAX 10000 // Width and height
#define MAP_RESOLUTION_MIN 50
#define MAP_RESOLUTION_MAX 500
#define MAP_RESOLUTION_DEFAULT 100
#define ISLAND_ABUNDANCE_MIN 10
#define ISLAND_ABUNDANCE_MAX 1000
#define ISLAND_ABUNDANCE_DEFAULT 120
#define ISLAND_SIZE_MIN 10
#define ISLAND_SIZE_MAX 100
#define ISLAND_SIZE_DEFAULT 50
#define COASTLINE_SMOOTHING_MIN 0
#define COASTLINE_SMOOTHING_MAX 100
#define COASTLINE_SMOOTHING_DEFAULT 5
#define BIOME_SIZE_MIN 2
#define BIOME_SIZE_MAX 20
#define BIOME_SIZE_DEFAULT 5


// Structs

typedef enum : uint8_t {
    ICE, SHALLOW_WATER, WATER, DEEP_WATER, ROCK, DESERT, JUNGLE, FOREST, PLAINS, TAIGA, SNOW,
    WATER_FORCED, LAND, LAND_ORIGIN

} DotType;
// The underlying order here is used in dottype_colors and type_counts

typedef struct Dot {
    uint16_t x;
    uint16_t y;
    DotType type;
} Dot;

typedef struct timespec timespec;


// Global Variables

uint32_t width; // Used for row-major order


// General Functions

// User Input

/**
 * Clear screen, print a prompt for user input, and get a sanitized uint32_t
 * input from the user between `min` and `max` inclusive.
 * 
 * @param name
 * @param desc
 * @param min
 * @param max
 * @param def default, `UINT32_MAX` for no default
 * @param higher_effect
 * 
 * @return valid user input
 */
uint32_t get_uint(
    char *name, char *desc, uint32_t min, uint32_t max, uint32_t def, char *higher_effect
) {

    // Clear Screen

    system("clear");

    // Print Prompt

    printf(
        ANSI_BLUE "Name" ANSI_RESET ": %s\n"
        ANSI_BLUE "Description" ANSI_RESET ": %s\n"
        ANSI_BLUE "Minimum" ANSI_RESET ": %u\n"
        ANSI_BLUE "Maximum" ANSI_RESET ": %u\n"
        ANSI_BLUE "Default" ANSI_RESET ": ",
        name, desc, min, max
    );
    if (def != UINT32_MAX)
        printf("%u\n", def);
    else
        printf("N/A\n");
    printf(ANSI_BLUE "Higher values" ANSI_RESET ": %s\n", higher_effect);

    // Get Input

    uint32_t result;

    while (true) {

        int scanf_return = scanf("%u", &result);
        while (getchar() != '\n');

        if (scanf_return != 1 || result < min || result > max)
            printf("Input must be a positive integer between %u and %u inclusive.\n", min, max);
        else
            break;

    }

    return result;

}

// Timespec Functions

/**
 * Calculate the difference between two timespecs.
 * 
 * Calculates `end - start`.
 * 
 * @param start
 * @param end
 * 
 * @return difference
 */
timespec diff_timespecs(timespec start, timespec end) {
    timespec diff = {end.tv_sec - start.tv_sec, end.tv_nsec - start.tv_nsec};
    if (diff.tv_nsec < 0) {
        diff.tv_nsec += 1000000000;
        diff.tv_sec--;
    }
    return diff;
}

/**
 * Calculate a section's time.
 * 
 * Calculates `time_now - start_time - sum(section_times[:len])`.
 * 
 * @param time_now
 * @param start_time
 * @param section_times
 * @param len length of `section_times` (or the part thereof to be used)
 */
timespec calc_section_timespec(
    timespec time_now, timespec start_time, const timespec *section_times, uint32_t len
) {
    timespec result = time_now;
    result.tv_sec -= start_time.tv_sec;
    result.tv_nsec -= start_time.tv_nsec;
    for (uint32_t i = 0; i < len; i++) {
        result.tv_sec -= section_times[i].tv_sec;
        result.tv_nsec -= section_times[i].tv_nsec;
    }
    while (result.tv_nsec < 0) {
        result.tv_nsec += 1000000000;
        result.tv_sec--;
    }
    return result;
}

// Dot Functions

uint32_t row_major(Dot dot) {
    return dot.y * width + dot.x;
}

// Sorting Functions

/**
 * Swaps two dots in an array.
 * 
 * @param dots
 * @param i first index to swap with
 * @param ii second index to swap with
 */
void swap(Dot *dots, uint32_t i, uint32_t ii) {
    const Dot temp = dots[i];
    dots[i] = dots[ii];
    dots[ii] = temp;
}

/**
 * Sort the given `dots` by row-major order using the quicksort algorithm.
 * 
 * A dot's value is equal to `dot.y * width + dot.x`.
 * 
 * @param dots array to sort
 * @param low
 * @param high
 * @param width needed to calculate a dot's value
 */
void quicksort_recursive(Dot *dots, uint32_t low, uint32_t high) {

    if (low < high) {

        // Calculate Pivot Value

        uint32_t pivot = row_major(dots[high]);

        // Move Smaller Elements to the Left

        uint32_t p = low;

        for (uint32_t i = low; i < high; i++)
            if (row_major(dots[i]) < pivot)
                swap(dots, p++, i);

        // Move Pivot to After Smaller Elements

        swap(dots, p, high);

        // Recursion

        if (p > low)
            quicksort_recursive(dots, low, p - 1);
        quicksort_recursive(dots, p + 1, high);

    }

}

/**
 * Merge two sorted sections of `dots_unsorted` into `dots_sorted`.
 * 
 * The second section must directly follow the first, so `dots_unsorted[len1]`
 * is the first dot in the second section.
 * 
 * @param dots_unsorted
 * @param dots_sorted
 * @param len1 length of first section of `dots_unsorted`
 * @param len2 length of second section
 */
void merge(Dot *dots_unsorted, Dot *dots_sorted, uint32_t len1, uint32_t len2) {

    uint32_t i1 = 0;
    uint32_t i2 = 0;

    while (i1 + i2 < len1 + len2) {
        if (
            i2 == len2 ||
            (i1 < len1 && row_major(dots_unsorted[i1]) < row_major(dots_unsorted[len1 + i2]))
        ) {
            dots_sorted[i1 + i2] = dots_unsorted[i1];
            i1++;
        } else {
            dots_sorted[i1 + i2] = dots_unsorted[len1 + i2];
            i2++;
        }
    }

}


// KD Tree Functions

/**
 * Sort the given `dots` until index is correctly sorted.
 * 
 * After completion, index will be in the correct place, everything to the left
 * will be lower or equal, and everything right will be greater or equal.
 * 
 * @param dots array to sort
 * @param low
 * @param high
 * @param index
 * @param axis determines whether to sort by x (false) or y (true) values
 */
void quickselect(Dot *dots, uint32_t low, uint32_t high, uint32_t index, bool axis) {

    while (low < high) {

        // Calculate Pivot Value

        uint32_t pi = low + rand() % (high - low);
        uint16_t pivot = axis ? dots[pi].y : dots[pi].x;

        // Sort Based on Pivot

        uint32_t lt = low;
        uint32_t i = low;
        uint32_t gt = high;

        while (i <= gt) {
            uint16_t key = axis ? dots[i].y : dots[i].x;
            if (key < pivot)
                swap(dots, lt++, i++);
            else if (key > pivot)
                swap(dots, i, gt--);
            else
                i++;
        }

        // Adjust low or high

        if (index < lt)
            high = lt - 1;
        else if (index > gt)
            low = gt + 1;
        else
            return;

    }

}

/**
 * Build a 2D KD Tree within `tree_dot` using `dots`.
 * 
 * Ensures the KD Tree is complete, so it has the smallest height for effiency,
 * and so it fits within the array. `low` and `high` must be non-negative, and
 * `low + high` must not overflow in a signed int.
 * 
 * @param dots array to build using. Won't contain the tree, but will still be
 * modified
 * @param tree_dots array to build tree in. Must have same capacity.
 * @param index the index to build on this call, must be 0 in initial call
 * @param low
 * @param high
 * @param depth current depth, must be 0 in initial call
 */
void build_recursive(
    Dot *tree_dots, uint32_t index, Dot *dots, uint32_t low, uint32_t high,
    uint32_t full_dots, uint32_t left_cap, uint32_t depth
) {

    // Base Cases

    if (low > high) return;
    if (low == high) {
        tree_dots[index] = dots[low];
        return;
    }

    // Quickselect Dots

    const uint32_t extra_dots = high + 1 - low - full_dots;
    uint32_t dots_index = low + full_dots / 2 + ((extra_dots < left_cap) ? extra_dots : left_cap);

    quickselect(dots, low, high, dots_index, depth % 2);
    // it seems this is faster than storing axis and doing !axis in each recursive call

    // Add Median Node to Tree

    tree_dots[index] = dots[dots_index];

    // Recursion

    const uint32_t left_index = index * 2 + 1;
    full_dots -= left_cap;
    left_cap /= 2;
    depth++;
    build_recursive(tree_dots, left_index, dots, low, dots_index - 1, full_dots, left_cap, depth);
    build_recursive(
        tree_dots, left_index + 1, dots, dots_index + 1, high, full_dots, left_cap, depth
    );

}

void build_kd_tree(Dot *tree_dots, Dot *dots, uint32_t low, uint32_t high, uint32_t len) {

    // Calculate Values

    uint32_t left_cap = 1; // Capacity of left side of last level
    uint32_t full_dots = 1; // Dots used by full levels
    while (full_dots <= len) {
        left_cap *= 2;
        full_dots += left_cap;
    }
    full_dots -= left_cap;
    left_cap /= 2;

    // Run Recursive Function

    build_recursive(tree_dots, 0, dots, low, high, full_dots, left_cap, 0);

}

/**
 * Query a 2D KD Tree for the nearest neighbor to a coordinate.
 * 
 * Query the 2D KD Tree to get the distance to `coord`'s nearest neighbor. When
 * `type_ptr` is not null, it stores the type of the nearest dot.
 * 
 * @param tree_dots KD Tree to search
 * @param len length of `tree_dots`
 * @param index index of Dot to check, must be 0 in initial call
 * @param coord coordinate to search with, of form {x, y}
 * @param depth current depth, must be 0 in initial call
 * @param min_dist_sq_ptr pointer to store squared minimum distance in
 * @param nearest_index_ptr pointer to store nearest neighbor index, or NULL
 * @param min_dist_care query ends if a distance below this is found, make 0 for
 * regular calls
 */
void nearest_neighbor_recursive(
    const Dot *tree_dots, uint32_t len, uint32_t index, const uint32_t coord[2], uint32_t depth,
    uint32_t *min_dist_sq_ptr, uint32_t *nearest_index_ptr, uint32_t min_dist_sq_care
) {

    // Calculate Distance

    const int32_t diff_x = coord[0] - tree_dots[index].x;
    const int32_t diff_y = coord[1] - tree_dots[index].y;
    const uint32_t dist_sq = diff_x * diff_x + diff_y * diff_y;
    // Distance is squared for efficiency

    // Update Minimum Distance and Index Pointers

    if (dist_sq < *min_dist_sq_ptr) {
        *min_dist_sq_ptr = dist_sq;
        if (nearest_index_ptr != NULL)
            *nearest_index_ptr = index;
        else if (dist_sq < min_dist_sq_care)
            return;
    }

    // Decide Whether Recursion is Needed

    const bool axis = depth % 2;
    const uint32_t dot_value = (axis ? tree_dots[index].y : tree_dots[index].x);
    const int32_t dist_line = dot_value - coord[axis];
    const uint32_t dist_line_sq = dist_line * dist_line;
    // Whether distance to splitting line is less than current max dist

    uint32_t index_child = 2 * index + 1;
    depth++;
    if (index_child < len && (dist_line_sq < *min_dist_sq_ptr || coord[axis] < dot_value))
        /*
        Recursion only if coord is close enough to dividing line, or coord would
        be inside bounds of left child
        */
        nearest_neighbor_recursive(
            tree_dots, len, index_child, coord, depth,
            min_dist_sq_ptr, nearest_index_ptr, min_dist_sq_care
        );
    index_child++;
    if (index_child < len && (dist_line_sq < *min_dist_sq_ptr || coord[axis] >= dot_value))
        nearest_neighbor_recursive(
            tree_dots, len, index_child, coord, depth,
            min_dist_sq_ptr, nearest_index_ptr, min_dist_sq_care
        );

}

/**
 * Query a 2D KD Tree for the nearest neighbors to a coordinate.
 * 
 * Query the 2D KD Tree to get the distance to `coord`'s nearest neighbors.
 * 
 * @param tree_dots KD Tree to search
 * @param len length of `tree_dots`
 * @param index index of Dot to check, must be 0 in initial call
 * @param coord coordinate to search with, of form {x, y}
 * @param dists_sq array to write neighbor squared distances to,
 * must be initialized with all 0s
 * @param dists_sq_len number of neighbors to track, must be greater than 0
 * @param depth current depth, must be 0 in initial call
 */
void nearest_neighbors_recursive(
    const Dot *tree_dots, uint32_t len, uint32_t index, const uint32_t coord[2],
    uint32_t *dists_sq, uint32_t dists_sq_len, uint32_t depth
) {

    // Calculate Distance

    const int32_t diff_x = tree_dots[index].x - coord[0];
    const int32_t diff_y = tree_dots[index].y - coord[1];
    const uint32_t dist_sq = diff_x * diff_x + diff_y * diff_y; // Squared distance

    // Update Distances List

    if (dist_sq < dists_sq[dists_sq_len - 1] && dist_sq != 0) {
        for (uint32_t i = dists_sq_len - 1; ; i--) {
            if (i == 0 || dist_sq >= dists_sq[i - 1]) {
                // Found insertion position, insert and break
                dists_sq[i] = dist_sq;
                break;
            }
            // After insertion position, shift element
            dists_sq[i] = dists_sq[i - 1];
        }
    }

    // Decide Whether Recursion is Needed

    const uint32_t max_dist = dists_sq[dists_sq_len - 1];
    const bool axis = depth % 2;
    const uint32_t dot_value = (axis ? tree_dots[index].y : tree_dots[index].x);
    const int32_t dist_line = dot_value - coord[axis];
    const uint32_t dist_line_sq = dist_line * dist_line;
    // Whether distance to splitting line is less than max_dist

    uint32_t index_child = 2 * index + 1;
    depth++;
    if (index_child < len && (dist_line_sq < max_dist || coord[axis] < dot_value))
        /*
        Recursion only if coord is close enough to dividing line, or coord would
        be inside bounds of left child
        */
        nearest_neighbors_recursive(
            tree_dots, len, index_child, coord, dists_sq, dists_sq_len, depth
        );
    index_child++;
    if (index_child < len && (dist_line_sq < max_dist || coord[axis] >= dot_value))
        nearest_neighbors_recursive(
            tree_dots, len, index_child, coord, dists_sq, dists_sq_len, depth
        );

}


// Multiprocessing Functions
// (Order of use)

/**
 * Set the process title.
 * 
 * Set the process title to `"bg-" + type + num`. If 0 <= `num` < 1000, then it
 * is padded to a length of 3 (4 including null character) with leading zeros
 * and then added to the process title as well. The length of `type + num` must
 * be at most 13 characters (including null character).
 * 
 * @param type the type of process
 * @param num process number if applicable, ignored if below 0
 */
void set_process_title(const char *type, const int num) {

    // Example: type is "worker", num is 6: "bg-worker006"

    // Concatenate Type

    char string[16] = "bg-";
    strncat(string, type, 13);

    // Concatenate Num

    if (0 <= num && num < 1000) {
        char num_str[4];
        snprintf(num_str, 4, "%03d", num);
        strncat(string, num_str, 4);
    }

    // Set Process Title

    #ifdef __linux__
        prctl(PR_SET_NAME, string, 0, 0, 0);
    #elif BSD || __Apple__
        setproctitle(string);
    #endif

}

/**
 * Track map generation progress.
 * 
 * Track map generation progress and display in terminal.
 * 
 * @param start_time time at start of map generation
 * @param section_progress completed work units for each section
 * @param section_progress_total total work units for each section
 * @param section_times elapsed time for each section
 */
void track_progress(
    timespec start_time,
    _Atomic uint32_t *section_progress, uint32_t *section_progress_total, timespec *section_times
) {

    char section_names[NUM_SECTIONS][20] = {
        "Setup", "Section Generation", "Section Assignment", "Coastline Smoothing",
        "Biome Generation", "Image Generation", "Finish"
    };
    double section_weights[NUM_SECTIONS] = {0.01, 0.01, 0.03, 0.24, 0.04, 0.53, 0.14};
    // Used for overall progress bar (e.g. Setup takes ~1% of total time)

    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    uint32_t columns = (uint32_t)w.ws_col - 47;
    if (100 < columns) columns = 100;

    while (true) {

        // Clear Screen

        system("clear");

        // Section Progress

        timespec time_now;
        clock_gettime(CLOCK_MONOTONIC, &time_now);

        double total_progress = 0.0;

        for (uint32_t i = 0; i < NUM_SECTIONS; i++) {

            // Calculate Section Progress

            double progress_section =
                atomic_load(&section_progress[i]) / (double)section_progress_total[i];
            total_progress += progress_section * section_weights[i];

            // Check if Section Complete

            char color[sizeof(ANSI_GREEN)];
            timespec section_time;
            if (section_times[i].tv_nsec != 0 || section_times[i].tv_sec != 0) {
                memcpy(color, ANSI_GREEN, sizeof(ANSI_GREEN));
                section_time = section_times[i]; // Section complete
            } else {
                memcpy(color, ANSI_BLUE, sizeof(ANSI_BLUE));
                if (i == 0 || section_times[i - 1].tv_nsec != 0 || section_times[i - 1].tv_sec != 0)
                    section_time = calc_section_timespec(time_now, start_time, section_times, i);
                    // Section in progress
                else
                    section_time = (timespec){0, 0}; // Section hasn't started
            }

            // Print Output for Section Progress

            printf(
                "%s[%d/%d] %-20s%8.2lf%% " ANSI_GREEN,
                color, i + 1, NUM_SECTIONS, section_names[i], progress_section * 100
            ); // "[1/NUM_SECTIONS] Setup               100.00% "
            uint32_t green_bars = (uint32_t)round(columns * progress_section);
            for (uint32_t ii = 0; ii < green_bars; ii++)
                printf("█");
            printf(ANSI_BLUE);
            for (uint32_t ii = 0; ii < columns - green_bars; ii++)
                printf("█");
            printf(
                ANSI_RESET "%2ld:%08.5lf\n", section_time.tv_sec / 60,
                fmod(section_time.tv_sec, 60.0) + section_time.tv_nsec / 1000000000.0
            );

        }

        // Total Progress

        char color[sizeof(ANSI_GREEN)];
        timespec total_time;
        bool break_now = false;
        if (section_times[NUM_SECTIONS].tv_nsec != 0 || section_times[NUM_SECTIONS].tv_sec != 0) {
            memcpy(color, ANSI_GREEN, sizeof(ANSI_GREEN));
            total_time = section_times[NUM_SECTIONS];
            break_now = true;
        } else {
            memcpy(color, ANSI_BLUE, sizeof(ANSI_BLUE));
            total_time = diff_timespecs(start_time, time_now);
        }

        printf("%s      Total Progress      %8.2lf%% " ANSI_GREEN, color, total_progress * 100);
        uint32_t green_bars = (uint32_t)round(columns * total_progress);
        for (uint32_t i = 0; i < green_bars; i++)
            printf("█");
        printf(ANSI_BLUE);
        for (uint32_t i = 0; i < columns - green_bars; i++)
            printf("█");
        printf(
            ANSI_RESET "%2ld:%08.5f\n", total_time.tv_sec / 60,
            fmod(total_time.tv_sec, 60.0) + total_time.tv_nsec / 1000000000.0
        );

        // Check Exit Status

        if (break_now)
            break; // All sections done, exit tracking process

        // Sleep 0.1 Seconds

        struct timespec sleep_time;
        sleep_time.tv_sec = 0;
        sleep_time.tv_nsec = 100000000;
        nanosleep(&sleep_time, NULL);

    }

}

/**
 * Assign sections of the map.
 * 
 * Land and water are randomly assigned based on a dot's distance from the
 * nearest land origin dot.
 * 
 * @param map_resolution
 * @param island_size
 * @param start_index start of worker's piece
 * @param end_index end of worker's piece
 * @param num_origin_dots
 * @param dots
 * @param section_progress
 */
void assign_sections(
    uint32_t map_resolution, uint32_t island_size,
    uint32_t start_index, uint32_t end_index, uint32_t num_origin_dots,
    Dot *dots, uint32_t *land_dots_count_ptr, _Atomic uint32_t *section_progress
) {

    srand(time(NULL) + getpid());

    uint32_t min_dist_sq;
    uint32_t land_dots_count = 0;

    for (uint32_t i = start_index; i < end_index; i++) {
        // Non-water dots are not included

        // Calculate Maximum Distance

        if (i != start_index && dots[i].y == dots[i - 1].y) {
            min_dist_sq = (uint32_t)sqrt(min_dist_sq) + dots[i].x - dots[i - 1].x + 1;
            min_dist_sq *= min_dist_sq;
        } else {
            min_dist_sq = UINT32_MAX;
        }

        // Find Distance to Nearest Origin Dot

        // min and dist are squared, sqrt is not done until later
        const uint32_t coord[2] = {dots[i].x, dots[i].y};
        uint32_t nearest_index;
        nearest_neighbor_recursive(
            dots, num_origin_dots, 0, coord, 0, &min_dist_sq, &nearest_index, 0
        );

        // Calculate Chance

        double dist = sqrt(min_dist_sq) / sqrt(map_resolution);
        double threshold = ((nearest_index % 20) / 19.0 * 1.5 + 0.25) * island_size / 10;

        int chance = (dist <= threshold) ? 9 : 1;

        if (rand() % 10 < chance) {
            dots[i].type = LAND;
            land_dots_count++;
        }

        atomic_fetch_add(&section_progress[2], 1);

    }

    *land_dots_count_ptr = land_dots_count;

}

/**
 * Smooth coastlines.
 * 
 * Smooth coastlines for land and water dots by comparing the nearest
 * `coastline_smoothing` dots of the same and opposite type.
 * 
 * @param coastline_smoothing
 * @param land_start
 * @param land_end
 * @param water_start
 * @param water_end
 * @param dots
 * @param land_tree_dots const can't be applied, but these dots are read-only
 * @param water_tree_dots const can't be applied, but these dots are read-only
 * @param num_land_dots
 * @param num_water_dots
 * @param section_progress
 */
void smooth_coastlines(
    uint32_t coastline_smoothing,
    uint32_t land_start, uint32_t land_end, uint32_t water_start, uint32_t water_end,
    Dot *dots, Dot* land_tree_dots, Dot *water_tree_dots,
    uint32_t num_land_dots, uint32_t num_water_dots,
    uint32_t *swapped_to_land_counts_ptr, uint32_t *swapped_to_water_counts_ptr,
    _Atomic uint32_t *section_progress
) {

    uint32_t dists_same_sq[COASTLINE_SMOOTHING_MAX];
    uint32_t dists_opp_sq[COASTLINE_SMOOTHING_MAX];
    // Avoiding VLAs

    uint32_t swapped_to_land_counts = 0;
    uint32_t swapped_to_water_counts = 0;

    // Setup for Land

    uint32_t start = land_start;
    Dot *same_tree_dots = land_tree_dots;
    Dot *opp_tree_dots = water_tree_dots;
    uint32_t num_same_dots = num_land_dots;
    uint32_t num_opp_dots = num_water_dots;
    DotType opp_type = WATER;

    // Coastline Smoothing

    for (uint32_t i = land_start; i < water_end; i++) {

        if (i == land_end) {

            // Setup for Water

            i = water_start;
            start = water_start;
            same_tree_dots = water_tree_dots;
            opp_tree_dots = land_tree_dots;
            num_same_dots = num_water_dots;
            num_opp_dots = num_land_dots;
            opp_type = LAND;

        }

        // Calculate Maximum Distances

        const bool same_y = (i != start && dots[i].y == dots[i - 1].y);
        if (same_y) {
            const uint32_t prev_dot_dist = dots[i].x - dots[i - 1].x + 1;
            uint32_t min_dist_same_sq =
                (uint32_t)sqrt(dists_same_sq[coastline_smoothing - 1]) + prev_dot_dist;
            min_dist_same_sq *= min_dist_same_sq;
            uint32_t min_dist_opp_sq =
                (uint32_t)sqrt(dists_opp_sq[coastline_smoothing - 1]) + prev_dot_dist;
            min_dist_opp_sq *= min_dist_opp_sq;
            for (uint32_t ii = 0; ii < coastline_smoothing; ii++) {
                dists_same_sq[ii] = min_dist_same_sq;
                dists_opp_sq[ii] = min_dist_opp_sq;
            }
        } else {
            for (uint32_t ii = 0; ii < coastline_smoothing; ii++)
                dists_same_sq[ii] = UINT32_MAX;
        }

        // Get Nearest Distances for Each Type

        uint32_t dot_coord[2] = {dots[i].x, dots[i].y};

        uint64_t sum_same = 0;
        uint64_t sum_opp = 0;

        nearest_neighbors_recursive(
            same_tree_dots, num_same_dots, 0, dot_coord, dists_same_sq, coastline_smoothing, 0
        );
        for (uint32_t ii = 0; ii < coastline_smoothing; ii++)
            sum_same += dists_same_sq[ii];

        if (!same_y) {
            const uint32_t max = (sum_same < UINT32_MAX) ? sum_same : UINT32_MAX;
            for (uint32_t ii = 0; ii < coastline_smoothing; ii++)
                dists_opp_sq[ii] = max;
        }

        nearest_neighbors_recursive(
            opp_tree_dots, num_opp_dots, 0, dot_coord, dists_opp_sq, coastline_smoothing, 0
        );
        for (uint32_t ii = 0; ii < coastline_smoothing; ii++)
            sum_opp += dists_opp_sq[ii];

        // Change Dot Type if Closer to Opposite Type

        if (sum_same > sum_opp) {
            dots[i].type = opp_type;
            if (opp_type == LAND)
                swapped_to_land_counts++;
            else
                swapped_to_water_counts++;
        }

        atomic_fetch_add(&section_progress[3], 1);

    }

    *swapped_to_land_counts_ptr = swapped_to_land_counts;
    *swapped_to_water_counts_ptr = swapped_to_water_counts;

}

/**
 * Generate water biomes.
 * 
 * Generate water biomes for `dots` from `start_index` to `end_index`. Water
 * biomes are generated based on distance to the equator and to land.
 * 
 * @param start_index
 * @param end_index
 * @param land_tree_dots
 * @param num_land_dots
 * @param height
 * @param dots
 * @param section_progress
 */
void generate_biomes_water(
    uint32_t start_index, uint32_t end_index, const Dot *land_tree_dots, uint32_t num_land_dots,
    uint32_t height, Dot *dots, _Atomic uint32_t *section_progress
) {

    uint32_t land_dist_sq;

    for (uint32_t i = start_index; i < end_index; i++) {

        // Calculate Distance to Equator

        const uint32_t equator_dist =
            (uint32_t)(fabs((double)dots[i].y - height / 2.0) / height * 20);

        // Calculate Maximum Land Distance

        if (i != start_index && dots[i].y == dots[i - 1].y) {
            land_dist_sq = (uint32_t)sqrt(land_dist_sq) + dots[i].x - dots[i - 1].x;
            land_dist_sq *= land_dist_sq;
        } else {
            land_dist_sq = UINT32_MAX;
        }

        // Calculate Distance to Land

        const uint32_t coord[2] = {dots[i].x, dots[i].y};
        nearest_neighbor_recursive(
            land_tree_dots, num_land_dots, 0, coord, 0, &land_dist_sq, NULL, 225
        );

        // Set Water Biome

        if (
            (land_dist_sq < 1225 && equator_dist >= 9) ||
            (land_dist_sq < 625 && equator_dist >= 8) ||
            (land_dist_sq < 225 && equator_dist >= 7)
        )
            dots[i].type = ICE;
        else if (land_dist_sq < 324)
            dots[i].type = SHALLOW_WATER;
        else if (land_dist_sq >= 1225)
            dots[i].type = DEEP_WATER;

        atomic_fetch_add(&section_progress[4], 1);

    }

}

/**
 * Generate land biomes.
 * 
 * Generate land biomes from `dots` from `start_index` to `end_index`. Land
 * biomes are generated based on the nearest biome origin dot.
 * 
 * @param start_index
 * @param end_index
 * @param biome_tree_dots
 * @param num_biome_dots
 * @param dots
 * @param section_progress
 */
void generate_biomes_land(
    uint32_t start_index, uint32_t end_index, const Dot *biome_tree_dots, uint32_t num_biome_dots,
    Dot *dots, _Atomic uint32_t *section_progress
) {

    uint32_t min_dist_sq;

    for (uint32_t i = start_index; i < end_index; i++) {

        if (dots[i].type != LAND)
            continue; // Skip biome origin dots

        // Calculate Minimum Distance

        if (i != start_index && dots[i].y == dots[i - 1].y) {
            min_dist_sq = (uint32_t)sqrt(min_dist_sq) + 1 + dots[i].x - dots[i - 1].x;
            min_dist_sq *= min_dist_sq;
        } else {
            min_dist_sq = UINT32_MAX;
        }

        // Find Nearest Biome Origin Dot

        uint32_t coord[2] = {dots[i].x, dots[i].y};
        uint32_t nearest_index;
        nearest_neighbor_recursive(
            biome_tree_dots, num_biome_dots, 0, coord, 0, &min_dist_sq, &nearest_index, 0
        );

        // Set Dot Type

        dots[i].type = biome_tree_dots[nearest_index].type;

        atomic_fetch_add(&section_progress[4], 1);

    }

}

/**
 * Generate a section of `pixels`.
 * 
 * Pixel color chosen based on the type of the nearest dot from `tree_dots`.
 * 
 * @param start_height
 * @param end_height
 * @param tree_dots
 * @param num_dots
 * @param pixels
 * @param type_counts
 * @param section_progress
 */
void generate_image(
    uint32_t start_height, uint32_t end_height, const Dot *tree_dots, uint32_t num_dots,
    png_byte *pixels, uint32_t *type_counts, _Atomic uint32_t *section_progress
) {

    const png_byte dottype_colors[NUM_DOTTYPES_IMAGE][3] = {
        {153, 221, 255}, {0, 0, 255}, {0, 0, 179}, {0, 0, 128},
        //    Ice        Shallow Water    Water     Deep Water
        {128, 128, 128}, {255, 185, 109}, {0, 77, 0}, {0, 128, 0},
        //   Rock            Desert         Jungle       Forest
        {0, 179, 0}, {152, 251, 152}, {245, 245, 245}
        // Plains         Taiga             Snow
    };
    uint32_t local_type_counts[NUM_DOTTYPES_IMAGE] = {0};

    // Generate Image

    for (uint32_t y = start_height; y < end_height; y++) {

        uint32_t min_dist_sq = UINT32_MAX;

        for (uint32_t x = 0; x < width; x++) {

            if (x != 0) {
                min_dist_sq = (uint32_t)sqrt(min_dist_sq) + 2;
                min_dist_sq *= min_dist_sq;
            }

            // Find Nearest Dot

            const uint32_t coord[2] = {x, y};
            uint32_t nearest_index = 0;
            nearest_neighbor_recursive(
                tree_dots, num_dots, 0, coord, 0, &min_dist_sq, &nearest_index, 0
            );

            // Add to Pixels

            png_byte color[3];
            memcpy(color, dottype_colors[tree_dots[nearest_index].type], sizeof(png_byte) * 3);
            int32_t shift = nearest_index % 21 - 10;
            for (uint32_t i = 0; i < 3; i++) {
                int32_t byte = color[i];
                byte += shift;
                if (byte > 255) byte = 255;
                else if (byte < 0) byte = 0;
                color[i] = (png_byte)byte;
            }
            memcpy(&pixels[3 * (y * width + x)], color, sizeof(png_byte) * 3);

            // Update Local Type Counts

            local_type_counts[tree_dots[nearest_index].type]++;

        }

        atomic_fetch_add(&section_progress[5], 1);

    }

    // Update Shared Type Counts

    for (int i = 0; i < 11; i++)
        type_counts[i] += local_type_counts[i];

}


// Main Function

/**
 * Main Function. ARGC and ARGV used for automated inputs mode.
 */
int main(int argc, char *argv[]) {

    // Set Main Process Title

    set_process_title("main", -1);

    // Get Inputs

    bool auto_mode = (argc >= 10);
    char output_file[229];
    // width is global
    uint32_t height;
    uint32_t map_resolution; // controls the number of dots
    uint32_t island_abundance; // controls the number of "Land Origin" and "Water Forced" dots
    uint32_t island_size;
    uint32_t coastline_smoothing;
    uint32_t processes;
    uint32_t biome_size;

    // Get Inputs

    if (!auto_mode) {

        // Manual Inputs Mode

        strncpy(output_file, "result.png", 10);
        output_file[10] = '\0';

        // Get Program Version

        char file_line[29];
        FILE *fptr = fopen(PATH_README, "r");
        fgets(file_line, 29, fptr);
        fgets(file_line, 29, fptr);
        fgets(file_line, 29, fptr);
        fclose(fptr);

        char version[12];
        strncpy(version, file_line + 12, 11);
        version[11] = '\0';

        // Copyright, license notice, etc.
        system("clear");
        printf(
            "Welcome to BiomeGen v%s\n"
            "Copyright (C) 2025 Liam Ralph\n"
            "https://github.com/liam-ralph\n"
            "This project is licensed under the GNU General Public License v3.0,\n"
            "except for result.png, this program's output, licensed under The Unlicense.\n"
            "Press " ANSI_BLUE "ENTER" ANSI_RESET " to begin.\n", version
        );
        getchar();
        system("clear");

        width = get_uint("Map Width", "", DIM_MIN, DIM_MAX, UINT32_MAX, "");
        // if this max is raised multiple types need to be changed, even if the max < 2^16

        height = get_uint("Map Height", "", DIM_MIN, DIM_MAX, UINT32_MAX, "");

        map_resolution = get_uint(
            "Map Resolution", "Affects number of polygons the map is divided into",
            MAP_RESOLUTION_MIN, MAP_RESOLUTION_MAX, MAP_RESOLUTION_DEFAULT,
            "Lower resolution, larger polygons"
        );

        island_abundance = get_uint(
            "Island Abundance", "Affects how many islands there are",
            ISLAND_ABUNDANCE_MIN, ISLAND_ABUNDANCE_MAX, ISLAND_ABUNDANCE_DEFAULT,
            "Fewer islands, less land overall"
        );


        island_size = get_uint(
            "Island Size", "Affects average island size",
            ISLAND_SIZE_MIN, ISLAND_SIZE_MAX, ISLAND_SIZE_DEFAULT,
            "Larger islands, more land overall"
        );


        coastline_smoothing = get_uint(
            "Coastline Smoothing", "Controls how smooth coastlines look",
            COASTLINE_SMOOTHING_MIN, COASTLINE_SMOOTHING_MAX, COASTLINE_SMOOTHING_DEFAULT,
            "More smoothing"
        );

        biome_size = get_uint(
            "Biome Size", "Affects average biome size",
            BIOME_SIZE_MIN, BIOME_SIZE_MAX, BIOME_SIZE_DEFAULT,
            "Larger biomes, fewer biomes"
        );

        #ifdef __linux__
            int max_processors = (int)sysconf(_SC_NPROCESSORS_ONLN);
        #elif BSD || __Apple__
            int max_processors;
            sysctlbyname("hw.ncpu", &ncpu, &sizeof(int), NULL, 0);
        #endif
        if (max_processors > 9999) max_processors = 9999;
        processes = get_uint(
            "Processors Used", "Number of CPU threads used in generation",
            1, max_processors, UINT32_MAX,
            "Faster generation, increased CPU load, higher CPU temperatures"
        );

        system("clear");

    } else {

        // Automated Inputs Mode

        width = atoi(argv[1]);
        height = atoi(argv[2]);
        map_resolution = atoi(argv[3]);
        island_abundance = atoi(argv[4]);
        island_size = atoi(argv[5]);
        coastline_smoothing = atoi(argv[6]);
        biome_size = atoi(argv[7]);
        processes = atoi(argv[8]);
        strncpy(output_file, argv[9], 228);
        output_file[228] = '\0';

    }

    timespec start_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    // --Setup--

    // Shared Memory

    _Atomic uint32_t *section_progress = mmap(
        NULL, sizeof(uint32_t) * NUM_SECTIONS,
        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0
    );

    uint32_t *section_progress_total = mmap(
        NULL, sizeof(uint32_t) * NUM_SECTIONS,
        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0
    );

    timespec *section_times = mmap(
        NULL, sizeof(timespec) * NUM_SECTIONS + 1,
        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0
    );

    for (uint32_t i = 0; i < NUM_SECTIONS; i++) {
        atomic_init(&section_progress[i], 0);
        section_progress_total[i] = 1;
        section_times[i] = (timespec){0, 0};
    }
    section_times[NUM_SECTIONS] = (timespec){0, 0};

    const uint32_t num_dots = width * height / map_resolution;

    Dot *dots = mmap(
        NULL, sizeof(Dot) * num_dots, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0
    );

    pid_t tracker_process_pid = -1;

    if (!auto_mode) {

        // Progress Tracking

        tracker_process_pid = fork();

        if (tracker_process_pid == 0) {
            set_process_title("tracker", -1);
            track_progress(start_time, section_progress, section_progress_total, section_times);
            exit(0);
        }

    }

    // Set Section Completion Time

    timespec time_now;
    clock_gettime(CLOCK_MONOTONIC, &time_now);
    section_times[0] = diff_timespecs(start_time, time_now);
    atomic_store(&section_progress[0], 1);

    // --Section Generation--
    // Create the initial list of dots

    section_progress_total[1] = num_dots;

    srand(time(NULL));

    const uint32_t num_special_dots = num_dots / island_abundance * 2;
    const uint32_t num_reg_dots = num_dots - num_special_dots;
    const uint32_t num_origin_dots = num_special_dots / 2;
    // Assumed in biome generation that num_water_forced_dots = num_origin_dots

    bool *used_coords = calloc(width * height, sizeof(bool));

    for (uint32_t i = 0; i < num_dots; i++) {

        // Find Unused Coordinate

        uint32_t ii;
        do {
            ii = rand() % (width * height);
        } while (used_coords[ii]);

        // Create Dot

        used_coords[ii] = true;

        dots[i] = (Dot){ .x = ii % width, .y = ii / width };
        if (i < num_origin_dots) {
            dots[i].type = LAND_ORIGIN; // origin points for islands
        } else if (i < num_special_dots) {
            dots[i].type = WATER_FORCED; // good for making lakes
        } else {
            dots[i].type = WATER; // default
        }

        atomic_fetch_add(&section_progress[1], 1);

    }

    free(used_coords);

    // Set Section Completion Time

    clock_gettime(CLOCK_MONOTONIC, &time_now);
    section_times[1] = calc_section_timespec(time_now, start_time, section_times, 1);

    // --Section Assignment--
    // Assign dots as "Land", "Land Origin", "Water", or "Water Forced"

    section_progress_total[2] = num_reg_dots;

    // Create Land Origin KD Tree

    Dot *tree_dots_temp = malloc(sizeof(Dot) * num_origin_dots);
    build_kd_tree(tree_dots_temp, dots, 0, num_origin_dots - 1, num_origin_dots);
    memcpy(dots, tree_dots_temp, sizeof(Dot) * num_origin_dots);
    free(tree_dots_temp);

    // Sort Regular Dots

    quicksort_recursive(dots, num_special_dots, num_dots - 1);

    // Create Regular Dot Piece Starts

    uint32_t reg_piece_length = num_reg_dots / processes;
    uint32_t reg_piece_starts[processes + 1];
    for (uint32_t i = 0; i < processes; i++)
        reg_piece_starts[i] = num_special_dots + i * reg_piece_length;
    reg_piece_starts[processes] = num_dots;

    // Land Dots Counter
    // Used to optimize the start of coastline smoothing

    uint32_t *land_dots_counts = mmap(
        NULL, sizeof(uint32_t) * processes,
        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0
    );

    // Run Workers

    pid_t fork_pids[processes]; // Create forks list
    for (uint32_t i = 0; i < processes; i++) {

        fork_pids[i] = fork(); // Create fork
        if (fork_pids[i] != 0)
            continue;

        set_process_title("sa-worker", i);
        // e.g. bg-worker000
        assign_sections(
            map_resolution, island_size, reg_piece_starts[i], reg_piece_starts[i + 1],
            num_origin_dots, dots, &land_dots_counts[i], section_progress
        );
        exit(0);

    }
    for (uint32_t i = 0; i < processes; i++)
        waitpid(fork_pids[i], NULL, 0); // Wait for workers

    // Set Section Completion Time

    clock_gettime(CLOCK_MONOTONIC, &time_now);
    section_times[2] = calc_section_timespec(time_now, start_time, section_times, 2);

    // --Coastline Smoothing--

    // Swapped Dots Counter
    // Used to optimize start of biome generation

    uint32_t *swapped_to_land_counts = mmap(
        NULL, sizeof(uint32_t) * processes,
        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0
    );
    uint32_t *swapped_to_water_counts = mmap(
        NULL, sizeof(uint32_t) * processes,
        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0
    );

    // Count Land and Water Dots

    uint32_t num_land_dots = 0;
    for (uint32_t i = 0; i < processes; i++)
        num_land_dots += land_dots_counts[i];
    munmap(land_dots_counts, sizeof(uint32_t) * processes);
    uint32_t num_water_dots = num_reg_dots - num_land_dots;

    // Coastline Smoothing

    if (coastline_smoothing != 0) {

        section_progress_total[3] = num_reg_dots;

        // Create Land and Water Dots

        Dot *land_dots = malloc(sizeof(Dot) * num_land_dots);
        Dot *water_dots = malloc(sizeof(Dot) * num_water_dots);
        uint32_t land_counter = 0;
        uint32_t water_counter = 0;

        for (uint32_t i = num_special_dots; i < num_dots; i++) {
            // Only includes "Land" and "Water" dots
            if (dots[i].type == LAND) 
                land_dots[land_counter++] = dots[i];
            else
                water_dots[water_counter++] = dots[i];
        }

        memcpy(&dots[num_special_dots], land_dots, sizeof(Dot) * num_land_dots);
        memcpy(&dots[num_special_dots + num_land_dots], water_dots, sizeof(Dot) * num_water_dots);

        // Build Trees

        Dot *land_tree_dots = malloc(sizeof(Dot) * num_land_dots);
        build_kd_tree(land_tree_dots, land_dots, 0, num_land_dots - 1, num_land_dots);
        free(land_dots);

        Dot *water_tree_dots = malloc(sizeof(Dot) * num_water_dots);
        build_kd_tree(water_tree_dots, water_dots, 0, num_water_dots - 1, num_water_dots);
        free(water_dots);

        // Create Piece Starts

        uint32_t land_piece_length = num_land_dots / processes;
        uint32_t land_piece_starts[processes + 1];
        for (uint32_t i = 0; i < processes; i++)
            land_piece_starts[i] = num_special_dots + i * land_piece_length;
        land_piece_starts[processes] = num_special_dots + num_land_dots;

        uint32_t water_piece_length = num_water_dots / processes;
        uint32_t water_piece_starts[processes + 1];
        for (uint32_t i = 0; i < processes; i++)
            water_piece_starts[i] = num_special_dots + num_land_dots + i * water_piece_length;
        water_piece_starts[processes] = num_dots;

        // Run Workers

        for (uint32_t i = 0; i < processes; i++) {

            fork_pids[i] = fork();
            if (fork_pids[i] != 0)
                continue;

            set_process_title("cs-worker", i);
            smooth_coastlines(
                coastline_smoothing,
                land_piece_starts[i], land_piece_starts[i + 1],
                water_piece_starts[i], water_piece_starts[i + 1],
                dots, land_tree_dots, water_tree_dots, num_land_dots, num_water_dots,
                &swapped_to_land_counts[i], &swapped_to_water_counts[i], section_progress
            );
            exit(0);

        }
        for (uint32_t i = 0; i < processes; i++)
            waitpid(fork_pids[i], NULL, 0);

        // Free Tree Dots

        free(land_tree_dots);
        free(water_tree_dots);

    } else {

        // Coastline smoothing would have no effect
        atomic_store(&section_progress[3], 1);

    }

    // Set Section Completion Time

    clock_gettime(CLOCK_MONOTONIC, &time_now);
    section_times[3] = calc_section_timespec(time_now, start_time, section_times, 3);

    // --Biome Generation--

    section_progress_total[4] = num_dots;

    // Remove "Land Origin" and "Water Forced" Dots

    for (uint32_t i = 0; i < num_origin_dots; i++)
        dots[i].type = LAND;
    for (uint32_t i = num_origin_dots; i < num_special_dots; i++)
        dots[i].type = WATER;

    // Calculate Number of Land and Water Dots

    const uint32_t former_num_land_dots = num_land_dots;
    const uint32_t former_num_water_dots = num_water_dots;

    uint32_t num_swapped_to_land = 0;
    uint32_t num_swapped_to_water = 0;
    for (uint32_t i = 0; i < processes; i++) {
        num_swapped_to_land += swapped_to_land_counts[i];
        num_swapped_to_water += swapped_to_water_counts[i];
    }

    int32_t num_swapped_net = (int32_t)num_swapped_to_land - num_swapped_to_water;
    munmap(swapped_to_land_counts, sizeof(uint32_t) * processes);
    munmap(swapped_to_water_counts, sizeof(uint32_t) * processes);
    num_land_dots += num_origin_dots + num_swapped_net;
    num_water_dots += num_origin_dots - num_swapped_net;
    // num_origin_dots = num_water_forced_dots (assumed below as well)

    // Allocate Memory

    Dot *land_dots_unsorted = malloc(sizeof(Dot) * num_land_dots);
    Dot *water_dots_unsorted = malloc(sizeof(Dot) * num_water_dots);

    // Split Land and Water Dots from Dots

    memcpy(land_dots_unsorted, dots, sizeof(Dot) * num_origin_dots);
    memcpy(water_dots_unsorted, &dots[num_origin_dots], sizeof(Dot) * num_origin_dots);

    uint32_t land_counter = num_origin_dots;
    uint32_t water_counter = num_origin_dots;
    for (uint32_t i = num_special_dots; i < num_dots; i++) {
        if (dots[i].type == LAND)
            land_dots_unsorted[land_counter++] = dots[i];
        else
            water_dots_unsorted[water_counter++] = dots[i];
    }

    // Sort Former Land Origin and Water Forced Dots

    quicksort_recursive(land_dots_unsorted, 0, num_origin_dots - 1);
    quicksort_recursive(water_dots_unsorted, 0, num_origin_dots - 1);

    // Merge Parts of Unsorted Dots into Sorted Dots

    // Section 1 = former land origin dots
    // Section 2 = former land dots, excluded swapped to water
    // Section 3 = former water dots swapped to land
    uint32_t len1 = num_origin_dots;
    uint32_t len2 = former_num_land_dots - num_swapped_to_water;
    merge(land_dots_unsorted, dots, len1, len2);
    merge(land_dots_unsorted, dots, len1 + len2, num_swapped_to_land);

    len2 = former_num_water_dots - num_swapped_to_land;
    merge(water_dots_unsorted, &dots[num_land_dots], len1, len2);
    merge(water_dots_unsorted, &dots[num_land_dots], len1 + len2, num_swapped_to_water);
    free(water_dots_unsorted);

    // Create Water Biomes
    // Adds ice, depth

    // Build Land Dots KD Tree

    Dot *land_tree_dots = malloc(sizeof(Dot) * num_land_dots);
    build_kd_tree(land_tree_dots, land_dots_unsorted, 0, num_land_dots - 1, num_land_dots);
    free(land_dots_unsorted);

    // Create Water Dots Piece Starts

    uint32_t water_piece_length = num_water_dots / processes;
    uint32_t water_piece_starts[processes + 1];
    for (uint32_t i = 0; i < processes; i++)
        water_piece_starts[i] = num_land_dots + i * water_piece_length;
    water_piece_starts[processes] = num_dots;

    // Run Workers

    for (uint32_t i = 0; i < processes; i++) {

        fork_pids[i] = fork();
        if (fork_pids[i] != 0)
            continue;

        set_process_title("bw-worker", i);
        generate_biomes_water(
            water_piece_starts[i], water_piece_starts[i + 1], land_tree_dots, num_land_dots,
            height, dots, section_progress
        );
        exit(0);

    }
    for (uint32_t i = 0; i < processes; i++)
        waitpid(fork_pids[i], NULL, 0);

    // Free Land Tree Dots

    free(land_tree_dots);

    // Add Biome Origin Dots
    // The area around a biome origin dot will have the same biome

    // Create Biome Origin Dot Indexes and Tree

    uint32_t num_biome_dots = num_land_dots / biome_size;
    Dot *biome_dots = malloc(sizeof(Dot) * num_biome_dots);
    Dot *biome_tree_dots = malloc(sizeof(Dot) * num_biome_dots);
    bool *used_biome_coords = calloc(num_land_dots, sizeof(bool));

    // Biome Probablities
    DotType probs[10][10] = {
        {ROCK, DESERT, DESERT, DESERT, JUNGLE, JUNGLE, JUNGLE, FOREST, FOREST, PLAINS},
        {ROCK, DESERT, DESERT, DESERT, JUNGLE, JUNGLE, FOREST, FOREST, PLAINS, PLAINS},
        {ROCK, DESERT, DESERT, JUNGLE, FOREST, FOREST, FOREST, PLAINS, PLAINS, PLAINS},
        {ROCK, DESERT, JUNGLE, FOREST, FOREST, FOREST, PLAINS, PLAINS, PLAINS, PLAINS},
        {ROCK, DESERT, FOREST, FOREST, FOREST, FOREST, PLAINS, PLAINS, PLAINS, PLAINS},
        {ROCK, FOREST, FOREST, FOREST, FOREST, FOREST, PLAINS, PLAINS, PLAINS, PLAINS},
        {ROCK, TAIGA, FOREST, FOREST, FOREST, FOREST, FOREST, PLAINS, PLAINS, PLAINS},
        {ROCK, SNOW, SNOW, TAIGA, TAIGA, FOREST, FOREST, FOREST, PLAINS, PLAINS},
        {SNOW, SNOW, SNOW, SNOW, TAIGA, TAIGA, TAIGA, TAIGA, TAIGA, FOREST},
        {SNOW, SNOW, SNOW, SNOW, SNOW, SNOW, SNOW, SNOW, SNOW, SNOW}
    };

    for (uint32_t i = 0; i < num_biome_dots; i++) {

        // Choose Index

        uint32_t index;
        do {
            index = rand() % num_land_dots;
        } while (used_biome_coords[index]);
        used_biome_coords[index] = true;

        // Choose Biome Type

        dots[index].type = probs[
            (dots[index].y == 0 || dots[index].y == height) ?
            9 : (uint32_t)abs((int)((dots[index].y - height / 2.0) / height * 20.0))
            // Equator distance
        ][rand() % 10];

        // Add to Biome Indexes and Dots

        memcpy(&biome_dots[i], &dots[index], sizeof(Dot));

        // Increment Progress

        atomic_fetch_add(&section_progress[4], 1);

    }

    free(used_biome_coords);
    build_kd_tree(biome_tree_dots, biome_dots, 0, num_biome_dots - 1, num_biome_dots);
    free(biome_dots);

    // Create Land Dots Piece Starts

    uint32_t land_piece_length = num_land_dots / processes;
    uint32_t land_piece_starts[processes + 1];
    for (uint32_t i = 0; i < processes; i++)
        land_piece_starts[i] = i * land_piece_length;
    land_piece_starts[processes] = num_land_dots;

    // Run Workers

    for (uint32_t i = 0; i < processes; i++) {

        fork_pids[i] = fork();
        if (fork_pids[i] != 0)
            continue;

        set_process_title("bl-worker", i);
        generate_biomes_land(
            land_piece_starts[i], land_piece_starts[i + 1], biome_tree_dots, num_biome_dots,
            dots, section_progress
        );
        exit(0);

    }
    for (uint32_t i = 0; i < processes; i++)
        waitpid(fork_pids[i], NULL, 0);

    // Free Biome Origin Tree Dots

    free(biome_tree_dots);

    // Set Section Completion Time

    clock_gettime(CLOCK_MONOTONIC, &time_now);
    section_times[4] = calc_section_timespec(time_now, start_time, section_times, 4);

    // --Image Generation--

    section_progress_total[5] = height;

    // Create Type Counts
    // Used by map statistics

    uint32_t *type_counts = mmap(
        NULL, sizeof(uint32_t) * NUM_DOTTYPES_IMAGE,
        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0
    );

    // Create Dots KD Tree

    Dot *tree_dots = malloc(sizeof(Dot) * num_dots);
    build_kd_tree(tree_dots, dots, 0, num_dots - 1, num_dots);
    munmap(dots, sizeof(Dot) * num_dots);

    // Create Pixels

    png_byte *pixels = mmap(
        NULL, sizeof(png_byte) * 3 * width * height,
        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0
    );

    // Create Section Starts

    uint32_t section_height = height / processes;
    uint32_t section_starts[processes + 1];
    for (uint32_t i = 0; i < processes; i++)
        section_starts[i] = i * section_height;
    section_starts[processes] = height;

    // Run Workers

    for (uint32_t i = 0; i < processes; i++) {

        fork_pids[i] = fork();
        if (fork_pids[i] != 0)
            continue;

        set_process_title("ig-worker", i);
        generate_image(
            section_starts[i], section_starts[i + 1], tree_dots, num_dots, pixels,
            type_counts, section_progress
        );
        exit(0);

    }
    for (uint32_t i = 0; i < processes; i++)
        waitpid(fork_pids[i], NULL, 0);

    // Free Tree

    free(tree_dots);

    // Set Section Completion Time

    clock_gettime(CLOCK_MONOTONIC, &time_now);
    section_times[5] = calc_section_timespec(time_now, start_time, section_times, 5);

    // --Finish--

    section_progress_total[6] = 1;

    // Create Image

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info_ptr = png_create_info_struct(png_ptr);

    png_set_IHDR(
        png_ptr, info_ptr, width, height,
        8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT
    );

    // Assign Image Pixels

    png_bytepp row_pointers = png_malloc(png_ptr, sizeof(png_bytep) * height);
    for (uint32_t i = 0; i < height; i++)
        row_pointers[i] = (png_bytep)&pixels[3 * i * width];

    // Write Image to File

    FILE *fptr = fopen(output_file, "w");
    png_init_io(png_ptr, fptr);
    png_set_rows(png_ptr, info_ptr, row_pointers);
    png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

    // Close File and Free Data

    fclose(fptr);

    png_free(png_ptr, row_pointers);

    munmap(pixels, sizeof(png_byte) * 3 * width * height);

    // Set Section Completion Time

    atomic_store(&section_progress[6], 1);

    clock_gettime(CLOCK_MONOTONIC, &time_now);
    section_times[6] = calc_section_timespec(time_now, start_time, section_times, 6);

    section_times[NUM_SECTIONS] = diff_timespecs(start_time, time_now);

    // Wait for Tracker Process

    if (!auto_mode)
        waitpid(tracker_process_pid, NULL, 0);

    // Shared Memory Cleanup

    munmap(section_progress, sizeof(uint32_t) * NUM_SECTIONS);
    munmap(section_progress_total, sizeof(uint32_t) * NUM_SECTIONS);
    munmap(section_times, sizeof(timespec) * NUM_SECTIONS + 1);

    // Completion

    struct timespec end_time;
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    char formatted_time[32];
    timespec completion_time = diff_timespecs(start_time, end_time);
    snprintf(
        formatted_time, 32, "%2ld:%08.5f", completion_time.tv_sec / 60,
            fmod(completion_time.tv_sec, 60.0) + completion_time.tv_nsec / 1000000000.0
    );

    if (!auto_mode) {

        // Print Result Statistics

        printf(
            ANSI_GREEN "Generation Complete" ANSI_RESET " %s\n\nStatistics\n", formatted_time
        );

        uint32_t count_water = 0;
        uint32_t count_land = 0;
        for (uint32_t i = ICE; i <= DEEP_WATER; i++)
            count_water += type_counts[i];
        for (uint32_t i = ROCK; i < NUM_DOTTYPES_IMAGE; i++)
            count_land += type_counts[i];
        double tot_pct_fact = 100.0 / (height * width); // total percentage factor
        printf(
            "Water %6.2f%%\nLand  %6.2f%%\n",
            count_water * tot_pct_fact, count_land * tot_pct_fact
        );

        uint32_t text_colors[NUM_DOTTYPES_IMAGE] = {117, 21, 19, 17, 243, 229, 22, 28, 40, 48, 255};
        char dottype_names[NUM_DOTTYPES_IMAGE][14] = {
            "Ice", "Shallow Water", "Water", "Deep Water",
            "Rock", "Desert", "Jungle", "Forest", "Plains", "Taiga", "Snow"
        };

        printf("       %% of Land/Water | %% of Total\n");
        double group_pct_fact = 100.0 / count_water;
        for (uint32_t i = 0; i < NUM_DOTTYPES_IMAGE; i++) {
            if (i == ROCK) group_pct_fact = 100.0 / count_land;
            printf(
                "\033[48;5;%dm%-14s" ANSI_RESET // Label
                "%7.2f%% | %6.2f%%\n", // Pct of Group | Pct of Total
                text_colors[i], dottype_names[i],
                type_counts[i] * group_pct_fact, type_counts[i] * tot_pct_fact
            );
        }

    } else {

        printf("%lf\n", completion_time.tv_sec + completion_time.tv_nsec / 1000000000.0);

    }

    munmap(type_counts, sizeof(uint32_t) * NUM_DOTTYPES_IMAGE);

    return 0;

}