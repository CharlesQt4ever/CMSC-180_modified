# Lab 05 Run — Swarm Sweep

Charles Andrei P. De los Reyes | 2023-15797 | B-3L

Outputs: `lab05_T_S.csv` (T_S), `lab05_table3.csv` (Tables 1+2), `lab05_table3_metrics.csv` (Table 3).

> **Crash recovery:** if any script dies mid-sweep, just re-run it — both `lab01_baseline.sh` and `table3.sh` auto-resume from their CSV.

---

## 1. SCP files to overqueen (from local PC)

```bash
scp lab05.c lab01.c table3.sh lab01_baseline.sh compute_table3.sh \
    cpdelosreyes@10.0.9.20:~
```

## 2. SSH to a drone and compile

Drone IPs: http://10.0.9.19/ (use `http`, not `https`).

```bash
ssh cpdelosreyes@<drone_ip>
gcc -O2 -o lab05 lab05.c -lpthread -lm
gcc -O2 -o lab01 lab01.c -lm
chmod +x table3.sh lab01_baseline.sh compute_table3.sh
```

## 3. Edit `table3.sh` with TODAY's IPs

```bash
nano table3.sh
```

```bash
USERNAME="cpdelosreyes"
MASTER_IP="<drone_you_ssh_ed_into>"
DRONE_IPS=(
    "<ip1>"
    "<ip2>"
    ...        # 16+ IPs
)
```

## 4. (Recommended) SSH key auth — skip 16+ password prompts

```bash
ssh-keygen -t rsa -N ""
ssh-copy-id <user>@<each drone IP>
```

## 5. Run the sweep

```bash
./lab01_baseline.sh    # → lab05_T_S.csv          (T_S)
./table3.sh            # → lab05_table3.csv       (Tables 1 + 2)
./compute_table3.sh    # → lab05_table3_metrics.csv (Table 3)
```

## 6. Pull results back (from local PC)

```bash
scp cpdelosreyes@<master_drone_ip>:~/lab05_T_S.csv .
scp cpdelosreyes@<master_drone_ip>:~/lab05_table3.csv .
scp cpdelosreyes@<master_drone_ip>:~/lab05_table3_metrics.csv .
```

---

## Cleanup (port already in use)

```bash
pkill -f lab05
fuser -k 5001/tcp
```

## Start a sweep over from scratch

```bash
rm lab05_T_S.csv     && ./lab01_baseline.sh
rm lab05_table3.csv  && ./table3.sh
```
