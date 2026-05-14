# Lab 04 — Command Cheat Sheet

---

## 1. Compile

```bash
gcc -O2 -o lab04_v3 lab04_v3.c -lpthread
```

## 2. Check how many cores this PC has

```bash
nproc
```

---

## 3. Run the slave

Argument order: `<n> <port> <id> <config> [core_id]`

```bash
# No core affinity (Table 1 style)
./lab04_v3 16000 5001 1 config.txt

# Pinned to a specific core (Table 2 style — replace 1 with the core number)
./lab04_v3 16000 5001 1 config.txt 1
```

## 4. Run the master

Argument order: `<n> <port> 0 <config> [input_file]`

```bash
# Random matrix (benchmark)
./lab04_v3 16000 5000 0 config.txt

# Fixed matrix from file (demo — prints per-hop output when n ≤ 32)
./lab04_v3 8 5000 0 config.txt input8.txt
```

---

## 5. Startup order

1. Start **every slave first**, each in its own terminal.
2. Wait until each one prints `[Slave N] Listening on port ...`.
3. Then start the master.

---

## 6. Cleanup (kill leftover processes)

```bash
pkill lab04_v3
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
./lab04_v3 8 5001 1 config.txt

# Terminal B
./lab04_v3 8 5002 2 config.txt

# Terminal C (master — last)
./lab04_v3 8 5000 0 config.txt input8.txt
```

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
./lab04_v3 8 5001 1 config_4pc.txt

# PC3
./lab04_v3 8 5002 2 config_4pc.txt

# PC4
./lab04_v3 8 5003 3 config_4pc.txt

# PC1 (master — last)
./lab04_v3 8 5000 0 config_4pc.txt input8.txt
```

---

## 9. Get this PC's IP address

```bash
hostname -I
```

---

## 10. What you read off the master's output

Last line of master output is the official time:

```
time elapsed: 0.123456 seconds
```

---

## 11. Full 4-PC layout (t = 16 slaves, 17 terminals total)

**Physical setup:**
- **PC1** — 5 terminals open (1 master + 4 slaves)
- **PC2** — 4 terminals open (4 slaves)
- **PC3** — 4 terminals open (4 slaves)
- **PC4** — 4 terminals open (4 slaves)
- Total: 1 master + 16 slaves
- Use `n = 16` (so every slave gets exactly 1 row) and `input16.txt`.

### Step 1 — Get each PC's IP

On every PC:
```bash
hostname -I
```
Write down the four IPs as PC1_IP, PC2_IP, PC3_IP, PC4_IP.

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

### Step 3 — Distribute files to every PC

From PC1, copy the binary + config + input to the other three PCs:
```bash
scp lab04_v3 config_17.txt input16.txt <user>@<PC2_IP>:~
scp lab04_v3 config_17.txt input16.txt <user>@<PC3_IP>:~
scp lab04_v3 config_17.txt input16.txt <user>@<PC4_IP>:~
```

### Step 4 — Start slaves (in this order, wait for "Listening..." after each)

**PC1 — slave terminals (4 of them):**
```bash
./lab04_v3 16 5001 1 config_17.txt
./lab04_v3 16 5002 2 config_17.txt
./lab04_v3 16 5003 3 config_17.txt
./lab04_v3 16 5004 4 config_17.txt
```

**PC2 — 4 terminals:**
```bash
./lab04_v3 16 5005 5 config_17.txt
./lab04_v3 16 5006 6 config_17.txt
./lab04_v3 16 5007 7 config_17.txt
./lab04_v3 16 5008 8 config_17.txt
```

**PC3 — 4 terminals:**
```bash
./lab04_v3 16 5009 9  config_17.txt
./lab04_v3 16 5010 10 config_17.txt
./lab04_v3 16 5011 11 config_17.txt
./lab04_v3 16 5012 12 config_17.txt
```

**PC4 — 4 terminals:**
```bash
./lab04_v3 16 5013 13 config_17.txt
./lab04_v3 16 5014 14 config_17.txt
./lab04_v3 16 5015 15 config_17.txt
./lab04_v3 16 5016 16 config_17.txt
```

### Step 5 — Start the master (last)

**PC1 — master terminal:**
```bash
./lab04_v3 16 5000 0 config_17.txt input16.txt
```

### Step 6 — What to expect

- Master prints the 16×16 matrix, the 16 slave assignments (each gets 1 row), the tree construction (`Tree: 4 direct children (O(log 16))`), and then the per-hop SENT blocks.
- Each slave prints its `RECEIVED 1 x 16 submatrix` line — row number is visible because row `r` has values starting with `(r+1)*100 + 1` (e.g., 501, 502, ..., 516 = row 5).
- Intermediate slaves also print `Forwarding N rows to Slave X`.
- Master prints `time elapsed: X.XXXXXX seconds` at the end.

### Step 7 — After the demo

On every PC:
```bash
pkill lab04_v3
```
