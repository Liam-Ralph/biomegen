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


// Structs

typedef enum DotType : uint8_t {
    ICE, SHALLOW_WATER, WATER, DEEP_WATER, ROCK, DESERT, JUNGLE, FOREST, PLAINS, TAIGA, SNOW,
    WATER_FORCED, LAND, LAND_ORIGIN
} DotType;

typedef struct Dot {
    uint16_t x;
    uint16_t y;
    DotType type;
} Dot;

typedef struct timespec timespec;


// General Functions

// User Input

/**
 * Get a sanitized integer input from the user between `min` and `max`,
 * both inclusive.
 * 
 * @param min
 * @param max
 * 
 * @return valid user input
 */
int get_int(int min, int max) {

    int result;

    while (true) {

        int scanf_return = scanf("%d", &result);
        while (getchar() != '\n');

        if (scanf_return != 1 || result < min || result > max)
            printf("Input must be an integer between %d and %d (both inclusive).\n", min, max);
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
 * Calculates `time_now - start_time - ∑(prev_sections)`.
 * 
 * @param time_now
 * @param start_time
 * @param prev_sections
 * @param len length of `prev_sections` (or the part thereof to be used)
 */
timespec calc_section_timespec(
    timespec time_now, timespec start_time, const timespec *prev_sections, size_t len
) {
    timespec result = time_now;
    result.tv_sec -= start_time.tv_sec;
    result.tv_nsec -= start_time.tv_nsec;
    for (size_t i = 0; i < len; i++) {
        result.tv_sec -= prev_sections[i].tv_sec;
        result.tv_nsec -= prev_sections[i].tv_nsec;
    }
    while (result.tv_nsec < 0) {
        result.tv_nsec += 1000000000;
        result.tv_sec--;
    }
}

// Sorting Functions

/**
 * Swaps two dots in an array.
 * 
 * @param dots
 * @param i first index to swap with
 * @param ii second index to swap with
 */
void swap(Dot *dots, size_t i, size_t ii) {
    const Dot temp = dots[i];
    dots[i] = dots[ii];
    dots[ii] = temp;
}

/**
 * Sort the given `dots` using the quicksort algorithm.
 * 
 * A dot's value is equal to `dot.y * width + dot.x`.
 * 
 * @param dots array to sort
 * @param low
 * @param high
 * @param width needed to calculate a dot's value
 */
void quicksort_recursive(Dot *dots, size_t low, size_t high, uint32_t width) {

    if (low < high) {

        // Calculate Pivot Value

        uint32_t pivot = dots[high].y * width + dots[high].x;

        // Move Smaller Elements to the Left

        size_t p = low;

        for (size_t i = low; i < high; i++)
            if (dots[i].y * width + dots[i].x < pivot)
                swap(dots, p++, i);

        // Move Pivot to After Smaller Elements

        swap(dots, p, high);

        // Recursion

        if (p > low)
            quicksort_recursive(dots, low, p - 1, width);
        quicksort_recursive(dots, p + 1, high, width);

    }

}


// KDTree Functions

/**
 * Sort the given `dots` until the median index is correctly sorted.
 * 
 * After completion, the median index will be in the correct place, everything
 * to the left will be lower or equal, and everything right will be greater or
 * equal.
 * 
 * @param dots array to sort
 * @param low
 * @param high
 * @param med_index
 * @param axis determines whether to sort by x (false) or y (true) values
 */
void median_select(Dot *dots, size_t low, size_t high, size_t med_index, bool axis) {

    while (low < high) {

        // Calculate Pivot Value

        size_t pi = low + rand() % (high - low);
        uint16_t pivot = axis ? dots[pi].y : dots[pi].x;

        // Sort Based on Pivot

        size_t lt = low;
        size_t i = low;
        size_t gt = high;

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

        if (med_index < lt)
            high = lt - 1;
        else if (med_index > gt)
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
void build_recursive(Dot *dots, Dot *tree_dots, size_t index, size_t low, size_t high, int depth) {

    // Base Cases

    if (low > high) return;
    if (low == high) {
        tree_dots[index] = dots[low];
        return;
    }

    // Median Sort Dots

    const size_t med_pos = (high + low) >> 1;

    median_select(dots, low, high, med_pos, depth % 2);
    // it seems this is faster than storing axis and doing !axis in each recursive call

    // Add Median Node to Tree
    // Every recursion adds one node

    tree_dots[index] = dots[med_pos];

    // Recursion

    const size_t left_index = index * 2 + 1;
    depth++;
    build_recursive(dots, tree_dots, left_index, low, med_pos - 1, depth);
    build_recursive(dots, tree_dots, left_index + 1, med_pos + 1, high, depth);

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
    Dot *tree_dots, size_t len, size_t index, const uint32_t coord[2], uint32_t depth,
    uint32_t *min_dist_sq_ptr, size_t *nearest_index_ptr, uint32_t min_dist_sq_care
) {

    // Calculate Distance

    const int32_t diff_x = coord[0] - tree_dots[index].x;
    const int32_t diff_y = coord[1] - tree_dots[index].y;
    const uint32_t dist_sq = diff_x * diff_x + diff_y * diff_y;
    // Distance is squared for efficiency

    // Update Minimum Distance and Index Pointers

    if (dist_sq < *min_dist_sq_ptr) {
        *min_dist_sq_ptr = dist_sq;
        if (nearest_index_ptr != NULL) {
            *nearest_index_ptr = index;
        } else if (dist_sq < min_dist_sq_care) {
            return;
        }
    }

    // Decide Whether Recursion is Needed

    const bool axis = depth % 2;
    const uint32_t dot_value = (axis ? tree_dots[index].y : tree_dots[index].x);
    const int32_t dist_line = dot_value - coord[axis];
    const uint32_t dist_line_sq = dist_line * dist_line;
    // Whether distance to splitting line is less than current max dist

    size_t index_child = 2 * index + 1;
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
 * must be {0...} on initial call
 * @param dists_sq_len number of neighbors to track, must be greater than 0
 * @param depth current depth, must be 0 in initial call
 */
void nearest_neighbors_recursive(
    Dot *tree_dots, size_t len, size_t index, const uint32_t coord[2],
    uint32_t *dists_sq, size_t dists_sq_len, uint32_t depth
) {

    // Calculate Distance

    const int32_t diff_x = tree_dots[index].x - coord[0];
    const int32_t diff_y = tree_dots[index].y - coord[1];
    const uint32_t dist_sq = diff_x * diff_x + diff_y * diff_y; // Squared distance

    // Update Distances List

    if (dist_sq < dists_sq[dists_sq_len - 1] && dist_sq != 0) {
        size_t i = dists_sq_len - 1;
        while (true) {
            if (i == 0 || dist_sq >= dists_sq[i - 1]) {
                // Found insertion position, insert and break
                dists_sq[i] = dist_sq;
                break;
            }
            // After insertion position, shift element
            dists_sq[i] = dists_sq[--i];
        }
    }

    // Decide Whether Recursion is Needed

    const uint32_t max_dist = dists_sq[dists_sq_len - 1];
    const bool axis = depth % 2;
    const uint32_t dot_value = (axis ? tree_dots[index].y : tree_dots[index].x);
    const int32_t dist_line = dot_value - coord[axis];
    const uint32_t dist_line_sq = dist_line * dist_line;
    // Whether distance to splitting line is less than max_dist

    size_t index_child = 2 * index + 1;
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
void set_process_title(const char type[], const int num) {

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

    char section_names[7][20] = {
        "Setup", "Section Generation", "Section Assignment", "Coastline Smoothing",
        "Biome Generation", "Image Generation", "Finish"
    };
    double section_weights[7] = {0.01, 0.01, 0.03, 0.24, 0.04, 0.53, 0.14};
    // Used for overall progress bar (e.g. Setup takes ~1% of total time)

    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    size_t columns = (size_t)w.ws_col - 47;
    if (100 < columns) columns = 100;

    while (true) {

        // Clear Screen

        system("clear");

        // Section Progress

        timespec time_now;
        clock_gettime(CLOCK_MONOTONIC, &time_now);

        double total_progress = 0.0;

        for (int i = 0; i < 7; i++) {

            // Calculate Section Progress

            double progress_section =
                atomic_load(&section_progress[i]) / (double)section_progress_total[i];
            total_progress += progress_section * section_weights[i];

            // Check if Section Complete

            char color[10];
            timespec section_time;
            if (section_times[i].tv_nsec != 0 || section_times[i].tv_sec != 0) {
                strncpy(color, ANSI_GREEN, 10);
                section_time = section_times[i]; // Section complete
            } else {
                strncpy(color, ANSI_BLUE, 10);
                if (i == 0 || section_times[i - 1].tv_nsec != 0 || section_times[i - 1].tv_sec != 0)
                    section_time = calc_section_timespec(time_now, start_time, section_times, i);
                    // Section in progress
                else
                    section_time = (timespec){0, 0}; // Section hasn't started
            }

            // Print Output for Section Progress

            printf(
                "%s[%d/7] %-20s%8.2lf%% %s",
                color, i + 1, section_names[i], progress_section * 100, ANSI_GREEN
            ); // "[1/7] Setup               100.00% "
            size_t green_bars = (size_t)round(columns * progress_section);
            for (size_t ii = 0; ii < green_bars; ii++)
                printf("█");
            printf("%s", ANSI_BLUE);
            for (size_t ii = 0; ii < columns - green_bars; ii++)
                printf("█");
            printf(
                "%s%2d:%08.5lf\n", ANSI_RESET, section_time.tv_sec / 60,
                section_time.tv_sec + section_time.tv_nsec / 1000000000.0
            );

        }

        // Total Progress

        char color[10];
        timespec total_time;
        bool break_now = false;
        if (section_times[7].tv_nsec != 0 || section_times[7].tv_sec != 0) {
            strncpy(color, ANSI_GREEN, 10);
            total_time = section_times[7];
            break_now = true;
        } else {
            strncpy(color, ANSI_BLUE, 10);
            total_time = diff_timespecs(start_time, time_now);
        }

        printf("%s      Total Progress      %8.2lf%% %s", color, total_progress * 100, ANSI_GREEN);
        size_t green_bars = (size_t)round(columns * total_progress);
        for (size_t i = 0; i < green_bars; i++)
            printf("█");
        printf("%s", ANSI_BLUE);
        for (size_t i = 0; i < columns - green_bars; i++)
            printf("█");
        printf(
            "%s%2d:%08.5f\n", ANSI_RESET, total_time.tv_sec / 60,
            total_time.tv_sec + total_time.tv_nsec / 1000000000.0
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
    size_t start_index, size_t end_index, uint32_t num_origin_dots,
    Dot *dots, uint32_t *land_dots_count_ptr, _Atomic uint32_t *section_progress
) {

    srand(time(NULL) + getpid());

    uint32_t min_dist_sq;
    uint32_t land_dots_count = 0;

    for (int i = start_index; i < end_index; i++) {
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
        uint32_t coord[2] = {dots[i].x, dots[i].y};
        size_t nearest_index;
        nearest_neighbor_recursive(
            dots, num_origin_dots, 0, coord, 0, &min_dist_sq, &nearest_index, 0
        );

        // Calculate Chance

        double dist = sqrt(min_dist_sq) / sqrt(map_resolution);
        double threshold = ((nearest_index % 20) / 19.0 * 1.5 + 0.25) * island_size;

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
 * @param land_tree_dots
 * @param water_tree_dots
 * @param num_land_dots
 * @param num_water_dots
 * @param section_progress
 */
void smooth_coastlines(
    uint32_t coastline_smoothing,
    size_t land_start, size_t land_end, size_t water_start, size_t water_end,
    Dot *dots, const Dot* land_tree_dots, const Dot *water_tree_dots,
    uint32_t num_land_dots, uint32_t num_water_dots, _Atomic uint32_t *section_progress
) {

    uint32_t dists_same_sq[coastline_smoothing];
    uint32_t dists_opp_sq[coastline_smoothing];

    // Setup for Land

    size_t start = land_start;
    Dot *same_tree_dots = land_tree_dots;
    Dot *opp_tree_dots = water_tree_dots;
    uint32_t num_same_dots = num_land_dots;
    uint32_t num_opp_dots = num_water_dots;
    DotType opp_type = WATER;

    // Coastline Smoothing

    for (size_t i = land_start; i < water_end; i++) {

        if (i == land_end) {

            // Setup for Water

            i = water_start;
            start = water_start;
            Dot *same_tree_dots = water_tree_dots;
            Dot *opp_tree_dots = land_tree_dots;
            uint32_t num_same_dots = num_water_dots;
            uint32_t num_opp_dots = num_land_dots;
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
            for (size_t ii = 0; ii < coastline_smoothing; ii++) {
                dists_same_sq[ii] = min_dist_same_sq;
                dists_opp_sq[ii] = min_dist_opp_sq;
            }
        } else {
            for (size_t ii = 0; ii < coastline_smoothing; ii++)
                dists_same_sq[ii] = UINT32_MAX;
        }

        // Get Nearest Distances for Each Type

        uint32_t dot_coord[2] = {dots[i].x, dots[i].y};

        uint64_t sum_same = 0;
        uint64_t sum_opp = 0;

        nearest_neighbors_recursive(
            same_tree_dots, num_same_dots, 0, dot_coord, dists_same_sq, coastline_smoothing, 0
        );
        for (size_t ii = 0; ii < coastline_smoothing; ii++)
            sum_same += dists_same_sq[ii];

        if (!same_y) {
            const uint32_t max = (sum_same < UINT32_MAX) ? sum_same : UINT32_MAX;
            for (size_t ii = 0; ii < coastline_smoothing; ii++)
                dists_opp_sq[ii] = max;
        }

        nearest_neighbors_recursive(
            opp_tree_dots, num_opp_dots, 0, dot_coord, dists_same_sq, coastline_smoothing, 0
        );
        for (size_t ii = 0; ii < coastline_smoothing; ii++)
            sum_opp += dists_opp_sq[ii];

        // Change Dot Type if Closer to Opposite Type

        if (sum_same > sum_opp)
            dots[i].type = opp_type;

        atomic_fetch_add(&section_progress[3], 1);

    }

}

/**
 * Generate water biomes for DOTS between START_INDEX and END_INDEX. Water
 * biomes are generated based on a dot's distance to the equator and the
 * distance to the nearest land dot.
 */
void generate_biomes_water(
    const int start_index, const int end_index, const int *water_dots, Node *land_tree_root,
    const int height, const int num_dots, Dot *dots, _Atomic int *section_progress
) {

    int land_dist;

    for (int i = start_index; i < end_index; i++) {

        // Calculate Distance to Equator

        const double equator_dist =
            fabs((double)water_dots[i * 3 + 1] - height / 2.0) / height * 20.0;

        // Calculate Maximum Land Distance

        if (i != start_index && water_dots[i * 3 + 1] == water_dots[(i - 1) * 3 + 1]) {
            int min_dist_sq =
                (int)sqrt(land_dist) + 1 + water_dots[i * 3] - water_dots[(i - 1) * 3];
            land_dist = min_dist_sq * min_dist_sq;
        } else {
            land_dist = INT_MAX;
        }

        // Calculate Distance to Land

        const int coord[2] = {water_dots[i * 3], water_dots[i * 3 + 1]};
        query_recursive(land_tree_root, coord, 0, NULL, &land_dist);

        // Set Water Biome

        char dot_type = 'W';
        if ( // Remember: all land distances are squared for efficiency
            (land_dist < 35 * 35 && equator_dist > 9) ||
            (land_dist < 25 * 25 && equator_dist > 8) ||
            (land_dist < 15 * 15 && equator_dist > 7)
        ) {
            dot_type = 'I';
        } else if (land_dist < 18 * 18) {
            dot_type = 's';
        } else if (land_dist >= 35 * 35) {
            dot_type = 'd';
        }

        dots[water_dots[i * 3 + 2]].type = dot_type;

        atomic_fetch_add(&section_progress[4], 1);

    }

}

/**
 * Generate land biomes for DOTS between START_INDEX and END_INDEX. Land biomes
 * are generated base on the nearest dot in BIOME_ORIGIN_INDEXES, the list of
 * dots whose biomes are already before this function.
 */
void generate_biomes_land(
    const int start_index, const int end_index, const int *land_dots,
    Node *origin_tree_root, const int biome_origin_indexes[],
    const int num_dots, Dot *dots, _Atomic int *section_progress
) {

    int min_dist;

    for (int i = start_index; i < end_index; i++) {

        // Calculate Maxminum Distance

        if (i != start_index && land_dots[i * 3 + 1] == land_dots[(i - 1) * 3 + 1]) {
            int min_dist_sq = (int)sqrt(min_dist) + 1 + land_dots[i * 3] - land_dots[(i - 1) * 3];
            min_dist = min_dist_sq * min_dist_sq;
        } else {
            min_dist = INT_MAX;
        }

        // Find Nearest Biome Origin Dot

        int origin_index = 0;
        const int coord[2] = {land_dots[i * 3], land_dots[i * 3 + 1]};
        query_recursive(origin_tree_root, coord, 0, &origin_index, &min_dist);

        // Set Dot Type

        dots[land_dots[i * 3 + 2]].type = dots[origin_index].type;

        atomic_fetch_add(&section_progress[4], 1);

    }

}

/**
 * Generate a section of the IMAGE_INDEXES, which contains the index in DOTS of
 * the nearest dot to each pixel. Also count the number of pixels of each type
 * for TYPE_COUNTS, to be used in statistics at the end of the main program.
 */
void generate_image(
    const int start_height, const int end_height, const int width, Node *tree_root,
    const int num_dots, const Dot *dots,
    int *image_indexes, int *type_counts, _Atomic int *section_progress
) {

    // Dot type counts for statistics, not used in image generation
    int local_type_counts[11] = {0};
    const char types[11] = {'I', 's', 'W', 'd', 'R', 'D', 'J', 'F', 'P', 'T', 'S'};

    // Generate Image

    for (int y = start_height; y < end_height; y++) {

        int min_dist = INT_MAX;

        for (int x = 0; x < width; x++) {

            int nearest_index = 0;

            if (min_dist != INT_MAX) {
                const int min_dist_sq = (int)sqrt(min_dist) + 2;
                /*
                max_dist = dist(previous x, previous nearest dot) + dist(previous x, current x)
                         = previous max_dist + 1
                +1 for floating point errors (ceil didn't work)
                */
                min_dist = min_dist_sq * min_dist_sq;
            }

            // Find Nearest Dot

            const int coord[2] = {x, y};
            query_recursive(tree_root, coord, 0, &nearest_index, &min_dist);

            // Add to Image Indexes and Local Type Counts

            image_indexes[y * width + x] = nearest_index;
            for (int i = 0; i < 11; i++) {
                if (dots[nearest_index].type == types[i]) {
                    local_type_counts[i]++;
                    break;
                }
            }

        }

        // Only update for each row of pixels
        atomic_fetch_add(&section_progress[5], 1);

    }

    // Update Shared Type Counts

    for (int i = 0; i < 11; i++) {
        type_counts[i] += local_type_counts[i];
    }

}


// Main Function

/**
 * Main Function. ARGC and ARGV used for automated inputs mode.
 */
int main(int argc, char *argv[]) {

    // Set Main Process Title

    set_process_title("main", -1);

    // Get Inputs

    bool auto_mode = (argc == 1);
    char output_file[229];
    uint32_t width, height, map_resolution, island_abundance, island_size,
        coastline_smoothing, processes;

    if (auto_mode) {

        // Manual Inputs Mode

        strncpy(output_file, "result.png", 11);

        // Get Program Version

        char file_line[29];
        FILE *fptr = fopen(PATH_README, "r");
        fgets(file_line, 29, fptr);
        fgets(file_line, 29, fptr);
        fgets(file_line, 29, fptr);
        fclose(fptr);

        char version[12];
        strncpy(version, file_line + 12, 12);
        version[strlen(version) - 1] = '\0';

        // Copyright, license notice, etc.
        system("clear");
        printf(
            "Welcome to BiomeGen v%s\n"
            "Copyright (C) 2025 Liam Ralph\n"
            "https://github.com/liam-ralph\n"
            "This project is licensed under the GNU General Public License v3.0,\n"
            "except for result.png, this program's output, licensed under The Unlicense.\n"
            "Press ENTER to begin.\n", version
        );
        getchar();
        system("clear");

        printf("Map Width (pixels):\n");
        width = get_int(500, 10000);
        // if this max is raised multiple types need to be changed, even if the max < 2^16

        printf("\nMap Height:\n");
        height = get_int(500, 10000);

        printf(
            "\nMap resolution controls the section size of the map.\n"
            "Choose a number between 50 and 500. 100 is the default.\n"
            "Larger numbers produce lower resolutions, with larger pieces\n"
            "while lower numbers take longer to generate.\n"
            "Map Resolution: "
        );
        map_resolution = get_int(50, 500); // controls the number of dots

        printf(
            "\nIsland abundance control how many islands there are,\n"
            "and the ration of land to water.\n"
            "Choose a number between 10 and 1000. 120 is the default.\n"
            "Larger numbers produces less land.\n"
            "Island Abundance: "
        );
        island_abundance = get_int(10, 1000);
        // controls the number of "Land Origin" and "Water Forced" dots

        printf(
            "\nIsland size controls average island size.\n"
            "Choose a number between 10 and 100. 50 is the default.\n"
            "Larger numbers produce larger islands.\n"
            "Island Size: "
        );
        island_size = get_int(10, 100);

        printf(
            "\nCoastline smoothing controls how smooth or rough coastlines look.\n"
            "Choose a number between 0 and 100. Larger numbers cause more smoothing.\n"
            "A value of 0 causes no smoothing. A value of 5 causes some smoothing,\n"
            "and is the default value.\n"
            "Coastline Smoothing: "
        );
        coastline_smoothing = get_int(0, 100);

        printf(
            "\nNow you must choose how many of your CPU's threads to use for map generation.\n"
            "Values exceeding your CPU's number of threads will slow map generation.\n"
            "The most efficient number of threads to use varies by hardware, OS,\n"
            "and CPU load. Values less than 4 threads are usually very inefficient.\n"
            "Ensure you monitor your CPU for overheating, and halt the program if\n"
            "high temperatures occur. Using fewer threads may reduce temperatures.\n"
            "Number of Threads: "
        );
        #ifdef __linux__
            int max_processors = (int)sysconf(_SC_NPROCESSORS_ONLN);
        #elif BSD || __Apple__
            int max_processors;
            sysctlbyname("hw.ncpu", &ncpu, &sizeof(int), NULL, 0);
        #endif
        if (max_processors > 9999) max_processors = 9999;
        processes = get_int(1, max_processors);

        system("clear");

    } else {

        // Automated Inputs Mode

        auto_mode = true;

        width = atoi(argv[1]);
        height = atoi(argv[2]);
        map_resolution = atoi(argv[3]);
        island_abundance = atoi(argv[4]);
        island_size = atoi(argv[5]);
        coastline_smoothing = atoi(argv[6]);
        processes = atoi(argv[7]);
        strncpy(output_file, argv[8], 229);

    }

    timespec start_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    // --Setup--

    // Shared Memory

    _Atomic uint32_t *section_progress = mmap(
        NULL, sizeof(uint32_t) * 7, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0
    );

    uint32_t *section_progress_total = mmap(
        NULL, sizeof(uint32_t) * 7, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0
    );

    timespec *section_times = mmap(
        NULL, sizeof(timespec) * 8, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0
    );

    for (size_t i = 0; i < 7; i++) {
        atomic_init(&section_progress[i], 0);
        section_progress_total[i] = 1;
        section_times[i] = (timespec){0, 0};
    }
    section_times[7] = (timespec){0, 0};

    const uint32_t num_dots = width * height / map_resolution;

    Dot *dots = mmap(
        NULL, sizeof(Dot) * num_dots, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0
    );

    uint32_t *type_counts = mmap(
        NULL, sizeof(uint32_t) * 11, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0
    );
    for (size_t i = 0; i < 11; i++)
        type_counts[i] = 0;

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

    bool *used_coords = calloc(width * height, sizeof(bool));

    for (int i = 0; i < num_dots; i++) {

        // Find Unused Coordinate

        uint32_t ii;
        do {
            ii = rand() % (width * height);
        } while (used_coords[ii]);

        // Create Dot

        used_coords[ii] = true;

        dots[i] = (Dot){ .x = ii % width, .y = ii / width };
        if (i < num_special_dots / 2) {
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

    // Create Land Origin KDTree

    const uint32_t num_origin_dots = num_special_dots / 2;
    Dot *tree_dots_temp = malloc(num_origin_dots * sizeof(Dot));
    build_recursive(dots, tree_dots_temp, 0, 0, num_origin_dots, 0);
    memcpy(dots, tree_dots_temp, num_origin_dots * sizeof(Dot));
    free(tree_dots_temp);

    // Sort Regular Dots

    quicksort_recursive(dots, num_special_dots, num_dots, width);

    // Create Regular Dot Piece Starts

    size_t reg_piece_length = num_reg_dots / processes;
    size_t reg_piece_starts[processes + 1];
    for (size_t i = 0; i < processes; i++)
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
    for (size_t i = 0; i < processes; i++) {

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
    for (size_t i = 0; i < processes; i++)
        waitpid(fork_pids[i], NULL, 0); // Wait for workers

    // Set Section Completion Time

    clock_gettime(CLOCK_MONOTONIC, &time_now);
    section_times[2] = calc_section_timespec(time_now, start_time, section_times, 2);

    // --Coastline Smoothing--

    if (coastline_smoothing != 0) {

        section_progress_total[3] = num_reg_dots;

        // Create Land and Water Dots

        uint32_t num_land_dots = 0;
        for (size_t i = 0; i < processes; i++)
            num_land_dots += land_dots_counts[i];
        munmap(land_dots_counts, sizeof(uint32_t) * processes);
        uint32_t num_water_dots = num_reg_dots - num_land_dots;

        Dot *land_dots = malloc(num_land_dots * sizeof(Dot));
        Dot *water_dots = malloc(num_water_dots * sizeof(Dot));
        uint32_t land_counter = 0;
        uint32_t water_counter = 0;

        for (size_t i = num_special_dots; i < num_dots; i++) {
            // Only includes "Land" and "Water" dots
            if (dots[i].type == LAND) 
                memcpy(&land_dots[land_counter++], &dots[i], sizeof(Dot));
            else
                memcpy(&water_dots[water_counter++], &dots[i], sizeof(Dot));
        }

        memcpy(&dots[num_special_dots], land_dots, num_land_dots * sizeof(Dot));
        memcpy(&dots[num_special_dots + num_land_dots], water_dots, num_water_dots * sizeof(Dot));

        // Build Trees

        Dot *land_tree_dots = malloc(num_land_dots * sizeof(Dot));
        memcpy(land_tree_dots, land_dots, num_land_dots * sizeof(Dot));
        build_recursive(land_dots, land_tree_dots, 0, 0, num_land_dots, 0);
        free(land_dots);

        Dot *water_tree_dots = malloc(num_water_dots * sizeof(Dot));
        memcpy(water_tree_dots, water_dots, num_water_dots * sizeof(Dot));
        build_recursive(water_dots, water_tree_dots, 0, 0, num_water_dots, 0);
        free(water_dots);

        // Create Piece Starts

        size_t land_piece_length = num_land_dots / processes;
        size_t land_piece_starts[processes + 1];
        for (size_t i = 0; i < processes; i++) {
            land_piece_starts[i] = num_special_dots + i * land_piece_length;
        }
        land_piece_starts[processes] = num_dots;

        size_t water_piece_length = num_water_dots / processes;
        size_t water_piece_starts[processes + 1];
        for (size_t i = 0; i < processes; i++) {
            water_piece_starts[i] = num_special_dots + i * water_piece_length;
        }
        water_piece_starts[processes] = num_dots;

        // Run Workers

        for (int i = 0; i < processes; i++) {

            fork_pids[i] = fork();
            if (fork_pids[i] != 0)
                continue;

            set_process_title("cl-worker", i);
            smooth_coastlines(
                coastline_smoothing,
                land_piece_starts[i], land_piece_starts[i + 1],
                water_piece_starts[i], water_piece_starts[i + 1],
                dots, land_tree_dots, water_dots, num_land_dots, num_water_dots, section_progress
            );
            exit(0);

        }
        for (int i = 0; i < processes; i++)
            waitpid(fork_pids[i], NULL, 0);

        free(land_tree_dots);
        free(water_tree_dots);

    } else {

        // Coastline smoothing would have no effect
        munmap(land_dots_counts, sizeof(uint32_t) * processes);
        atomic_store(&section_progress[3], 1);

    }

    // Set Section Completion Time

    clock_gettime(CLOCK_MONOTONIC, &time_now);
    section_times[3] = calc_section_timespec(time_now, start_time, section_times, 3);

    // --Biome Generation--

    section_progress_total[4] = num_dots;

    // Remove "Land Origin" and "Water Forced" Dots

    for (int i = 0; i < num_special_dots; i++) {
        Dot *dot = &dots[i];
        if (dot->type == 'l') {
            dot->type = 'L';
        } else if (dot->type == 'w') {
            dot->type = 'W';
        }
    }

    // Create Water Biomes
    // Adds ice, depth

    // Build Land Dots KDTree

    int num_land_dots = 0;
    int num_water_dots = 0;
    int *land_dots = malloc(num_dots * 3 * sizeof(int));
    int *water_dots = malloc(num_dots * 3 * sizeof(int));

    for (int i = 0; i < num_dots; i++) {
        const Dot *dot = &dots[i];
        if (dots[i].type == 'L') {
            land_dots[num_land_dots * 3] = dot->x;
            land_dots[num_land_dots * 3 + 1] = dot->y;
            land_dots[num_land_dots * 3 + 2] = i;
            num_land_dots++;
        } else {
            water_dots[num_water_dots * 3] = dot->x;
            water_dots[num_water_dots * 3 + 1] = dot->y;
            water_dots[num_water_dots * 3 + 2] = i;
            num_water_dots++;
        }
    }

    Node *land_tree_root = NULL;
    land_tree_root = build_recursive(land_dots, num_land_dots, 0);
    free(land_dots);

    // Sort Water Dots

    quicksort_recursive(water_dots, 0, num_water_dots - 1, width);

    // Create Piece Starts Water Dots

    int water_piece_length = num_water_dots / processes;
    int water_piece_starts[processes + 1];
    for (int i = 0; i < processes; i++) {
        water_piece_starts[i] = i * water_piece_length;
    }
    water_piece_starts[processes] = num_water_dots;

    // Run Workers

    for (int i = 0; i < processes; i++) {

        fork_pids[i] = fork();
        if (fork_pids[i] != 0) {
            continue;
        }

        set_process_title("worker", i);
        generate_biomes_water(
            water_piece_starts[i], water_piece_starts[i + 1], water_dots, land_tree_root,
            height, num_dots, dots, section_progress
        );
        exit(0);

    }
    for (int i = 0; i < processes; i++) {
        waitpid(fork_pids[i], NULL, 0);
    }

    // Free Water Dots and Land Tree

    free(water_dots);
    free_recursive(land_tree_root);

    // Add Biome Origin Dots
    // The area around a biome origin dot will have the same biome

    int biome_origin_indexes[num_dots / 10];

    int ii = 0;
    for (int i = 0; i < num_dots / 10; i++) {

        // Biome origin dot must be land
        while (dots[ii].type != 'L') {
            ii++;
        }
        biome_origin_indexes[i] = ii;

        Dot *dot = &dots[ii];

        const double equator_dist = fabs((double)dot->y - height / 2.0) / height * 20.0;

        char probs[10];
        if (equator_dist < 1) {
            memcpy(
                probs, (char[]){'R', 'D', 'D', 'D', 'J', 'J', 'J', 'F', 'F', 'P'}, sizeof(probs)
            );
        } else if (equator_dist < 2) {
            memcpy(
                probs, (char[]){'R', 'D', 'D', 'D', 'J', 'J', 'F', 'F', 'P', 'P'}, sizeof(probs)
            );
        } else if (equator_dist < 3) {
            memcpy(
                probs, (char[]){'R', 'D', 'D', 'J', 'F', 'F', 'F', 'P', 'P', 'P'}, sizeof(probs)
            );
        } else if (equator_dist < 4) {
            memcpy(
                probs, (char[]){'R', 'D', 'J', 'F', 'F', 'F', 'P', 'P', 'P', 'P'}, sizeof(probs)
            );
        } else if (equator_dist < 5) {
            memcpy(
                probs, (char[]){'R', 'D', 'F', 'F', 'F', 'F', 'P', 'P', 'P', 'P'}, sizeof(probs)
            );
        } else if (equator_dist < 6) {
            memcpy(
                probs, (char[]){'R', 'F', 'F', 'F', 'F', 'F', 'P', 'P', 'P', 'P'}, sizeof(probs)
            );
        } else if (equator_dist < 7) {
            memcpy(
                probs, (char[]){'R', 'T', 'F', 'F', 'F', 'F', 'F', 'P', 'P', 'P'}, sizeof(probs)
            );
        } else if (equator_dist < 8) {
            memcpy(
                probs, (char[]){'R', 'S', 'S', 'T', 'T', 'F', 'F', 'F', 'P', 'P'}, sizeof(probs)
            );
        } else if (equator_dist < 9) {
            memcpy(
                probs, (char[]){'S', 'S', 'S', 'S', 'T', 'T', 'T', 'T', 'T', 'F'}, sizeof(probs)
            );
        } else {
            for (int ii = 0; ii < 10; ii++) {
                probs[ii] = 'S';
            }
        }

        /*
        Probability Chart, 1 box = 10% Chance
        Uppercase/lowercase are an attempt to make it easier to read, they mean nothing
        This also means s represents snow, not shallow water
        0-1 | r D D D J J J f f P
        1-2 | r D D D J J f f P P
        2-3 | r D D J f f f P P P
        3-4 | r D J f f f P P P P
        4-5 | r D f f f f P P P P
        5-6 | r f f f f f P P P P
        6-7 | r T f f f f f P P P
        7-8 | r s s T T f f f P P
        8-9 | s s s s T T T T f f
        9-10| s s s s s s s s s s
        */

        dot->type = probs[rand() % 10];

        ii++;

        atomic_fetch_add(&section_progress[4], 1);

    }

    // Create Land Biomes
    // Land dots are assigned the biome of the nearest biome origin dot

    // Create Biome Origin KDTree

    int num_biome_dots = num_dots / 10;
    int *biome_dots = malloc(num_biome_dots * 3 * sizeof(int));
    for (int i = 0; i < num_biome_dots; i++) {
        const Dot *dot = &dots[biome_origin_indexes[i]];
        biome_dots[i * 3] = dot->x;
        biome_dots[i * 3 + 1] = dot->y;
        biome_dots[i * 3 + 2] = biome_origin_indexes[i];
    }
    Node *biome_tree_root = NULL;
    biome_tree_root = build_recursive(biome_dots, num_biome_dots, 0);
    free(biome_dots);

    // Create and Sort Lands
    // Original "land_dots" was freed in water biome generation

    int num_land_dots2 = 0;
    int *land_dots2 = malloc(num_dots * 3 * sizeof(int));
    for (int i = 0; i < num_dots; i++) {
        const Dot *dot = &dots[i];
        if (dot->type == 'L') {
            land_dots2[num_land_dots2 * 3] = dot->x;
            land_dots2[num_land_dots2 * 3 + 1] = dot->y;
            land_dots2[num_land_dots2 * 3 + 2] = i;
            num_land_dots2++;
        }
    }

    quicksort_recursive(land_dots2, 0, num_land_dots2 - 1, width);

    // Create Piece Starts Land Dots

    int land_piece_length = num_land_dots2 / processes;
    int land_piece_starts[processes + 1];
    for (int i = 0; i < processes; i++) {
        land_piece_starts[i] = i * land_piece_length;
    }
    land_piece_starts[processes] = num_land_dots2;

    // Run Workers

    for (int i = 0; i < processes; i++) {

        fork_pids[i] = fork();
        if (fork_pids[i] != 0) {
            continue;
        }

        set_process_title("worker", i);
        generate_biomes_land(
            land_piece_starts[i], land_piece_starts[i + 1], land_dots2,
            biome_tree_root, biome_origin_indexes, num_dots, dots, section_progress
        );
        exit(0);

    }
    for (int i = 0; i < processes; i++) {
        waitpid(fork_pids[i], NULL, 0);
    }

    // Free Land Dots and Biome Tree

    free(land_dots2);
    free_recursive(biome_tree_root);

    // Set Section Completion Time

    clock_gettime(CLOCK_MONOTONIC, &time_now);
    section_times[4] = (double)(time_now.tv_sec - start_time.tv_sec) +
        (time_now.tv_nsec - start_time.tv_nsec) / 1000000000.0 - sum_list_double(section_times, 7);

    // --Image Generation--

    section_progress_total[5] = height;

    // Create Section Starts

    const int section_height = height / processes;

    int section_starts[processes + 1];
    for (int i = 0; i < processes; i++) {
        section_starts[i] = i * section_height;
    }
    section_starts[processes] = height;

    // Create Dots KDTree

    int *dot_coords = malloc(num_dots * 3 * sizeof(int));
    for (int i = 0; i < num_dots; i++) {
        const Dot *dot = &dots[i];
        dot_coords[i * 3] = dot->x;
        dot_coords[i * 3 + 1] = dot->y;
        dot_coords[i * 3 + 2] = i;
    }
    Node *tree_root = NULL;
    tree_root = build_recursive(dot_coords, num_dots, 0);
    free(dot_coords);

    // Run Workers

    for (int i = 0; i < processes; i++) {

        fork_pids[i] = fork();
        if (fork_pids[i] != 0) {
            continue;
        }

        set_process_title("worker", i);
        generate_image(
            section_starts[i], section_starts[i + 1], width, tree_root,
            num_dots, dots, image_indexes, type_counts, section_progress
        );
        exit(0);

    }
    for (int i = 0; i < processes; i++) {
        waitpid(fork_pids[i], NULL, 0);
    }

    // Free Tree

    free_recursive(tree_root);

    // Set Section Completion Time

    clock_gettime(CLOCK_MONOTONIC, &time_now);
    section_times[5] = (double)(time_now.tv_sec - start_time.tv_sec) +
        (time_now.tv_nsec - start_time.tv_nsec) / 1000000000.0 - sum_list_double(section_times, 7);

    // --Finish--

    section_progress_total[6] = height * 5;

    // Create Image

    FILE *fptr = fopen(output_file, "w");
    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info_ptr = png_create_info_struct(png_ptr);

    png_set_IHDR(
        png_ptr, info_ptr, width, height,
        8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT
    );

    // Assign Image Pixels

    png_byte **row_pointers = png_malloc(png_ptr, height * sizeof(png_byte *));
    for (int y = 0; y < height; y++) {
        png_byte *row = png_malloc(png_ptr, 3 * sizeof(unsigned char) * width);
        row_pointers[y] = row;
        for (int x = 0; x < width; x++) {
            int rgb[3] = {20, 0, 0};
            const int image_index = image_indexes[y * width + x];
            switch (dots[image_index].type) {
                case 'I':
                    rgb[0] = 153;
                    rgb[1] = 221;
                    rgb[2] = 255;
                    break;
                case 's':
                    rgb[0] = 0;
                    rgb[1] = 0;
                    rgb[2] = 255;
                    break;
                case 'W':
                    rgb[0] = 0;
                    rgb[1] = 0;
                    rgb[2] = 179;
                    break;
                case 'd':
                    rgb[0] = 0;
                    rgb[1] = 0;
                    rgb[2] = 128;
                    break;
                case 'R':
                    rgb[0] = 128;
                    rgb[1] = 128;
                    rgb[2] = 128;
                    break;
                case 'D':
                    rgb[0] = 255;
                    rgb[1] = 185;
                    rgb[2] = 109;
                    break;
                case 'J':
                    rgb[0] = 0;
                    rgb[1] = 77;
                    rgb[2] = 0;
                    break;
                case 'F':
                    rgb[0] = 0;
                    rgb[1] = 128;
                    rgb[2] = 0;
                    break;
                case 'P':
                    rgb[0] = 0;
                    rgb[1] = 179;
                    rgb[2] = 0;
                    break;
                case 'T':
                    rgb[0] = 152;
                    rgb[1] = 251;
                    rgb[2] = 152;
                    break;
                case 'S':
                    rgb[0] = 245;
                    rgb[1] = 245;
                    rgb[2] = 245;
                    break;
                default:
                    rgb[0] = 40;
                    rgb[1] = 0;
                    rgb[2] = 0;
                    break;
            }
            for (int i = 0; i < 3; i++) {
                /*
                Adds slight color variation
                Every pixel around the same dot has the same variation
                */
                int rgb_val = rgb[i];
                rgb_val += image_index % 20 - 10;
                if (rgb_val > 255){
                    rgb_val = 255;
                } else if (rgb_val < 0) {
                    rgb_val = 0;
                }
                rgb[i] = rgb_val;
            }
            *row++ = rgb[0];
            *row++ = rgb[1];
            *row++ = rgb[2];
        }
        atomic_fetch_add(&section_progress[6], 1);
    }

    // Write Image to File

    png_init_io(png_ptr, fptr);
    png_set_rows(png_ptr, info_ptr, row_pointers);
    png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

    for (int y = 0; y < height; y++) {
        png_free(png_ptr, row_pointers[y]);
    }
    png_free(png_ptr, row_pointers);

    fclose(fptr);

    atomic_store(&section_progress[6], height * 5);

    // Set Section Completion Time

    clock_gettime(CLOCK_MONOTONIC, &time_now);
    section_times[6] = (double)(time_now.tv_sec - start_time.tv_sec) +
        (time_now.tv_nsec - start_time.tv_nsec) / 1000000000.0 - sum_list_double(section_times, 7);

    section_times[7] = (double)(time_now.tv_sec - start_time.tv_sec) +
        (time_now.tv_nsec - start_time.tv_nsec) / 1000000000.0;

    // Wait for Tracker Process

    if (!auto_mode) {
        waitpid(tracker_process_pid, NULL, 0);
    }

    // Shared Memory Cleanup

    munmap(section_progress, sizeof(int) * 7);
    munmap(section_progress_total, sizeof(int) * 7);
    munmap(section_times, sizeof(double) * 8);
    munmap(dots, sizeof(Dot) * num_dots);
    munmap(image_indexes, sizeof(int) * width * height);

    // Completion

    struct timespec end_time;
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    char formatted_time[32];
    double completion_time = (double)(time_now.tv_sec - start_time.tv_sec) +
        (time_now.tv_nsec - start_time.tv_nsec) / 1000000000.0;
    snprintf(
        formatted_time, 32, "%2d:%08.5f", (int)completion_time / 60, fmod(completion_time, 60.0)
    );

    if (!auto_mode) {

        // Print Result Statistics

        printf(
            "%sGeneration Complete%s %s\n\nStatistics\n", ANSI_GREEN, ANSI_RESET, formatted_time
        );

        int count_water = 0;
        int count_land = 0;
        for (int i = 0; i < 4; i++) {
            count_water += type_counts[i];
        }
        for (int i = 4; i < 11; i++) {
            count_land += type_counts[i];
        }
        double tot_pct_fact = 100.0 / (height * width); // total percentage factor
        printf(
            "Water %6.2f%%\nLand  %6.2f%%\n",
            count_water * tot_pct_fact, count_land * tot_pct_fact
        );

        int text_colors[11] = {117, 21, 19, 17, 243, 229, 22, 28, 40, 48, 255};
        char types[11][14] = {
            "Ice", "Shallow Water", "Water", "Deep Water",
            "Rock", "Desert", "Jungle", "Forest", "Plains", "Taiga", "Snow"
        };

        printf("       %% of Land/Water | %% of Total\n");
        for (int i = 0; i < 11; i++) {

            double group_pct_fact = 100.0;
            if (i < 4) {
                group_pct_fact /= count_water;
            } else {
                group_pct_fact /= count_land;
            }

            printf(
                "\033[48;5;%dm%-14s%s" // Label
                "%7.2f%% | %6.2f%%\n", // Pct of Group | Pct of Total
                text_colors[i], types[i], ANSI_RESET,
                type_counts[i] * group_pct_fact, type_counts[i] * tot_pct_fact
            );

        }

    } else {

        printf("%f\n", completion_time);

    }

    munmap(type_counts, sizeof(int) * 11);

    return 0;

}