# Lab 05 — Command Cheat Sheet

---

## 1. Compile

```bash
# Linux (lab PC / swarm drone)
gcc -O2 -o lab05 lab05.c -lpthread -lm

# Windows (local dev)
gcc -O2 -o lab05.exe lab05.c -lws2_32 -lpthread
```

If you hit `sched.h` errors on Linux:
```bash
gcc -O2 -o lab05 lab05.c -lpthread -lm -D_GNU_SOURCE
```

## 2. Check how many cores this PC has

```bash
nproc
```

---

## 3. Run the slave

Argument order: `<n> <port> <id> <config> [core_id]`

```bash
# No core affinity (table1.sh style)
./lab05 16000 5001 1 config.txt

# Pinned to a specific core (table2.sh / table3.sh style)
./lab05 16000 5001 1 config.txt 1
```

The slave prints its own `time elapsed: <seconds>` line, which is the
**MMT compute window only** (LRP05 Table 2 source).

## 4. Run the master

Argument order: `<n> <port> 0 <config> [input_file]`

```bash
# Random matrix (benchmark mode)
./lab05 16000 5000 0 config.txt

# Fixed matrix from file (demo — prints X, T, all per-hop output when n ≤ 32)
./lab05 8 5000 0 config.txt input8.txt
```

The master prints its own `time elapsed: <seconds>` line — that is the
distribute → compute → reduce wall-clock (LRP05 Table 1 source).

---

## 5. Startup order

1. Start **every slave first**, each in its own terminal.
2. Wait until each one prints `[Slave N] Listening on port ...`.
3. Then start the master.

The master retries `connect()` for ~15 s; if a slave is still booting,
that's fine.

---

## 6. Cleanup (kill leftover processes)

```bash
pkill lab05
```

Run this before re-running if you get "port already in use" errors.

---

## 7. Minimum config.txt for local 2-slave test

```
master 127.0.0.1 5000
slave  127.0.0.1 5001
slave  127.0.0.1 5002
```

Run order:

```bash
# Terminal A
./lab05 8 5001 1 config.txt

# Terminal B
./lab05 8 5002 2 config.txt

# Terminal C (master — last)
./lab05 8 5000 0 config.txt input8.txt
```

Watch for: Master prints X → each slave prints its received X + own MMT
strip → master prints rebuilt T.

---

## 8. 4-PC demo config (fill in real IPs at the lab)

`config_4pc.txt`:
```
master <PC1_IP> 5000
slave  <PC2_IP> 5001
slave  <PC3_IP> 5002
slave  <PC4_IP> 5003
```

Run order (one terminal per PC):

```bash
# PC2
./lab05 8 5001 1 config_4pc.txt 1

# PC3
./lab05 8 5002 2 config_4pc.txt 1

# PC4
./lab05 8 5003 3 config_4pc.txt 1

# PC1 (master — last)
./lab05 8 5000 0 config_4pc.txt input8.txt
```

The trailing `1` on each slave pins it to core 1 (LRP05 spec item 4 —
slave processes in core-affine manner).

---

## 9. Get this PC's IP address

```bash
hostname -I    # linux
ipconfig       # windows
```

---

## 10. What you read off each terminal

- **Master last line:** `time elapsed: 0.123456 seconds` ← LRP05 Table 1
- **Each slave last `time elapsed:` line:** ← collect all, take **max**
  per run for LRP05 Table 2

---

## 11. Sweep automations (collect Tables 1 + 2 in one shot)

```bash
chmod +x table1.sh table2.sh table3.sh
./table1.sh    # single PC, no affinity → lab05_table1.csv
./table2.sh    # single PC, with affinity → lab05_table2.csv
./table3.sh    # swarm + core affinity → lab05_table3.csv (canonical)
```

Each CSV has columns `n,t,run,master_time,slave_max`. The script also
prints two formatted tables at the end (master and slave-max).

---

## 12. Full 4-PC layout (t = 16 slaves, 17 terminals total)

**Physical setup:**
- **PC1** — 5 terminals (1 master + 4 slaves)
- **PC2 / PC3 / PC4** — 4 terminals each (4 slaves)
- Total: 1 master + 16 slaves
- Use `n = 16` and `input16.txt` for the demo so output stays readable

### Step 1 — Get each PC's IP

On every PC: `hostname -I`. Write down PC1_IP through PC4_IP.

### Step 2 — Create `config_17.txt` on PC1

```
master <PC1_IP> 5000
slave  <PC1_IP> 5001
slave  <PC1_IP> 5002
slave  <PC1_IP> 5003
slave  <PC1_IP> 5004
slave  <PC2_IP> 5005
slave  <PC2_IP> 5006
slave  <PC2_IP> 5007
slave  <PC2_IP> 5008
slave  <PC3_IP> 5009
slave  <PC3_IP> 5010
slave  <PC3_IP> 5011
slave  <PC3_IP> 5012
slave  <PC4_IP> 5013
slave  <PC4_IP> 5014
slave  <PC4_IP> 5015
slave  <PC4_IP> 5016
```

### Step 3 — Distribute files

From PC1:
```bash
scp lab05 config_17.txt input16.txt <user>@<PC2_IP>:~
scp lab05 config_17.txt input16.txt <user>@<PC3_IP>:~
scp lab05 config_17.txt input16.txt <user>@<PC4_IP>:~
```

### Step 4 — Start slaves (wait for `Listening...` after each; trailing 1 pins to core 1)

**PC1 (4 terminals):**
```bash
./lab05 16 5001 1 config_17.txt 1
./lab05 16 5002 2 config_17.txt 2
./lab05 16 5003 3 config_17.txt 3
./lab05 16 5004 4 config_17.txt 4
```

**PC2 / PC3 / PC4 (4 terminals each), e.g., PC2:**
```bash
./lab05 16 5005 5 config_17.txt 1
./lab05 16 5006 6 config_17.txt 2
./lab05 16 5007 7 config_17.txt 3
./lab05 16 5008 8 config_17.txt 4
```

(Adjust `core_id` per available cores on the PC.)

### Step 5 — Master on PC1

```bash
./lab05 16 5000 0 config_17.txt input16.txt
```

### Step 6 — What you'll see

- Master prints the 16×16 X, the 16 column assignments (each slave gets
  exactly 1 column), the tree (`Tree: 4 direct children (O(log 16))`),
  and per-hop SENT/RECEIVED blocks.
- Each slave prints its received X, its own MMT column, and (for
  forwarders) the assembled subtree T-strip.
- Master prints rebuilt T and the final `time elapsed`.

### Step 7 — Cleanup on every PC

```bash
pkill lab05
```
