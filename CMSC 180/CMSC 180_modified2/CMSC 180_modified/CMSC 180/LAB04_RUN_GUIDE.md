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
lab04_v2.c          <- source code
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
gcc -o lab04_v2 lab04_v2.c -lpthread -lm
```

No `-lws2_32` on Linux. If you get a `sched.h` error:
```bash
gcc -o lab04_v2 lab04_v2.c -lpthread -lm -D_GNU_SOURCE
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
./lab04_v2 3 5001 1 config.txt

# Terminal 2 (slave 2):
./lab04_v2 3 5002 1 config.txt

# Terminal 3 (master, with input file):
./lab04_v2 3 5000 0 config.txt input.txt
```

Expected:
- Master prints full 3x3 matrix, then submatrix for each slave
- Slave 1 prints rows 0-1: `11 12 13 / 14 15 16`
- Slave 2 prints row 2: `17 18 19`
- Both slaves compute local MMT and send "ack"
- Master prints received acks and time elapsed

Take a **screenshot** — this is the grading basis (item 6 in the PDF).

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
| Web dashboard | http://hive.swarm.ics.uplb.edu.ph or http://10.0.9.19:9000 |
| Network | Must be on UP network (10.0.X.X) |
| Username | UP email without `@up.edu.ph` |
| Password | MD5 hash of `username + student_number` (no spaces) |
| File transfer | `scp` to overqueen (10.0.9.19), drones share the filesystem |
| RULE | DO NOT run programs on the overqueen — use drones only |

### 3.2 Generate your password

1. Username: remove `@up.edu.ph` from your UP email
   - Example: `cadelosreyes@up.edu.ph` -> username = `cadelosreyes`
2. Concatenate: `cadelosreyes2023-15797`
3. MD5 hash it at https://www.md5hashgenerator.com/
4. That hash string is your password

### 3.3 Upload files to the Swarm

From your laptop (must be on UP network):
```bash
scp lab04_v2.c table3.sh input.txt <username>@10.0.9.19:~/
```

### 3.4 Compile on the Swarm

SSH into any drone (not the overqueen):
```bash
ssh <username>@<any_drone_ip>
cd ~
gcc -o lab04_v2 lab04_v2.c -lpthread -lm
chmod +x lab04_v2 table3.sh
```

Since drones share the filesystem, compiling once makes it available everywhere.

### 3.5 Find available drones

1. Open http://hive.swarm.ics.uplb.edu.ph (use `http`, NOT `https`)
2. Log in
3. Check which drones are online
4. Write down their IPs (10.0.9.XX format)
5. You need at least 17 drones for t=16 (1 master + 16 slaves)

### 3.6 Edit `table3.sh` with actual drone IPs

Before running, edit the top of `table3.sh`:

```bash
# open with nano or vi
nano table3.sh
```

Change these variables:
```bash
# your swarm username
USERNAME="cadelosreyes"

# the drone YOU are on (master)
MASTER_IP="10.0.9.21"

# all available slave drone IPs (list at least 16)
DRONE_IPS=(
    "10.0.9.22"
    "10.0.9.23"
    "10.0.9.24"
    ...fill with real drone IPs...
)
```

### 3.7 Set up SSH key-based auth (recommended)

Without this, you'll be prompted for your password for every slave on every run
(that's 16 password prompts x 48 runs = painful).

```bash
# on the master drone, generate ssh key (press Enter for all prompts)
ssh-keygen -t rsa -N ""

# copy key to each slave drone
ssh-copy-id <username>@10.0.9.22
ssh-copy-id <username>@10.0.9.23
# ... repeat for all drones

# test it works (should not ask for password)
ssh <username>@10.0.9.22 "echo hello"
```

If `ssh-copy-id` is not available:
```bash
cat ~/.ssh/id_rsa.pub | ssh <username>@10.0.9.22 'mkdir -p ~/.ssh && cat >> ~/.ssh/authorized_keys'
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
# SSH into your chosen master drone
ssh <username>@10.0.9.21

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
   ./lab04_v2 4000 5001 1 config_swarm.txt
   ```
3. SSH into master drone, run master:
   ```bash
   ./lab04_v2 4000 5000 0 config_swarm.txt
   ```
4. Write down the time from the master terminal
5. Repeat for next run

This is slow but guaranteed to work regardless of SSH automation issues.

To make the manual approach faster, you can start slaves in one terminal
using SSH commands:
```bash
# from any machine, start slave on a remote drone in background
ssh <username>@10.0.9.22 "./lab04_v2 4000 5001 1 config_swarm.txt" &
ssh <username>@10.0.9.23 "./lab04_v2 4000 5001 1 config_swarm.txt" &
sleep 3
# then run master locally
./lab04_v2 4000 5000 0 config_swarm.txt
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
# kill all lab04_v2 processes
pkill -f lab04_v2
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
- URL must be `http://` not `https://`
- Double-check MD5 hash: `username + student_number` with no spaces
- Must be connected to UP network (your IP should be 10.0.X.X)

### "Permission denied" running scripts or executable
```bash
chmod +x lab04_v2 table1.sh table2.sh table3.sh
```

### SSH asks for password every time (Swarm)
Set up SSH keys — see Section 3.7 above.

### Not enough RAM for n=16000
- 16000 x 16000 x 4 bytes = ~976 MB just for the matrix
- If a drone runs out of memory, the malloc will fail
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
# kill all instances of lab04_v2
pkill -f lab04_v2
# verify
ps aux | grep lab04_v2
```

---

## Part 6: Checklist

### Before Lab Class
```
[ ] lab04_v2.c compiles on Linux: gcc -o lab04_v2 lab04_v2.c -lpthread -lm
[ ] All 3 scripts (table1.sh, table2.sh, table3.sh) on USB/ready to transfer
[ ] input.txt and config.txt for verification test
[ ] Know your Swarm username
[ ] Know your Swarm password (MD5 hash pre-computed)
[ ] Tested locally with input.txt (verification screenshot ready)
```

### At the Lab (order of operations)
```
[ ] Copy files to lab PC
[ ] Compile: gcc -o lab04_v2 lab04_v2.c -lpthread -lm
[ ] chmod +x table1.sh table2.sh table3.sh
[ ] Run verification test with input.txt (screenshot it)
[ ] Run ./table1.sh -> save lab04_table1.csv
[ ] Run ./table2.sh -> save lab04_table2.csv
[ ] Upload files to Swarm: scp lab04_v2.c table3.sh <user>@10.0.9.19:~/
[ ] SSH into a drone, compile, chmod +x
[ ] Edit table3.sh with real drone IPs
[ ] Set up SSH keys (optional but saves time)
[ ] Run ./table3.sh -> save lab04_table3.csv
[ ] Copy all CSV files back to USB
```

---

## Quick Reference: All Commands

```bash
# === compile ===
gcc -o lab04_v2 lab04_v2.c -lpthread -lm          # linux
gcc -o lab04_v2.exe lab04_v2.c -lws2_32 -lpthread  # windows

# === make scripts executable ===
chmod +x table1.sh table2.sh table3.sh

# === run tables ===
./table1.sh     # Table 1: single PC, no affinity  -> lab04_table1.csv
./table2.sh     # Table 2: single PC, with affinity -> lab04_table2.csv
./table3.sh     # Table 3: swarm, multi-PC          -> lab04_table3.csv

# === manual single run ===
./lab04_v2 <n> <port> 1 config.txt              # slave, no affinity
./lab04_v2 <n> <port> 1 config.txt <core_id>    # slave, with affinity
./lab04_v2 <n> <port> 0 config.txt              # master, random matrix
./lab04_v2 <n> <port> 0 config.txt input.txt    # master, from file

# === swarm upload ===
scp lab04_v2.c table3.sh <user>@10.0.9.19:~/

# === swarm ssh ===
ssh <user>@<drone_ip>

# === kill stuck processes ===
pkill -f lab04_v2
```
