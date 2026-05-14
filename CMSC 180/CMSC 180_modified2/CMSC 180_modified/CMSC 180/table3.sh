#!/bin/bash
# ============================================================
# Table 3: Different PCs (ICS Swarm)
# ============================================================
# Run this on the MASTER drone. It will SSH into slave drones
# to start slaves remotely, then run the master locally.
#
# BEFORE RUNNING:
#   1. Edit DRONE_IPS below with actual drone IPs
#   2. Make sure lab04_v3 is compiled and accessible on all
#      drones (upload to overqueen, drones share filesystem)
#   3. Make sure ssh key-based auth works (or you'll be
#      prompted for password for every slave on every run)
#   4. Run this script ON one of the drones (NOT overqueen)
#
# Usage:
#   chmod +x table3.sh
#   ./table3.sh
#
# Output:
#   - lab04_table3.csv (raw timing data)
#   - formatted table printed to terminal
# ============================================================


# ==============================
# EDIT THESE BEFORE RUNNING
# ==============================


# your swarm username (UP email without @up.edu.ph)
USERNAME="cpdelosreyes"


# path to lab04_v3 on the swarm (shared filesystem from overqueen)
# adjust this if your home directory path is different
REMOTE_EXE="/home/$USERNAME/lab04_v3"


# the drone you are running this script on (master)
MASTER_IP="10.0.9.125"


# all available drone IPs for slaves (list at least 16)
# the script will pick the first t drones from this list
# IMPORTANT: IPs are dynamic! Check http://10.0.9.19/ for today's IPs
DRONE_IPS=(
    "10.0.9.168"
    "10.0.9.158"
    "10.0.9.166"
    "10.0.9.117"
    "10.0.9.176"
    "10.0.9.156"
    "10.0.9.111"
    "10.0.9.143"
    "10.0.9.179"
    "10.0.9.136"
    "10.0.9.173"
    "10.0.9.167"
    "10.0.9.109"
    "10.0.9.164"
    "10.0.9.133"
    "10.0.9.186"
)


# ==============================
# DO NOT EDIT BELOW THIS LINE
# ==============================


EXE="$REMOTE_EXE"
MASTER_PORT=5000
SLAVE_PORT=5001
CSV="lab04_table3.csv"


N_VALUES=(4000 8000 16000)
T_VALUES=(2 4 8 16)
RUNS=3


# ssh options: skip host key check, timeout after 5 seconds
SSH_OPTS="-o StrictHostKeyChecking=no -o ConnectTimeout=5"


# check if executable exists locally
if [ ! -f "$EXE" ]; then
    echo "Error: $EXE not found on this drone."
    echo "Upload to overqueen first: scp lab04_v3.c $USERNAME@10.0.9.20:~"
    echo "Then compile: gcc -o lab04_v3 lab04_v3.c -lpthread -lm"
    exit 1
fi


# check we have enough drones
MAX_T=16
if [ "${#DRONE_IPS[@]}" -lt "$MAX_T" ]; then
    echo "Warning: Only ${#DRONE_IPS[@]} drone IPs listed. t=16 needs 16 drones."
    echo "For t values that need more drones than available, slaves will"
    echo "share drones using different ports."
    echo ""
fi


echo "=========================================="
echo "Table 3: Different PCs (ICS Swarm)"
echo "=========================================="
echo "Master drone: $MASTER_IP"
echo "Slave drones: ${DRONE_IPS[*]:0:4} ..."
echo "Username: $USERNAME"
echo ""


# csv header
echo "n,t,run,time" > "$CSV"


TOTAL=$(( ${#N_VALUES[@]} * ${#T_VALUES[@]} * RUNS ))
COUNT=0


for n in "${N_VALUES[@]}"; do
    for t in "${T_VALUES[@]}"; do


        # generate config file
        CONFIG="config_swarm_t${t}.txt"
        echo "master $MASTER_IP $MASTER_PORT" > "$CONFIG"


        # assign drones to slaves
        # if enough drones: 1 slave per drone, all use same port
        # if not enough: wrap around drones, use different ports
        SLAVE_IPS=()
        SLAVE_PORTS=()
        DRONE_COUNT=${#DRONE_IPS[@]}


        for ((i=0; i<t; i++)); do
            DRONE_IDX=$((i % DRONE_COUNT))
            DRONE_ROUND=$((i / DRONE_COUNT))
            S_IP="${DRONE_IPS[$DRONE_IDX]}"
            S_PORT=$((SLAVE_PORT + DRONE_ROUND))


            SLAVE_IPS+=("$S_IP")
            SLAVE_PORTS+=("$S_PORT")
            echo "slave $S_IP $S_PORT" >> "$CONFIG"
        done


        for ((run=1; run<=RUNS; run++)); do
            COUNT=$((COUNT + 1))
            printf "[%d/%d] n=%d, t=%d, run %d ... " "$COUNT" "$TOTAL" "$n" "$t" "$run"


            # copy config to shared filesystem so drones can access it
            cp "$CONFIG" "$HOME/$CONFIG" 2>/dev/null


            # start slaves on remote drones via ssh
            SLAVE_PIDS=()
            for ((i=0; i<t; i++)); do
                S_IP="${SLAVE_IPS[$i]}"
                S_PORT="${SLAVE_PORTS[$i]}"


                # ssh into drone, run slave in background, redirect output
                ssh $SSH_OPTS "$USERNAME@$S_IP" \
                    "cd ~ && $EXE $n $S_PORT 1 $CONFIG > /dev/null 2>&1" &
                SLAVE_PIDS+=($!)
            done


            # wait for slaves to start listening on remote drones
            sleep 3


            # run master locally and capture output
            MASTER_OUTPUT=$($EXE $n $MASTER_PORT 0 "$CONFIG" 2>&1)


            # extract time from master output
            TIME_VAL=$(echo "$MASTER_OUTPUT" | grep -oP 'time elapsed:\s+\K[\d.]+')


            if [ -n "$TIME_VAL" ]; then
                printf "%s seconds\n" "$TIME_VAL"
                echo "$n,$t,$run,$TIME_VAL" >> "$CSV"
            else
                printf "ERROR\n"
                echo "$n,$t,$run,ERROR" >> "$CSV"
                # print full output for debugging
                echo "  Debug output:"
                echo "$MASTER_OUTPUT" | head -5 | sed 's/^/    /'
            fi


            # wait for ssh sessions to finish
            for pid in "${SLAVE_PIDS[@]}"; do
                wait "$pid" 2>/dev/null
            done


            # brief pause to release ports
            sleep 2
        done


        # clean up temp config
        rm -f "$CONFIG"
        rm -f "$HOME/$CONFIG" 2>/dev/null
    done
done


echo ""
echo "=========================================="
echo "Table 3 complete! Results saved to: $CSV"
echo "=========================================="
echo ""


# print formatted table
printf "%-10s %-5s %-14s %-14s %-14s %-14s\n" "n" "t" "Run 1" "Run 2" "Run 3" "Average"
printf "%s\n" "-----------------------------------------------------------------------"


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