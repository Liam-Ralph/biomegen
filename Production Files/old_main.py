# Imports
import multiprocessing
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
ANSI_CYAN = "\u001b[38;5;6m"
ANSI_RESET = "\u001b[0m"

# Functions
# (Alphabetical order)

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

# Multiprocessing Functions
# (Order of use)

def track_progress(setup_progress, section_assignment_progress, section_generation_progress,
biome_generation_progress, image_generation_progress):
    try:

        start_time = time.time()

        while True:

            command = "clear"
            if os.name in ("nt", "dos"):
                command = "cls"
            os.system(command)

            print("Total Time: " + "{:.1f}".format(time.time() - start_time) + "s")

            total_setup_progress = (
                "{:.2f}".format(setup_progress.value)
            )

            sum_section_generation_progress = 0.0
            for value in section_generation_progress:
                sum_section_generation_progress += value
            total_section_generation_progress = (
                "{:.2f}".format(sum_section_generation_progress / len(section_generation_progress))
            )

            sum_section_assignment_progress = 0.0
            for value in section_assignment_progress:
                sum_section_assignment_progress += value
                #print(value, end = " | ")
            #print()
            total_section_assignment_progress = (
                "{:.2f}".format(sum_section_assignment_progress / len(section_assignment_progress))
            )

            sum_biome_generation_progress = 0.0
            for value in biome_generation_progress:
                sum_biome_generation_progress += value
            total_biome_generation_progress = (
                "{:.2f}".format(sum_biome_generation_progress / len(biome_generation_progress))
            )

            sum_image_generation_progress = 0.0
            for value in image_generation_progress:
                sum_image_generation_progress += value
            total_image_generation_progress = (
                "{:.2f}".format(sum_image_generation_progress / len(image_generation_progress))
            )

            if total_setup_progress != "100.00":
                print(ANSI_CYAN, end = "")
            else:
                print(ANSI_GREEN, end = "")
            print(("Setup Progress").ljust(30) + ANSI_RESET + (total_setup_progress + "%").rjust(7))

            if total_section_generation_progress != "100.00":
                print(ANSI_CYAN, end = "")
            else:
                print(ANSI_GREEN, end = "")
            print(
                ("Section Generation Progress").ljust(30) + ANSI_RESET +
                (total_section_generation_progress + "%").rjust(7)
            )

            if total_section_assignment_progress != "100.00":
                print(ANSI_CYAN, end = "")
            else:
                print(ANSI_GREEN, end = "")
            print(
                ("Section Assignment Progress").ljust(30) + ANSI_RESET +
                (total_section_assignment_progress + "%").rjust(7)
            )

            if total_biome_generation_progress != "100.00":
                print(ANSI_CYAN, end = "")
            else:
                print(ANSI_GREEN, end = "")
            print(
                ("Biome Generation Progress").ljust(30) + ANSI_RESET +
                (total_biome_generation_progress + "%").rjust(7)
            )

            if total_image_generation_progress != "100.00":
                print(ANSI_CYAN, end = "")
            else:
                print(ANSI_GREEN, end = "")
            print(
                ("Image Generation Progress").ljust(30) + ANSI_RESET +
                (total_image_generation_progress + "%").rjust(7)
            )

            time.sleep(0.1)

            if (total_setup_progress == total_section_generation_progress ==
            total_section_assignment_progress == total_biome_generation_progress ==
            total_image_generation_progress == "100.00") or total_image_generation_progress == "100.00": # remove later
                break

    except Exception:
        with open("errors.txt", "w") as file:
            file.write("Timer\n" + traceback.format_exc() + "\n")

def generate_sections(process_num, reps, section_generation_progress, sec_height, 
my_height, relative_island_abundance, dot_coords, width):
    try:

        start_height = sec_height * process_num
        coords = (
            random.sample(range(start_height * width, (start_height + my_height) * width), reps)
        )

        local_dot_coords = []
        for i in range(reps):
            dot_type = "Water"
            if random.randint(1, relative_island_abundance) == 1:
                dot_type = "Land Start"
            elif random.randint(1, (relative_island_abundance - 1)) == 1:
                dot_type = "Lake"
            local_dot_coords.append(Dot(coords[i] % width, coords[i] // width, dot_type))
            section_generation_progress[process_num] = (i + 1) / reps * 100

        dot_coords.extend(local_dot_coords)

    except Exception:
        with open("errors.txt", "a") as file:
            file.write("Section Generation, Process " + str(process_num) + "\n" +
                traceback.format_exc() + "\n")

def assign_sections(process_num, section_assignment_progress,
dot_coords_piece, dot_coords, island_size):
    try:

        dot_coords_xy = []
        for dot in dot_coords:
            dot_coords_xy.append((dot.x, dot.y))
        tree = scipy.spatial.KDTree(dot_coords_xy)

        for i in range(len(dot_coords_piece)):

            dot = dot_coords_piece[i]

            if dot.type == "Land Start":
                expansions = random.randint(island_size // 2, island_size * 2)
                for ii in range(expansions):
                    dd, ii_ = tree.query([dot.x, dot.y], k = [ii + 1])
                    expansion_dot = dot_coords[ii_[0]]
                    if expansion_dot.type == "Water":
                        expansion_dot.type = "Land"

            section_assignment_progress[process_num] = (
                (i + 1) / len(dot_coords_piece) * 100
            )

    except Exception:
        with open("errors.txt", "a") as file:
            file.write("Section Assignment, Process " + str(process_num) + "\n" +
                traceback.format_exc() + "\n")

def parallel_copy(dot_coords, processes):

    chunk_size = len(dot_coords) // processes
    chunks = [dot_coords[i:i + chunk_size] for i in range(0, len(dot_coords), chunk_size)]
    
    with multiprocessing.Pool(processes) as pool:
        result_chunks = pool.map(copy_chunk, chunks)

    # Combine all chunks into a single list
    copied_dots = []
    for chunk in result_chunks:
        copied_dots.extend(chunk)

    return copied_dots

def copy_chunk(chunk):
    return list(chunk)

def generate_image(process_num, reps, image_generation_progress,
image_results, processes, dot_coords, width, height):
    try:

        image_section = PIL.Image.new("RGB", (width, reps), "white")
        pixels = image_section.load()
        dot_coords_xy = []
        for dot in dot_coords:
            dot_coords_xy.append((dot.x, dot.y))
        tree = scipy.spatial.KDTree(dot_coords_xy)

        start_num = process_num * (height // processes)
        for y in range(reps):

            indexes = tree.query([(x, y + start_num) for x in range(width)])[1]

            for x in range(width):
                
                pixel_type = "Error"
                pixel_type = dot_coords[indexes[x]].type

                match(pixel_type):
                    case "Land":
                        pixels[x, y] = (0, 102, 0)
                    case "Land Start":
                        pixels[x, y] = (50, 50, 50)
                    case "Water":
                        pixels[x, y] = (0, 0, 204)
                    case "Lake":
                        pixels[x, y] = (0, 0, 204)
                    case "Error":
                        pixels[x, y] = (255, 0, 255)

            image_generation_progress[process_num] = ((y + 1) * width) / (reps * width) * 100
        
        image_results.append(image_section)
    
    except Exception:
        with open("errors.txt", "a") as file:
            file.write("Image Generation, Process " + str(process_num) + "\n" +
                traceback.format_exc() + "\n")


if __name__ == "__main__":

    with open("errors.txt", "w") as file:
        file.write("")

    command = "clear"
    if os.name in ("nt", "dos"):
        command = "cls"
    os.system(command)

    print(
        "Welcome to MapMaker v1.0\n" + 
        "\nMap Width:"
    )
    width = get_int(100, 10_000)

    print("\nMap Height:")
    height = get_int(100, 10_000)

    print(
        "\nIsland abundance controls how many islands are generated.\n" +
        "Choose a number between 50 and 1000. 100 is the default.\n" +
        "Larger numbers produce fewer islands.\n" +
        "Island Abundance:"
    )
    island_abundance = get_int(50, 1000)

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
        "Larger numbers produces more land.\n" +
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
    if processes > 2:
        processes -= 2

    start_time_main = time.time()
    
    setup_progress = multiprocessing.Value('d', 0.0)
    section_generation_progress = multiprocessing.Array('d', [0.0] * processes)
    section_assignment_progress = multiprocessing.Array('d', [0.0] * processes)
    biome_generation_progress = multiprocessing.Array('d', [0.0] * processes)
    image_generation_progress = multiprocessing.Array('d', [0.0] * processes)
    dot_coords = multiprocessing.Manager().list([])
    process_list = []

    # Setup

    process_progress = multiprocessing.Process(target = track_progress,
        args = (setup_progress, section_assignment_progress, section_generation_progress,
        biome_generation_progress, image_generation_progress))
    process_progress.start()

    setup_progress.value = 100.0

    # Section Generation

    reps = [0] * processes
    for i in range(width * height // island_abundance):
        reps[random.randint(0, len(reps) - 1)] += 1

    section_height = height // processes

    for i in range(processes - 1):
        process_list.append(multiprocessing.Process(target = generate_sections,
            args = (i, reps[i], section_generation_progress, section_height,
            section_height, relative_island_abundance, dot_coords, width)))
    process_list.append(multiprocessing.Process(target = generate_sections,
        args = (processes - 1, reps[-1], section_generation_progress, section_height,
        height - (processes - 1) * section_height, relative_island_abundance, dot_coords, width)))
    for process in process_list:
        process.start()
    for process in process_list:
        process.join()
    process_list = []

    # Section Assignment

    piece_size = len(dot_coords) // processes
    dot_coords_pieces = [
        dot_coords[i : i + piece_size] for i in range(0, len(dot_coords), piece_size)
    ]
    dot_coords_pieces.append(dot_coords[i : len(dot_coords) - 1])

    for i in range(processes - 1):
        process_list.append(multiprocessing.Process(target = assign_sections,
            args = (i, section_assignment_progress, dot_coords_pieces[i],
            dot_coords, island_size)))
    process_list.append(multiprocessing.Process(target = assign_sections,
        args = (processes - 1, section_assignment_progress,
        dot_coords_pieces[-1], dot_coords, island_size)))
    for process in process_list:
        process.start()
    for process in process_list:
        process.join()
    process_list = []

    # Image Generation

    image_results = multiprocessing.Manager().list([])
    local_dot_coords = parallel_copy(dot_coords, processes)

    for i in range(processes - 1):
        process_list.append(multiprocessing.Process(target = generate_image,
            args = (i, section_height, image_generation_progress,
            image_results, processes, local_dot_coords, width, height)))
    process_list.append(multiprocessing.Process(target = generate_image,
        args = (processes - 1, int(height - (processes - 1) * section_height),
        image_generation_progress, image_results,
        processes, local_dot_coords, width, height)))
    for process in process_list:
        process.start()
    for process in process_list:
        process.join()
    process_list = []
    image = PIL.Image.new("RGB", (width, height))
    down_shift = 0
    for result in image_results:
        image.paste(result, (0, down_shift))
        down_shift += section_height
    image.save("result.png")

    print("{:.1f}".format(time.time() - start_time_main))