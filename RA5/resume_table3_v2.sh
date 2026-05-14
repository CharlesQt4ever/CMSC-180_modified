#!/bin/bash
# Lab 05 — resume table3 (n=16000 only). Same shape as resume_table3.sh
# but with a longer per-run timeout for the largest problem sizes.
USERNAME="cpdelosreyes"
EXE="/home/$USERNAME/lab05"
MASTER_IP="10.0.9.134"
MASTER_PORT=5000
SLAVE_PORT=5001
CSV="lab05_table3.csv"
LOG_DIR="lab05_slavelogs_t3"
SSH_OPTS="-o StrictHostKeyChecking=no -o ConnectTimeout=5"
RUN_TIMEOUT=900

DRONE_IPS=(
"10.0.9.132" "10.0.9.128" "10.0.9.110" "10.0.9.109"
"10.0.9.186" "10.0.9.111" "10.0.9.185" "10.0.9.155"
"10.0.9.174" "10.0.9.156" "10.0.9.158" "10.0.9.118"
"10.0.9.179" "10.0.9.176" "10.0.9.164" "10.0.9.129"
)

REMAINING=(
"16000 4 1" "16000 4 2" "16000 4 3"
"16000 8 1" "16000 8 2" "16000 8 3"
"16000 16 1" "16000 16 2" "16000 16 3"
)

mkdir -p "$LOG_DIR"
TOTAL=${#REMAINING[@]}
COUNT=0
DRONE_COUNT=${#DRONE_IPS[@]}

echo "Resume Lab 05 Table 3 v2 (n=16000): $TOTAL runs"
[ -f "$CSV" ] && echo "Existing CSV rows: $(( $(wc -l < "$CSV") - 1 ))"
echo "Per-run timeout: ${RUN_TIMEOUT}s"
echo ""

for combo in "${REMAINING[@]}"; do
    read n t run <<< "$combo"
    COUNT=$((COUNT + 1))

    CONFIG="config_swarm_t${t}.txt"
    echo "master $MASTER_IP $MASTER_PORT" > "$CONFIG"
    SLAVE_IPS=(); SLAVE_PORTS=(); SLAVE_CORES=()
    for ((i=0; i<t; i++)); do
        S_IP="${DRONE_IPS[$((i % DRONE_COUNT))]}"
        S_PORT=$((SLAVE_PORT + i / DRONE_COUNT))
        CORE_ID=$(( (i / DRONE_COUNT) + 1 ))
        SLAVE_IPS+=("$S_IP"); SLAVE_PORTS+=("$S_PORT"); SLAVE_CORES+=("$CORE_ID")
        echo "slave $S_IP $S_PORT" >> "$CONFIG"
    done
    cp "$CONFIG" "$HOME/$CONFIG" 2>/dev/null

    printf "[%d/%d] n=%d, t=%d, run %d ... " "$COUNT" "$TOTAL" "$n" "$t" "$run"

    SLAVE_PIDS=()
    SLAVE_LOGS=()
    for ((i=0; i<t; i++)); do
        SLAVE_ID=$((i + 1))
        LOG="$LOG_DIR/n${n}_t${t}_r${run}_s${SLAVE_ID}_${SLAVE_IPS[$i]}_p${SLAVE_PORTS[$i]}.log"
        SLAVE_LOGS+=("$LOG")
        ssh $SSH_OPTS "$USERNAME@${SLAVE_IPS[$i]}" \
            "cd ~ && $EXE $n ${SLAVE_PORTS[$i]} $SLAVE_ID $CONFIG ${SLAVE_CORES[$i]} > $LOG 2>&1" &
        SLAVE_PIDS+=($!)
    done

    sleep 5

    MASTER_OUTPUT=$(timeout ${RUN_TIMEOUT}s $EXE $n $MASTER_PORT 0 "$CONFIG" 2>&1)
    MASTER_TIME=$(echo "$MASTER_OUTPUT" | grep -oP 'time elapsed:\s+\K[\d.]+' | tail -1)

    for pid in "${SLAVE_PIDS[@]}"; do kill $pid 2>/dev/null; done
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
        printf "master=%s slave_max=%s\n" "$MASTER_TIME" "$SLAVE_MAX"
        echo "$n,$t,$run,$MASTER_TIME,$SLAVE_MAX" >> "$CSV"
    else
        printf "ERROR (master=%s slave_max=%s)\n" "${MASTER_TIME:-NULL}" "${SLAVE_MAX:-NULL}"
        echo "$n,$t,$run,${MASTER_TIME:-ERROR},${SLAVE_MAX:-ERROR}" >> "$CSV"
    fi

    sleep 5
    rm -f "$CONFIG" "$HOME/$CONFIG" 2>/dev/null
done

echo ""
echo "Resume v2 complete. CSV: $CSV"
wc -l "$CSV"
echo ""
echo "When done, scp back to lab PC:"
echo "  scp ${USERNAME}@${MASTER_IP}:~/${CSV} ."
