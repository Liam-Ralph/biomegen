# Imports
import multiprocessing
import os
from PIL import Image
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

if __name__ == "__main__":

    with open("MapMaker/errors.txt", "w") as file:
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
