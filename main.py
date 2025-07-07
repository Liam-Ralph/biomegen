# Copyright (C) 2025 Liam Ralph
# https://github.com/liam-ralph

# This program, including this file, is licensed under the
# GNU General Public License v3.0 (GNU GPLv3), with one exception.
# See LICENSE or this project's source for more information.
# Project Source: https://github.com/liam-ralph/map-maker

# result.png, the output of this program, is licensed under The Unlicense.
# See LICENSE_PNG or this project's source for more information.

# MapMaker, a terminal application for generating png maps.


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

def format_time(time_seconds):
    seconds = f"{(time_seconds % 60):.3f}".rjust(6, "0")
    minutes = str(int(time_seconds // 60))
    return (minutes + ":" + seconds).rjust(7)

def get_int(min, max):

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

def raise_error(location, traceback_output, notes=None):
    with open("errors.txt", "a") as file:
        file.write("Error at " + location + "\n\n" + traceback_output + "\n")
        if notes != None:
            for note in notes:
                file.write(note)
        file.write("\n\n")


# Multiprocessing Functions
# (Order of use)

def initialize_pool(section_progress_value, dots_value, image_sections_value, lock_value):

    global section_progress
    global dots
    global image_sections
    global lock

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
        section_weights = [0.05, 0.01, 0.09, 0.20, 0.30, 0.15, 0.20]

        while True:

            clear_screen()
            total_progress = 0

            # Section Progress

            time_now = time.time()

            for i in range(len(section_names)):

                section_total = section_progress_total[i]
                progress_section = section_progress[i] / section_total
                total_progress += progress_section * section_weights[i]

                if section_progress[i] == section_progress_total[i]:
                    color = ANSI_GREEN
                    section_time = section_times[i]
                else:
                    color = ANSI_BLUE
                    if i == 0 or section_times[i - 1] != 0:
                        section_time = time.time() - start_time - sum(section_times)
                    else:
                        section_time = 0

                print(
                    color + "[" + str(i + 1) + "/7] " + section_names[i].ljust(20) +
                    "{:.2f}% ".format(progress_section * 100).rjust(8) +
                    ANSI_GREEN + "█" * round(progress_section * 20) +
                    ANSI_BLUE + "█" * (20 - round(progress_section * 20)) +
                    ANSI_RESET + " " + format_time(section_time)
                )

            # Total Progress

            if sum(section_progress) == sum(section_progress_total):
                color = ANSI_GREEN
            else:
                color = ANSI_BLUE
            print(
                color + "      Total Progress      " +
                "{:.2f}% ".format(total_progress * 100).rjust(8) +
                ANSI_GREEN + "█" * round(total_progress * 20) +
                ANSI_BLUE + "█" * (20 - round(total_progress * 20)) +
                ANSI_RESET + " " + format_time(time_now - start_time)
            )

            if sum(section_progress) == sum(section_progress_total):
                break
            else:
                time.sleep(0.1)

    except:
        raise_error(
            "track_progress", traceback.format_exc()
        )

def copy_piece(piece_range):
    return dots[piece_range[0]:piece_range[1]]

def assign_sections(piece_range, map_resolution, local_dots, origin_dots, island_size):

    try:

        tree = scipy.spatial.KDTree([(dot.x, dot.y) for dot in origin_dots])

        for i in range(piece_range[0], piece_range[1]):

            dot = local_dots[i]

            if dot.type == "Water":

                index = tree.query((dot.x, dot.y))[1]

                nearest_origin_dot = origin_dots[index]

                dist = (
                    math.sqrt(
                        (nearest_origin_dot.x - dot.x) ** 2 + (nearest_origin_dot.y - dot.y) ** 2
                    ) / math.sqrt(map_resolution)
                )

                if dist <= (index % 20 / 19 * 1.5 + 0.25) * island_size:
                    chance = ((0.9) ** (1 / dist)) ** dist
                else:
                    chance = ((0.1) ** (1 / dist)) ** dist

                if chance > 1.0:
                    chance = 1.0
                if random.random() < chance:
                    dots[i] = Dot(dot.x, dot.y, "Land")

            with lock:
                section_progress[2] += 1

    except:
        raise_error(
            "assign_sections", traceback.format_exc(),
            notes = (
                "Input \"map_resolution\": " + str(map_resolution),
            )
        )

def smooth_coastlines(piece_range, coastline_smoothing, local_dots):

    try:

        for i in (1, -1):

            land_dots = [dot for dot in local_dots if dot.type == "Land"]
            water_dots = [dot for dot in local_dots if dot.type == "Water"]

            land_tree = scipy.spatial.KDTree([(dot.x, dot.y) for dot in land_dots])
            water_tree = scipy.spatial.KDTree([(dot.x, dot.y) for dot in water_dots])

            list_dots = list(range(piece_range[0], piece_range[1]))

            if i == -1:
                list_dots.reverse()

            for ii in list_dots:

                dot = dots[ii]

                types = ["Land", "Water"]

                if dot.type not in types:
                    with lock:
                        section_progress[3] += 1
                    continue

                same_dist = 0.0
                opp_dist = 1.0

                if dot.type == "Land":

                    same_dist = land_tree.query((dot.x, dot.y), k=coastline_smoothing)[0]
                    opp_dist = water_tree.query((dot.x, dot.y), k=coastline_smoothing)[0]

                elif dot.type == "Water":

                    same_dist = water_tree.query((dot.x, dot.y), k=coastline_smoothing)[0]
                    opp_dist = land_tree.query((dot.x, dot.y), k=coastline_smoothing)[0]
                if type(same_dist) is float:
                    same_dist = [same_dist]
                    opp_dist = [opp_dist]
                if dot.type in types and sum(same_dist) > sum(opp_dist):
                    types.remove(dot.type)
                    with lock:
                        dots[ii] = Dot(dot.x, dot.y, types[0])

                with lock:
                    section_progress[3] += 1

    except:
        raise_error(
            "smooth_coastlines", traceback.format_exc(),
            notes = (
                "Input \"coastline smoothing\": " + str(coastline_smoothing),
            )
        )

def clean_dots(start_index, local_dots_section):

    try:

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

        for i in [index for index in range(piece_range[0], piece_range[1]) if local_dots[index].type == "Water"]:

            dot = dots[i]

            equator_dist = abs(dot.y - height / 2) / height * 20
            land_dist = tree.query((dot.x, dot.y))[0]

            if (
                (land_dist < 35 and equator_dist > 9) or
                (land_dist < 25 and equator_dist > 8) or
                (land_dist < 15 and equator_dist > 7)
            ):
                dot_type = "Ice"
            elif land_dist < 18:
                dot_type = "Shallow Water"
            elif land_dist < 35:
                dot_type = "Water"
            else:
                dot_type = "Deep Water"

            dots[i] = Dot(dot.x, dot.y, dot_type)

            with lock:
                section_progress[4] += 1

    except:
        raise_error("generate_biomes_water", traceback.format_exc())

def assign_biomes(piece_range, biome_origin_dots, local_dots):

    try:

        tree = scipy.spatial.KDTree([(dot.x, dot.y) for dot in biome_origin_dots])

        for i in [index for index in range(piece_range[0], piece_range[1]) if local_dots[index].type == "Land"]:

            dot = local_dots[i]

            dots[i] = Dot(dot.x, dot.y, biome_origin_dots[tree.query((dot.x, dot.y))[1]].type)
            
            with lock:
                section_progress[4] += 1

    except:
        raise_error("clean_dots", traceback.format_exc())

def generate_image(start_height, section_height, process_num, local_dots, width):

    try:

        image_local = (
            PIL.Image.new("RGB", (width, section_height), (255, 153, 194))
        )
        pixels = image_local.load()
        tree = scipy.spatial.KDTree([(dot.x, dot.y) for dot in local_dots])

        for y in range(section_height):

            indexes = tree.query([(x, y + start_height) for x in range(width)])[1]

            for x in range(width):

                pixel_type = "Error"
                pixel_type = local_dots[indexes[x]].type

                match (pixel_type):
                    case "Ice":
                        colors = [153, 221, 255]
                    case "Snow":
                        colors = [245, 245, 245]
                    case "Shallow Water":
                        colors = [0, 0, 255]
                    case "Water":
                        colors = [0, 0, 179]
                    case "Deep Water":
                        colors = [0, 0, 128]
                    case "Sand":
                        colors = [255, 153, 51]
                    case "Desert":
                        colors = [255, 185, 109]
                    case "Forest":
                        colors = [0, 128, 0]
                    case "Taiga":
                        colors = [152, 251, 152]
                    case "Jungle":
                        colors = [0, 77, 0]
                    case "Plains":
                        colors = [0, 179, 0]
                    case "Rock":
                        colors = [128, 128, 128]
                    case "Error":
                        colors = [255, 102, 163]
                    case _:
                        colors = [204, 0, 82]
                for i in range(len(colors)):
                    colors[i] += indexes[x] % 20 - 10
                    if colors[i] > 255:
                        colors[i] = 255
                    elif colors[i] < 0:
                        colors[i] = 0
                pixels[x, y] = tuple(colors)

            with lock:
                section_progress[5] += 1

        image_sections[process_num] = image_local

    except:
        raise_error(
            "generate_image", traceback.format_exc(),
            notes=(
                "Input \"start_height\": " + str(start_height),
                "Input \"section_height\": " + str(section_height),
                "Input \"process_num\": " + str(process_num),
                "Input \"width\": " + str(width)
            )
        )


# Main Function

def main():

    with open("errors.txt", "w") as file:
        file.write("")

    clear_screen()

    print(
        "Welcome to MapMaker v1.0\n" +
        "\nMap Width:"
    )
    width = get_int(500, 10_000)

    print("\nMap Height:")
    height = get_int(500, 10_000)

    print(
        "\nMap resolution controls the section size of the map.\n" +
        "Choose a number between 50 and 500. 100 is the default.\n" +
        "Larger numbers produce lower resolutions,\n" +
        "while lower numbers take longer and have less variation.\n" +
        "Map Resolution:"
    )
    map_resolution = get_int(50, 500)

    print(
        "\nIsland abundance controls the ratio of land to water.\n" +
        "Choose a number between 10 and 90. 50 is the default.\n" +
        "Larger numbers produces less land.\n" +
        "Island Abundance:"
    )
    island_abundance = get_int(10, 90)

    print(
        "\nIsland size controls average island size.\n" +
        "Choose a number between 10 and 100. 40 is the default.\n" +
        "Larger numbers produce larger islands.\n" +
        "Island Size:"
    )
    island_size = get_int(10, 100) / 10

    print(
        "\nCoastline smoothing controls how clean coastlines look.\n" +
        "Integers between 1 and 100 cause smoother coastlines, fewer islands,\n" +
        "and fewer lakes. A value of 0 causes no smoothing. A value of 10\n" +
        "moderate smoothing. Larger numbers produce more smoothing.\n" +
        "Coastline Smoothing:"
    )
    coastline_smoothing = get_int(0, 100)

    print(
        "\nNow you must choose how many of your CPU's threads to use for map generation.\n" +
        "Values exceeding your CPU's number of threads will slow map generation.\n" +
        "Ensure you monitor your CPU for overheating, and halt the program if\n" +
        "high temperatures occur. Using fewer threads may reduce temperatures.\n" +
        "Number of Threads:"
    )
    processes = get_int(1, 32)

    start_time = time.time()

    clear_screen()

    manager = multiprocessing.Manager()

    section_progress = multiprocessing.Array(ctypes.c_int, [0, 0, 0, 0, 0, 0, 0])
    section_progress_total = multiprocessing.Array(ctypes.c_int, [1, 1, 1, 1, 1, 1, 1])
    section_times = multiprocessing.Array(ctypes.c_double, [0, 0, 0, 0, 0, 0, 0])
    dots = manager.list([])
    image_sections = manager.list([PIL.Image.new("RGB", (100, 100), (255, 0, 102))] * processes)
    lock = multiprocessing.Lock()

    with multiprocessing.Pool(processes, initializer=initialize_pool,
    initargs=(section_progress, dots, image_sections, lock)) as pool:

        try:

            # Progress Tracking

            tracker_process = multiprocessing.Process(target=track_progress,
                args=(section_progress, section_progress_total, section_times, start_time))
            tracker_process.start()

            section_times[0] = time.time() - start_time
            section_progress[0] = 1

            # Section Generation

            num_dots = width * height // map_resolution

            section_progress_total[1] = num_dots

            coords = random.sample(range(0, width * height), num_dots)
            local_dots = []
            num_special_dots = num_dots // island_abundance

            [local_dots.append(Dot(coords[i] % width, coords[i] // width, "Land Origin")) for i in range(num_special_dots)]
            section_progress[1] = num_special_dots
            [local_dots.append(Dot(coords[i] % width, coords[i] // width, "Water Forced")) for i in range(num_special_dots, num_special_dots * 2)]
            section_progress[1] += num_special_dots
            [local_dots.append(Dot(coords[i] % width, coords[i] // width, "Water")) for i in range(num_special_dots * 2, num_dots)]
            section_progress[1] = num_dots

            dots.extend(local_dots)

            section_times[1] = time.time() - start_time - sum(section_times)

            # Section Assignment

            section_progress_total[2] = num_dots

            piece_lengths = [num_dots // processes] * (processes - 1)
            piece_lengths.append(num_dots - sum(piece_lengths))
            piece_ranges = [
                [i * piece_lengths[0], i * piece_lengths[0] + piece_lengths[i]]
                for i in range(processes)
            ]

            local_dots = []
            results = pool.map(copy_piece, piece_ranges)
            for result in results:
                local_dots.extend(result)

            origin_dots = [dot for dot in local_dots if dot.type == "Land Origin"]

            results = []
            for i in range(processes):
                results.append(pool.apply_async(assign_sections,
                    (piece_ranges[i], map_resolution, local_dots, origin_dots, island_size)))
            [result.wait() for result in results]

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
                        (piece_ranges[i], coastline_smoothing, local_dots)))
                [result.wait() for result in results]

            else:

                section_progress[3] = 1

            section_times[3] = time.time() - start_time - sum(section_times)

            # Biome Generation

            section_progress_total[4] = num_dots

            local_dots = []
            results = pool.map(copy_piece, piece_ranges)
            for result in results:
                local_dots.extend(result)

            results = []
            for i in range(processes):
                results.append(pool.apply_async(clean_dots,
                    (piece_ranges[i][0], local_dots[piece_ranges[i][0]:piece_ranges[i][1]])))
            [result.wait() for result in results]

            local_dots = []
            results = pool.map(copy_piece, piece_ranges)
            for result in results:
                local_dots.extend(result)

            biome_origin_dot_indexes = [i for i in random.sample(range(0, num_dots), num_dots // 10) if local_dots[i].type == "Land"]

            results = []
            for i in range(processes):
                results.append(pool.apply_async(generate_biomes_water, (piece_ranges[i], local_dots, height)))
            [result.wait() for result in results]

            local_dots = []
            results = pool.map(copy_piece, piece_ranges)
            for result in results:
                local_dots.extend(result)

            for i in biome_origin_dot_indexes:

                dot = local_dots[i]
                equator_dist = abs(dot.y - height / 2) / height * 20

                if equator_dist < 1:
                    probs = ["Rock"] + ["Desert"] * 3 + ["Jungle"] * 4 + ["Plains"] * 2
                elif equator_dist < 2:
                    probs = ["Rock"] + ["Desert"] * 2 + ["Jungle"] * 4 + ["Plains"] * 3
                elif equator_dist < 3:
                    probs = ["Rock"] + ["Jungle"] * 3 + ["Forest"] * 2 + ["Plains"] * 4
                elif equator_dist < 4:
                    probs = ["Rock"] + ["Jungle"] * 2 + ["Forest"] * 3 + ["Plains"] * 4
                elif equator_dist < 6:
                    probs = ["Rock"] +  ["Forest"] * 4 + ["Plains"] * 5
                elif equator_dist < 7:
                    probs = ["Rock"] + ["Taiga"] * 2 + ["Forest"] * 3 + ["Plains"] * 4
                elif equator_dist < 8:
                    probs = ["Rock"] +  ["Snow"] * 2 + ["Taiga"] * 4 + ["Forest"] * 3
                elif equator_dist < 9:
                    probs = ["Snow"] * 6 + ["Taiga"] * 4
                else:
                    probs = ["Snow"] * 10

                dot_type = probs[random.randint(0, 9)]

                dots[i] = Dot(dot.x, dot.y, dot_type)

                section_progress[4] += 1

            local_dots = []
            results = pool.map(copy_piece, piece_ranges)
            for result in results:
                local_dots.extend(result)

            biome_origin_dots = [local_dots[i] for i in biome_origin_dot_indexes]

            results = []
            for i in range(processes):
                results.append(pool.apply_async(assign_biomes, (piece_ranges[i], biome_origin_dots, local_dots)))
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

            results = []
            for i in range(processes):
                results.append(pool.apply_async(generate_image,
                    (start_heights[i], section_heights[i], i, local_dots, width)))
            [result.wait() for result in results]

            section_times[5] = time.time() - start_time - sum(section_times)

            # Image Stitching

            image = PIL.Image.new("RGB", (width, height), (255, 51, 133))
            shift = 0
            for section in image_sections:
                image.paste(section, (0, shift))
                shift += section_heights[0]
            image.save("production_files/result.png")

            section_times[6] = time.time() - start_time - sum(section_times)
            section_progress[6] = 1

        except:
            raise_error("Pool Parent Process", traceback.format_exc())

    tracker_process.join()
    print(
        ANSI_GREEN + "Generation Complete " + ANSI_RESET +
        format_time(time.time() - start_time)
    )


if __name__ == "__main__":
    main()
