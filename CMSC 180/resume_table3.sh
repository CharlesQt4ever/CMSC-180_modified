#!/bin/bash
USERNAME="cpdelosreyes"
EXE="/home/$USERNAME/lab04_v3"
MASTER_IP="10.0.9.134"
MASTER_PORT=5000
SLAVE_PORT=5001
CSV="lab04_table3.csv"
SSH_OPTS="-o StrictHostKeyChecking=no -o ConnectTimeout=5"
RUN_TIMEOUT=200

DRONE_IPS=(
"10.0.9.132" "10.0.9.128" "10.0.9.110" "10.0.9.109"
"10.0.9.186" "10.0.9.111" "10.0.9.185" "10.0.9.155"
"10.0.9.174" "10.0.9.156" "10.0.9.158" "10.0.9.118"
"10.0.9.179" "10.0.9.176" "10.0.9.164" "10.0.9.129"
)

REMAINING=(
"8000 16 2" "8000 16 3"
"16000 2 1" "16000 2 2" "16000 2 3"
"16000 4 1" "16000 4 2" "16000 4 3"
"16000 8 1" "16000 8 2" "16000 8 3"
"16000 16 1" "16000 16 2" "16000 16 3"
)

TOTAL=${#REMAINING[@]}
COUNT=0
DRONE_COUNT=${#DRONE_IPS[@]}

echo "Resuming Table 3: $TOTAL runs remaining"
echo "Existing CSV rows: $(( $(wc -l < "$CSV") - 1 ))"
echo ""

for combo in "${REMAINING[@]}"; do
read n t run <<< "$combo"
COUNT=$((COUNT + 1))

CONFIG="config_swarm_t${t}.txt"
echo "master $MASTER_IP $MASTER_PORT" > "$CONFIG"
SLAVE_IPS=(); SLAVE_PORTS=()
for ((i=0; i<t; i++)); do
S_IP="${DRONE_IPS[$((i % DRONE_COUNT))]}"
S_PORT=$((SLAVE_PORT + i / DRONE_COUNT))
SLAVE_IPS+=("$S_IP"); SLAVE_PORTS+=("$S_PORT")
echo "slave $S_IP $S_PORT" >> "$CONFIG"
done
cp "$CONFIG" "$HOME/$CONFIG" 2>/dev/null

printf "[%d/%d] n=%d, t=%d, run %d ... " "$COUNT" "$TOTAL" "$n" "$t" "$run"

SLAVE_PIDS=()
for ((i=0; i<t; i++)); do
ssh $SSH_OPTS "$USERNAME@${SLAVE_IPS[$i]}" "cd ~ && $EXE $n ${SLAVE_PORTS[$i]} 1 $CONFIG > /dev/null 2>&1" &
SLAVE_PIDS+=($!)
done

sleep 5

MASTER_OUTPUT=$(timeout ${RUN_TIMEOUT}s $EXE $n $MASTER_PORT 0 "$CONFIG" 2>&1)
TIME_VAL=$(echo "$MASTER_OUTPUT" | grep -oP 'time elapsed:\s+\K[\d.]+')

if [ -n "$TIME_VAL" ]; then
printf "%s seconds\n" "$TIME_VAL"
echo "$n,$t,$run,$TIME_VAL" >> "$CSV"
else
printf "ERROR (timeout or failure)\n"
echo "$n,$t,$run,ERROR" >> "$CSV"
fi

for pid in "${SLAVE_PIDS[@]}"; do kill $pid 2>/dev/null; done
wait 2>/dev/null

for ip in "${DRONE_IPS[@]}"; do
ssh $SSH_OPTS "$USERNAME@$ip" "pkill lab04_v3 2>/dev/null" 2>/dev/null &
done
wait

sleep 5
rm -f "$CONFIG" "$HOME/$CONFIG" 2>/dev/null
done

echo ""
echo "=========================================="
echo "Resume complete. CSV: $CSV"
echo "=========================================="
wc -l "$CSV"
