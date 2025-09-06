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
    savepng="${task_pieces[1]}" # Whether to save the png file
    inputs="${task_pieces[2]}" # Python automode inputs

    echo "Running task $inputs for $reps repititions."

    if [ "$savepng" = "y" ]; then
        inputs="${inputs/".png"/"0.png"}" # Prep png path for saving multiple files
    fi

    # Run reps, calculate sum

    sum=0
    for ii in $(seq 1 $reps); do
        if [ "$savepng" = "y" ]; then
            inputs="${inputs/"$((($ii) - 1)).png"/"$ii.png"}"
            # Rep 1 saved in file1.png, rep 2 in file2.png, etc.
        fi
        result=$(python3 main.py $inputs) # Run Python program
        echo "Rep $ii Time: $(printf "%.4f" "$result")"
        sum=$(echo "scale=6; $sum + $result" | bc)
    done

    if [ "$savepng" = "n" ]; then
        # Get file path and remove file
        IFS=" " read -ra input_pieces <<< "$inputs"
        rm "${input_pieces[7]}"
    fi

    # Print average time

    echo "Average Time: $(echo "scale=4; $sum / $reps" | bc)"
    echo ""

done