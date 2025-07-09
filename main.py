# Copyright (C) 2025 Liam Ralph
# https://github.com/liam-ralph

# This program, including this file, is licensed under the
# GNU General Public License v3.0 (GNU GPLv3), with one exception.
# See LICENSE or this project's source for more information.
# Project Source: https://github.com/liam-ralph/biomegen

# result.png, the output of this program, is licensed under The Unlicense.
# See LICENSE_PNG or this project's source for more information.

# BiomeGen, a terminal application for generating png maps.


# Imports

# Multiprocessing

import multiprocessing

# Math

import math
import scipy
import scipy.spatial

# Other

import ctypes
import os
import PIL.Image
import random
import time
import traceback


# Classes

class Dot:
    def __init__(self, x, y, dot_type):
        self.x = x
        self.y = y
        self.type = dot_type


# Text Colors

ANSI_GREEN = "\u001b[38;5;2m"
ANSI_BLUE = "\u001b[38;5;4m"
ANSI_RESET = "\u001b[0m"


# Functions
# (Alphabetical order)

def clear_screen():
    command = "clear"
    if os.name in ("nt", "dos"):
        command = "cls"
    os.system(command)

def format_time(time_seconds): # E.g. 86.34521s --> 01:26.345
    seconds = f"{(time_seconds % 60):.3f}".rjust(6, "0")
    minutes = str(int(time_seconds // 60))
    return (minutes + ":" + seconds).rjust(8)

def get_int(min, max): # Integer input sanitization

    while True:

        choice = input()

        try:
            choice = int(choice)
        except ValueError:
            print("Input must be a whole number.")
            continue

        if min <= choice <= max:
            break
        else:
            print("Input must be between", min, "and", max, "(both inclusive).")

    return choice

def raise_error(location, traceback_output):
    # Errors in the terminal are often overwritten, so this saves them to error.txt
    with open("errors.txt", "a") as file:
        file.write("Error at " + location + "\n\n" + traceback_output + "\n\n\n")


# Multiprocessing Functions
# (Order of use)

def initialize_pool(section_progress_value, dots_value, image_sections_value, lock_value):

    # Creates shared variables

    global section_progress
    global dots
    global image_sections # Holder for image pieces in generate_image
    global lock # Lock to prevent two processes from updating the same variable simultaneously

    section_progress = section_progress_value
    dots = dots_value
    image_sections = image_sections_value
    lock = lock_value

def track_progress(section_progress, section_progress_total, section_times, start_time):

    try:

        section_names = [
            "Setup", "Section Generation", "Section Assignment", "Coastline Smoothing",
            "Biome Generation", "Image Generation", "Finish"
        ]
        section_weights = [0.02, 0.01, 0.11, 0.38, 0.29, 0.17, 0.02]
        # Used for overall progress bar (e.g. Setup takes ~2% of total time)

        while True:

            clear_screen()

            # Section Progress

            time_now = time.time()

            total_progress = 0

            for i in range(len(section_names)):

                progress_section = section_progress[i] / section_progress_total[i]
                total_progress += progress_section * section_weights[i]

                if section_progress[i] == section_progress_total[i]: # Checking if section complete
                    color = ANSI_GREEN
                    section_time = section_times[i] # Section complete
                else:
                    color = ANSI_BLUE
                    if i == 0 or section_times[i - 1] != 0: # Section is in progress
                        section_time = time.time() - start_time - sum(section_times)
                    else:
                        section_time = 0 # Section hasn't started

                print(
                    color + "[" + str(i + 1) + "/7] " + section_names[i].ljust(20) + # Section name
                    "{:.2f}% ".format(progress_section * 100).rjust(8) + # Section progress %
                    ANSI_GREEN + "█" * round(progress_section * 20) + # Green part of progress bar
                    ANSI_BLUE + "█" * (20 - round(progress_section * 20)) + # Blue part of bar
                    ANSI_RESET + " " + format_time(section_time) # Section time
                )

            # Total Progress

            if sum(section_progress) == sum(section_progress_total):
                color = ANSI_GREEN # All sections complete
            else:
                color = ANSI_BLUE # Section in progress
            print(
                color + "      Total Progress      " +
                "{:.2f}% ".format(total_progress * 100).rjust(8) + # Total progress %
                ANSI_GREEN + "█" * round(total_progress * 20) + # Green part of bar
                ANSI_BLUE + "█" * (20 - round(total_progress * 20)) + # Blue part
                ANSI_RESET + " " + format_time(time_now - start_time) # Total time
            )

            if sum(section_progress) == sum(section_progress_total):
                break # Tracking process ends when all section have completed
            else:
                time.sleep(0.1) # Delay before refreshing

    except:
        raise_error("track_progress", traceback.format_exc())

def copy_piece(piece_range):
    # For copying dots into local_dots, as a local variable is faster to access
    return dots[piece_range[0]:piece_range[1]]

def assign_sections(map_resolution, island_size, piece_range, origin_dots, local_dots):

    try:

        # Used to find the nearest origin dot
        tree = scipy.spatial.KDTree([(dot.x, dot.y) for dot in origin_dots])

        for i in range(piece_range[0], piece_range[1]):

            dot = local_dots[i]

            if dot.type == "Water": # Ignore "Water Forced" and "Land Origin"

                index = tree.query((dot.x, dot.y))[1] # Find index of nearest origin dot

                nearest_origin_dot = origin_dots[index]

                dist = (
                    math.sqrt(
                        (nearest_origin_dot.x - dot.x) ** 2 + (nearest_origin_dot.y - dot.y) ** 2
                    ) / math.sqrt(map_resolution)
                ) # Find distance between dot and nearest_origin_dot

                if dist <= (index % 20 / 19 * 1.5 + 0.25) * island_size:
                # Random num (0.25 - 1.75) * island_size
                    chance = 0.9
                else:
                    chance = 0.1

                if random.random() < chance:
                    dots[i] = Dot(dot.x, dot.y, "Land")

            with lock:
                section_progress[2] += 1

    except:
        raise_error("assign_sections", traceback.format_exc())

def smooth_coastlines(coastline_smoothing, piece_range, local_dots):

    try:

        for i in (1, -1):
        # Smooths starting with first and last dot
        # (shouldn't make a difference, more of a just in case)

            land_dots = [dot for dot in local_dots if dot.type == "Land"]
            water_dots = [dot for dot in local_dots if dot.type == "Water"]

            land_tree = scipy.spatial.KDTree([(dot.x, dot.y) for dot in land_dots])
            # Measures distance to nearest land dot
            water_tree = scipy.spatial.KDTree([(dot.x, dot.y) for dot in water_dots])
            # Measures distance to nearest water dot

            list_dots = list(range(piece_range[0], piece_range[1]))

            if i == -1:
                list_dots.reverse()

            for ii in list_dots:

                dot = dots[ii]

                types = ["Land", "Water"]

                if dot.type not in types: # "Water Forced" and "Land Origin"
                    with lock:
                        section_progress[3] += 1
                    continue

                same_dist = 0.0 # Distance to nearest dot of same type
                opp_dist = 1.0

                if dot.type == "Land":

                    same_dist = land_tree.query((dot.x, dot.y), k=coastline_smoothing)[0]
                    # Includes the nearest k dots
                    # Larger number of dots creates more clumping and smoother coastlines
                    opp_dist = water_tree.query((dot.x, dot.y), k=coastline_smoothing)[0]

                elif dot.type == "Water":

                    same_dist = water_tree.query((dot.x, dot.y), k=coastline_smoothing)[0]
                    opp_dist = land_tree.query((dot.x, dot.y), k=coastline_smoothing)[0]
    
                if type(same_dist) is float: # If coastline_smoothing == 1
                    same_dist = [same_dist]
                    opp_dist = [opp_dist]
                if sum(same_dist) > sum(opp_dist):
                # If average distance to the same type of dot is greater
                # than average distance to opposite type dot for the nearest k dots
                    types.remove(dot.type)
                    with lock:
                        dots[ii] = Dot(dot.x, dot.y, types[0])

                with lock:
                    section_progress[3] += 1

    except:
        raise_error("smooth_coastlines", traceback.format_exc())

def clean_dots(local_dots_section, start_index):

    try:

        # Remove all "Land Origin" and "Water Forced", which become "Land" and "Water"

        for i in range(len(local_dots_section)):

            dot = local_dots_section[i]

            if dot.type == "Land Origin":

                dots[i + start_index] = Dot(dot.x, dot.y, "Land")

            elif dot.type == "Water Forced":

                dots[i + start_index] = Dot(dot.x, dot.y, "Water")

    except:
        raise_error("clean_dots", traceback.format_exc())

def generate_biomes_water(piece_range, local_dots, height):

    try:

        tree = (
            scipy.spatial.KDTree([(dot.x, dot.y) for dot in local_dots if dot.type == "Land"])
        )
        # Finds nearest land dot

        for i in [index for index in range(piece_range[0], piece_range[1])
        if local_dots[index].type == "Water"]: # For every "Water" dot in piece_range

            dot = dots[i]

            equator_dist = abs(dot.y - height / 2) / height * 20
            # Distance from equator 0-10, where 0 is on equator and 10 is top or bottom of page
            land_dist = tree.query((dot.x, dot.y))[0]
            # Distance to nearest land dot

            if (
                (land_dist < 35 and equator_dist > 9) or
                (land_dist < 25 and equator_dist > 8) or
                (land_dist < 15 and equator_dist > 7)
            ):
                dot_type = "Ice" # All water near poles is ice
            elif land_dist < 18:
                dot_type = "Shallow Water" # Near land is shallow
            elif land_dist < 35:
                dot_type = "Water"
            else:
                dot_type = "Deep Water" # Far from land is deep

            dots[i] = Dot(dot.x, dot.y, dot_type)

            with lock:
                section_progress[4] += 1

    except:
        raise_error("generate_biomes_water", traceback.format_exc())

def assign_biomes(piece_range, biome_origin_dots, local_dots):

    try:

        tree = scipy.spatial.KDTree([(dot.x, dot.y) for dot in biome_origin_dots])
        # Finds nearest biome origin dot
        # (a dot that sets the surrounding land to be a certain biome)

        for i in [index for index in range(piece_range[0], piece_range[1])
        if local_dots[index].type == "Land"]: # For every "Land" dot

            dot = local_dots[i]

            dots[i] = Dot(dot.x, dot.y, biome_origin_dots[tree.query((dot.x, dot.y))[1]].type)
            # Dot becomes the type of the nearest biome origin dot
            
            with lock:
                section_progress[4] += 1

    except:
        raise_error("assign_biomes", traceback.format_exc())

def generate_image(start_height, section_height, process_num, local_dots, width):

    try:

        type_counts = [0] * 11
        # Counts pixels of each biome and water type for statistics

        image_local = (
            PIL.Image.new("RGB", (width, section_height), (255, 153, 194))
        ) # Create empty image for section
        pixels = image_local.load() # Load image into pixel array
        tree = scipy.spatial.KDTree([(dot.x, dot.y) for dot in local_dots])
        # Find nearest dot to a point

        for y in range(section_height):

            indexes = tree.query([(x, y + start_height) for x in range(width)])[1]
            # Finds nearest dot's index for every x value in the row y

            for x in range(width):

                pixel_type = "Error"
                pixel_type = local_dots[indexes[x]].type # Find pixel type

                type_counts[(
                    "Ice", "Shallow Water", "Water", "Deep Water",
                    "Rock", "Desert", "Jungle", "Forest", "Plains", "Taiga", "Snow"
                ).index(pixel_type)] += 1

                match (pixel_type): # Assign color based on type
                    case "Ice":
                        colors = [153, 221, 255]
                    case "Shallow Water":
                        colors = [0, 0, 255]
                    case "Water":
                        colors = [0, 0, 179]
                    case "Deep Water":
                        colors = [0, 0, 128]
                    case "Rock":
                        colors = [128, 128, 128]
                    case "Desert":
                        colors = [255, 185, 109]
                    case "Jungle":
                        colors = [0, 77, 0]
                    case "Forest":
                        colors = [0, 128, 0]
                    case "Plains":
                        colors = [0, 179, 0]
                    case "Taiga":
                        colors = [152, 251, 152]
                    case "Snow":
                        colors = [245, 245, 245]
                    case "Error":
                        colors = [255, 102, 163]
                    case _:
                        colors = [204, 0, 82]
                for i in range(len(colors)):
                    # Adds slight color variation
                    # Every pixel around the same dot has the same variation
                    colors[i] += indexes[x] % 20 - 10
                    if colors[i] > 255:
                        colors[i] = 255
                    elif colors[i] < 0:
                        colors[i] = 0
                pixels[x, y] = tuple(colors) # Update image section with pixel color

            with lock:
                section_progress[5] += 1

        image_sections[process_num] = image_local

        return type_counts

    except:
        raise_error("generate_image", traceback.format_exc())
        return [0] * 11


# Main Function

def main():

    with open("errors.txt", "w") as file:
        file.write("")

    clear_screen()

    # Copyright, license notice, etc.
    print(
        "Welcome to BiomeGen v1.0\n" +
        "Copyright (C) 2025 Liam Ralph\n" +
        "https://github.com/liam-ralph\n" +
        "This project is licensed under the GNU General Public License v3.0,\n" +
        "except for result.png, this program's output, licensed under The Unlicense.\n" +
        "\u001b[38;5;1mWARNING: In some terminals, the refreshing progress screen\n" + 
        "may flash, which could cause problems for people with epilepsy.\n" + ANSI_RESET +
        "Press ENTER to begin."
    )
    # I do not know if the flashing lights this program sometimes makes could reasonably cause
    # epilepsy or not, but I put this just in case
    input()
    clear_screen()

    print("Map Width (pixels):")
    width = get_int(500, 10_000)

    print("\nMap Height:")
    height = get_int(500, 10_000)

    print(
        "\nMap resolution controls the section size of the map.\n" +
        "Choose a number between 50 and 500. 100 is the default.\n" +
        "Larger numbers produce lower resolutions, with larger pieces\n" +
        "while lower numbers take longer to generate.\n" +
        "Map Resolution:"
    )
    map_resolution = get_int(50, 500)

    print(
        "\nIsland abundance control how many islands there are,\n" +
        "and the ration of land to water.\n" +
        "Choose a number between 10 and 1000. 50 is the default.\n" +
        "Larger numbers produces less land.\n" +
        "Island Abundance:"
    )
    island_abundance = get_int(10, 1000)

    print(
        "\nIsland size controls average island size.\n" +
        "Choose a number between 10 and 100. 40 is the default.\n" +
        "Larger numbers produce larger islands.\n" +
        "Island Size:"
    )
    island_size = get_int(10, 100) / 10

    print(
        "\nCoastline smoothing controls how smooth or rough coastlines look.\n" +
        "Choose a number between 1 and 100. Larger numbers cause more smoothing.\n" +
        "A value of 0 causes no smoothing. A value of 10 causes moderate smoothing,\n" +
        "and is the default value.\n" +
        "Coastline Smoothing:"
    )
    coastline_smoothing = get_int(0, 100)

    print(
        "\nNow you must choose how many of your CPU's threads to use for map generation.\n" +
        "Values exceeding your CPU's number of threads will slow map generation.\n" +
        "The most efficient number of threads to use varies by hardware, OS,\n" +
        "and CPU load. Values less than 4 threads are usually very inefficient.\n" +
        "Ensure you monitor your CPU for overheating, and halt the program if\n" +
        "high temperatures occur. Using fewer threads may reduce temperatures.\n" +
        "Number of Threads:"
    )
    processes = get_int(1, 64) # Change this for CPUs with >64 threads

    start_time = time.time()

    clear_screen()

    manager = multiprocessing.Manager()

    section_progress = multiprocessing.Array(ctypes.c_int, [0, 0, 0, 0, 0, 0, 0])
    section_progress_total = multiprocessing.Array(ctypes.c_int, [1, 1, 1, 1, 1, 1, 1])
    section_times = multiprocessing.Array(ctypes.c_double, [0, 0, 0, 0, 0, 0, 0])
    dots = manager.list([])
    image_sections = manager.list([PIL.Image.new("RGB", (100, 100), (255, 0, 102))] * processes)
    lock = multiprocessing.Lock()
    # Lock to prevent two processes from updating the same variable simultaneously

    with multiprocessing.Pool(processes, initializer=initialize_pool,
    initargs=(section_progress, dots, image_sections, lock)) as pool:

        try:

            # Progress Tracking

            tracker_process = multiprocessing.Process(target=track_progress,
                args=(section_progress, section_progress_total, section_times, start_time))
            tracker_process.start()

            section_times[0] = time.time() - start_time
            # Everyting from "start_time = " to here is part of Setup
            section_progress[0] = 1

            # Section Generation
            # Creating the initial list of dots

            num_dots = width * height // map_resolution
            # The map is divided by a number of dots, which form polygons out of the nearest pixels
            # to each dot, so only dots are used during map generation, and pixels are only assigned
            # at the very end

            section_progress_total[1] = num_dots
            # A section's progress = section_progress[x] / section_progress_total[x]
            # For this section, the total number of "steps" taken == num_dots

            coords = random.sample(range(0, width * height), num_dots)
            # Randomly creates coords for each dot, not in any order
            local_dots = [] # Faster to add items to local_dots than dots
            num_special_dots = num_dots // island_abundance

            [local_dots.append(Dot(coords[i] % width, coords[i] // width, "Land Origin"))
                for i in range(num_special_dots)]
            # Add x "Land Origin" dots with random coords to local_dots, where x = num_special_dots
            section_progress[1] = num_special_dots
            [local_dots.append(Dot(coords[i] % width, coords[i] // width, "Water Forced"))
                for i in range(num_special_dots, num_special_dots * 2)]
            section_progress[1] += num_special_dots
            [local_dots.append(Dot(coords[i] % width, coords[i] // width, "Water"))
                for i in range(num_special_dots * 2, num_dots)]
            section_progress[1] = num_dots

            dots.extend(local_dots)

            section_times[1] = time.time() - start_time - sum(section_times)

            # Section Assignment
            # Assigning dots as "Land", "Land Origin", "Water", or "Water Forced"

            section_progress_total[2] = num_dots

            piece_lengths = [num_dots // processes] * (processes - 1)
            piece_lengths.append(num_dots - sum(piece_lengths))
            piece_ranges = [
                [i * piece_lengths[0], i * piece_lengths[0] + piece_lengths[i]]
                for i in range(processes)
            ]
            # Used to create x pieces of around size num_dots / x, where x = num_processes

            local_dots = []
            results = pool.map(copy_piece, piece_ranges)
            for result in results:
                local_dots.extend(result)
            # Copying dots in local_dots, since local variables are faster to access

            origin_dots = [dot for dot in local_dots if dot.type == "Land Origin"]

            results = []
            # results list needed for result.wait(), no result is actually returned in most cases
            for i in range(processes):
                results.append(pool.apply_async(assign_sections,
                    (map_resolution, island_size, piece_ranges[i], origin_dots, local_dots)))
            [result.wait() for result in results] # Wait for all process to finish assign_sections

            section_times[2] = time.time() - start_time - sum(section_times)

            # Coastline Smoothing

            if coastline_smoothing != 0:

                section_progress_total[3] = num_dots * 2

                local_dots = []
                results = pool.map(copy_piece, piece_ranges)
                for result in results:
                    local_dots.extend(result)

                results = []
                for i in range(processes):
                    results.append(pool.apply_async(smooth_coastlines,
                        (coastline_smoothing, piece_ranges[i], local_dots)))
                    # piece_ranges is reused multiple times without being remade
                [result.wait() for result in results]

            else: # Skip everything, no smoothing needed

                section_progress[3] = 1

            section_times[3] = time.time() - start_time - sum(section_times)

            # Biome Generation
            # Creating biomes

            section_progress_total[4] = num_dots

            # Removing "Land Origin" and "Water Forced" dots, they aren't needed anymore

            local_dots = []
            results = pool.map(copy_piece, piece_ranges)
            for result in results:
                local_dots.extend(result)

            results = []
            for i in range(processes):
                results.append(pool.apply_async(clean_dots,
                    (local_dots[piece_ranges[i][0]:piece_ranges[i][1]], piece_ranges[i][0])))
            [result.wait() for result in results]

            # Creating water biomes to add depth and ice at poles

            local_dots = []
            results = pool.map(copy_piece, piece_ranges)
            for result in results:
                local_dots.extend(result)

            results = []
            for i in range(processes):
                results.append(pool.apply_async(
                    generate_biomes_water, (piece_ranges[i], local_dots, height)))
            [result.wait() for result in results]

            # Adding "biome origin dots", which decide what biome that area of land will be

            local_dots = []
            results = pool.map(copy_piece, piece_ranges)
            for result in results:
                local_dots.extend(result)

            biome_origin_dot_indexes = [i for i in random.sample(range(0, num_dots), num_dots // 10)
                if local_dots[i].type == "Land"] # 10% of all land dots become biome origin dots

            for i in biome_origin_dot_indexes:

                dot = local_dots[i]
                equator_dist = abs(dot.y - height / 2) / height * 20
                # Distance from equator 0-10, where 0 is on equator and 10 is top or bottom of page

                if equator_dist < 1:
                    probs = ["Rock"] + ["Desert"] * 3 + ["Jungle"] * 3 + ["Forest"] * 2 + ["Plains"]
                elif equator_dist < 2:
                    probs = (
                        ["Rock"] + ["Desert"] * 3 + ["Jungle"] * 2 + ["Forest"] * 2 + ["Plains"] * 2
                    )
                elif equator_dist < 3:
                    probs = ["Rock"] + ["Desert"] * 2 + ["Jungle"] + ["Forest"] * 3 + ["Plains"] * 3
                elif equator_dist < 4:
                    probs = ["Rock"] + ["Desert"] + ["Jungle"] + ["Forest"] * 3 + ["Plains"] * 4
                elif equator_dist < 5:
                    probs = ["Rock"] + ["Desert"] + ["Forest"] * 4 + ["Plains"] * 4
                elif equator_dist < 6:
                    probs = ["Rock"] +  ["Forest"] * 5 + ["Plains"] * 4
                elif equator_dist < 7:
                    probs = ["Rock"] + ["Taiga"] + ["Forest"] * 5 + ["Plains"] * 3
                elif equator_dist < 8:
                    probs = (
                        ["Rock"] +  ["Snow"] * 2 + ["Taiga"] * 2 + ["Forest"] * 3 + ["Plains"] * 2
                    )
                elif equator_dist < 9:
                    probs = ["Snow"] * 4 + ["Taiga"] * 5 + ["Forest"]
                else:
                    probs = ["Snow"] * 10

                # Probability Chart, 1 box = 10% Chance
                # r = Rock, D = Desert, etc. Numbers represent equator distance
                # Uppercase/lowercase are an attempt to make it easier to read, they mean nothing
                # 0-1 | r D D D J J J f f P
                # 1-2 | r D D D J J f f P P
                # 2-3 | r D D J f f f P P P
                # 3-4 | r D J f f f P P P P
                # 4-5 | r D f f f f P P P P
                # 5-6 | r f f f f f P P P P
                # 6-7 | r T f f f f f P P P
                # 7-8 | r s s T T f f f P P
                # 8-9 | s s s s T T T T f f
                # 9-10| s s s s s s s s s s

                dot_type = probs[random.randint(0, 9)]

                dots[i] = Dot(dot.x, dot.y, dot_type)

                section_progress[4] += 1

            # Add land biomes, dots are assigned the biome of the nearest biome origin dot

            local_dots = []
            results = pool.map(copy_piece, piece_ranges)
            for result in results:
                local_dots.extend(result)

            biome_origin_dots = [local_dots[i] for i in biome_origin_dot_indexes]

            results = []
            for i in range(processes):
                results.append(pool.apply_async(
                    assign_biomes, (piece_ranges[i], biome_origin_dots, local_dots)))
            [result.wait() for result in results]

            section_times[4] = time.time() - start_time - sum(section_times)

            # Image Generation

            section_progress_total[5] = height

            local_dots = []
            results = pool.map(copy_piece, piece_ranges)
            for result in results:
                local_dots.extend(result)

            start_heights = list(range(0, height, height // processes))
            section_heights = [height // processes] * (processes - 1)
            section_heights.append(height - sum(section_heights))
            # Image is generated in x sections, where x = num_processes
            # Sections are full width, but only around height / num_processes

            results = []
            for i in range(processes):
                results.append(pool.apply_async(generate_image,
                    (start_heights[i], section_heights[i], i, local_dots, width)))
                # i tells process which section it is
            [result.wait() for result in results]

            type_counts = [0] * 11 # Counting total pixels of each biome and water type
            for result in results:
                for i in range(11):
                    type_counts[i] += result.get()[i]

            section_times[5] = time.time() - start_time - sum(section_times)

            # Image Stitching

            image = PIL.Image.new("RGB", (width, height), (255, 51, 133)) # Create blank image
            shift = 0
            for section in image_sections: # Adds image section onto image
                image.paste(section, (0, shift))
                shift += section_heights[0]
            image.save("result.png") # Change this to change result location

            section_times[6] = time.time() - start_time - sum(section_times)
            section_progress[6] = 1

        except:
            raise_error("main", traceback.format_exc())

    tracker_process.join() # Tracker process closes self after all sections complete
    print(
        ANSI_GREEN + "Generation Complete " + ANSI_RESET +
        format_time(time.time() - start_time) + "\n\nStatistics"
    )

    types = (
        "Ice", "Shallow Water", "Water", "Deep Water",
        "Rock", "Desert", "Jungle", "Forest", "Plains", "Taiga", "Snow"
    )
    text_colors = ("117", "21", "19", "17", "243", "229", "22", "28", "40", "48", "255")
    # Colored text for biome labels

    count_water = sum(type_counts[:4])
    count_land = sum(type_counts[4:])
    print(
        "Water " +
        "{:.2f}%".format(count_water / (height * width) * 100).rjust(6)
    )
    print(
        "Land  " +
        "{:.2f}%".format(count_land / (height * width) * 100).rjust(6)
    )

    print("     % of Land/Water | % of Total")
    for i in range(11):
        if i < 4:
            count_group = count_water
        else:
            count_group = count_land
        print(
            "\u001b[48;5;" + text_colors[i] + "m" + types[i].ljust(13) + ANSI_RESET + # Label
            "{:.2f}%".format(type_counts[i] / count_group * 100).rjust(7) + " | " +
            # Percentage of land/water e.g. 30% of all land is forest
            "{:.2f}%".format(type_counts[i] / (height * width) * 100).rjust(6) # Overall percentage
        )


if __name__ == "__main__":
    main()
