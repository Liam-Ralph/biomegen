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

#include <limits.h>
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

typedef struct {
    int x;
    int y;
    char type[13];
} Dot;

typedef struct Node {
    int coord[2];
    struct Node *left;
    struct Node *right;
} Node;


// Debugging Functions

void record_val(const long value, const char *name) {
    if (strcmp(name, "clear") == 0) {
        FILE *fptr = fopen("production-files/record.txt", "w");
        fprintf(fptr, "Record Cleared\n");
        fclose(fptr);
    } else {
        FILE *fptr = fopen("production-files/record.txt", "a");
        fprintf(fptr, "%s | %ld\n", name, value);
        fclose(fptr);
    }
}

void record_str(const char *str, const char *name) {
    FILE *fptr = fopen("production-files/record.txt", "a");
    fprintf(fptr, "%s | %s\n", name, str);
    fclose(fptr);
}


// Functions
// (Alphabetical order)

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


// KDTree Functions

int get_depth(struct Node* root) {
    if (root == NULL)
        return -1;

    // compute the height of left and right subtrees
    int lHeight = get_depth(root->left);
    int rHeight = get_depth(root->right);

    return (lHeight > rHeight ? lHeight : rHeight) + 1;
}

/**
 * Insert a coordinate into the KDTree. The function will recursive navigate
 * down the tree based on the coordinate's position until it finds a null node,
 * where it creates a new node with the coordinate.
 */
Node *insert_recursive(Node *node, const int *coord, const int depth) {

    if (node == NULL) {
        Node *new_node = malloc(sizeof(Node));
        new_node->coord[0] = coord[0];
        new_node->coord[1] = coord[1];
        new_node->left = NULL;
        new_node->right = NULL;
        return new_node;
    }

    const int axis = depth % 2;

    if (coord[axis] < node->coord[axis]) {
        node->left = insert_recursive(node->left, coord, depth + 1);
    } else {
        node->right = insert_recursive(node->right, coord, depth + 1);
    }

    return node;

}

void nth_sort_recursive(
    int coords[], const int low, const int high, const int axis, const int med_index
) {

    if (low < high) {

        const int pivot_index = rand() % high;
        int pivot = {coords[(pivot_index) * 2 + axis]};

        int i = low - 1;

        for (int ii = low; ii <= high - 1; ii++) {
            if (coords[ii * 2 + axis] < pivot) {
                i++;
                const int temp[2] = {coords[i * 2], coords[i * 2 + 1]};
                coords[i * 2] = coords[ii * 2];
                coords[i * 2 + 1] = coords[ii * 2 + 1];
                coords[ii * 2] = temp[0];
                coords[ii * 2 + 1] = temp[1];
            }
        }

        i++;
        const int temp[2] = {coords[i * 2], coords[i * 2 + 1]};
        coords[i * 2] = coords[pivot_index * 2];
        coords[i * 2 + 1] = coords[pivot_index * 2];
        coords[high * 2] = temp[0];
        coords[high * 2 + 1] = temp[1];

        if (pivot_index > med_index) {
            nth_sort_recursive(coords, low, i - 1, axis, med_index);
        } else {
            nth_sort_recursive(coords, i + 1, high, axis, med_index);
        }

    }

}

Node *build_recursive(const int num_coords, int coords[num_coords * 2], const int depth) {

    const int axis = depth % 2;

    // for (int i = 0; i < num_coords - 1; i++) {
    //     bool swapped = false;
    //     for (int ii = 0; ii < num_coords - i - 1; ii++) {
    //         if (coords[ii * 2 + axis] > coords[(ii + 1) * 2 + axis]) {
    //             int temp[2] = {coords[ii * 2], coords[ii * 2 + 1]};
    //             coords[ii * 2] = coords[(ii + 1) * 2];
    //             coords[ii * 2 + 1] = coords[(ii + 1) * 2 + 1];
    //             coords[(ii + 1) * 2] = temp[0];
    //             coords[(ii + 1) * 2 + 1] = temp[1];
    //             swapped = true;
    //         }
    //     }
    //     if (!swapped) {
    //         break;
    //     }
    // }

    nth_sort_recursive(coords, 0, num_coords - 1, axis, num_coords / 2);

    const int med_pos = num_coords / 2;
    Node *node = malloc(sizeof(Node));
    node->coord[0] = coords[med_pos * 2];
    node->coord[1] = coords[med_pos * 2 + 1];
    node->left = NULL;
    node->right = NULL;

    const int num_coords_right = num_coords - 1 - med_pos;
    if (num_coords_right > 0) {

        int *coords_right = malloc(num_coords_right * 2 * sizeof(int));
        for (int i = med_pos + 1; i < num_coords; i++) {
            coords_right[(i - med_pos - 1) * 2] = coords[i * 2];
            coords_right[(i - med_pos - 1) * 2 + 1] = coords[i * 2 + 1];
        }
        node->right = build_recursive(num_coords_right, coords_right, depth + 1);
        free(coords_right);

        const int num_coords_left = num_coords - num_coords_right;
        if (num_coords_left > 0) {
            int *coords_left = malloc(num_coords_left * 2 * sizeof(int));
            for (int i = 0; i < med_pos; i++) {
                coords_left[i * 2] = coords[i * 2];
                coords_left[i * 2 + 1] = coords[i * 2 + 1];
            }
            node->left = build_recursive(num_coords_left, coords_left, depth + 1);
            free(coords_left);
        }

    }

    return node;

}

/**
 * Query the KDTree to modify dists, the distances of the nearest dists_len
 * points to coord. Recursively navigates down the KDTree, editing dists and
 * the value at max_dist_ptr whenever it finds a node whose distance is less
 * than the value at max_dist_ptr. All distances are squared for efficiency.
 */
void query_recursive(
    Node *node, const int *coord, const int depth,
    int *dists, const int dists_len, int *max_dist_ptr
) {

    const int diff_x = node->coord[0] - coord[0];
    const int diff_y = node->coord[1] - coord[1];
    const int dist = diff_x * diff_x + diff_y * diff_y;

    if (dist < *max_dist_ptr && dist != 0) {
        int pos_max = 0;
        for (int i = 1; i < dists_len; i++) {
            if (dists[i] > dists[pos_max]) {
                pos_max = i;
            }
        }
        dists[pos_max] = dist;
        *max_dist_ptr = dist;
    }

    const int axis = depth % 2;

    if (node->left != NULL) {
        const int min_dist_left = node->left->coord[axis] - coord[axis];
        if (min_dist_left * min_dist_left < *max_dist_ptr) {
            query_recursive(node->left, coord, depth + 1, dists, dists_len, max_dist_ptr);
        }
    }
    if (node->right != NULL) {
        const int min_dist_right = node->right->coord[axis] - coord[axis];
        if (min_dist_right * min_dist_right < *max_dist_ptr) {
            query_recursive(node->right, coord, depth + 1, dists, dists_len, max_dist_ptr);
        }
    }

}

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
 * Track the progress of map generation, and show the progress in the terminal.
 * start_time is the time at the start of map generation in main(), and the
 * other inputs are all shared memory used to track section progress and times.
 * Not used in automated inputs mode.
 */
void track_progress(
    struct timespec start_time,
    _Atomic int *section_progress, int *section_progress_total, float *section_times
) {

    // Setting Tracker Process Title

    #ifdef __linux__

        prctl(PR_SET_NAME, "biomegentracker", 0, 0, 0);

    #elif BSD || __Apple__

        setproctitle("biomegen-tracker");

    #endif

    char section_names[7][20] = {
        "Setup", "Section Generation", "Section Assignment", "Coastline Smoothing",
        "Biome Generation", "Image Generation", "Finish"
    };
    float section_weights[7] = {0.02f, 0.01f, 0.11f, 0.38f, 0.29f, 0.17f, 0.02f};
    // Used for overall progress bar (e.g. Setup takes ~2% of total time)

    while (true) {

        system("clear");

        // Section Progress

        struct timespec time_now;
        clock_gettime(CLOCK_REALTIME, &time_now);
        float time_diff = (float)(time_now.tv_sec - start_time.tv_sec) +
            (time_now.tv_nsec - start_time.tv_nsec) / 1000000000.0;

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
            char formatted_time[32];
            snprintf(
                formatted_time, 32, "%2d:%08.5f", (int)section_time / 60, fmod(section_time, 60.0f)
            );
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
        char formatted_time[32];
        float total_time = (float)(time_now.tv_sec - start_time.tv_sec) +
            (time_now.tv_nsec - start_time.tv_nsec) / 1000000000.0;
        snprintf(formatted_time, 32, "%2d:%08.5f", (int)total_time / 60, fmod(total_time, 60.0f));
        printf("%s%s\n", ANSI_RESET, formatted_time);

        // Checking Exit Status

        if (strcmp(color, ANSI_GREEN) == 0) {
            break; // All sections done, exit tracking process
        } else {
            // Sleep 0.1 seconds
            struct timespec sleep_time;
            sleep_time.tv_sec = 0;
            sleep_time.tv_nsec = 100000000;
            nanosleep(&sleep_time, &sleep_time);
        }

    }

}

/**
 * Assign sections of the map. Land and water are randomly assigned based on a
 * dot's distance from the nearest land origin dot. Returns the number of land
 * dots assigned for use in later sections.
 */
void assign_sections(
    const int map_resolution, const float island_size, const int start_index, const int end_index,
    const int num_origin_dots, const int origin_dots[num_origin_dots][2],
    Dot *dots, _Atomic int *section_progress, int return_pipe[2]
) {

    // Setting Worker Process Title

    #ifdef __linux__

        prctl(PR_SET_NAME, "biomegen-worker", 0, 0, 0);

    #elif BSD || __Apple__

        setproctitle("biomegen-worker");

    #endif

    int num_land_dots = 0;

    srand(time(NULL));

    for (int i = start_index; i < end_index; i++) {

        Dot *dot = &dots[i];

        if (dot->type[5] != '\0') {
        // Ignore "Water Forced" and "Land Origin", which are not 6 characters
            atomic_fetch_add(&section_progress[2], 1);
            continue;
        }

        int min; // min and dist are squared, sqrt is not done until later
        for (int ii = 0; ii < num_origin_dots; ii++) {

            const int diff_x = origin_dots[ii][0] - dot->x;
            const int diff_y = origin_dots[ii][1] - dot->y;
            const int dist = diff_x * diff_x + diff_y * diff_y;

            if (ii == 0 || dist < min) {
                min = dist;
            }

        }

        float threshold = ((float)(rand() % 20) / 19.0f * 1.5f + 0.25f) * island_size;
        int threshold_sq = (int)(threshold * threshold);

        int chance = (min <= threshold_sq) ? 9 : 1;

        if (rand() % 10 < chance) {
            strcpy(dot->type, "Land");
            num_land_dots++;
        }

        atomic_fetch_add(&section_progress[2], 1);

    }

    write(return_pipe[1], &num_land_dots, sizeof(int));

}

/**
 * Smooth map coastlines for a more realistic, aesthetically pleasing map.
 * Reassigns land and water dots based on the average distance of the nearest
 * coastline_smoothing dots of the same and opposite types.
 */
void smooth_coastlines(
    const int coastline_smoothing, const int width, const int height,
    const int start_index, const int end_index,
    const int num_dots, const int num_special_dots, const int num_reg_dots,
    Dot *dots, _Atomic int *section_progress
) {

    // Setting Worker Process Title

    #ifdef __linux__

        prctl(PR_SET_NAME, "biomegen-worker", 0, 0, 0);

    #elif BSD || __Apple__

        setproctitle("biomegen-worker");

    #endif

    for (int _ = 0; _ < 2; _++) {

        int num_land_dots = 0;
        int num_water_dots = 0;
        int *land_dots = malloc(num_reg_dots * 2 * sizeof(int));
        int *water_dots = malloc(num_reg_dots * 2 * sizeof(int));

        for (int i = num_special_dots; i < num_dots; i++) {
            if (dots[i].type[4] == '\0') {
                Dot *dot = &dots[i];
                land_dots[num_land_dots * 2] = dot->x;
                land_dots[num_land_dots * 2 + 1] = dot->y;
                num_land_dots++;
            } else if (dots[i].type[5] == '\0') {
                Dot *dot = &dots[i];
                water_dots[num_water_dots * 2] = dot->x;
                water_dots[num_water_dots * 2 + 1] = dot->y;
                num_water_dots++;
            }
        }

        Node *land_tree_root = NULL;
        Node *water_tree_root = NULL;

        // for (int i = num_special_dots; i < num_dots; i++) {
        //     Dot *dot = &dots[i];
        //     int coord[2] = {dot->x, dot->y};
        //     if (dot->type[4] == '\0') {
        //         land_tree_root = insert_recursive(land_tree_root, coord, 0);
        //     } else {
        //         water_tree_root = insert_recursive(water_tree_root, coord, 0);
        //     }
        // }

        land_tree_root = build_recursive(num_land_dots, land_dots, 0);
        water_tree_root = build_recursive(num_water_dots, water_dots, 0);

        record_val((int)(log(num_land_dots) / log(2)), "min land depth");
        record_val(get_depth(land_tree_root), "actual land depth");
        record_val((int)(log(num_water_dots) / log(2)), "min water depth");
        record_val(get_depth(water_tree_root), "actual water depth");

        free(land_dots);
        free(water_dots);

        for (int i = start_index; i < end_index; i++) {

            Dot *dot = &dots[i];
            int dot_coord[2] = {dot->x, dot->y};

            const bool land_dot = (dot->type[4] == '\0');

            if(!land_dot && dot->type[5] != '\0') {
                // ignore if dot.type != "Land" or "Water"
                atomic_fetch_add(&section_progress[3], 1);
                continue;
            }

            long sums[2] = {0, 0}; // same type, opposite
            Node *tree_roots[2];

            if (land_dot) {
                tree_roots[0] = land_tree_root;
                tree_roots[1] = water_tree_root;
            } else {
                tree_roots[0] = water_tree_root;
                tree_roots[1] = land_tree_root;
            }

            for (int ii = 0; ii < 2; ii++) {

                int dists[coastline_smoothing];
                for (int iii = 0; iii < coastline_smoothing; iii++) {
                    dists[iii] = INT_MAX;
                }

                int max_dist = INT_MAX;
                int *max_dist_ptr = &max_dist;

                query_recursive(
                    tree_roots[ii], dot_coord, 0, dists, coastline_smoothing, max_dist_ptr
                );

                for (int iii = 0; iii < coastline_smoothing; iii++) {
                    sums[ii] += dists[iii];
                }

            }

            if (sums[0] > sums[1]) {
                (land_dot) ? strcpy(dot->type, "Water") : strcpy(dot->type, "Land");
            }

            atomic_fetch_add(&section_progress[3], 1);
            
        }

        free_recursive(land_tree_root);
        free_recursive(water_tree_root);

    }

}

/**
 * Smooth map coastlines for a more realistic, aesthetically pleasing map.
 * Reassigns land and water dots based on the average distance of the nearest
 * coastline_smoothing dots of the same and opposite types.
 */
void smooth_coastlines_old(
    const int coastline_smoothing, const int width, const int start_index, const int end_index,
    const int num_dots, const int num_special_dots, const int num_reg_dots,
    Dot *dots, _Atomic int *section_progress
) {

    // Setting Worker Process Title

    #ifdef __linux__

        prctl(PR_SET_NAME, "biomegen-worker", 0, 0, 0);

    #elif BSD || __Apple__

        setproctitle("biomegen-worker");

    #endif

    int *land_dots = malloc(num_reg_dots * 2 * sizeof(int)); // list of land dot coords
    int *water_dots = malloc(num_reg_dots * 2 * sizeof(int));

    for (int _ = 0; _ < 2; _++) {

        int num_land_dots = 0;
        int num_water_dots = 0;

        for (int i = num_special_dots; i < num_dots; i++) {
            if (dots[i].type[4] == '\0') {
                Dot *dot = &dots[i];
                land_dots[num_land_dots * 2] = dot->x;
                land_dots[num_land_dots * 2 + 1] = dot->y;
                num_land_dots++;
            } else if (dots[i].type[5] == '\0') {
                Dot *dot = &dots[i];
                water_dots[num_water_dots * 2] = dot->x;
                water_dots[num_water_dots * 2 + 1] = dot->y;
                num_water_dots++;
            }
        }

        for (int i = start_index; i < end_index; i++) {

            Dot *dot = &dots[i];
            int dot_x = dot->x;
            int dot_y = dot->y;

            const bool land_dot = (dot->type[4] == '\0');

            if (!land_dot && dot->type[5] != '\0') {
                // ignore if dot.type != "Land" || "Water"
                atomic_fetch_add(&section_progress[3], 1);
                continue;
            }

            long sum_land;
            long sum_water;

            for (int ii = 0; ii < 2; ii++) {

                int dists[coastline_smoothing];
                for (int iii = 0; iii < coastline_smoothing; iii++) {
                    dists[iii] = INT_MAX;
                }
                int max_dist = INT_MAX;

                int num_comp_dots;
                if (ii == 0) {
                    num_comp_dots = num_land_dots;
                } else {
                    num_comp_dots = num_water_dots;
                }

                for (int iii = 0; iii < num_comp_dots; iii++) {

                    int diff_x;
                    int diff_y;
                    if (ii == 0) {
                        diff_x = land_dots[iii * 2] - dot_x;
                        diff_y = land_dots[iii * 2 + 1] - dot_y;
                    } else {
                        diff_x = water_dots[iii * 2] - dot_x;
                        diff_y = water_dots[iii * 2 + 1] - dot_y;
                    }
                    const int dist = diff_x * diff_x + diff_y * diff_y;

                    if (dist >= max_dist) {
                        // Outside dists, ignore
                        continue;
                    }

                    int pos_max = 0;
                    for (int iv = 1; iv < coastline_smoothing; iv++) {
                        if (dists[iv] > dists[pos_max]) {
                            pos_max = iv;
                        }
                    }

                    dists[pos_max] = dist;
                    max_dist = dist;

                }

                long sum = 0;
                for (int iii = 0; iii < coastline_smoothing; iii++) {
                    sum += dists[iii];
                }
                if (ii == 0) {
                    sum_land = sum;
                } else {
                    sum_water = sum;
                }

            }

            if (land_dot) {
                if (sum_land > sum_water) {
                    strcpy(dot->type, "Water");
                }
            } else {
                if (sum_water > sum_land) {
                    strcpy(dot->type, "Land");
                }
            }

            atomic_fetch_add(&section_progress[3], 1);

        }

    }

    free(land_dots);
    free(water_dots);

}


// Main Function

/**
 * Main Function. argc and argv used for automated inputs mode.
 */
int main(int argc, char *argv[]) {

    // Setting Main Process Title

    #ifdef __linux__

        prctl(PR_SET_NAME, "biomegen-main", 0, 0, 0);

    #elif BSD || __Apple__

        setproctitle("biomegen-main");

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
        epilepsy or not, but I put this just in case
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

    Dot *dots = mmap(
        NULL, sizeof(Dot) * num_dots, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0
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
    section_times[0] = (float)(time_now.tv_sec - start_time.tv_sec) +
        (time_now.tv_nsec - start_time.tv_nsec) / 1000000000.0;
    atomic_store(&section_progress[0], 1);

    // Section Generation
    // Creating the initial list of dots

    section_progress_total[1] = num_dots;

    srand(time(NULL));

    const int num_special_dots = num_dots / island_abundance * 2;
    const int num_reg_dots = num_dots - num_special_dots;

    bool *used_coords = calloc(width * height, sizeof(bool));

    for (int i = 0; i < num_dots; i++) {

        // Find unused coordinate

        int ii;
        do {
            ii = rand() % (width * height);
        } while (used_coords[ii]);

        // Create dot at rand_index[x, y]

        used_coords[ii] = true;

        Dot new_dot = { .x = ii % width, .y = ii / width };
        if (i < num_special_dots / 2) {
            strcpy(new_dot.type, "Land Origin"); // Origin points for islands
        } else if (i < num_special_dots) {
            strcpy(new_dot.type, "Water Forced"); // Forced water (good for making lakes)
        } else {
            strcpy(new_dot.type, "Water"); // Water (default)
        }

        dots[i] = new_dot;

        atomic_fetch_add(&section_progress[1], 1);

    }

    free(used_coords);

    clock_gettime(CLOCK_REALTIME, &time_now);
    section_times[1] = (float)(time_now.tv_sec - start_time.tv_sec) +
        (time_now.tv_nsec - start_time.tv_nsec) / 1000000000.0 - sum_list_float(section_times, 7);

    // Section Assignment
    // Assigning dots as "Land", "Land Origin", "Water", or "Water Forced"

    section_progress_total[2] = num_reg_dots;

    int origin_dots[num_special_dots / 2][2];
    for (int ii = 0; ii < num_special_dots / 2; ii++) {
        Dot *dot = &dots[ii];
        origin_dots[ii][0] = dot->x;
        origin_dots[ii][1] = dot->y;
    }

    const int piece_length = num_reg_dots / processes;

    int piece_starts[processes + 1];
    for (int i = 0; i < processes; i++) {
        piece_starts[i] = i * piece_length + num_special_dots;
    }
    piece_starts[processes - 1] = num_dots - piece_length;
    piece_starts[processes] = num_dots;
    /*
    used to create x pieces of size num_reg_dots / x, where x = processes
    last piece may be larger, special dots are skipped
    */

    int num_land_dots = 0;
    int return_pipe[2];
    pipe(return_pipe);
    int fork_pids[processes];
    for (int i = 0; i < processes; i++) {
        fork_pids[i] = fork();
        if (fork_pids[i] == 0) {
            assign_sections(
                map_resolution, island_size, piece_starts[i], piece_starts[i + 1],
                num_special_dots / 2, origin_dots, dots, section_progress, return_pipe
            );
            exit(0);
        }
    }
    for (int i = 0; i < processes; i++) {
        waitpid(fork_pids[i], NULL, 0);
        int return_val;
        read(return_pipe[0], &return_val, sizeof(int));
        num_land_dots += return_val;
    }

    clock_gettime(CLOCK_REALTIME, &time_now);
    section_times[2] = (float)(time_now.tv_sec - start_time.tv_sec) +
        (time_now.tv_nsec - start_time.tv_nsec) / 1000000000.0 - sum_list_float(section_times, 7);

    // Coastline Smoothing

    if (coastline_smoothing != 0) {

        section_progress_total[3] = num_reg_dots * 2;

        for (int i = 0; i < processes; i++) {
            fork_pids[i] = fork();
            if (fork_pids[i] == 0) {
                smooth_coastlines(
                    coastline_smoothing, width, height, piece_starts[i], piece_starts[i + 1],
                    num_dots, num_special_dots, num_reg_dots, dots, section_progress
                );
                exit(0);
            }
        }
        for (int i = 0; i < processes; i++) {
            waitpid(fork_pids[i], NULL, 0);
        }

    } else {

        atomic_store(&section_progress[3], 1);

    }

    clock_gettime(CLOCK_REALTIME, &time_now);
    section_times[3] = (float)(time_now.tv_sec - start_time.tv_sec) +
        (time_now.tv_nsec - start_time.tv_nsec) / 1000000000.0 - sum_list_float(section_times, 7);

    
    atomic_store(&section_progress[4], 1);
    atomic_store(&section_progress[5], 1);

    // Finish

    clock_gettime(CLOCK_REALTIME, &time_now);
    section_times[6] = (float)(time_now.tv_sec - start_time.tv_sec) +
        (time_now.tv_nsec - start_time.tv_nsec) / 1000000000.0 - sum_list_float(section_times, 7);
    atomic_store(&section_progress[6], 1);

    if (!auto_mode) {
        // Wait for Tracker Process
        waitpid(tracker_process_pid, NULL, 0);
    }

    // Shared Memory Cleanup

    munmap(section_progress, sizeof(int) * 7);
    munmap(section_progress_total, sizeof(int) * 7);
    munmap(section_times, sizeof(float) * 7);
    munmap(dots, sizeof(Dot) * num_dots);

    // Completion

    struct timespec end_time;
    clock_gettime(CLOCK_REALTIME, &end_time);
    float completion_time = (float)(time_now.tv_sec - start_time.tv_sec) +
        (time_now.tv_nsec - start_time.tv_nsec) / 1000000000.0;

    if (!auto_mode) {

        // Print Result Statistics

    } else {

        printf("%f\n", completion_time);

    }

    return 0;

}