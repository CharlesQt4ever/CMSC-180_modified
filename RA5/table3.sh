#!/bin/bash
# ============================================================
# Lab 05 — Table 3: Different PCs (ICS Swarm), Core-Affine
# ============================================================
# This is the canonical LRP05 sweep: slaves on separate drones,
# each pinned to a core. Captures BOTH master time (LRP05 Table 1)
# and the maximum slave compute time (LRP05 Table 2) per run.
#
# Run on the MASTER drone. SSHes into slave drones to start
# slaves remotely, then runs the master locally.
#
# BEFORE RUNNING:
#   1. Edit DRONE_IPS below with TODAY's drone IPs (http://10.0.9.19/)
#   2. Make sure lab05 is compiled and in $REMOTE_EXE on the
#      shared filesystem (compile once on any drone via overqueen scp)
#   3. Set up SSH key auth, otherwise expect 16 password prompts/run
#   4. Run this script ON one of the drones (NOT overqueen)
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
    "10.0.9.132"
    "10.0.9.128"
    "10.0.9.110"
    "10.0.9.109"
    "10.0.9.186"
    "10.0.9.111"
    "10.0.9.185"
    "10.0.9.155"
    "10.0.9.174"
    "10.0.9.156"
    "10.0.9.158"
    "10.0.9.118"
    "10.0.9.179"
    "10.0.9.176"
    "10.0.9.164"
    "10.0.9.129"
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
RUN_TIMEOUT=600   # n=16000 over the swarm can take ~3 min; allow margin

SSH_OPTS="-o StrictHostKeyChecking=no -o ConnectTimeout=5"

if [ ! -f "$EXE" ]; then
    echo "Error: $EXE not found on this drone."
    echo "Upload to overqueen first: scp lab05.c $USERNAME@10.0.9.20:~"
    echo "Then on a drone: gcc -O2 -o lab05 lab05.c -lpthread -lm"
    exit 1
fi

mkdir -p "$LOG_DIR"
echo "n,t,run,master_time,slave_max" > "$CSV"

TOTAL=$(( ${#N_VALUES[@]} * ${#T_VALUES[@]} * RUNS ))
COUNT=0
DRONE_COUNT=${#DRONE_IPS[@]}

echo "=========================================="
echo "Lab 05 — Table 3: ICS Swarm, Core-Affine"
echo "=========================================="
echo "Master: $MASTER_IP, drones available: $DRONE_COUNT"
echo "Run timeout: ${RUN_TIMEOUT}s"
echo ""

for n in "${N_VALUES[@]}"; do
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
            # core index per drone: round 0 -> core 1, round 1 -> core 2, ...
            # (drones have ~2 cores; core 0 reserved for OS)
            CORE_ID=$((ROUND + 1))
            SLAVE_IPS+=("$S_IP")
            SLAVE_PORTS+=("$S_PORT")
            SLAVE_CORES+=("$CORE_ID")
            echo "slave $S_IP $S_PORT" >> "$CONFIG"
        done

        for ((run=1; run<=RUNS; run++)); do
            COUNT=$((COUNT + 1))
            printf "[%d/%d] n=%d, t=%d, run %d ... " "$COUNT" "$TOTAL" "$n" "$t" "$run"

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

            if [ -n "$MASTER_TIME" ] && [ -n "$SLAVE_MAX" ]; then
                printf "master=%s, slave_max=%s\n" "$MASTER_TIME" "$SLAVE_MAX"
                echo "$n,$t,$run,$MASTER_TIME,$SLAVE_MAX" >> "$CSV"
            else
                printf "ERROR (master=%s slave_max=%s)\n" "${MASTER_TIME:-NULL}" "${SLAVE_MAX:-NULL}"
                echo "$n,$t,$run,${MASTER_TIME:-ERROR},${SLAVE_MAX:-ERROR}" >> "$CSV"
                echo "  Master output (head):"
                echo "$MASTER_OUTPUT" | head -3 | sed 's/^/    /'
            fi

            sleep 5
        done

        rm -f "$CONFIG" "$HOME/$CONFIG" 2>/dev/null
    done
done

echo ""
echo "=========================================="
echo "Table 1 (master) — swarm, core-affine"
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
echo "Table 2 (slave max) — swarm, core-affine"
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
