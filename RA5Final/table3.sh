#!/bin/bash
# ============================================================
# Lab 05 — Table 3 sweep: ICS Swarm, Core-Affine (CANONICAL)
# ============================================================
# Captures BOTH master time (Paper Table 1) and slave-max time
# (Paper Table 2) per run.
#
# AUTO-RESUME: each completed run is appended to the CSV
# IMMEDIATELY, and re-running this script picks up where it
# left off — runs already present in lab05_table3.csv (with a
# non-ERROR result) are skipped.
#
# To start over from scratch: rm lab05_table3.csv
#
# BEFORE RUNNING:
#   1. Edit DRONE_IPS below with TODAY's drone IPs
#   2. Edit MASTER_IP with the drone you SSH-ed into
#   3. lab05 must be compiled and at $REMOTE_EXE on shared FS
#   4. SSH key auth recommended (otherwise 16+ password prompts)
#   5. Run this script ON one of the drones (NOT overqueen)
#
# Output:
#   - lab05_table3.csv     (n, t, run, master_time, slave_max)
# ============================================================

# ==============================
# EDIT THESE BEFORE RUNNING
# ==============================

USERNAME="cpdelosreyes"
REMOTE_EXE="/home/$USERNAME/lab05"

# the drone you are running this script on (master). Update daily.
MASTER_IP="10.0.9.134"

# at least 16 drone IPs. Wraps with extra ports if t > drone count.
# IPs are dynamic — refresh from http://10.0.9.19/ every session.
DRONE_IPS=(
    "10.0.9.178"
    "10.0.9.167"
    "10.0.9.141"
    "10.0.9.132"
    "10.0.9.144"
    "10.0.9.156"
    "10.0.9.179"
    "10.0.9.124"
    "10.0.9.140"
    "10.0.9.163"
    "10.0.9.169"
    "10.0.9.166"
    "10.0.9.180"
    "10.0.9.146"
    "10.0.9.129"
    "10.0.9.174"
)

# ==============================
# DO NOT EDIT BELOW THIS LINE
# ==============================

EXE="$REMOTE_EXE"
MASTER_PORT=5000
SLAVE_PORT=5001
CSV="lab05_table3.csv"
LOG_DIR="lab05_slavelogs_t3"

N_VALUES=(4000 8000 16000)
T_VALUES=(2 4 8 16)
RUNS=3

SSH_OPTS="-o StrictHostKeyChecking=no -o ConnectTimeout=5"

# Per-n timeout (n=16000 over the swarm can take ~3 min per run)
get_timeout() {
    case "$1" in
        4000)  echo 300 ;;
        8000)  echo 600 ;;
        16000) echo 1200 ;;
        *)     echo 600 ;;
    esac
}

if [ ! -f "$EXE" ]; then
    echo "Error: $EXE not found on this drone."
    echo "Upload to overqueen first: scp lab05.c $USERNAME@10.0.9.20:~"
    echo "Then on a drone: gcc -O2 -o lab05 lab05.c -lpthread -lm"
    exit 1
fi

mkdir -p "$LOG_DIR"

# ==============================
# AUTO-RESUME: load already-completed (n,t,run) tuples from CSV
# ==============================
declare -A DONE
EXISTING=0
if [ -f "$CSV" ]; then
    while IFS=',' read -r n_ t_ run_ mt_ sm_; do
        [ "$n_" = "n" ] && continue
        # Only count as done if both times are real numbers
        if [ -n "$mt_" ] && [ "$mt_" != "ERROR" ] && [ -n "$sm_" ] && [ "$sm_" != "ERROR" ]; then
            DONE["$n_,$t_,$run_"]=1
            EXISTING=$((EXISTING + 1))
        fi
    done < "$CSV"
else
    echo "n,t,run,master_time,slave_max" > "$CSV"
fi

TOTAL=$(( ${#N_VALUES[@]} * ${#T_VALUES[@]} * RUNS ))
COUNT=0
DRONE_COUNT=${#DRONE_IPS[@]}
REMAINING=$((TOTAL - EXISTING))

echo "=========================================="
echo "Lab 05 — Table 3: ICS Swarm, Core-Affine"
echo "=========================================="
echo "Master: $MASTER_IP, drones available: $DRONE_COUNT"
if [ "$EXISTING" -gt 0 ]; then
    echo "Resume: $EXISTING/$TOTAL runs already saved in $CSV"
    echo "        $REMAINING runs remaining"
else
    echo "Fresh start: $TOTAL runs to do"
fi
echo ""

for n in "${N_VALUES[@]}"; do
    RUN_TIMEOUT=$(get_timeout "$n")
    for t in "${T_VALUES[@]}"; do

        CONFIG="config_swarm_t${t}.txt"
        echo "master $MASTER_IP $MASTER_PORT" > "$CONFIG"

        SLAVE_IPS=()
        SLAVE_PORTS=()
        SLAVE_CORES=()
        for ((i=0; i<t; i++)); do
            DRONE_IDX=$((i % DRONE_COUNT))
            ROUND=$((i / DRONE_COUNT))
            S_IP="${DRONE_IPS[$DRONE_IDX]}"
            S_PORT=$((SLAVE_PORT + ROUND))
            CORE_ID=$((ROUND + 1))
            SLAVE_IPS+=("$S_IP")
            SLAVE_PORTS+=("$S_PORT")
            SLAVE_CORES+=("$CORE_ID")
            echo "slave $S_IP $S_PORT" >> "$CONFIG"
        done

        for ((run=1; run<=RUNS; run++)); do
            COUNT=$((COUNT + 1))

            # Skip if already done
            if [ -n "${DONE[$n,$t,$run]}" ]; then
                printf "[%d/%d] n=%d, t=%d, run %d ... SKIP (already saved)\n" \
                    "$COUNT" "$TOTAL" "$n" "$t" "$run"
                continue
            fi

            printf "[%d/%d] n=%d, t=%d, run %d (timeout=%ds) ... " \
                "$COUNT" "$TOTAL" "$n" "$t" "$run" "$RUN_TIMEOUT"

            cp "$CONFIG" "$HOME/$CONFIG" 2>/dev/null

            SLAVE_PIDS=()
            SLAVE_LOGS=()
            for ((i=0; i<t; i++)); do
                S_IP="${SLAVE_IPS[$i]}"
                S_PORT="${SLAVE_PORTS[$i]}"
                CORE_ID="${SLAVE_CORES[$i]}"
                SLAVE_ID=$((i + 1))
                LOG="$LOG_DIR/n${n}_t${t}_r${run}_s${SLAVE_ID}_${S_IP}_p${S_PORT}.log"
                SLAVE_LOGS+=("$LOG")
                ssh $SSH_OPTS "$USERNAME@$S_IP" \
                    "cd ~ && $EXE $n $S_PORT $SLAVE_ID $CONFIG $CORE_ID > $LOG 2>&1" &
                SLAVE_PIDS+=($!)
            done

            sleep 5

            MASTER_OUTPUT=$(timeout ${RUN_TIMEOUT}s $EXE $n $MASTER_PORT 0 "$CONFIG" 2>&1)
            MASTER_TIME=$(echo "$MASTER_OUTPUT" | grep -oP 'time elapsed:\s+\K[\d.]+' | tail -1)

            for pid in "${SLAVE_PIDS[@]}"; do
                kill $pid 2>/dev/null
            done
            wait 2>/dev/null

            for ip in "${DRONE_IPS[@]}"; do
                ssh $SSH_OPTS "$USERNAME@$ip" "pkill lab05 2>/dev/null" 2>/dev/null &
            done
            wait

            SLAVE_MAX=""
            for LOG in "${SLAVE_LOGS[@]}"; do
                if [ -f "$LOG" ]; then
                    STIME=$(grep -oP 'time elapsed:\s+\K[\d.]+' "$LOG" | tail -1)
                    if [ -n "$STIME" ]; then
                        if [ -z "$SLAVE_MAX" ] || awk "BEGIN{exit !($STIME > $SLAVE_MAX)}"; then
                            SLAVE_MAX="$STIME"
                        fi
                    fi
                fi
            done

            # Append result to CSV — done IMMEDIATELY so a crash here loses
            # at most this single run. sync flushes to disk for power-fail safety.
            if [ -n "$MASTER_TIME" ] && [ -n "$SLAVE_MAX" ]; then
                printf "master=%s, slave_max=%s\n" "$MASTER_TIME" "$SLAVE_MAX"
                echo "$n,$t,$run,$MASTER_TIME,$SLAVE_MAX" >> "$CSV"
                DONE["$n,$t,$run"]=1
            else
                printf "ERROR (master=%s slave_max=%s)\n" "${MASTER_TIME:-NULL}" "${SLAVE_MAX:-NULL}"
                echo "$n,$t,$run,${MASTER_TIME:-ERROR},${SLAVE_MAX:-ERROR}" >> "$CSV"
                echo "  Master output (head):"
                echo "$MASTER_OUTPUT" | head -3 | sed 's/^/    /'
            fi
            sync 2>/dev/null

            sleep 5
        done

        rm -f "$CONFIG" "$HOME/$CONFIG" 2>/dev/null
    done
done

echo ""
echo "=========================================="
echo "Paper Table 1 (master) — swarm, core-affine"
echo "=========================================="
printf "%-10s %-5s %-14s %-14s %-14s %-14s\n" "n" "t" "Run 1" "Run 2" "Run 3" "Average"
printf -- "-----------------------------------------------------------------------\n"
for n in "${N_VALUES[@]}"; do
    for t in "${T_VALUES[@]}"; do
        TIMES=()
        for ((run=1; run<=RUNS; run++)); do
            val=$(grep "^${n},${t},${run}," "$CSV" | grep -v ERROR | tail -1 | cut -d',' -f4)
            TIMES+=("${val:-MISSING}")
        done
        if [ -n "${TIMES[0]}" ] && [ "${TIMES[0]}" != "MISSING" ] && [ "${TIMES[0]}" != "ERROR" ]; then
            AVG=$(echo "scale=6; (${TIMES[0]} + ${TIMES[1]} + ${TIMES[2]}) / 3" | bc 2>/dev/null)
            printf "%-10s %-5s %-14s %-14s %-14s %-14s\n" "$n" "$t" "${TIMES[0]}" "${TIMES[1]}" "${TIMES[2]}" "${AVG:-N/A}"
        else
            printf "%-10s %-5s %-14s %-14s %-14s %-14s\n" "$n" "$t" "${TIMES[0]}" "${TIMES[1]}" "${TIMES[2]}" "N/A"
        fi
    done
    echo ""
done

echo "=========================================="
echo "Paper Table 2 (slave max) — swarm, core-affine"
echo "=========================================="
printf "%-10s %-5s %-14s %-14s %-14s %-14s\n" "n" "t" "Max Run 1" "Max Run 2" "Max Run 3" "Average"
printf -- "-----------------------------------------------------------------------\n"
for n in "${N_VALUES[@]}"; do
    for t in "${T_VALUES[@]}"; do
        TIMES=()
        for ((run=1; run<=RUNS; run++)); do
            val=$(grep "^${n},${t},${run}," "$CSV" | grep -v ERROR | tail -1 | cut -d',' -f5)
            TIMES+=("${val:-MISSING}")
        done
        if [ -n "${TIMES[0]}" ] && [ "${TIMES[0]}" != "MISSING" ] && [ "${TIMES[0]}" != "ERROR" ]; then
            AVG=$(echo "scale=6; (${TIMES[0]} + ${TIMES[1]} + ${TIMES[2]}) / 3" | bc 2>/dev/null)
            printf "%-10s %-5s %-14s %-14s %-14s %-14s\n" "$n" "$t" "${TIMES[0]}" "${TIMES[1]}" "${TIMES[2]}" "${AVG:-N/A}"
        else
            printf "%-10s %-5s %-14s %-14s %-14s %-14s\n" "$n" "$t" "${TIMES[0]}" "${TIMES[1]}" "${TIMES[2]}" "N/A"
        fi
    done
    echo ""
done

echo "Raw CSV: $CSV"
echo "Slave logs kept under: $LOG_DIR/"
echo ""
echo "If any rows show MISSING or ERROR, just re-run ./table3.sh — it"
echo "will skip everything that's already saved and re-attempt the rest."
