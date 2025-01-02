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

class TreeManager(multiprocessing.managers.BaseManager):
    pass


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
    seconds = "{:.1f}".format(time_seconds % 60).rjust(4, "0")
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
        file.write("Error at " + location + "\n" + traceback_output + "\n")
        if notes != None:
            for note in notes:
                file.write(note, "\n")
        file.write("\n")


# Multiprocessing Functions
# (Order of use)

def initialize_pool(section_progress_value,
dots_value, width_value, height_value, processes_value, start_time_value,
island_abundance_value, island_size_value, relative_island_abundance_value,
image_sections_value, lock_value):
    
    global section_progress
    global dots
    global width
    global height
    global processes
    global start_time
    global island_abundance
    global island_size
    global relative_island_abundance
    global current_section_progress
    global image_sections
    global lock

    section_progress = section_progress_value
    dots = dots_value
    width = width_value
    height = height_value
    processes = processes_value
    start_time = start_time_value
    island_abundance = island_abundance_value
    island_size = island_size_value
    relative_island_abundance = relative_island_abundance_value
    image_sections = image_sections_value
    lock = lock_value

def track_progress(section_progress, section_progress_total, start_time, section_times):

    try:

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
                    section_times[i] = time.time() - start_time.value
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
            total_progress /= 0.35 # remove later

            if sum(section_progress) == sum(section_progress_total):
                color = ANSI_GREEN
            else:
                color = ANSI_BLUE
            print(
                color + "      Total Progress      " +
                "{:.2f}% ".format(total_progress * 100).rjust(8) +
                ANSI_GREEN + "█" * round(total_progress * 20) +
                ANSI_BLUE + "█" * (20 - round(total_progress * 20)) +
                ANSI_RESET + " " + format_time(time.time() - start_time.value)
            )

            print(str(section_progress[0]))
            print(str(section_progress_total[0]))
            print(str(section_progress[3]))
            print(str(section_progress_total[3]))

            if (sum(section_progress) == sum(section_progress_total) and not
            all(value > 0 for value in section_progress)) and section_progress[3] > 0: # remove later
                break
            else:
                time.sleep(0.1)

    except:
        raise_error("track_progress", traceback.format_exc())

def generate_sections(coords):

    try:

        for coord in coords:

            dot_type = "Water"
            if random.randint(1, relative_island_abundance.value) == 1:
                dot_type = "Land Origin"
            elif random.randint(1, (relative_island_abundance.value - 1)) == 1:
                dot_type = "Water Forced"
            dots.append(Dot(coord % width.value, coord // width.value, dot_type))

            with lock:
                section_progress[0] += 1

    except:
        raise_error(
            "generate_sections", traceback.format_exc(), notes = ("Input: " + str(coords), )
        )

def generate_image(start_height, section_height, process_num, local_dots):

    try:

        image_local = (
            PIL.Image.new("RGB", (width.value, section_height), (255, 153, 194))
        )
        pixels = image_local.load()
        dot_coords = []
        for dot in dots:
            dot_coords.append((dot.x, dot.y))
        tree = scipy.spatial.KDTree(dot_coords)

        for y in range(section_height):

            indexes = tree.query([(x, y + start_height) for x in range(width.value)])[1]

            for x in range(width.value):

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
            notes = ("Input: " + str(section_height), )
        )


if __name__ == "__main__":

    with open("errors.txt", "w") as file:
        file.write("")

    clear_screen()

    print(
        "Welcome to MapMaker v1.0\n" + 
        "\nMap Width:"
    )
    width_input = get_int(100, 10_000)

    print("\nMap Height:")
    height_input = get_int(100, 10_000)

    print(
        "\nIsland abundance controls how many islands are generated.\n" +
        "Choose a number between 50 and 1000. 100 is the default.\n" +
        "Larger numbers produce fewer islands.\n" +
        "Island Abundance:"
    )
    island_abundance_input = get_int(50, 1000)

    print(
        "\nIsland size controls average island size.\n" +
        "Choose a number between 10 and 100. 20 is the default.\n" +
        "Larger numbers produce larger islands.\n" +
        "Island Size:"
    )
    island_size_input = get_int(10, 100)

    print(
        "\nRelative island abundance controls the ratio of land to water.\n" +
        "Choose a number between 10 and 50. 30 is the default.\n" +
        "Larger numbers produces more land.\n" +
        "Relative Island Abundance:"
    )
    relative_island_abundance_input = get_int(10, 100)

    print(
        "\nNow you must choose how many of your CPU's threads to use for map generation.\n" +
        "Values exceeding your CPU's number of threads will slow map generation.\n" +
        "A maximum of four less than your CPU's number of threads is recommended,\n" +
        "to leave threads available for your system's background processes.\n" +
        "Ensure you monitor your CPU for overheating, and halt the program if\n" +
        "high temperatures occur. Using fewer threads may reduce temperatures.\n" +
        "Number of Threads:"
    )
    processes_input = get_int(1, 32)

    clear_screen()

    TreeManager.register("Tree", scipy.spatial.KDTree)

    manager = multiprocessing.Manager()
    tree_manager = TreeManager()
    tree_manager.start()

    section_progress = multiprocessing.Array(ctypes.c_int, [0, 0, 0, 0])
    section_progress_total = multiprocessing.Array(ctypes.c_int, [0, 0, 0, 0])
    dots = manager.list([])
    width = multiprocessing.Value(ctypes.c_int, width_input)
    height = multiprocessing.Value(ctypes.c_int, height_input)
    processes = multiprocessing.Value(ctypes.c_int, processes_input)
    start_time = multiprocessing.Value(ctypes.c_double, time.time())
    section_times = multiprocessing.Array(ctypes.c_double, [0.0, 0.0, 0.0, 0.0])
    island_abundance = multiprocessing.Value(ctypes.c_int, island_abundance_input)
    island_size = multiprocessing.Value(ctypes.c_int, island_size_input)
    relative_island_abundance = multiprocessing.Value(ctypes.c_int, relative_island_abundance_input)
    image_sections = manager.list([PIL.Image.new("RGB", (100, 100), (255, 0, 102))] * processes.value)
    lock = multiprocessing.Lock()

    with multiprocessing.Pool(processes_input, initializer = initialize_pool, initargs =
    (section_progress, dots, width, height, processes, start_time,
    island_abundance, island_size, relative_island_abundance, image_sections, lock)) as pool:

        try:

            # Progress Tracking

            tracker_process = multiprocessing.Process(target = track_progress,
                args = (section_progress, section_progress_total, start_time, section_times))
            tracker_process.start()

            # Section Generation

            num_dots = width.value * height.value // island_abundance.value
            section_progress_total[0] = num_dots

            results = []

            coords = random.sample(range(0, width.value * height.value), num_dots)
            coord_sections = [coords[i : i + num_dots // processes.value] for i in range(0, num_dots, num_dots // processes.value)]
            if num_dots % processes.value != 0:
                coord_sections[-2].extend(coord_sections[-1])
                coord_sections.pop()
    
            for i in range(processes.value):
                results.append(pool.apply_async(generate_sections, (coord_sections[i], )))
            [result.wait() for result in results]

            # Image Generation

            section_progress_total[3] = height.value

            results = []

            start_heights = list(range(0, height.value, height.value // processes.value))
            section_heights = [height.value // processes.value] * (processes.value - 1)
            section_heights.append(height.value - sum(section_heights))

            local_dots = []
            local_dots.extend(dots)

            for i in range(processes.value):
                results.append(pool.apply_async(generate_image, (start_heights[i], section_heights[i], i, local_dots)))
            [result.wait() for result in results]

            tracker_process.join()

            # Image Stitching

            image = PIL.Image.new("RGB", (width.value, height.value), (255, 51, 133))
            shift = 0
            for section in image_sections:
                image.paste(section, (0, shift))
                shift += section_heights[0]
            image.save("result.png")

        except:
            raise_error("Pool Parent Process", traceback.format_exc())

    print(
        ANSI_GREEN + "Generation Complete " + ANSI_RESET +
        format_time(time.time() - start_time.value)
    )