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

import ctypes
import math
import multiprocessing
import multiprocessing.managers
import os
import PIL.Image
import random
import scipy
import scipy.spatial
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
    seconds = f"{(time_seconds % 60):.2f}".rjust(5, "0")
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
                file.write(note + "\n")
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

def track_progress(section_progress, section_progress_total, start_time):

    try:

        section_times = [0.0, 0.0, 0.0, 0.0, 0.0]

        while True:

            clear_screen()
            total_progress = 0

            # Section Progress

            section_names = [
                "Section Generation", "Section Assignment", "Coastline Smoothing",
                "Biome Generation", "Image Generation"
            ]
            section_weights = [0.05, 0.25, 0.30, 0.20, 0.20]

            for i in range(len(section_names)):
                section_total = section_progress_total[i]
                if section_total == 0:
                    section_total = 1
                progress_section = section_progress[i] / section_total
                total_progress += progress_section * section_weights[i]

                if section_progress[i] == section_progress_total[i]:
                    color = ANSI_GREEN
                else:
                    color = ANSI_BLUE

                    if i == 0 or section_progress[i - 1] == section_progress_total[i - 1]:
                        section_times[i] = time.time() - start_time
                        section_times[i] -= sum(section_times[ii] for ii in range(i))
                
                print(
                    color + "[" + str(i + 1) + "/5] " + section_names[i].ljust(20) +
                    "{:.2f}% ".format(progress_section * 100).rjust(8) +
                    ANSI_GREEN + "█" * round(progress_section * 20) +
                    ANSI_BLUE + "█" * (20 - round(progress_section * 20)) +
                    ANSI_RESET + " " + format_time(section_times[i])
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
                ANSI_RESET + " " + format_time(time.time() - start_time)
            )

            if sum(section_progress) == sum(section_progress_total):
                break
            else:
                time.sleep(0.1)

    except:
        raise_error(
            "track_progress", traceback.format_exc(),
            notes=(
                "Input \"section_progress\": (Not available)",
                "Input \"section_progress_total\": (Not available)",
                "Input \"start_time\": " + str(start_time)
            )
        )

def calc_pieces_coords(num_dots, processes, width, height):
    
    try:

        piece_lengths = [num_dots // processes] * (processes - 1)
        piece_lengths.append(num_dots - sum(piece_lengths))

        coords = random.sample(range(0, width * height), num_dots)

        coord_sections = [
            coords[i * piece_lengths[0] : i * piece_lengths[0] + piece_lengths[i]]
            for i in range(processes)
        ]

        return coord_sections

    except:
        raise_error(
            "calc_sections", traceback.format_exc(),
            notes=(
                "Input \"num_dots\": " + str(num_dots),
                "Input \"processes\": " + str(processes),
                "Input \"width\": " + str(width),
                "Input \"height\": " + str(height)
            )
        )

def generate_sections(coords, island_abundance, width):

    try:

        local_dots = []

        for coord in coords:

            dot_type = "Water"
            if random.randint(1, island_abundance) == 1:
                dot_type = "Land Origin"
            elif random.randint(1, (island_abundance - 1)) == 1:
                dot_type = "Water Forced"
            local_dots.append(Dot(coord % width, coord // width, dot_type))

            with lock:
                section_progress[0] += 1

        dots.extend(local_dots)

    except:
        raise_error(
            "generate_sections", traceback.format_exc(),
            notes = (
                "Input \"coords\": (Not available)",
                "Input \"island_abundance\": " + str(island_abundance),
                "Input \"width\": " + str(width),
            )
        )

def copy_piece(piece_range):
    return dots[piece_range[0] : piece_range[1]]

def assign_sections(piece_range, map_resolution, local_dots, origin_dots, dist_multipliers):

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
                
                if dist <= dist_multipliers[index]:
                    chance = ((0.9) ** (1 / dist)) ** dist
                else:
                    chance = ((0.1) ** (1 / dist)) ** dist

                if chance > 1.0:
                    chance = 1.0
                if random.random() < chance:
                    dots[i] = Dot(dot.x, dot.y, "Land")

            with lock:
                section_progress[1] += 1

    except:
        raise_error(
            "assign_sections", traceback.format_exc(),
            notes=(
                "Input \"map_resolution\": " + str(map_resolution),
                "Input \"local_dots\": (Not available)"
            )
        )

def smooth_coastlines(piece_range, coastline_smoothing):



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
                section_progress[4] += 1

        image_sections[process_num] = image_local

    except:
        raise_error(
            "generate_image", traceback.format_exc(),
            notes=(
                "Input \"start_height\": " + str(start_height),
                "Input \"section_height\": " + str(section_height),
                "Input \"process_num\": " + str(process_num),
                "Input \"local_dots\": (Not available)",
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
    width = get_int(100, 10_000)

    print("\nMap Height:")
    height = get_int(100, 10_000)

    print(
        "\nMap resolution controls the section size of the map.\n" +
        "Choose a number between 50 and 500. 100 is the default.\n" +
        "Larger numbers produce lower resolutions,\n" +
        "while higher numbers take longer and have less variation.\n" + 
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
        "Choose a number between 1 and 10. 3 is the default.\n" +
        "Larger numbers produce larger islands.\n" +
        "Island Size:"
    )
    island_size = get_int(0, 10)

    print(
        "\nCoastline smoothing controls how clean coastlines look.\n" +
        "Integers between 1 and 100 cause smoother coastlines, fewer islands,\n" +
        "and fewer lakes. A value of 0 causes no smoothing. Larger numbers\n" +
        "produce more smoothing\n" +
        "Coastline Smoothing:"
    )
    coastline_smoothing = get_int(0, 100)

    print(
        "\nNow you must choose how many of your CPU's threads to use for map generation.\n" +
        "Values exceeding your CPU's number of threads will slow map generation.\n" +
        "A maximum of four less than your CPU's number of threads is recommended,\n" +
        "to leave threads available for your system's background processes.\n" +
        "Ensure you monitor your CPU for overheating, and halt the program if\n" +
        "high temperatures occur. Using fewer threads may reduce temperatures.\n" +
        "Number of Threads:"
    )
    processes = get_int(1, 32)

    start_time = time.time()

    clear_screen()

    manager = multiprocessing.Manager()

    section_progress = multiprocessing.Array(ctypes.c_int, [0, 0, 0, 0, 0])
    section_progress_total = multiprocessing.Array(ctypes.c_int, [1, 1, 1, 1, 1])
    dots = manager.list([])
    image_sections = manager.list([PIL.Image.new("RGB", (100, 100), (255, 0, 102))] * processes)
    lock = multiprocessing.Lock()

    with multiprocessing.Pool(processes, initializer=initialize_pool,
    initargs=(section_progress, dots, image_sections, lock)) as pool:

        try:

            # Progress Tracking

            tracker_process = multiprocessing.Process(target=track_progress,
                args=(section_progress, section_progress_total, start_time))
            tracker_process.start()

            # Section Generation

            num_dots = width * height // map_resolution

            section_progress_total[0] = num_dots

            results = []

            coord_sections = pool.apply(calc_pieces_coords, (num_dots, processes, width, height))
    
            for i in range(processes):
                results.append(pool.apply_async(generate_sections,
                    (coord_sections[i], island_abundance, width)))
            [result.wait() for result in results]

            # Section Assignment
            
            section_progress_total[1] = num_dots

            local_dots = []
            piece_lengths = [num_dots // processes] * (processes - 1)
            piece_lengths.append(num_dots - sum(piece_lengths))
            piece_ranges = [
                [i * piece_lengths[0], i * piece_lengths[0] + piece_lengths[i]]
                for i in range(processes)
            ]
            results = pool.map(copy_piece, piece_ranges)
            for result in results:
                local_dots.extend(result)

            results = []

            origin_dots = [dot for dot in local_dots if dot.type == "Land Origin"]
            dist_multipliers = [
                random.uniform(island_size / 2, island_size * 2) for i in range(len(origin_dots))
            ]

            for i in range(processes):
                results.append(pool.apply_async(assign_sections,
                    (piece_ranges[i], map_resolution, local_dots, origin_dots, dist_multipliers)))
            [result.wait() for result in results]

            # Biome Generation

            if coastline_smoothing != 0:

                section_progress_total[2] = coastline_smoothing * num_dots

                for i in range(coastline_smoothing):

                    results = []

            else:

                section_progress[2] = 1

            # Old Biome Generation
            """
            num = coastline_smoothing
            section_progress_total[3] = 5
            if num != 0:
                for ii in (1, -1):
                    land_dots = [dot for dot in dots if dot.type == "Land"]
                    water_dots = [dot for dot in dots if dot.type == "Water"]
                    tree1 = scipy.spatial.KDTree([(dot.x, dot.y) for dot in land_dots])
                    tree2 = scipy.spatial.KDTree([(dot.x, dot.y) for dot in water_dots])
                    for i in range(0, len(dots), ii):
                        dot = dots[i]
                        if dot.type == "Land":
                            dist = tree1.query((dot.x, dot.y), k=num, workers=processes)[0]
                            dist2 = tree2.query((dot.x, dot.y), k=num, workers=processes)[0]
                        elif dot.type == "Water":
                            dist = tree2.query((dot.x, dot.y), k=num, workers=processes)[0]
                            dist2 = tree1.query((dot.x, dot.y), k=num, workers=processes)[0]
                        if dot.type in ("Land", "Water") and sum(dist) > sum(dist2):
                            items = ["Land", "Water"]
                            items.remove(dot.type)
                            dots[i] = Dot(dot.x, dot.y, items[0])
            section_progress[3] = 1

            for i in range(len(dots)):
                dot = dots[i]
                if dot.type == "Land Origin":
                    dots[i] = Dot(dot.x, dot.y, "Land")
                elif dot.type == "Water Forced":
                    dots[i] = Dot(dot.x, dot.y, "Water")
            section_progress[3] = 2

            tree = (
                scipy.spatial.KDTree([(dot.x, dot.y) for dot in dots if dot.type == "Land"])
            )
            section_progress[3] = 3
            biome_origin_dot_indexes = random.sample(range(len(dots)), len(dots) // 10)
            biome_origin_dot_indexes = [index for index in biome_origin_dot_indexes if dots[index].type == "Land"]
            for i in biome_origin_dot_indexes:
                dot = dots[i]
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

            for i in [index for index in range(len(dots)) if dots[index].type == "Water"]:
                
                dot = dots[i]

                equator_dist = abs(dot.y - height / 2) / height * 20
                land_dist = tree.query((dot.x, dot.y), workers=processes)[0]

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
            section_progress[3] = 4

            biome_origin_dots = [dots[i] for i in biome_origin_dot_indexes]
            tree = scipy.spatial.KDTree([(dot.x, dot.y) for dot in biome_origin_dots]) 
            for i in [index for index in range(len(dots)) if dots[index].type == "Land"]:
                dot = dots[i]
                dots[i] = Dot(dot.x, dot.y, biome_origin_dots[tree.query((dot.x, dot.y), workers=processes)[1]].type)

            section_progress[3] = 5
            
            """

            # Image Generation

            section_progress_total[4] = height

            results = pool.map(copy_piece, piece_ranges)
            for result in results:
                local_dots.extend(result)

            results = []

            start_heights = list(range(0, height, height // processes))
            section_heights = [height // processes] * (processes - 1)
            section_heights.append(height - sum(section_heights))

            for i in range(processes):
                results.append(pool.apply_async(generate_image,
                    (start_heights[i], section_heights[i], i, local_dots, width)))
            [result.wait() for result in results]

            tracker_process.join()

            # Image Stitching

            image = PIL.Image.new("RGB", (width, height), (255, 51, 133))
            shift = 0
            for section in image_sections:
                image.paste(section, (0, shift))
                shift += section_heights[0]
            image.save("result.png")

        except:
            raise_error("Pool Parent Process", traceback.format_exc())

    print(
        ANSI_GREEN + "Generation Complete " + ANSI_RESET +
        format_time(time.time() - start_time)
    )


if __name__ == "__main__":
    main()
