# Lab 04 Run Guide — How to Run in the Lab / ICS Swarm

Charles Andrei P. De los Reyes | 2023-15797 | B-3L

---

## Overview

Lab 04 requires **three tables** of timing data, all from the master's perspective:

| Table | Script | Where | Core Affinity |
|-------|--------|-------|---------------|
| Table 1 | `table1.sh` | Single PC | No |
| Table 2 | `table2.sh` | Single PC | Yes |
| Table 3 | `table3.sh` | ICS Swarm (multiple drones) | No |

Each table: n = 4000, 8000, 16000 x t = 2, 4, 8, 16 x 3 runs = **144 total runs**.

Each script auto-generates config files, starts/stops slaves, runs the master,
captures timing, and prints the formatted table + CSV at the end.

---

## Files You Need to Bring

```
lab04_v3.c          <- source code
table1.sh           <- script for Table 1
table2.sh           <- script for Table 2
table3.sh           <- script for Table 3
input.txt           <- for verification test (3x3 matrix)
config.txt          <- for manual verification test
```

---

## Part 0: First-Time Setup (Do Once on the Lab PC)

### 0.1 Copy files to the lab PC

Use a USB drive or scp to get all files onto the lab PC.

### 0.2 Compile

```bash
gcc -o lab04_v3 lab04_v3.c -lpthread -lm
```

No `-lws2_32` on Linux. If you get a `sched.h` error:
```bash
gcc -o lab04_v3 lab04_v3.c -lpthread -lm -D_GNU_SOURCE
```

### 0.3 Make scripts executable

```bash
chmod +x table1.sh table2.sh table3.sh
```

### 0.4 Verify the program works (do this first!)

Run a quick verification test with `input.txt` before collecting timing data.

Open 3 terminals:
```bash
# Terminal 1 (slave 1):
./lab04_v3 3 5001 1 config.txt

# Terminal 2 (slave 2):
./lab04_v3 3 5002 1 config.txt

# Terminal 3 (master, with input file):
./lab04_v3 3 5000 0 config.txt input.txt
```

Expected:
- Master prints full 3x3 matrix, then submatrix for each slave
- Slave 1 prints rows 0-1: `11 12 13 / 14 15 16`
- Slave 2 prints row 2: `17 18 19`
- Both slaves send "ack" to parent (tree ack)
- Master prints received acks and time elapsed

Take a **screenshot** — this is the grading basis (item 6 in the PDF).

---

## Part 0.5: Running Manually (Without Scripts)

Use this section when you want to:
- Demo the tree distribution during the RA4 presentation (the professor asked for visible per-hop matrix prints).
- Debug a failing run without the script's cleanup automation in the way.
- Time one specific (n, t) combination without doing the full 48-run sweep.

### Argument cheat sheet

```
./lab04_v3 <n> <port> <s> <config_file> [input_file | core_id]
```

| Arg | Meaning |
|-----|---------|
| `<n>` | Matrix size (master reads from `input_file` if supplied and overrides this) |
| `<port>` | Port this process will bind/connect to |
| `<s>` | Role: `0` = master, `1..t` = slave id |
| `<config_file>` | Text file listing master + slaves |
| `[input_file]` | Master only: read matrix from a file (triggers the verification prints) |
| `[core_id]` | Slave only: pin this slave to a CPU core (Table 2 style) |

### Slaves must start FIRST, master LAST

The master's `connect()` retries for ~15 s (30 × 500 ms). If it can't find a listener, it gives up. Always start all slave terminals first, then run the master.

---

### Demo A — Quick verification (t = 2, n = 3)

Uses your existing `config.txt` and `input.txt`. Needs **3 terminals** on the same Linux PC.

**`config.txt`** (already in your folder):
```
master 127.0.0.1 5000
slave 127.0.0.1 5001
slave 127.0.0.1 5002
```

**Terminal 1 — Slave 1:**
```bash
./lab04_v3 3 5001 1 config.txt
```

**Terminal 2 — Slave 2:**
```bash
./lab04_v3 3 5002 2 config.txt
```

**Terminal 3 — Master (run last, with the input file):**
```bash
./lab04_v3 3 5000 0 config.txt input.txt
```

**What you should see on the master terminal:**
```
--- Full Matrix M (3 x 3) ---
11 12 13
14 15 16
17 18 19
[Master] Tree: 1 direct children (O(log 2))
[-> Slave 2] SENT 1 rows (rows 2 to 2):
17 18 19
[Master] Received 'ack' from Slave 2 (subtree: 1)
time elapsed: 0.00XXXX seconds
```

**Slave 1 terminal** should print rows 0–1 under `[Slave 1] RECEIVED ...`. **Slave 2 terminal** should print row 2 under `[Slave 2] RECEIVED ...`.

---

### Demo B — Presentation run (t = 4, n = 8)

This is the run that visibly demonstrates the **tree forwarding hop** (Slave 3 receives a chunk then forwards part of it to Slave 4). Needs **5 terminals**.

**Create `config4.txt`:**
```
master 127.0.0.1 5000
slave 127.0.0.1 5001
slave 127.0.0.1 5002
slave 127.0.0.1 5003
slave 127.0.0.1 5004
```

**Create `input8.txt`** (first line = n, then n rows of comma-separated ints):
```
8
11,12,13,14,15,16,17,18
21,22,23,24,25,26,27,28
31,32,33,34,35,36,37,38
41,42,43,44,45,46,47,48
51,52,53,54,55,56,57,58
61,62,63,64,65,66,67,68
71,72,73,74,75,76,77,78
81,82,83,84,85,86,87,88
```

**Run (in order):**
```bash
# terminal 1–4 (slaves)
./lab04_v3 8 5001 1 config4.txt
./lab04_v3 8 5002 2 config4.txt
./lab04_v3 8 5003 3 config4.txt
./lab04_v3 8 5004 4 config4.txt

# terminal 5 (master, last)
./lab04_v3 8 5000 0 config4.txt input8.txt
```

**What to point at during the demo:**

| Screen line | What it proves |
|-------------|----------------|
| `[Master] Tree: 2 direct children (O(log 4))` | Only 2 parallel sends from the master, not 4. |
| `[-> Slave 3] SENT 4 rows ...` (on master) | Master gave Slave 3 the whole right half. |
| `[Slave 3] RECEIVED 4 x 8 submatrix ...` | Slave 3 actually got the 4 rows. |
| `[-> Slave 4] SENT 2 rows ...` (on Slave 3's terminal) | Slave 3 forwarded half of its chunk further down the tree. |
| `[Slave 4] RECEIVED 2 x 8 submatrix ...` | Slave 4 got its chunk from Slave 3, not from the master. |
| `[Slave 3] Received ack from Slave 4` | Ack travels back up the tree. |
| `time elapsed:` on master | Wraps only the distribution + ack phase (spec items 2d–2h). |

This is exactly the "print the matrix after every send or receive" behaviour the professor asked for in her RA4 announcement.

---

### Demo C — Custom timing run (large n, manual timing)

Use this to reproduce a single row of Table 1/2 by hand, e.g. n = 4000, t = 4, no affinity. No matrix printing (guard is `n <= 32`), so the output stays clean.

**`config4.txt`** (same as Demo B).

Open **5 terminals**:
```bash
# terminals 1–4: slaves
./lab04_v3 4000 5001 1 config4.txt
./lab04_v3 4000 5002 2 config4.txt
./lab04_v3 4000 5003 3 config4.txt
./lab04_v3 4000 5004 4 config4.txt

# terminal 5: master (no input file → random matrix)
./lab04_v3 4000 5000 0 config4.txt
```

Read the master's last line: `time elapsed: 0.02XXXX seconds`. That's the number you'd record.

For a core-affinity run (Table 2 style), add a core id to each slave:
```bash
./lab04_v3 4000 5001 1 config4.txt 1    # slave 1 pinned to core 1
./lab04_v3 4000 5002 2 config4.txt 2    # slave 2 pinned to core 2
./lab04_v3 4000 5003 3 config4.txt 3
./lab04_v3 4000 5004 4 config4.txt 4
```

---

### Running many slaves with fewer terminals (tmux)

Opening 17 terminals for t = 16 is painful. Use tmux:
```bash
tmux new -s lab04
# inside tmux: Ctrl+b " to split horizontal, Ctrl+b % to split vertical
# Ctrl+b arrow-keys to move between panes
# run one slave per pane, then run the master in the last pane
```

Or use a one-liner that backgrounds the slaves in the same terminal (quick, but mixes output):
```bash
for i in $(seq 1 4); do
    ./lab04_v3 8 $((5000 + i)) $i config4.txt &
done
sleep 1
./lab04_v3 8 5000 0 config4.txt input8.txt
wait
```

The `&` backgrounds each slave, `sleep 1` lets them start listening, the master runs in the foreground, `wait` waits for everything to finish.

---

### Cleanup after a manual run

If something goes wrong (Ctrl+C'd the master, zombie slaves, port still in use):
```bash
pkill -f lab04_v3          # kill leftover processes
sleep 2                    # let the kernel release ports
lsof -i :5001 -i :5002     # verify nothing is holding the ports
```

---

### Windows note

This program uses `pthread` and `sched_setaffinity`. Manual runs only work reliably on **Linux**. On Windows you'd need MinGW + Winsock linkage (`-lws2_32`) and the affinity path falls through to `SetProcessAffinityMask`, but the lab grading environment is Linux — do your real runs there.

---

## Part 1: Table 1 — Single PC, No Core Affinity

### What `table1.sh` does

1. For each (n, t) combination, generates a config file with t slaves on `127.0.0.1`
2. Starts t slave processes in the background
3. Waits 2 seconds for slaves to be ready
4. Runs the master and captures `time elapsed` from its output
5. Waits for all slaves to finish, pauses to release ports
6. Repeats 3 times per (n, t)
7. Saves raw data to `lab04_table1.csv`
8. Prints formatted table at the end

### How to run

```bash
./table1.sh
```

That's it. Sit back and wait. It runs all 48 tests automatically.

### Output

Terminal will show progress:
```
==========================================
Table 1: Single PC, No Core Affinity
==========================================

[1/48] n=4000, t=2, run 1 ... 0.045231 seconds
[2/48] n=4000, t=2, run 2 ... 0.043892 seconds
...
```

At the end, it prints the formatted table:
```
n          t     Run 1          Run 2          Run 3          Average
-----------------------------------------------------------------------
4000       2     0.045231       0.043892       0.044102       0.044408
4000       4     ...
...
```

Raw CSV saved to `lab04_table1.csv` for backup.

### How long does it take?

Rough estimate per run:
- n=4000: ~1-2 seconds
- n=8000: ~3-5 seconds
- n=16000: ~10-20 seconds

Total: roughly 15-30 minutes depending on the PC.

---

## Part 2: Table 2 — Single PC, With Core Affinity

### What `table2.sh` does

Same as `table1.sh`, but:
- Detects CPU cores with `nproc`
- Reserves core 0 for the OS
- Assigns each slave to a core round-robin: slave 1 -> core 1, slave 2 -> core 2, etc.
- If t > available cores, wraps around (e.g., 8 cores, t=16: cores 1,2,3,4,5,6,7,1,2,3,4,5,6,7,1,2)

### How to run

```bash
./table2.sh
```

It will show detected cores at the top:
```
==========================================
Table 2: Single PC, With Core Affinity
==========================================
Detected 8 cores. Using cores 1-7 for slaves.
```

### Output

Same format as Table 1. Saved to `lab04_table2.csv`.

---

## Part 3: Table 3 — Different PCs (ICS Swarm)

This is the most complex setup. The script SSHes into remote drones
to start slaves, then runs the master on the local drone.

### 3.1 Swarm Quick Reference

| Item | Details |
|------|---------|
| Swarm main page | http://10.0.9.19/ (drone list + IPs) |
| Overqueen | http://10.0.9.20:9090 (gateway node, file upload target) |
| Drones | 45 drones, 2 cores each, 2MB DRAM |
| Username | UP email without `@up.edu.ph` |
| Password | MD5 hash of `username + student_number` (no spaces, 9-digit number) |
| File transfer | `scp yourfile <user>@10.0.9.20:~` (to overqueen, drones share filesystem) |
| RULE | DO NOT run programs on the overqueen — use drones only |
| IPs are DYNAMIC | IPs change via DHCP — always check http://10.0.9.19/ for current IPs |

### 3.2 Generate your password

1. Username: remove `@up.edu.ph` from your UP email
   - Example: `cpdelosreyes@up.edu.ph` -> username = `cpdelosreyes`
2. Concatenate username + 9-digit student number: `cpdelosreyes202315797`
3. MD5 hash it (the swarm page links to a generator)
4. That 32-character hash string is your password

### 3.3 Upload files to the Swarm

```bash
scp lab04_v3.c table3.sh <username>@10.0.9.20:~
```

### 3.4 Compile on the Swarm

SSH into any drone (not the overqueen):
```bash
ssh <username>@<any_drone_ip>
cd ~
gcc -o lab04_v3 lab04_v3.c -lpthread -lm
chmod +x lab04_v3 table3.sh
```

Since drones share the filesystem, compiling once makes it available everywhere.

### 3.5 Find available drones

1. Open http://10.0.9.19/ (use `http`, NOT `https` — browsers may auto-upgrade)
2. Scroll to the drone table
3. Write down IPs of drones you want to use (format: 10.0.9.XXX)
4. You need at least 17 drones for t=16 (1 master + 16 slaves)
5. IPs are assigned by DHCP and may change daily — always check the page

You can also access drones via the Cockpit web interface by clicking
drone names on the page (use `http`, not `https`).

### 3.6 Edit `table3.sh` with actual drone IPs

Before running, edit the top of `table3.sh`:

```bash
nano table3.sh
```

Change these variables with **today's IPs from http://10.0.9.19/**:
```bash
# your swarm username
USERNAME="cpdelosreyes"

# the drone YOU are on (master) — fanny
MASTER_IP="10.0.9.125"

# all available slave drone IPs (list at least 16)
# IMPORTANT: get these from http://10.0.9.19/ — they change daily!
DRONE_IPS=(
    "10.0.9.168"    # sora
    "10.0.9.158"    # natan
    "10.0.9.166"    # rafaela
    ... etc ...
)
```

### 3.7 Set up SSH key-based auth (recommended)

Without this, you'll be prompted for your password for every slave on every run
(that's 16 password prompts x 48 runs = painful).

```bash
# on the master drone, generate ssh key (press Enter for all prompts)
ssh-keygen -t rsa -N ""

# copy key to each slave drone
ssh-copy-id cpdelosreyes@10.0.9.168
ssh-copy-id cpdelosreyes@10.0.9.158
# ... repeat for all 16 slave drones

# test it works (should not ask for password)
ssh cpdelosreyes@10.0.9.168 "echo hello"
```

If `ssh-copy-id` is not available:
```bash
cat ~/.ssh/id_rsa.pub | ssh cpdelosreyes@10.0.9.168 'mkdir -p ~/.ssh && cat >> ~/.ssh/authorized_keys'
```

Since drones share filesystem, you might only need to do this once and all
drones will have your key.

### 3.8 What `table3.sh` does

1. For each (n, t), generates a config file with master IP + slave drone IPs
2. SSHes into each slave drone and starts the slave process remotely
3. Waits 3 seconds for remote slaves to start listening
4. Runs the master locally on the current drone
5. Captures the master's `time elapsed`
6. Waits for SSH sessions to finish
7. Repeats 3 times per (n, t)
8. Saves raw data to `lab04_table3.csv`
9. Prints formatted table

### 3.9 How to run

```bash
# SSH into your chosen master drone (use today's IP from the swarm page)
ssh cpdelosreyes@10.0.9.125

# run the script
cd ~
./table3.sh
```

### 3.10 Handling not enough drones

The script handles this automatically. If you have fewer drones than t:
- It wraps around and puts multiple slaves on the same drone
- Each slave on the same drone gets a different port (5001, 5002, ...)
- Example: 8 drones available, t=16 -> 2 slaves per drone

### 3.11 Manual fallback (if the script doesn't work on Swarm)

If SSH automation doesn't work (firewall, permissions, etc.), run manually:

**For each (n, t, run) combination:**

1. SSH into each slave drone in separate terminal tabs
2. Start slaves manually:
   ```bash
   # on each slave drone:
   ./lab04_v3 4000 5001 1 config_swarm.txt
   ```
3. SSH into master drone, run master:
   ```bash
   ./lab04_v3 4000 5000 0 config_swarm.txt
   ```
4. Write down the time from the master terminal
5. Repeat for next run

This is slow but guaranteed to work regardless of SSH automation issues.

To make the manual approach faster, you can start slaves in one terminal
using SSH commands:
```bash
# from any machine, start slave on a remote drone in background
ssh cpdelosreyes@10.0.9.168 "./lab04_v3 4000 5001 1 config_swarm.txt" &
ssh cpdelosreyes@10.0.9.158 "./lab04_v3 4000 5001 1 config_swarm.txt" &
sleep 3
# then run master locally
./lab04_v3 4000 5000 0 config_swarm.txt
```

---

## Part 4: Answering Report Questions

### Question 3: "Repeat with core affinity. What happened?"
- Compare Table 1 vs Table 2 averages
- Core affinity reduces context switching overhead
- Effect is more visible when PC has enough physical cores for all slaves
- With fewer cores than slaves, the benefit diminishes (wrap-around sharing)

### Question 4: "Repeat on different PCs. What happened?"
- Compare Table 1 vs Table 3 averages
- Network latency adds overhead -> times may be higher for small n
- For large n, data transfer time dominates over connection setup
- True parallelism: each drone has its own CPU and memory

### Question 5: "What communication technique did you use?"
- Answer: **one-to-many personalized broadcast**
- Master sends a **unique** submatrix to **each** slave
- It's "personalized" because each slave gets different data
- It's "one-to-many" because one master sends to many slaves
- It's NOT a regular broadcast (regular = same data to all)

---

## Part 5: Troubleshooting

### "Address already in use" error
A previous run left the port occupied.
```bash
# option 1: wait 30-60 seconds and retry
# option 2: kill the process holding the port
kill $(lsof -t -i:5001) 2>/dev/null
# option 3: use fuser
fuser -k 5001/tcp
```

If this happens during a script run, stop the script (Ctrl+C), kill leftover processes, wait a moment, then restart:
```bash
# kill all lab04_v3 processes
pkill -f lab04_v3
sleep 2
# restart the script
./table1.sh
```

### "Connection refused" on master
- Slaves must start BEFORE master
- The master retries connections for 15 seconds (30 retries x 500ms)
- If slaves are on remote drones, the 3-second wait in `table3.sh` might not be enough
  - Edit `table3.sh` and increase `sleep 3` to `sleep 5` or `sleep 10`

### Master says "No ack from Slave X"
- That slave crashed or timed out
- Check if the slave drone is still running (SSH in and check)
- May be a memory issue for large n (n=16000 = ~1GB matrix)

### Script shows "ERROR" for a run
- The master didn't output `time elapsed`
- Debug output is printed (first 5 lines of master output)
- Common cause: slaves weren't ready in time, or port conflict

### Swarm login not working
- URL must be `http://` not `https://` (browsers may auto-upgrade — change it back)
- Double-check MD5 hash: `username + 9-digit student_number` with no spaces
- Must be on ICS PC-Lab 3 workstation network
- IPs are dynamic — check http://10.0.9.19/ for today's IPs

### "Permission denied" running scripts or executable
```bash
chmod +x lab04_v3 table1.sh table2.sh table3.sh
```

### SSH asks for password every time (Swarm)
Set up SSH keys — see Section 3.7 above.

### Not enough RAM for n=16000
- 16000 x 16000 x 4 bytes = ~976 MB just for the matrix
- Swarm drones only have **2MB DRAM** — this will likely fail for large n
- The master drone needs the full matrix; slaves only need their submatrix
- Try n=4000 and n=8000 first, then attempt n=16000
- If it fails, note it in your report as a hardware limitation

### `bc` command not found (for table formatting)
The scripts use `bc` for average calculation. If not installed:
```bash
# the CSV still has all raw data — calculate averages manually
cat lab04_table1.csv
```

### `grep -oP` not supported
Some minimal Linux installs don't have Perl regex in grep. Alternative:
```bash
# replace grep -oP with sed
TIME_VAL=$(echo "$MASTER_OUTPUT" | sed -n 's/.*time elapsed: \([0-9.]*\) seconds.*/\1/p')
```

If this affects the script, edit the `grep -oP` line in table1.sh/table2.sh/table3.sh.

### Leftover zombie processes after Ctrl+C
```bash
# kill all instances of lab04_v3
pkill -f lab04_v3
# verify
ps aux | grep lab04_v3
```

---

## Part 6: Checklist

### Before Lab Class
```
[ ] lab04_v3.c compiles on Linux: gcc -o lab04_v3 lab04_v3.c -lpthread -lm
[ ] All 3 scripts (table1.sh, table2.sh, table3.sh) on USB/ready to transfer
[ ] input.txt and config.txt for verification test
[ ] Know your Swarm username
[ ] Know your Swarm password (MD5 hash pre-computed)
[ ] Tested locally with input.txt (verification screenshot ready)
```

### At the Lab (order of operations)
```
[ ] Copy files to lab PC
[ ] Compile: gcc -o lab04_v3 lab04_v3.c -lpthread -lm
[ ] chmod +x table1.sh table2.sh table3.sh
[ ] Run verification test with input.txt (screenshot it)
[ ] Run ./table1.sh -> save lab04_table1.csv
[ ] Run ./table2.sh -> save lab04_table2.csv
[ ] Check http://10.0.9.19/ for today's drone IPs
[ ] Upload files to Swarm: scp lab04_v3.c table3.sh <user>@10.0.9.20:~
[ ] SSH into a drone, compile, chmod +x
[ ] Edit table3.sh with today's drone IPs from the swarm page
[ ] Set up SSH keys (optional but saves time)
[ ] Run ./table3.sh -> save lab04_table3.csv
[ ] Copy all CSV files back to USB
```

---

## Quick Reference: All Commands

```bash
# === compile ===
gcc -o lab04_v3 lab04_v3.c -lpthread -lm          # linux
gcc -o lab04_v3.exe lab04_v3.c -lws2_32 -lpthread  # windows

# === make scripts executable ===
chmod +x table1.sh table2.sh table3.sh

# === run tables ===
./table1.sh     # Table 1: single PC, no affinity  -> lab04_table1.csv
./table2.sh     # Table 2: single PC, with affinity -> lab04_table2.csv
./table3.sh     # Table 3: swarm, multi-PC          -> lab04_table3.csv

# === manual single run ===
./lab04_v3 <n> <port> 1 config.txt              # slave, no affinity
./lab04_v3 <n> <port> 1 config.txt <core_id>    # slave, with affinity
./lab04_v3 <n> <port> 0 config.txt              # master, random matrix
./lab04_v3 <n> <port> 0 config.txt input.txt    # master, from file

# === swarm upload (from ICS PC-Lab 3 only) ===
scp lab04_v3.c table3.sh <user>@10.0.9.20:~

# === swarm ssh ===
ssh <user>@<drone_ip>    # get IP from http://10.0.9.19/

# === kill stuck processes ===
pkill -f lab04_v3
```
