#!/bin/bash

echo ""

# Reading Tasks File

raw_file=$(cat autorun_tasks.txt)
readarray -t tasks <<< "$raw_file" # Split tasks by newline

# Running Tasks

for i in "${tasks[@]}"; do

    # Split task

    IFS=":" read -ra task_pieces <<< "$i"
    reps="$((task_pieces[0]))" # Repetitions
    rep_times="${task_pieces[1]}" # Whether to list each rep's runtime
    save_png="${task_pieces[2]}" # Whether to save the png file
    inputs="${task_pieces[3]}" # Python automode inputs

    echo "Running task $inputs for $reps repititions."

    if [ "$save_png" = "y" ]; then
        inputs="${inputs/".png"/"0.png"}" # Prep png path for saving multiple files
    fi

    # Run reps, calculate sum

    sum=0
    if [ "$rep_times" = "n" ]; then
        echo "1"
    fi

    for ii in $(seq 1 $reps); do

        if [ "$save_png" = "y" ]; then
            inputs="${inputs/"$((($ii) - 1)).png"/"$ii.png"}"
            # Rep 1 saved in file1.png, rep 2 in file2.png, etc.
        fi

        result=$(python3 main.py $inputs) # Run Python program

        if [ "$rep_times" = "y" ]; then
            echo "Rep $ii Time: $(printf "%.4f" "$result")" # Rep 1 Time: 1.0000
        elif [ "$ii" != 0 ]; then
            num_blocks=$((20 * $ii / $reps))
            printf "\r\033[K\u001b[48;5;2m"
            printf " %.0s" $(seq 1 "$num_blocks")
            printf "\u001b[0m"
        fi

        sum=$(echo "scale=6; $sum + $result" | bc)

    done

    if [ "$save_png" = "n" ]; then
        # Get file path and remove file
        IFS=" " read -ra input_pieces <<< "$inputs"
        rm "${input_pieces[7]}"
    fi

    # Print average time

    if [ "$rep_times" = "n" ]; then
        echo ""
    fi

    echo "Average Time: $(echo "scale=4; $sum / $reps" | bc)"
    echo ""

done