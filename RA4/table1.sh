#!/bin/bash
# ============================================================
# Table 1: Single PC, No Core Affinity
# ============================================================
# Run this on a single lab PC or your own machine.
# It starts slaves in background, runs the master, and
# collects the master's time elapsed for each combination.
#
# Usage:
#   chmod +x table1.sh
#   ./table1.sh
#
# Output:
#   - lab04_table1.csv (raw timing data)
#   - formatted table printed to terminal
# ============================================================

EXE="./lab04_v3"
MASTER_PORT=5000
CSV="lab04_table1.csv"

N_VALUES=(4000 8000 16000)
T_VALUES=(2 4 8 16)
RUNS=3

# check if executable exists
if [ ! -f "$EXE" ]; then
    echo "Error: $EXE not found."
    echo "Compile first: gcc -o lab04_v3 lab04_v3.c -lpthread -lm"
    exit 1
fi

# csv header
echo "n,t,run,time" > "$CSV"

TOTAL=$(( ${#N_VALUES[@]} * ${#T_VALUES[@]} * RUNS ))
COUNT=0

echo "=========================================="
echo "Table 1: Single PC, No Core Affinity"
echo "=========================================="
echo ""

for n in "${N_VALUES[@]}"; do
    for t in "${T_VALUES[@]}"; do

        # generate config file for this t
        CONFIG="config_t${t}.txt"
        echo "master 127.0.0.1 $MASTER_PORT" > "$CONFIG"
        for ((i=1; i<=t; i++)); do
            echo "slave 127.0.0.1 $((MASTER_PORT + i))" >> "$CONFIG"
        done

        for ((run=1; run<=RUNS; run++)); do
            COUNT=$((COUNT + 1))
            printf "[%d/%d] n=%d, t=%d, run %d ... " "$COUNT" "$TOTAL" "$n" "$t" "$run"

            # start slaves in background, suppress output
            SLAVE_PIDS=()
            for ((i=1; i<=t; i++)); do
                $EXE $n $((MASTER_PORT + i)) 1 "$CONFIG" > /dev/null 2>&1 &
                SLAVE_PIDS+=($!)
            done

            # wait for slaves to start listening
            sleep 2

            # run master and capture output
            MASTER_OUTPUT=$($EXE $n $MASTER_PORT 0 "$CONFIG" 2>&1)

            # extract time from master output
            TIME_VAL=$(echo "$MASTER_OUTPUT" | grep -oP 'time elapsed:\s+\K[\d.]+')

            if [ -n "$TIME_VAL" ]; then
                printf "%s seconds\n" "$TIME_VAL"
                echo "$n,$t,$run,$TIME_VAL" >> "$CSV"
            else
                printf "ERROR\n"
                echo "$n,$t,$run,ERROR" >> "$CSV"
            fi

            # wait for all slave processes to finish
            for pid in "${SLAVE_PIDS[@]}"; do
                wait "$pid" 2>/dev/null
            done

            # brief pause to release ports
            sleep 1
        done

        # clean up temp config
        rm -f "$CONFIG"
    done
done

echo ""
echo "=========================================="
echo "Table 1 complete! Results saved to: $CSV"
echo "=========================================="
echo ""

# print formatted table
printf "%-10s %-5s %-14s %-14s %-14s %-14s\n" "n" "t" "Run 1" "Run 2" "Run 3" "Average"
printf "%s\n" "-----------------------------------------------------------------------"

for n in "${N_VALUES[@]}"; do
    for t in "${T_VALUES[@]}"; do
        # read the 3 runs for this n,t
        TIMES=()
        for ((run=1; run<=RUNS; run++)); do
            val=$(grep "^${n},${t},${run}," "$CSV" | cut -d',' -f4)
            TIMES+=("$val")
        done

        if [ "${#TIMES[@]}" -eq "$RUNS" ] && [ -n "${TIMES[0]}" ] && [ "${TIMES[0]}" != "ERROR" ]; then
            AVG=$(echo "scale=6; (${TIMES[0]} + ${TIMES[1]} + ${TIMES[2]}) / 3" | bc)
            printf "%-10s %-5s %-14s %-14s %-14s %-14s\n" "$n" "$t" "${TIMES[0]}" "${TIMES[1]}" "${TIMES[2]}" "$AVG"
        else
            printf "%-10s %-5s %-14s %-14s %-14s %-14s\n" "$n" "$t" "ERROR" "ERROR" "ERROR" "ERROR"
        fi
    done
    echo ""
done
