# Lab 05 Run Guide — How to Run in the Lab / ICS Swarm

Charles Andrei P. De los Reyes | 2023-15797 | B-3L

---

## Overview

LRP05 requires **two tables** of timing data and a derived **third table**
of performance metrics:

| Table | Source | Where it comes from |
|-------|--------|---------------------|
| Table 1 | Master `time elapsed` (full distribute → MMT → reduce wall-clock) | column 4 of the CSV |
| Table 2 | **Maximum** of slave `time elapsed` (compute window only) | column 5 of the CSV |
| Table 3 | T_O, S, E, pT_P derived from T_S (lab01) and T_P (Table 1 averages) | spreadsheet, after sweeps |

The **canonical environment** per LRP05 item 4 is "different machines,
slave processes core-affine" — that's what `table3.sh` produces. The
single-PC variants (`table1.sh`, `table2.sh`) are for sanity checking and
to compare against the swarm result.

Each sweep:
- n ∈ {4000, 8000, 16000} × t ∈ {2, 4, 8, 16} × 3 runs = **36 runs per script**
- Auto-generates the config file
- Starts/stops slaves
- Captures both master time and slave-max time per run
- Writes a CSV with `n,t,run,master_time,slave_max`
- Prints both Table 1 and Table 2 formatted at the end

---

## Files You Need to Bring

```
lab05.c              <- source code
table1.sh            <- single PC, no affinity sweep
table2.sh            <- single PC, with affinity sweep
table3.sh            <- swarm + affinity sweep (CANONICAL)
input8.txt           <- 8x8 verification matrix
input16.txt          <- 16x16 verification matrix
config_4pc.txt       <- 4-PC demo template (fill IPs at lab)
```

---

## Part 0: First-Time Setup

### 0.1 Compile

```bash
gcc -O2 -o lab05 lab05.c -lpthread -lm
```

If you get a `sched.h` error:
```bash
gcc -O2 -o lab05 lab05.c -lpthread -lm -D_GNU_SOURCE
```

### 0.2 Make scripts executable

```bash
chmod +x table1.sh table2.sh table3.sh resume_table3.sh resume_table3_v2.sh
```

### 0.3 Verify the program works

Quick 8×8 demo to confirm column distribution + MMT + reduce all work:

```bash
# config4.txt:
#   master 127.0.0.1 5000
#   slave 127.0.0.1 5001
#   slave 127.0.0.1 5002
#   slave 127.0.0.1 5003
#   slave 127.0.0.1 5004

# 4 slave terminals (each pinned to a core):
./lab05 8 5001 1 config4.txt 1
./lab05 8 5002 2 config4.txt 2
./lab05 8 5003 3 config4.txt 3
./lab05 8 5004 4 config4.txt 4

# 5th terminal — master with input file:
./lab05 8 5000 0 config4.txt input8.txt
```

Expected:
- Master prints full X, slave assignments, tree split, per-hop SENT
- Each slave prints received X, its own MMT column(s)
- Forwarder slaves print the assembled subtree T-strip
- Master prints rebuilt T (every column should normalize to [0,1])
- Master prints `time elapsed`

Take a screenshot — that's the LRP05 grading basis (item 1.1: "compute
the MMT into a matrix T of the respective columns").

---

## Part 0.5: Running Manually (Without Scripts)

Use this section for the demo or to time a single (n, t) combination.

### Argument cheat sheet

```
./lab05 <n> <port> <s> <config_file> [input_file | core_id]
```

| Arg | Meaning |
|-----|---------|
| `<n>` | Matrix size (master overrides from `input_file` if supplied) |
| `<port>` | Port this process binds/connects to |
| `<s>` | Role: `0` = master, `1..t` = slave id |
| `<config_file>` | Lines like `master ip port` and `slave ip port` |
| `[input_file]` | Master only: read X from a file |
| `[core_id]` | Slave only: pin process to a CPU core |

### Slaves first, master last

The master retries `connect()` for ~15 s. Always start all slaves, wait
for each to print `Listening on port ...`, then start the master.

---

### Demo A — Quick verification (t = 2, n = 3)

Edit `config.txt` to be:
```
master 127.0.0.1 5000
slave 127.0.0.1 5001
slave 127.0.0.1 5002
```

Create `input3.txt`:
```
3
11,12,13
14,15,16
17,18,19
```

3 terminals on the same PC:
```bash
./lab05 3 5001 1 config.txt 1     # slave 1
./lab05 3 5002 2 config.txt 2     # slave 2
./lab05 3 5000 0 config.txt input3.txt   # master, last
```

What the master should print:
```
--- Full Matrix X (3 x 3) ---
11 12 13
14 15 16
17 18 19
[Master] Slave 1 -> 2 cols (cols 0 to 1)
[Master] Slave 2 -> 1 cols (cols 2 to 2)
[Master] Tree: 1 direct children (O(log 2))
... per-hop SENT lines ...
[Master] Received T-strip from Slave 2 (cols 2..2, subtree=1)
time elapsed: 0.000XYZ seconds
--- Full Matrix T (3 x 3) ---
0.0000  0.0000  0.0000
0.5000  0.5000  0.5000
1.0000  1.0000  1.0000
```

(Every column min is row 0, max is row 2 → MMT gives 0.0/0.5/1.0 down
each column.) Each slave prints its received X and its computed strip.

---

### Demo B — Presentation run (t = 4, n = 8)

Demonstrates the **forwarding hop** (Slave 3 receives the right half of X
and forwards Slave 4's column range further). Needs **5 terminals**.

Use `config_4pc.txt` (4 slaves, all on 127.0.0.1 for local rehearsal).

```bash
./lab05 8 5001 1 config_4pc.txt 1
./lab05 8 5002 2 config_4pc.txt 2
./lab05 8 5003 3 config_4pc.txt 3
./lab05 8 5004 4 config_4pc.txt 4

./lab05 8 5000 0 config_4pc.txt input8.txt   # master, last
```

What to point at during the demo:

| Screen line | What it proves |
|-------------|----------------|
| Master: `Tree: 2 direct children (O(log 4))` | Only 2 parallel sends from the master |
| Master: `[-> Slave 3] SENT X + work [cols 4..7] (subtree of 2 slave(s))` | Master gave Slave 3 its half of the column work |
| Slave 3: `RECEIVED X (n=8), assignment cols [4..7] (subtree=2), my MMT cols [4..5]` | Slave 3 received its work descriptor |
| Slave 3: `Forwarding to Slave 4: cols [6..7], subtree=1` | Slave 3 forwarded Slave 4's columns, **before** its compute timer started |
| Slave 3: `time elapsed:` | MMT compute window only (LRP05 Table 2) |
| Slave 3: `Subtree T-strip ready (cols [4..7])` | Slave 3 assembled its own + Slave 4's strips |
| Slave 3: `Sent T-strip to parent (cols [4..7], 2 slave(s) in subtree)` | M1PR up the tree |
| Master: `Received T-strip from Slave 3 (cols 4..7, subtree=2)` | Master got Slave 3's full subtree strip |
| Master: `time elapsed:` | LRP05 Table 1 source |

---

### Demo C — Custom timing run (large n, manual timing)

Reproduce one row of Table 1/2 by hand, e.g., n = 4000, t = 4:

```bash
# 4 slave terminals, each pinned to a core
./lab05 4000 5001 1 config_4pc.txt 1
./lab05 4000 5002 2 config_4pc.txt 2
./lab05 4000 5003 3 config_4pc.txt 3
./lab05 4000 5004 4 config_4pc.txt 4

# master
./lab05 4000 5000 0 config_4pc.txt
```

Read off:
- Master's last line → master time (Table 1)
- Each slave's `time elapsed: ...` line → take the max → Table 2

Output stays clean because `n > 32` skips the matrix prints.

---

### Running many slaves with fewer terminals (tmux)

```bash
tmux new -s lab05
# Ctrl+b " — split horizontal
# Ctrl+b % — split vertical
# Ctrl+b arrow — move between panes
# run one slave per pane, master in the last
```

Or one-liner backgrounding:
```bash
for i in $(seq 1 4); do
    ./lab05 8 $((5000 + i)) $i config_4pc.txt $i &
done
sleep 1
./lab05 8 5000 0 config_4pc.txt input8.txt
wait
```

---

### Cleanup after a manual run

```bash
pkill -f lab05
sleep 2
lsof -i :5001 -i :5002 -i :5003 -i :5004
```

---

## Part 1: Table 1 (single PC, no affinity) — `./table1.sh`

What it does:
1. For each (n, t), generates a config file with t slaves on `127.0.0.1`
2. Starts t slave processes with stdout redirected to per-slave log files
3. Waits 2 s, runs the master, captures `master_time`
4. Joins all slaves; reads each log; computes `slave_max`
5. Repeats 3 runs per (n, t)
6. Saves `n,t,run,master_time,slave_max` to `lab05_table1.csv`
7. Prints formatted Table 1 (master) and Table 2 (slave-max) at the end

How to run:
```bash
./table1.sh
```

How long: ~15–30 minutes depending on PC.

---

## Part 2: Table 2 (single PC, with affinity) — `./table2.sh`

Same as `table1.sh` but pins each slave to a distinct core (round-robin
starting at core 1). Output: `lab05_table2.csv`.

```bash
./table2.sh
```

---

## Part 3: Table 3 (ICS Swarm, core-affine) — `./table3.sh`

This is the **canonical LRP05 environment**.

### 3.1 Swarm quick reference

| Item | Details |
|------|---------|
| Drone list / today's IPs | http://10.0.9.19/ (use `http`, not `https`) |
| Overqueen (file landing) | `10.0.9.20` (port 9090 = Cockpit; SSH = scp only — never run programs here) |
| Drones | ~45 nodes, ~2 cores each, **2 GB RAM** each |
| Username | UP email without `@up.edu.ph` |
| Password | MD5 hash of `username + 9-digit student number` |
| File transfer | `scp file <user>@10.0.9.20:~` (drones share filesystem) |
| Network | must be on ICS PC-Lab 3 workstation network |
| **CRITICAL** | IPs are dynamic — refresh from http://10.0.9.19/ daily |
| Memory note | n=16000 with full-X broadcast → ~1 GB X + ~1 GB T per drone. Borderline OOM on 2 GB drones; close other processes. |

### 3.2 Generate your password

1. Username: `cpdelosreyes` (UP email without `@up.edu.ph`)
2. Concatenate username + 9-digit student number: `cpdelosreyes202315797`
3. MD5 hash → 32-char string is your password

### 3.3 Upload files

```bash
scp lab05.c table3.sh resume_table3.sh resume_table3_v2.sh <user>@10.0.9.20:~
```

### 3.4 Compile on a drone

```bash
ssh <user>@<drone_ip>     # any drone; NOT overqueen
cd ~
gcc -O2 -o lab05 lab05.c -lpthread -lm
chmod +x lab05 table3.sh resume_table3.sh resume_table3_v2.sh
```

### 3.5 Find available drones

1. Open http://10.0.9.19/ (NOT https)
2. Note the IPs of 16+ drones
3. Pick one to be your master

### 3.6 Edit `table3.sh`

```bash
nano table3.sh
```

Change at the top:
```bash
USERNAME="cpdelosreyes"
MASTER_IP="10.0.9.134"   # the drone you SSH-ed into
DRONE_IPS=(
    "10.0.9.132"  # today's drone IPs (NOT yesterday's!)
    "10.0.9.128"
    ...
)
```

### 3.7 SSH key auth (recommended)

Without keys, you'll be prompted 16 times per run. Set up once:

```bash
ssh-keygen -t rsa -N ""
ssh-copy-id <user>@<each drone IP>
ssh <user>@<drone> "echo hello"     # verify no password
```

Drones share filesystem, so you may only need this on one drone for it to
"just work" everywhere.

### 3.8 Run the sweep

```bash
ssh <user>@<master_drone_ip>
cd ~
./table3.sh
```

Watch progress for ~1–2 hours (n=16000 over the swarm is slow).

### 3.9 If `table3.sh` crashes mid-sweep

Edit `resume_table3.sh`, set `REMAINING=("n t run" ...)` to the tuples
that still need to run. Then:

```bash
./resume_table3.sh
```

Use `resume_table3_v2.sh` for n=16000 — it bumps `RUN_TIMEOUT=900` (15 min).

### 3.10 Pull results back

```bash
scp <user>@<master_drone_ip>:~/lab05_table3.csv .
```

---

## Part 4: Building Table 3 (performance metrics)

LRP05 Research Activity 4 requires:

| Field | Formula |
|-------|---------|
| T_S | Lab 01 serial runtime for the same n |
| T_P | Average master time from your Table 1 (lab05_table3.csv col 4) |
| p | t (number of slaves) |
| T_O | p · T_P − T_S |
| S | T_S / T_P |
| E | S / p |
| pT_P | p · T_P |

Build the table in a spreadsheet:
1. Run lab01.c at n = 4000, 8000, 16000 → T_S per n (constant for each n).
2. Average the 3 master_time values per (n, t) from `lab05_table3.csv` → T_P.
3. Compute T_O, S, E, pT_P from the formulas.

See `LAB05_ANSWERS_DRAFT.md` for the discussion template (superlinearity,
cost-optimality).

---

## Part 5: Answering the LRP05 Research Activities

### Research Activity 2 (n, t, master time)
- 3D plot or 2D-with-3-lines plot using Table 1 (master) averages
- Discuss pattern as n and t increase

### Research Activity 3 (n, t, slave-max time)
- Same plot shape using Table 2 averages
- Why max, not average? Because the master cannot stop until **every**
  slave has finished — the slowest slave bounds the parallel time.
  Averaging would understate the actual parallel runtime.

### Research Activity 4 (Table 3 + discussion)
- Compare Figure 1 (comm + comp) vs Figure 2 (comp only)
- Difference ≈ communication + idling overhead
- Discuss superlinearity and cost-optimality

---

## Part 6: Troubleshooting

### "Address already in use"
```bash
pkill -f lab05
sleep 2
fuser -k 5001/tcp
```

### "Connection refused" on master
- Slaves must start before master
- Master retries for 15 s — if `table3.sh` shows ERROR, increase `sleep 5`

### Slave runs out of memory at n = 16000
- Approach C requires ~1 GB X + ~strip GB T per slave
- Swarm drones have 2 GB RAM total — close other tabs/processes
- Worst case: skip n=16000 row, document as hardware limitation

### Slave log file empty / `slave_max=NULL` in CSV
- Slave crashed before writing `time elapsed`
- Check `lab05_slavelogs_t3/` for `Killed` or OOM messages
- Re-run that combination via `resume_table3.sh`

### `bc` command not found
The scripts use `bc` for averages. Without it the CSV still has raw data:
```bash
cat lab05_table1.csv
```

### `grep -oP` not supported on minimal Linux
Replace with `sed`:
```bash
TIME=$(echo "$OUTPUT" | sed -n 's/.*time elapsed: \([0-9.]*\) seconds.*/\1/p')
```

---

## Part 7: Checklist

### Before lab class
```
[ ] lab05.c compiles on Linux: gcc -O2 -o lab05 lab05.c -lpthread -lm
[ ] All sweep scripts ready
[ ] input8.txt + input16.txt for verification
[ ] Know your Swarm username + password
[ ] Verification screenshot captured (input8 demo)
```

### At the lab
```
[ ] Compile + chmod
[ ] Verify with input8.txt (screenshot)
[ ] Run ./table1.sh → lab05_table1.csv
[ ] Run ./table2.sh → lab05_table2.csv
[ ] Check http://10.0.9.19/ for today's drone IPs
[ ] scp files to <user>@10.0.9.20:~
[ ] SSH to a drone, compile, chmod
[ ] Edit table3.sh with today's IPs
[ ] (optional) ssh-keygen + ssh-copy-id
[ ] Run ./table3.sh → lab05_table3.csv
[ ] scp lab05_table*.csv back to lab PC
[ ] Build Table 3 (performance metrics) in spreadsheet using lab01 T_S
```

---

## Quick Reference: All Commands

```bash
# === compile ===
gcc -O2 -o lab05 lab05.c -lpthread -lm                # linux
gcc -O2 -o lab05.exe lab05.c -lws2_32 -lpthread       # windows

# === sweeps ===
chmod +x table1.sh table2.sh table3.sh resume_table3.sh resume_table3_v2.sh
./table1.sh    # single PC, no affinity   → lab05_table1.csv
./table2.sh    # single PC, with affinity → lab05_table2.csv
./table3.sh    # swarm, core-affine       → lab05_table3.csv (canonical)
./resume_table3.sh      # continue partial run
./resume_table3_v2.sh   # continue n=16000 with longer timeout

# === manual single run ===
./lab05 <n> <port> <id> config.txt              # slave, no affinity
./lab05 <n> <port> <id> config.txt <core_id>    # slave, core-affine
./lab05 <n> <port> 0 config.txt                 # master, random matrix
./lab05 <n> <port> 0 config.txt input.txt       # master, from file

# === swarm ===
scp lab05.c table3.sh <user>@10.0.9.20:~
ssh <user>@<drone_ip>     # check http://10.0.9.19/ for today's IPs

# === cleanup ===
pkill -f lab05
```
