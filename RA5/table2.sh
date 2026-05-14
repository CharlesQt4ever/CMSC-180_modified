#!/bin/bash
# ============================================================
# Lab 05 — Table 2: Single PC, With Core Affinity
# ============================================================
# Same as table1.sh but each slave is pinned to a specific core
# (round-robin starting at core 1; core 0 reserved for the OS).
#
# Captures BOTH the master time and the slave compute-max per run.
#
# Output:
#   - lab05_table2.csv     (n, t, run, master_time, slave_max)
# ============================================================

EXE="./lab05"
MASTER_PORT=5000
CSV="lab05_table2.csv"
LOG_DIR="lab05_slavelogs_t2"

N_VALUES=(4000 8000 16000)
T_VALUES=(2 4 8 16)
RUNS=3

if [ ! -f "$EXE" ]; then
    echo "Error: $EXE not found."
    echo "Compile first: gcc -O2 -o lab05 lab05.c -lpthread -lm"
    exit 1
fi

NUM_CORES=$(nproc 2>/dev/null || echo 4)
AVAILABLE_CORES=$((NUM_CORES - 1))
[ "$AVAILABLE_CORES" -lt 1 ] && AVAILABLE_CORES=1

mkdir -p "$LOG_DIR"
echo "n,t,run,master_time,slave_max" > "$CSV"

TOTAL=$(( ${#N_VALUES[@]} * ${#T_VALUES[@]} * RUNS ))
COUNT=0

echo "=========================================="
echo "Lab 05 — Table 2: Single PC, Core-Affine"
echo "=========================================="
echo "Detected $NUM_CORES cores. Using cores 1-$AVAILABLE_CORES for slaves."
echo ""

for n in "${N_VALUES[@]}"; do
    for t in "${T_VALUES[@]}"; do

        CONFIG="config_t${t}.txt"
        echo "master 127.0.0.1 $MASTER_PORT" > "$CONFIG"
        for ((i=1; i<=t; i++)); do
            echo "slave 127.0.0.1 $((MASTER_PORT + i))" >> "$CONFIG"
        done

        for ((run=1; run<=RUNS; run++)); do
            COUNT=$((COUNT + 1))
            printf "[%d/%d] n=%d, t=%d, run %d ... " "$COUNT" "$TOTAL" "$n" "$t" "$run"

            SLAVE_PIDS=()
            SLAVE_LOGS=()
            for ((i=1; i<=t; i++)); do
                CORE_ID=$(( ((i - 1) % AVAILABLE_CORES) + 1 ))
                LOG="$LOG_DIR/n${n}_t${t}_r${run}_s${i}_c${CORE_ID}.log"
                SLAVE_LOGS+=("$LOG")
                $EXE $n $((MASTER_PORT + i)) $i "$CONFIG" $CORE_ID > "$LOG" 2>&1 &
                SLAVE_PIDS+=($!)
            done

            sleep 2

            MASTER_OUTPUT=$($EXE $n $MASTER_PORT 0 "$CONFIG" 2>&1)
            MASTER_TIME=$(echo "$MASTER_OUTPUT" | grep -oP 'time elapsed:\s+\K[\d.]+' | tail -1)

            for pid in "${SLAVE_PIDS[@]}"; do
                wait "$pid" 2>/dev/null
            done

            SLAVE_MAX=""
            for LOG in "${SLAVE_LOGS[@]}"; do
                STIME=$(grep -oP 'time elapsed:\s+\K[\d.]+' "$LOG" | tail -1)
                if [ -n "$STIME" ]; then
                    if [ -z "$SLAVE_MAX" ] || awk "BEGIN{exit !($STIME > $SLAVE_MAX)}"; then
                        SLAVE_MAX="$STIME"
                    fi
                fi
            done

            if [ -n "$MASTER_TIME" ] && [ -n "$SLAVE_MAX" ]; then
                printf "master=%s, slave_max=%s\n" "$MASTER_TIME" "$SLAVE_MAX"
                echo "$n,$t,$run,$MASTER_TIME,$SLAVE_MAX" >> "$CSV"
            else
                printf "ERROR (master=%s slave_max=%s)\n" "${MASTER_TIME:-NULL}" "${SLAVE_MAX:-NULL}"
                echo "$n,$t,$run,${MASTER_TIME:-ERROR},${SLAVE_MAX:-ERROR}" >> "$CSV"
            fi

            sleep 1
        done

        rm -f "$CONFIG"
    done
done

echo ""
echo "=========================================="
echo "Table 1 (master) — single PC, with affinity"
echo "=========================================="
printf "%-10s %-5s %-14s %-14s %-14s %-14s\n" "n" "t" "Run 1" "Run 2" "Run 3" "Average"
printf -- "-----------------------------------------------------------------------\n"
for n in "${N_VALUES[@]}"; do
    for t in "${T_VALUES[@]}"; do
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

echo "=========================================="
echo "Table 2 (slave max) — single PC, with affinity"
echo "=========================================="
printf "%-10s %-5s %-14s %-14s %-14s %-14s\n" "n" "t" "Max Run 1" "Max Run 2" "Max Run 3" "Average"
printf -- "-----------------------------------------------------------------------\n"
for n in "${N_VALUES[@]}"; do
    for t in "${T_VALUES[@]}"; do
        TIMES=()
        for ((run=1; run<=RUNS; run++)); do
            val=$(grep "^${n},${t},${run}," "$CSV" | cut -d',' -f5)
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

echo "Raw CSV: $CSV"
echo "Slave logs kept under: $LOG_DIR/"
