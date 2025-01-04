# Imports
import ctypes
import multiprocessing
import multiprocessing.managers
import os
import PIL.Image
import random
import scipy
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

def raise_error(location, traceback_output, notes = None):
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

        section_times = [0.0, 0.0, 0.0, 0.0]

        while True:

            clear_screen()
            total_progress = 0

            # Section Progress

            section_names = [
                "Section Generation", "Section Assignment", "Biome Generation", "Image Generation"
            ]
            section_weights = [0.05, 0.15, 0.50, 0.30]

            for i in range(4):
                section_total = section_progress_total[i]
                if section_total == 0:
                    section_total = 1
                progress_section = section_progress[i] / section_total
                total_progress += progress_section * section_weights[i]

                if section_progress[i] == section_progress_total[i]:
                    color = ANSI_GREEN
                else:
                    section_times[i] = time.time() - start_time
                    section_times[i] -= sum(section_times[ii] for ii in range(i))
                    color = ANSI_BLUE
                print(
                    color + "[" + str(i + 1) + "/4] " + section_names[i].ljust(20) +
                    "{:.2f}% ".format(progress_section * 100).rjust(8) +
                    ANSI_GREEN + "█" * round(progress_section * 20) +
                    ANSI_BLUE + "█" * (20 - round(progress_section * 20)) +
                    ANSI_RESET + " " + format_time(section_times[i])
                )

            # Total Progress
            total_progress /= 0.50 # remove later

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

            print(str(section_progress[0]))
            print(str(section_progress_total[0]))
            print(str(section_progress[1]))
            print(str(section_progress_total[1]))
            print(str(section_progress[3]))
            print(str(section_progress_total[3]))

            if (sum(section_progress) == sum(section_progress_total) and not
            all(value > 0 for value in section_progress)) and section_progress[3] > 0: # remove later
                break
            else:
                time.sleep(0.1)

    except:
        raise_error(
            "track_progress", traceback.format_exc(),
            notes = (
                "Input \"section_progress\": " + str(section_progress),
                "Input \"section_progress_total\": " + str(section_progress_total),
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
            notes = (
                "Input \"num_dots\": " + str(num_dots),
                "Input \"processes\": " + str(processes),
                "Input \"width\": " + str(width),
                "Input \"height\": " + str(height)
            )
        )

def generate_sections(coords, relative_island_abundance, width):

    try:

        local_dots = []

        for coord in coords:

            dot_type = "Water"
            if random.randint(1, relative_island_abundance) == 1:
                dot_type = "Land Origin"
            elif random.randint(1, (relative_island_abundance - 1)) == 1:
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
                "Input \"relative_island_abundance\": " + str(relative_island_abundance),
                "Input \"width\": " + str(width),
            )
        )

def copy_piece(piece_range):
    return dots[piece_range[0] : piece_range[1]]

def assign_sections(island_size, origin_dots, local_dots):

    try:

        tree = scipy.spatial.KDTree([(dot.x, dot.y) for dot in local_dots])

        for dot in origin_dots:

            if dot.type == "Land Origin":

                expansions = random.randint(island_size // 2, island_size * 2)
                indexes = tree.query([(dot.x, dot.y)], k = expansions + 1)[1][0]

                for index in indexes:
                    ref_dot = local_dots[index]
                    if ref_dot.type == "Water":
                        dots[index] = Dot(ref_dot.x, ref_dot.y, "Land")

            with lock:
                section_progress[1] += 1

    except:
        raise_error(
            "assign_sections", traceback.format_exc(),
            notes = (
                "Input \"island_size\": " + str(island_size),
                "Input \"origin_dots\": (Not available, length: " + str(len(origin_dots)) + ")",
                "Input \"local_dots\": (Not available)"
            )
        )

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
                    case "Land":
                        pixels[x, y] = (0, 204, 0)
                    case "Land Origin":
                        pixels[x, y] = (0, 102, 0)
                    case "Water":
                        pixels[x, y] = (0, 0, 204)
                    case "Water Forced":
                        pixels[x, y] = (0, 0, 102)
                    case "Error":
                        pixels[x, y] = (255, 102, 163)
                    case _:
                        pixels[x, y] = (204, 0, 82)

            with lock:
                section_progress[3] += 1

        image_sections[process_num] = image_local

    except:
        raise_error(
            "generate_image", traceback.format_exc(),
            notes = (
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
        "\nIsland abundance controls how many islands are generated.\n" +
        "Choose a number between 50 and 500. 100 is the default.\n" +
        "Larger numbers produce fewer islands.\n" +
        "Island Abundance:"
    )
    island_abundance = get_int(50, 500)

    print(
        "\nIsland size controls average island size.\n" +
        "Choose a number between 10 and 100. 20 is the default.\n" +
        "Larger numbers produce larger islands.\n" +
        "Island Size:"
    )
    island_size = get_int(10, 100)

    print(
        "\nRelative island abundance controls the ratio of land to water.\n" +
        "Choose a number between 10 and 50. 30 is the default.\n" +
        "Larger numbers produces less land.\n" +
        "Relative Island Abundance:"
    )
    relative_island_abundance = get_int(10, 100)

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

    section_progress = multiprocessing.Array(ctypes.c_int, [0, 0, 0, 0])
    section_progress_total = multiprocessing.Array(ctypes.c_int, [0, 0, 0, 0])
    dots = manager.list([])
    image_sections = manager.list([PIL.Image.new("RGB", (100, 100), (255, 0, 102))] * processes)
    lock = multiprocessing.Lock()

    with multiprocessing.Pool(processes, initializer = initialize_pool, initargs =
    (section_progress, dots, image_sections, lock)) as pool:

        try:

            # Progress Tracking

            tracker_process = multiprocessing.Process(target = track_progress,
                args = (section_progress, section_progress_total, start_time))
            tracker_process.start()

            # Section Generation

            num_dots = width * height // island_abundance
            section_progress_total[0] = num_dots

            results = []

            coord_sections = pool.apply(calc_pieces_coords, (num_dots, processes, width, height))
    
            for i in range(processes):
                results.append(pool.apply_async(generate_sections,
                    (coord_sections[i], relative_island_abundance, width)))
            [result.wait() for result in results]

            # Section Assignment

            section_progress_total[1] = 1

            results = []

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
            
            origin_dots = [dot for dot in local_dots if dot.type == "Land Origin"]
            num_origin_dots = len(origin_dots)
            section_progress_total[1] = num_origin_dots

            results = []

            start_indexes = list(range(0, num_origin_dots, num_origin_dots // processes))
            index_lengths = [num_origin_dots // processes] * (processes - 1)
            index_lengths.append(num_origin_dots - sum(index_lengths))

            for i in range(processes):
                results.append(pool.apply_async(assign_sections,
                    (island_size, origin_dots[start_indexes[i] : start_indexes[i] + index_lengths[i]], local_dots)))
            [result.wait() for result in results]

            # Image Generation

            section_progress_total[3] = height

            results = []

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
