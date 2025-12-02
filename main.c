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

#define _POSIX_C_SOURCE 199309L // Needed for CLOCK_REALTIME

#include <limits.h>
#include <math.h>
#include <png.h>
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

typedef struct {
    short x;
    short y;
    char type;
    /*
    I = Ice
    s = Shallow Water
    W = Water
    d = Deep Water

    R = Rock
    D = Desert
    J = Jungle
    F = Forest
    P = Plains
    T = Taiga
    S = Snow

    w = Water Forced
    L = Land
    l = Land Origin
    */
} Dot;

typedef struct Node {
    int coord[2];
    int index;
    struct Node *left;
    struct Node *right;
} Node;

// General Functions
// (Alphabetical order)

/**
 * Get a sanitized integer input from the user between MIN and MAX,
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
int sum_list_int(const int list[], const int list_len) {
    int sum = 0;
    for (int i = 0; i < list_len; i++) {
        sum += list[i];
    }
    return sum;
}

/**
 * Return the sum of a list of floats.
 */
float sum_list_float(const float list[], const int list_len) {
    float sum = 0;
    for (int i = 0; i < list_len; i++) {
        sum += list[i];
    }
    return sum;
}

void quicksort_recursive(int *coords, const int low, const int high, const int width) {

    if (low < high) {

        /*
        Pivot value is the value at coords[high]. Every value less than pivot
        will be to the left of wherever pivot ends up. Remember: a coord is
        {x, y, index (of equivalent dot in dots)}. The array isn't 2D as it
        requires malloc.
        */
        int pivot = coords[high * 3 + 1] * width + coords[high * 3];

        // Move Smaller Elements to the Left

        int i = low - 1;

        for (int ii = low; ii <= high - 1; ii++) {
            if (coords[ii * 3 + 1] * width + coords[ii * 3] < pivot) {
                i++;
                const int temp[3] = {coords[i * 3], coords[i * 3 + 1], coords[i * 3 + 2]};
                coords[i * 3] = coords[ii * 3];
                coords[i * 3 + 1] = coords[ii * 3 + 1];
                coords[i * 3 + 2] = coords[ii * 3 + 2];
                coords[ii * 3] = temp[0];
                coords[ii * 3 + 1] = temp[1];
                coords[ii * 3 + 2] = temp[2];
            }
        }

        // Move Pivot to After Smaller Elements

        i++;
        const int temp[3] = {coords[i * 3], coords[i * 3 + 1], coords[i * 3 + 2]};
        coords[i * 3] = coords[high * 3];
        coords[i * 3 + 1] = coords[high * 3 + 1];
        coords[i * 3 + 2] = coords[high * 3 + 2];
        coords[high * 3] = temp[0];
        coords[high * 3 + 1] = temp[1];
        coords[high * 3 + 2] = temp[2];

        // Decide Whether Recursion is Needed
        /*
        Cases:
        i == med_index, the median is in the correct spot, no more sorting
        i > med_index, median is in left section, left section needs sorting
        i < med_index, median in right section, sort right section
        */

        quicksort_recursive(coords, low, i - 1, width);
        quicksort_recursive(coords, i + 1, high, width);

    }

}


// KDTree Functions

/**
 * Sort the given COORDS until the median index is correctly sorted. In other
 * words, median index will be correct, everything before median index will be
 * less than the value at median index, and everything after will be greater.
 * AXIS determines which value of a coordinate is its value (x or y) to sort
 * based on. HIGH and LOW determine the sorting bounds for a given level of
 * recursion.
 */
void median_sort_recursive(
    int *coords, const int low, const int high, const int axis, const int med_index
) {

    if (low < high) {

        /*
        Pivot value is the value at coords[high]. Every value less than pivot
        will be to the left of wherever pivot ends up. Remember: a coord is
        {x, y, index (of equivalent dot in dots)}. The array isn't 2D as it
        requires malloc.
        */
        int pivot = coords[high * 3 + axis];

        // Move Smaller Elements to the Left

        int i = low - 1;

        for (int ii = low; ii <= high - 1; ii++) {
            if (coords[ii * 3 + axis] < pivot) {
                i++;
                const int temp[3] = {coords[i * 3], coords[i * 3 + 1], coords[i * 3 + 2]};
                coords[i * 3] = coords[ii * 3];
                coords[i * 3 + 1] = coords[ii * 3 + 1];
                coords[i * 3 + 2] = coords[ii * 3 + 2];
                coords[ii * 3] = temp[0];
                coords[ii * 3 + 1] = temp[1];
                coords[ii * 3 + 2] = temp[2];
            }
        }

        // Move Pivot to After Smaller Elements

        i++;
        const int temp[3] = {coords[i * 3], coords[i * 3 + 1], coords[i * 3 + 2]};
        coords[i * 3] = coords[high * 3];
        coords[i * 3 + 1] = coords[high * 3 + 1];
        coords[i * 3 + 2] = coords[high * 3 + 2];
        coords[high * 3] = temp[0];
        coords[high * 3 + 1] = temp[1];
        coords[high * 3 + 2] = temp[2];

        // Decide Whether Recursion is Needed
        /*
        Cases:
        i == med_index, the median is in the correct spot, no more sorting
        i > med_index, median is in left section, left section needs sorting
        i < med_index, median in right section, sort right section
        */

        if (i > med_index) {
            median_sort_recursive(coords, low, i - 1, axis, med_index);
        } else if (i < med_index) {
            median_sort_recursive(coords, i + 1, high, axis, med_index);
        }

    }

}

/**
 * Build a KDTree from COORDS, and return the root node. Ensures the KDTree is
 * built with the lowest possible depth for maximum efficiency when querying the
 * tree. Every recursion creates one dot and calls this function to insert its
 * children from an array of possible dots. DEPTH should be 0. COORDS should be
 * of length NUM_COORDS * 3.
 */
Node *build_recursive(int *coords, const int num_coords, const int depth) {

    const int med_pos = num_coords / 2;

    // Sort Coords Around Median Position
    /*
    Median coord will be in correct place, everything less will be to the left,
    everything else to the right.
    */

    median_sort_recursive(coords, 0, num_coords - 1, depth % 2, med_pos);

    // Add Median Node to Tree
    // Every recursion adds one node

    Node *node = malloc(sizeof(Node));
    node->coord[0] = coords[med_pos * 3];
    node->coord[1] = coords[med_pos * 3 + 1];
    node->index = coords[med_pos * 3 + 2];
    node->left = NULL;
    node->right = NULL;

    // Decide whether node will have a left child

    const int num_coords_left = med_pos;

    if (num_coords_left > 0) {

        int *coords_left = malloc(num_coords_left * 3 * sizeof(int));
        for (int i = 0; i < med_pos; i++) {
            coords_left[i * 3] = coords[i * 3];
            coords_left[i * 3 + 1] = coords[i * 3 + 1];
            coords_left[i * 3 + 2] = coords[i * 3 + 2];
        }
        node->left = build_recursive(coords_left, num_coords_left, depth + 1);
        free(coords_left);

        // Decide whether node will have a right child

        const int num_coords_right = num_coords - med_pos - 1;

        if (num_coords_right > 0) {
            int *coords_right = malloc(num_coords_right * 3 * sizeof(int));
            for (int i = 0; i < num_coords_right; i++) {
                const int index = i + med_pos + 1;
                coords_right[i * 3] = coords[index * 3];
                coords_right[i * 3 + 1] = coords[index * 3 + 1];
                coords_right[i * 3 + 2] = coords[index * 3 + 2];
            }
            node->right = build_recursive(coords_right, num_coords_right, depth + 1);
            free(coords_right);
        }

    }

    // Return node to place it within the tree
    return node;

}

/**
 * Query the KDTree to modify MIN_DIST, the distance to the nearest node. When
 * INDEX_PTR is not null, it stores the index of the nearest node, which
 * corresponds to the index of the dot it was created from. Calls itself
 * recursively to query children of NODE. DEPTH should be 0 when NODE is a root
 * node.
 */
void query_recursive(
    Node *node, const int coord[2], const int depth, int *index_ptr, int *min_dist_ptr
) {

    // Calculate Distance

    const int diff_x = node->coord[0] - coord[0];
    const int diff_y = node->coord[1] - coord[1];
    // Distance is squared for efficiency
    const int dist = diff_x * diff_x + diff_y * diff_y;

    // Update Minimum Distance and Index Pointers

    if (dist < *min_dist_ptr) {
        if (index_ptr != NULL) {
            *index_ptr = node->index;
        }
        *min_dist_ptr = dist;
    }

    // Decide Whether Recursion is Needed

    const int axis = depth % 2;

    const int dist_line = node->coord[axis] - coord[axis];
    const int dist_sq = dist_line * dist_line;
    // Whether distance to splitting line is less than max_dist
    if (node->left != NULL && (dist_sq < *min_dist_ptr || coord[axis] < node->coord[axis])) {
        /*
        Recursion only if coord is close enough to dividing line, or coord would
        be inside bounds of left child
        */
        query_recursive(node->left, coord, depth + 1, index_ptr, min_dist_ptr);
    }
    if (node->right != NULL && (dist_sq < *min_dist_ptr || coord[axis] >= node->coord[axis])) {
        query_recursive(node->right, coord, depth + 1, index_ptr, min_dist_ptr);
    }

}

/**
 * Query the KDTree to modify DISTS, the distances of the nearest DISTS_LEN
 * points to COORD. Recursively navigates down the KDTree, editing DISTS and
 * the value at MAX_DIST_PTR whenever it finds a node whose distance is less
 * than the value at MAX_DIST_PTR. All distances are squared for efficiency.
 * DEPTH should be 0 when NODE is a root node.
 */
void query_dist_recursive(
    Node *node, const int coord[2], const int depth,
    int dists[], const int dists_len
) {

    // Calculate Distance

    const int diff_x = node->coord[0] - coord[0];
    const int diff_y = node->coord[1] - coord[1];
    const int dist = diff_x * diff_x + diff_y * diff_y; // Squared distance

    // Update Distances List

    if (dist < dists[dists_len - 1] && dist != 0) {
        for (int i = dists_len - 1; i >= 0; i--) {
            if (i == 0 || dist >= dists[i - 1]) {
                // Found insertion position, insert and break
                dists[i] = dist;
                break;
            }
            // After insertion position, shift element
            dists[i] = dists[i - 1];
        }
    }

    // Decide Whether Recursion is Needed

    const int axis = depth % 2;

    const int dist_line = node->coord[axis] - coord[axis];
    const int dist_sq = dist_line * dist_line;
    // Whether distance to splitting line is less than max_dist
    if (node->left != NULL && (dist_sq < dists[dists_len - 1] || coord[axis] < node->coord[axis])) {
        /*
        Recursion only if coord is close enough to dividing line, or coord would
        be inside bounds of left child
        */
        query_dist_recursive(node->left, coord, depth + 1, dists, dists_len);
    }
    if (
        node->right != NULL && (dist_sq < dists[dists_len - 1] || coord[axis] >= node->coord[axis])
    ) {
        query_dist_recursive(node->right, coord, depth + 1, dists, dists_len);
    }

}

/**
 * Recursively free NODE and its children.
 */
void free_recursive(Node *node) {
    if (node->left != NULL) {
        free_recursive(node->left);
    }
    if (node->right != NULL) {
        free_recursive(node->right);
    }
    free(node);
    node = NULL;
}


// Multiprocessing Functions
// (Order of use)

/**
 * Set the process's title to "biogen-" + TYPE. If NUM >= 0, then NUM, padded to
 * a length of 2 (not including null character) with leading zeros will be added
 * as well. The length of TYPE + NUM (as a string) must be at max 9 characters
 * (including null character).
 */
void set_process_title(const char type[], const int num) {

    // Example: type is "worker", num is 6: "biogen-worker6"

    // Concatenate Type

    char string[16] = "biogen-";
    strncat(string, type, 9);

    // Concatenate Num

    if (0 <= num && num < 100) {
        char num_str[3];
        snprintf(num_str, 3, "%02d", num);
        strncat(string, num_str, 3);
    }

    // Set Process Title

    #ifdef __linux__
        prctl(PR_SET_NAME, string, 0, 0, 0);
    #elif BSD || __Apple__
        setproctitle(string);
    #endif

}

/**
 * Track the progress of map generation, and show the progress in the terminal.
 * START_TIME is the time at the start of map generation in main(), and the
 * other inputs are all shared memory used to track section progress and times.
 * Not used in automated inputs mode.
 */
void track_progress(
    struct timespec start_time,
    _Atomic int *section_progress, int *section_progress_total, float *section_times
) {

    char section_names[7][20] = {
        "Setup", "Section Generation", "Section Assignment", "Coastline Smoothing",
        "Biome Generation", "Image Generation", "Finish"
    };
    float section_weights[7] = {0.01, 0.01, 0.02, 0.60, 0.06, 0.10, 0.20};
    // Used for overall progress bar (e.g. Setup takes ~1% of total time)

    while (true) {

        // Clear Screen

        system("clear");

        // Section Progress

        struct timespec time_now;
        clock_gettime(CLOCK_REALTIME, &time_now);
        float time_diff = (float)(time_now.tv_sec - start_time.tv_sec) +
            (time_now.tv_nsec - start_time.tv_nsec) / 1000000000.0;

        float total_progress = 0.0;

        for (int i = 0; i < 7; i++) {

            // Calculate Section Progress

            float progress_section =
                atomic_load(&section_progress[i]) / (float)section_progress_total[i];
            total_progress += progress_section * section_weights[i];

            // Check if Section Complete

            char color[10];
            float section_time;
            if (section_times[i] != 0.0) {
                strncpy(color, ANSI_GREEN, 10);
                section_time = section_times[i]; // Section complete
            } else {
                strncpy(color, ANSI_BLUE, 10);
                if (i == 0 || section_times[i - 1] != 0.0) {
                    section_time = time_diff - sum_list_float(section_times, 7);
                    // Section in progress
                } else {
                    section_time = 0.0; // Section hasn't started
                }
            }

            // Print Output for Section Progress

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
            char formatted_time[32];
            snprintf(
                formatted_time, 32, "%2d:%08.5f", (int)section_time / 60, fmod(section_time, 60.0f)
            );
            printf("%s%s\n", ANSI_RESET, formatted_time);

        }

        // Total Progress

        char color[10];
        float total_time;
        if (section_times[7] != 0.0) {
            strncpy(color, ANSI_GREEN, 10);
            total_time = section_times[7];
        } else {
            strncpy(color, ANSI_BLUE, 10);
            total_time = (float)(time_now.tv_sec - start_time.tv_sec) +
                (time_now.tv_nsec - start_time.tv_nsec) / 1000000000.0;
        }

        printf("%s      Total Progress      %8.2f%% %s", color, total_progress * 100, ANSI_GREEN);
        int green_bars = (int)round(20 * total_progress);
        for (int i = 0; i < green_bars; i++) {
            printf("█");
        }
        printf("%s", ANSI_BLUE);
        for (int i = 0; i < 20 - green_bars; i++) {
            printf("█");
        }
        char formatted_time[32];
        snprintf(formatted_time, 32, "%2d:%08.5f", (int)total_time / 60, fmod(total_time, 60.0f));
        printf("%s%s\n", ANSI_RESET, formatted_time);

        // Check Exit Status

        if (strcmp(color, ANSI_GREEN) == 0) {
            break; // All sections done, exit tracking process
        }

        // Sleep 0.1 Seconds

        struct timespec sleep_time;
        sleep_time.tv_sec = 0;
        sleep_time.tv_nsec = 100000000;
        nanosleep(&sleep_time, &sleep_time);

    }

}

/**
 * Assign sections of the map. Land and water are randomly assigned based on a
 * dot's distance from the nearest land origin dot.
 */
void assign_sections(
    const int map_resolution, const float island_size, const int start_index, const int end_index,
    Node *origin_tree_root, Dot *dots, _Atomic int *section_progress
) {

    srand(0);

    for (int i = start_index; i < end_index; i++) {
    // Non-water dots are not included

        Dot *dot = &dots[i];

        // Find Distance to Nearest Origin Dot

        int min_dist = INT_MAX;
        // min and dist are squared, sqrt is not done until later
        int min_index = 0;
        int coord[2] = {dot->x, dot->y};
        query_recursive(origin_tree_root, coord, 0, &min_index, &min_dist);

        // Calculate Chance

        float dist = sqrt(min_dist) / sqrt(map_resolution);
        float threshold = ((float)(min_index % 20) / 19.0f * 1.5f + 0.25f) * island_size;

        int chance = (dist <= threshold) ? 9 : 1;

        if (rand() % 10 < chance) {
            dot->type = 'L'; // Land
        }

        atomic_fetch_add(&section_progress[2], 1);

    }

}

/**
 * Smooth map coastlines for a more realistic, aesthetically pleasing map.
 * Reassigns land and water dots based on the average distance of the nearest
 * COASTLINE_SMOOTHING dots of the same and opposite types.
 */
void smooth_coastlines(
    const int coastline_smoothing,
    const int *land_dots, const int land_start, const int land_end, Node *land_tree_root,
    const int *water_dots, const int water_start, const int water_end, Node *water_tree_root,
    const int num_dots, const int num_land_dots, const int num_water_dots,
    Dot *dots, _Atomic int *section_progress
) {

    int dists_same[coastline_smoothing];
    int dists_opp[coastline_smoothing];

    // Coastline Smoothing for Land Dots

    for (int i = land_start; i < land_end; i++) {

        int dot_coord[2] = {land_dots[i * 3], land_dots[i * 3 + 1]};

        // Calculate Maximum Distances

        bool same_y = false;
        if (i != 0 && land_dots[i * 3 + 1] == land_dots[(i - 1) * 3 + 1]) {
            same_y = true;
            for (int ii = 0; ii < coastline_smoothing; ii++) {
                const int prev_dot_dist = land_dots[i * 3] - land_dots[(i - 1) * 3];
                int min_dist_sq = (int)sqrt(dists_same[ii]) + 1 + prev_dot_dist;
                // + 1 needed for floating-point errors (ceil didn't work)
                dists_same[ii] = min_dist_sq * min_dist_sq;
                min_dist_sq = (int)sqrt(dists_opp[ii]) + 1 + prev_dot_dist;
                dists_opp[ii] = min_dist_sq * min_dist_sq;
            }
        } else {
            for (int ii = 0; ii < coastline_smoothing; ii++) {
                dists_same[ii] = INT_MAX;
            }
        }

        // Get Nearest Distances for Each Type

        long sum_same = 0;
        long sum_opp = 0;

        query_dist_recursive(land_tree_root, dot_coord, 0, dists_same, coastline_smoothing);
        for (int ii = 0; ii < coastline_smoothing; ii++) {
            sum_same += dists_same[ii];
        }

        if (!same_y) {
            const int max = (sum_same < INT_MAX) ? sum_same : INT_MAX;
            for (int ii = 0; ii < coastline_smoothing; ii++) {
                dists_opp[ii] = max;
            }
        }

        query_dist_recursive(water_tree_root, dot_coord, 0, dists_opp, coastline_smoothing);
        for (int ii = 0; ii < coastline_smoothing; ii++) {
            sum_opp += dists_opp[ii];
        }

        // Change Dot Type if Closer to Opposite Type

        if (sum_same > sum_opp) {
            dots[land_dots[i * 3 + 2]].type = 'W';
        }

        atomic_fetch_add(&section_progress[3], 1);

    }

    // Coastline Smoothing for Water Dots

    for (int i = water_start; i < water_end; i++) {

        int dot_coord[2] = {water_dots[i * 3], water_dots[i * 3 + 1]};

        // Calculate Maximum Distances

        bool same_y = false;
        if (i != 0 && water_dots[i * 3 + 1] == water_dots[(i - 1) * 3 + 1]) {
            same_y = true;
            for (int ii = 0; ii < coastline_smoothing; ii++) {
                const int prev_dot_dist = water_dots[i * 3] - water_dots[(i - 1) * 3];
                int min_dist_sq = (int)sqrt(dists_same[ii]) + 1 + prev_dot_dist;
                dists_same[ii] = min_dist_sq * min_dist_sq;
                min_dist_sq = (int)sqrt(dists_opp[ii]) + 1 + prev_dot_dist;
                dists_opp[ii] = min_dist_sq * min_dist_sq;
            }
        } else {
            for (int ii = 0; ii < coastline_smoothing; ii++) {
                dists_same[ii] = INT_MAX;
            }
        }

        // Get Nearest Distances for Each Type

        long sum_same = 0;
        long sum_opp = 0;

        query_dist_recursive(water_tree_root, dot_coord, 0, dists_same, coastline_smoothing);
        for (int ii = 0; ii < coastline_smoothing; ii++) {
            sum_same += dists_same[ii];
        }

        if (!same_y) {
            const int max = (sum_same < INT_MAX) ? sum_same : INT_MAX;
            for (int ii = 0; ii < coastline_smoothing; ii++) {
                dists_opp[ii] = max;
            }
        }

        query_dist_recursive(land_tree_root, dot_coord, 0, dists_opp, coastline_smoothing);
        for (int ii = 0; ii < coastline_smoothing; ii++) {
            sum_opp += dists_opp[ii];
        }

        // Change Dot Type if Closer to Opposite Type

        if (sum_same > sum_opp) {
            dots[water_dots[i * 3 + 2]].type = 'L';
        }

        atomic_fetch_add(&section_progress[3], 1);

    }

}

/**
 * Generate water biomes for DOTS between START_INDEX and END_INDEX. Water
 * biomes are generated based on a dot's distance to the equator and the
 * distance to the nearest land dot.
 */
void generate_biomes_water(
    const int start_index, const int end_index, const int height, Node *land_tree_root,
    const int num_dots, Dot *dots, _Atomic int *section_progress
) {

    // Generate Water Biomes

    for (int i = start_index; i < end_index; i++) {

        // Get Dot Info

        Dot *dot = &dots[i];

        if (dot->type != 'W') {
            continue;
        }

        // Calculate Distance to Equator

        const float equator_dist = fabs((float)dot->y - height / 2.0) / height * 20.0;

        // Calculate Distance to Land

        int land_dist_sq = INT_MAX;
        const int coord[2] = {dot->x, dot->y};
        query_recursive(land_tree_root, coord, 0, NULL, &land_dist_sq);

        // Set Water Biome

        if ( // Remember: all land distances are squared for efficiency
            (land_dist_sq < 35 * 35 && equator_dist > 9) ||
            (land_dist_sq < 25 * 25 && equator_dist > 8) ||
            (land_dist_sq < 15 * 15 && equator_dist > 7)
        ) {
            dot->type = 'I';
        } else if (land_dist_sq < 18 * 18) {
            dot->type = 's';
        } else if (land_dist_sq >= 35 * 35) {
            dot->type = 'd';
        }

        atomic_fetch_add(&section_progress[4], 1);

    }

}

/**
 * Generate land biomes for DOTS between START_INDEX and END_INDEX. Land biomes
 * are generated base on the nearest dot in BIOME_ORIGIN_INDEXES, the list of
 * dots whose biomes are already before this function.
 */
void generate_biomes_land(
    const int start_index, const int end_index, Node *origin_tree_root, const int num_dots,
    const int biome_origin_indexes[], Dot *dots, _Atomic int *section_progress
) {

    // Generate Land Biomes

    for (int i = start_index; i < end_index; i++) {

        Dot *dot = &dots[i];

        if (dot->type != 'L') {
            continue;
        }

        // Find Nearest Biome Origin Dot

        int min_dist = INT_MAX;
        int origin_index = 0;
        const int coord[2] = {dot->x, dot->y};
        query_recursive(origin_tree_root, coord, 0, &origin_index, &min_dist);

        // Set Dot Type

        dot->type = dots[origin_index].type;

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

    bool auto_mode;
    char output_file[229];
    int width, height, map_resolution, island_abundance, coastline_smoothing, processes;
    float island_size;

    if (argc == 1) {

        // Manual Inputs Mode

        auto_mode = false;

        strncpy(output_file, "result.png", 11);

        // Get Program Version

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
        system("clear");
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
        epilepsy or not, but I put this just in case. You can increase sleep_time.tv_nsec to help
        with this.
        */
        getchar();
        system("clear");

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
        map_resolution = get_int(50, 500); // controls the number of dots

        printf(
            "\nIsland abundance control how many islands there are,\n"
            "and the ration of land to water.\n"
            "Choose a number between 10 and 1000. 120 is the default.\n"
            "Larger numbers produces less land.\n"
            "Island Abundance:\n"
        );
        island_abundance = get_int(10, 1000);
        // controls the number of "Land Origin" and "Water Forced" dots

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

        system("clear");

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
        strncpy(output_file, argv[8], 229);

    }

    struct timespec start_time;
    clock_gettime(CLOCK_REALTIME, &start_time);

    // --Setup--

    // Shared Memory

    _Atomic int *section_progress = mmap(
        NULL, sizeof(int) * 7, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0
    );

    int *section_progress_total = mmap(
        NULL, sizeof(int) * 7, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0
    );

    float *section_times = mmap(
        NULL, sizeof(float) * 8, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0
    );

    for (int i = 0; i < 8; i++) {
        atomic_init(&section_progress[i], 0);
        section_progress_total[i] = 1;
        section_times[i] = 0;
    }

    const int num_dots = width * height / map_resolution;

    Dot *dots = mmap(
        NULL, sizeof(Dot) * num_dots, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0
    );

    int *image_indexes = mmap(
        NULL, sizeof(int) * width * height,
        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0
    );

    for (int i = 0; i < width * height; i++) {
        image_indexes[i] = 0;
    }

    int *type_counts = mmap(
        NULL, sizeof(int) * 11, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0
    );

    int tracker_process_pid = -1;

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

    struct timespec time_now;
    clock_gettime(CLOCK_REALTIME, &time_now);
    section_times[0] = (float)(time_now.tv_sec - start_time.tv_sec) +
        (time_now.tv_nsec - start_time.tv_nsec) / 1000000000.0;
    atomic_store(&section_progress[0], 1);

    // --Section Generation--
    // Create the initial list of dots

    section_progress_total[1] = num_dots;

    srand(0);

    const int num_special_dots = num_dots / island_abundance * 2;
    const int num_reg_dots = num_dots - num_special_dots;

    bool *used_coords = calloc(width * height, sizeof(bool));

    for (int i = 0; i < num_dots; i++) {

        // Find Unused Coordinate

        int ii;
        do {
            ii = rand() % (width * height);
        } while (used_coords[ii]);

        // Create Dot

        used_coords[ii] = true;

        Dot new_dot = { .x = ii % width, .y = ii / width };
        if (i < num_special_dots / 2) {
            new_dot.type = 'l'; // Land Origin, origin points for islands
        } else if (i < num_special_dots) {
            new_dot.type = 'w'; // Water Forced (good for making lakes)
        } else {
            new_dot.type = 'W'; // Water (default)
        }

        dots[i] = new_dot;

        atomic_fetch_add(&section_progress[1], 1);

    }

    free(used_coords);

    // Set Section Completion Time

    clock_gettime(CLOCK_REALTIME, &time_now);
    section_times[1] = (float)(time_now.tv_sec - start_time.tv_sec) +
        (time_now.tv_nsec - start_time.tv_nsec) / 1000000000.0 - sum_list_float(section_times, 7);

    // --Section Assignment--
    // Assign dots as "Land", "Land Origin", "Water", or "Water Forced"

    section_progress_total[2] = num_reg_dots;

    // Create Piece Starts

    int piece_length = num_reg_dots / processes;

    int piece_starts[processes + 1];
    for (int i = 0; i < processes; i++) {
        piece_starts[i] = i * piece_length + num_special_dots;
    }
    piece_starts[processes] = num_dots;
    /*
    used to create x pieces of size num_reg_dots / x, where x = processes
    last piece may be larger, special dots are skipped
    */

    // Create Land Origin KDTree

    const int num_origin_dots = num_special_dots / 2;
    int land_origin_dots[num_origin_dots * 3 * sizeof(int)];
    for (int i = 0; i < num_origin_dots; i++) {
        Dot *dot = &dots[i];
        land_origin_dots[i * 3] = dot->x;
        land_origin_dots[i * 3 + 1] = dot->y;
        land_origin_dots[i * 3 + 2] = i;
    }
    Node *origin_tree_root = NULL;
    origin_tree_root = build_recursive(land_origin_dots, num_origin_dots, 0);

    // Run Workers

    int fork_pids[processes]; // Create forks list
    for (int i = 0; i < processes; i++) {

        fork_pids[i] = fork(); // Create fork
        if (fork_pids[i] != 0) {
            continue;
        }

        // e.g. biogen-worker00
        set_process_title("worker", i);
        assign_sections( // Run worker
            map_resolution, island_size, piece_starts[i], piece_starts[i + 1],
            origin_tree_root, dots, section_progress
        );
        exit(0); // Kill worker

    }
    for (int i = 0; i < processes; i++) {
        waitpid(fork_pids[i], NULL, 0); // Wait for workers
    }

    // Free Land Origin Tree

    free_recursive(origin_tree_root);

    // Set Section Completion Time

    clock_gettime(CLOCK_REALTIME, &time_now);
    section_times[2] = (float)(time_now.tv_sec - start_time.tv_sec) +
        (time_now.tv_nsec - start_time.tv_nsec) / 1000000000.0 - sum_list_float(section_times, 7);

    // --Coastline Smoothing--

    if (coastline_smoothing != 0) {

        section_progress_total[3] = num_reg_dots;

        // Create Land and Water Dots

        int num_land_dots = 0;
        int num_water_dots = 0;
        int *land_dots = malloc(num_reg_dots * 3 * sizeof(int));
        int *water_dots = malloc(num_reg_dots * 3 * sizeof(int));
        // num_land_dots + num_water_dots == num_reg_dots, so this is the max

        for (int i = num_special_dots; i < num_dots; i++) {
        // Only includes "Land" and "Water" dots
            const Dot *dot = &dots[i];
            if (dot->type == 'L') {
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

        // Create Land and Water KDTrees

        Node *land_tree_root = NULL;
        Node *water_tree_root = NULL;

        land_tree_root = build_recursive(land_dots, num_land_dots, 0);
        water_tree_root = build_recursive(water_dots, num_water_dots, 0);

        // Sort Land and Water Dots

        quicksort_recursive(land_dots, 0, num_land_dots - 1, width);
        quicksort_recursive(water_dots, 0, num_water_dots - 1, width);

        // Create Piece Starts for Land and Water Dots

        int land_piece_length = num_land_dots / processes;
        int land_piece_starts[processes + 1];
        for (int i = 0; i < processes; i++) {
            land_piece_starts[i] = i * land_piece_length;
        }
        land_piece_starts[processes] = num_land_dots;

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
            smooth_coastlines(
                coastline_smoothing,
                land_dots, land_piece_starts[i], land_piece_starts[i + 1], land_tree_root,
                water_dots, water_piece_starts[i], water_piece_starts[i + 1], water_tree_root,
                num_dots, num_land_dots, num_water_dots, dots, section_progress
            );
            exit(0);

        }
        for (int i = 0; i < processes; i++) {
            waitpid(fork_pids[i], NULL, 0);
        }

        // Free Dot Lists and Trees

        free(land_dots);
        free(water_dots);

        free_recursive(land_tree_root);
        free_recursive(water_tree_root);

    } else {

        // Coastline smoothing would have no effect
        atomic_store(&section_progress[3], 1);

    }

    // Set Section Completion Time

    clock_gettime(CLOCK_REALTIME, &time_now);
    section_times[3] = (float)(time_now.tv_sec - start_time.tv_sec) +
        (time_now.tv_nsec - start_time.tv_nsec) / 1000000000.0 - sum_list_float(section_times, 7);

    // --Biome Generation--

    atomic_store(&section_progress_total[4], num_dots);

    // Remove "Land Origin" and "Water Forced" Dots

    for (int i = 0; i < num_special_dots; i++) {
        Dot *dot = &dots[i];
        if (dot->type == 'l') {
            dot->type = 'L';
        } else if (dot->type == 'w') {
            dot->type = 'W';
        }
    }

    // Create Piece Starts for All Dots

    piece_length = num_dots / processes;
    for (int i = 0; i < processes; i++) {
        piece_starts[i] = i * piece_length;
    }
    piece_starts[processes] = num_dots;

    // Create Water Biomes
    // Adds ice, depth

    // Build Land Dots KDTree

    int num_land_dots = 0;
    int *land_dots = malloc(num_dots * 3 * sizeof(int));
    for (int i = 0; i < num_dots; i++) {
        if (dots[i].type == 'L') {
            const Dot *dot = &dots[i];
            land_dots[num_land_dots * 3] = dot->x;
            land_dots[num_land_dots * 3 + 1] = dot->y;
            land_dots[num_land_dots * 3 + 2] = i;
            num_land_dots++;
        }
    }
    Node *land_tree_root = NULL;
    land_tree_root = build_recursive(land_dots, num_land_dots, 0);
    free(land_dots);

    // Run Workers

    for (int i = 0; i < processes; i++) {

        fork_pids[i] = fork();
        if (fork_pids[i] != 0) {
            continue;
        }

        set_process_title("worker", i);
        generate_biomes_water(
            piece_starts[i], piece_starts[i + 1], height, land_tree_root,
            num_dots, dots, section_progress
        );
        exit(0);

    }
    for (int i = 0; i < processes; i++) {
        waitpid(fork_pids[i], NULL, 0);
    }

    // Free Land Tree

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

        const float equator_dist = fabs((float)dot->y - height / 2.0) / height * 20.0;

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

    // Run Workers

    for (int i = 0; i < processes; i++) {

        fork_pids[i] = fork();
        if (fork_pids[i] != 0) {
            continue;
        }

        set_process_title("worker", i);
        generate_biomes_land(
            piece_starts[i], piece_starts[i + 1], biome_tree_root, num_dots,
            biome_origin_indexes, dots, section_progress
        );
        exit(0);

    }
    for (int i = 0; i < processes; i++) {
        waitpid(fork_pids[i], NULL, 0);
    }

    // Free Tree

    free_recursive(biome_tree_root);

    // Set Section Completion Time

    clock_gettime(CLOCK_REALTIME, &time_now);
    section_times[4] = (float)(time_now.tv_sec - start_time.tv_sec) +
        (time_now.tv_nsec - start_time.tv_nsec) / 1000000000.0 - sum_list_float(section_times, 7);

    // --Image Generation--

    atomic_store(&section_progress_total[5], height);

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

    clock_gettime(CLOCK_REALTIME, &time_now);
    section_times[5] = (float)(time_now.tv_sec - start_time.tv_sec) +
        (time_now.tv_nsec - start_time.tv_nsec) / 1000000000.0 - sum_list_float(section_times, 7);

    // --Finish--

    atomic_store(&section_progress_total[6], height);

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

    clock_gettime(CLOCK_REALTIME, &time_now);
    section_times[6] = (float)(time_now.tv_sec - start_time.tv_sec) +
        (time_now.tv_nsec - start_time.tv_nsec) / 1000000000.0 - sum_list_float(section_times, 7);

    // Set Section Completion Time

    section_times[7] = (float)(time_now.tv_sec - start_time.tv_sec) +
        (time_now.tv_nsec - start_time.tv_nsec) / 1000000000.0;

    // Wait for Tracker Process

    if (!auto_mode) {
        waitpid(tracker_process_pid, NULL, 0);
    }

    // Shared Memory Cleanup

    munmap(section_progress, sizeof(int) * 7);
    munmap(section_progress_total, sizeof(int) * 7);
    munmap(section_times, sizeof(float) * 8);
    munmap(dots, sizeof(Dot) * num_dots);
    munmap(image_indexes, sizeof(int) * width * height);

    // Completion

    struct timespec end_time;
    clock_gettime(CLOCK_REALTIME, &end_time);
    char formatted_time[32];
    float completion_time = (float)(time_now.tv_sec - start_time.tv_sec) +
        (time_now.tv_nsec - start_time.tv_nsec) / 1000000000.0;
    snprintf(
        formatted_time, 32, "%2d:%08.5f", (int)completion_time / 60, fmod(completion_time, 60.0f)
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
        float tot_pct_fact = 100.0 / (height * width); // total percentage factor
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

            float group_pct_fact = 100.0;
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