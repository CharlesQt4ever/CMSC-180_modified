# Lab 04 RA4 Presentation Guide — Live Demo on 4 PCs

Charles Andrei P. De los Reyes | 2023-15797 | B-3L

---

## What the Professor Asked For

From the class announcement:

> 1. Please set up your RA4 in 4 computers (preferably use the computers in your computer row)
> 2. Please print the matrix after every send or receive (especially if you implemented the efficient 1 to Many Personalized Broadcast which runs at O(log n)).

So the demo must:
- Run across **4 physical lab PCs** (not loopback on one machine).
- **Print the matrix at every send and every receive** — already implemented (`worker_func` line ~100 and `run_slave` line ~320 in `lab04_v3.c`).

---

## Day-Before Checklist

```
[ ] lab04_v3.c compiles cleanly on Linux:
    gcc -O2 -o lab04_v3 lab04_v3.c -lpthread -lm

[ ] input8.txt ready (8x8 matrix — first line is 8, then 8 comma-separated rows)

[ ] config_4pc.txt template ready (IPs will be filled in at the lab)

[ ] Bring on USB drive:
    - lab04_v3.c
    - input8.txt
    - config_4pc.txt (template — edit at the lab)
    - this guide

[ ] Optional backup: screenshot of a successful demo on loopback
    (to show if the 4-PC network setup fails at the lab)
```

---

## Pre-Demo Setup at the Lab (10 minutes before class)

### Step 1 — Pick 4 adjacent PCs

Pick PCs in the same row. Label them **PC1 (master), PC2, PC3, PC4**. Write a sticky note on each so you don't mix them up mid-demo.

### Step 2 — Get each PC's IP

On each of the 4 PCs, open a terminal and run:
```bash
hostname -I
```
Write down the IP next to the PC label. You should get 4 IPs like `10.0.XX.YY`.

Verify they can ping each other. From PC1:
```bash
ping -c 2 <PC2_IP>
ping -c 2 <PC3_IP>
ping -c 2 <PC4_IP>
```
All should respond. If one doesn't, swap in a different PC.

### Step 3 — Copy files to each PC

Plug the USB into each PC in turn and copy `lab04_v3.c` and `input8.txt` to `~/Downloads/` (or any working directory). Then compile on each PC:
```bash
cd ~/Downloads
gcc -O2 -o lab04_v3 lab04_v3.c -lpthread -lm
chmod +x lab04_v3
```

Alternative (faster): compile once, then `scp` the binary:
```bash
# on PC1 only
gcc -O2 -o lab04_v3 lab04_v3.c -lpthread -lm
scp lab04_v3 input8.txt <user>@<PC2_IP>:~/
scp lab04_v3 input8.txt <user>@<PC3_IP>:~/
scp lab04_v3 input8.txt <user>@<PC4_IP>:~/
```

### Step 4 — Create `config_4pc.txt` on PC1

On PC1:
```bash
cat > config_4pc.txt <<EOF
master <PC1_IP> 5000
slave <PC2_IP> 5001
slave <PC3_IP> 5002
slave <PC4_IP> 5003
EOF
cat config_4pc.txt   # verify it looks right
```

Copy this config to the other 3 PCs:
```bash
scp config_4pc.txt <user>@<PC2_IP>:~/
scp config_4pc.txt <user>@<PC3_IP>:~/
scp config_4pc.txt <user>@<PC4_IP>:~/
```

### Step 5 — Check firewall isn't blocking

On each PC, make sure ports 5000–5003 aren't blocked:
```bash
sudo ufw status          # should be "inactive" on lab PCs; if active, allow ports
# if firewall is active:
# sudo ufw allow 5000:5003/tcp
```
Lab PCs usually have firewall off, but check anyway.

### Step 6 — Dry run

On PC1, do a quick loopback sanity check (still on PC1, just to confirm the binary works):
```bash
# no need for full 4-PC test — just confirm the binary runs
./lab04_v3 2>&1 | head -1
# expected: Usage: ./lab04_v3 <n> <p> <s> <config_file> [input_file|core_id]
```

---

## The Demo Itself (during class)

You have **5 terminals** total: 1 slave terminal on each of PC2/3/4, and 1 master terminal on PC1. Optionally a 2nd terminal on PC1 showing `config_4pc.txt`.

### Step 1 — Start the 3 slaves (in this exact order)

**PC2 terminal:**
```bash
cd ~
./lab04_v3 8 5001 1 config_4pc.txt
```
Wait until it prints:
```
[Slave 1] Listening on port 5001...
```

**PC3 terminal:**
```bash
cd ~
./lab04_v3 8 5002 2 config_4pc.txt
```
Wait for `[Slave 2] Listening on port 5002...`

**PC4 terminal:**
```bash
cd ~
./lab04_v3 8 5003 3 config_4pc.txt
```
Wait for `[Slave 3] Listening on port 5003...`

### Step 2 — Run the master on PC1

```bash
cd ~
./lab04_v3 8 5000 0 config_4pc.txt input8.txt
```

### Step 3 — What you'll see, and what to point at

#### On PC1 (master):

```
[Master] n=8, slaves=3, mode=file

--- Full Matrix M (8 x 8) ---
11 12 13 14 15 16 17 18
...
81 82 83 84 85 86 87 88

[Master] Slave 1 ... -> 3 rows
[Master] Slave 2 ... -> 3 rows
[Master] Slave 3 ... -> 2 rows

[Master] Tree: 2 direct children (O(log 3))

[-> Slave 2] SENT 5 rows (rows 3 to 7):    <-- POINT AT THIS
<rows 3-7 printed>

[-> Slave 1] SENT 3 rows (rows 0 to 2):    <-- POINT AT THIS
<rows 0-2 printed>

[Master] Received 'ack' from Slave 2 (subtree: 2)
[Master] Received 'ack' from Slave 1 (subtree: 1)
time elapsed: 0.00XXXX seconds
```

#### On PC3 (Slave 2 — the forwarder):

```
[Slave 2] Received 5 rows (rows 3 to 7), covers 2 slave(s)
[Slave 2] RECEIVED 5 x 8 submatrix (rows 3 to 7):    <-- POINT AT THIS
<rows 3-7 printed>

[Slave 2] Forwarding 2 rows to Slave 3
[-> Slave 3] SENT 2 rows (rows 6 to 7):              <-- POINT AT THIS (KEY!)
<rows 6-7 printed>

[Slave 2] Received ack from Slave 3
[Slave 2] Sent 'ack' to parent.
```

#### On PC4 (Slave 3 — the leaf):

```
[Slave 3] RECEIVED 2 x 8 submatrix (rows 6 to 7):    <-- POINT AT THIS
71 72 73 74 75 76 77 78
81 82 83 84 85 86 87 88
```

---

## What to Say While Pointing at the Screen

Practice saying these phrases while pointing at the corresponding output:

### Phrase 1 — Tree structure (point at master)
> "Here you see `Tree: 2 direct children (O(log 3))`. For t = 3 slaves, the master only initiates 2 parallel sends instead of 3 sequential ones. That's the `log t` efficiency."

### Phrase 2 — Personalized data (point at SENT blocks on master)
> "Notice each slave receives a **different** submatrix — Slave 1 gets rows 0–2, Slave 2 gets rows 3–7. That's why this is 'personalized' broadcast, not regular broadcast."

### Phrase 3 — The forwarding hop (point at PC3's output)
> "This is the key part. Slave 2 received 5 rows from the master, then **forwarded** the last 2 rows to Slave 3. Slave 3 never talks to the master directly. That's how the tree makes distribution scale logarithmically."

### Phrase 4 — Ack propagation (point at master's ack lines)
> "The ack from Slave 2 only arrives **after** Slave 3 has acked back to Slave 2. So the tree acks travel up the same shape in O(log t) time, not O(t)."

### Phrase 5 — Timing (point at `time elapsed`)
> "The timer wraps only the distribution phase — from the first send to the last ack. It uses `CLOCK_MONOTONIC` for nanosecond precision."

---

## Likely Questions + Canned Answers

**Q: Why 4 PCs but only 3 slaves? Your config has 3, not 4.**
A: "My master is on PC1 and my 3 slaves are on PC2, PC3, PC4. That's 4 PCs total, one master + three workers. If you want t = 4 I can add a second slave on PC1 using a different port." (If asked, run with t = 4 quickly.)

**Q: What communication technique did you use?**
A: "One-to-many personalized broadcast, implemented as a binomial (binary) tree. Each slave receives a unique submatrix, and the tree runs at O(log t) depth."

**Q: Why is this faster than a flat parallel send?**
A: "In a flat parallel scheme, every byte still passes through the master's single outgoing link. My tree turns each receiving slave into a new source, so at depth d, 2^d slaves are transmitting simultaneously. The master never becomes the bandwidth bottleneck."

**Q: Does the timer include matrix generation?**
A: "No. `clock_gettime` starts right before `pthread_create` and stops right after the last `pthread_join`. Matrix generation, config parsing, and result printing are all outside the timer."

**Q: Why does your slave print "[Slave X] Forwarding N rows to Slave Y"?**
A: "That's my per-hop diagnostic. The professor asked us to print the matrix after every send or receive, so Slave 2 in this demo prints both what it received *and* what it forwards downstream."

**Q: Would threads help if the slaves were doing real computation (like MMT)?**
A: "Yes, but even more importantly, core affinity would matter. My Table 2 shows affinity didn't help here because the workload is I/O-bound; slaves spend most time inside `recv()`. For compute-heavy slaves, pinning to cores would preserve cache locality."

---

## Emergency Backup Plan

If something fails live (PC won't ping, ssh breaks, firewall issue):

### Plan B — Run on 1 PC with 4 terminals

Still demonstrates the tree and per-hop prints, just on loopback:
```bash
# 3 slave terminals
./lab04_v3 8 5001 1 config.txt   # config.txt = 1 master + 3 slaves on 127.0.0.1
./lab04_v3 8 5002 2 config.txt
./lab04_v3 8 5003 3 config.txt

# master
./lab04_v3 8 5000 0 config.txt input8.txt
```

Say: "My 4-PC setup hit a network issue; here it is on loopback — same tree behavior, same per-hop prints, just faster because no Ethernet."

### Plan C — Show a pre-recorded screenshot

Have a screenshot of a successful loopback run on your phone or USB. Worst case.

---

## Post-Demo Cleanup

After you finish, on each PC:
```bash
pkill -f lab04_v3          # kill any leftover processes
```
So the next person can use the PCs without port conflicts.

---

## One-Page Cheat Sheet (print this separately if you want)

```
==========================================
DEMO COMMANDS (memorize the order)
==========================================

1. Get IPs of 4 PCs:       hostname -I  (on each)
2. Write config_4pc.txt on PC1:
     master  <PC1_IP> 5000
     slave   <PC2_IP> 5001
     slave   <PC3_IP> 5002
     slave   <PC4_IP> 5003
3. scp config_4pc.txt + lab04_v3 + input8.txt to PC2, PC3, PC4
4. Start slaves (wait for "Listening..." each time):
     PC2:  ./lab04_v3 8 5001 1 config_4pc.txt
     PC3:  ./lab04_v3 8 5002 2 config_4pc.txt
     PC4:  ./lab04_v3 8 5003 3 config_4pc.txt
5. Run master on PC1:
     ./lab04_v3 8 5000 0 config_4pc.txt input8.txt
6. Point at:
     - "Tree: 2 direct children (O(log 3))"
     - "[-> Slave N] SENT ..."  on master
     - "[Slave N] RECEIVED ..."  on each slave
     - "[Slave 2] Forwarding ..."  (THE forwarding hop)
     - Ack propagation + "time elapsed"
==========================================
```
