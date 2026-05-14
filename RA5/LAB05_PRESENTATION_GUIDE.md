# Lab 05 RA5 Presentation Guide — Live Demo on 4 PCs

Charles Andrei P. De los Reyes | 2023-15797 | B-3L

---

## What the Professor Will Likely Ask For

Per the lab04 announcement pattern (which is the basis for the lab05 demo):

1. Set up RA5 across 4 lab computers (preferably the same row).
2. Print the matrix at every send and every receive (already implemented;
   guarded by `n ≤ 32`).
3. Demonstrate the **two new** lab05 behaviors on top of lab04:
   - Each slave prints its own MMT strip after the timed compute.
   - The master prints the **rebuilt T** at the end.

So the demo must:
- Run across **4 physical lab PCs**.
- Show distribute (X going down the tree) AND reduce (T-strips coming
  back up).
- Show one slave's compute timer fire (LRP05 Table 2 source) **and** the
  master's distribute → reduce timer fire (Table 1 source).

---

## Day-Before Checklist

```
[ ] lab05.c compiles cleanly on Linux:
    gcc -O2 -o lab05 lab05.c -lpthread -lm

[ ] input8.txt ready (8x8 matrix — first line is 8, then 8 rows
    of 8 comma-separated ints; same file as lab04 demo)

[ ] config_4pc.txt template ready (IPs filled in at the lab)

[ ] Bring on USB drive:
    - lab05.c
    - input8.txt
    - config_4pc.txt (template)
    - this guide

[ ] Backup: screenshot of a successful loopback demo
```

---

## Pre-Demo Setup at the Lab (10 minutes before class)

### Step 1 — Pick 4 adjacent PCs

Label them **PC1 (master), PC2, PC3, PC4**. Sticky notes help.

### Step 2 — Get each PC's IP

```bash
hostname -I
```

Verify ping. From PC1:
```bash
ping -c 2 <PC2_IP>
ping -c 2 <PC3_IP>
ping -c 2 <PC4_IP>
```

### Step 3 — Copy + compile

```bash
# on PC1
cd ~/Downloads
gcc -O2 -o lab05 lab05.c -lpthread -lm
chmod +x lab05

# scp to other PCs
scp lab05 input8.txt <user>@<PC2_IP>:~/
scp lab05 input8.txt <user>@<PC3_IP>:~/
scp lab05 input8.txt <user>@<PC4_IP>:~/
```

### Step 4 — Create `config_4pc.txt` on PC1

```bash
cat > config_4pc.txt <<EOF
master <PC1_IP> 5000
slave <PC2_IP> 5001
slave <PC3_IP> 5002
slave <PC4_IP> 5003
EOF
```

scp to other 3 PCs.

### Step 5 — Firewall check

```bash
sudo ufw status   # should be inactive
# if active: sudo ufw allow 5000:5003/tcp
```

### Step 6 — Dry run

On PC1 only, sanity-check:
```bash
./lab05 2>&1 | head -1
# expected: Usage: ./lab05 <n> <p> <s> <config_file> [input_file|core_id]
```

---

## The Demo Itself (during class)

**5 terminals total:** 1 slave on each of PC2/3/4, 1 master on PC1.
(For a 4-slave demo, also open a 4th slave terminal on PC1.)

### Step 1 — Start the 3 slaves (in this order)

**PC2 — Slave 1:**
```bash
cd ~
./lab05 8 5001 1 config_4pc.txt 1
```
Wait for `[Slave 1] Listening on port 5001...`

**PC3 — Slave 2:**
```bash
./lab05 8 5002 2 config_4pc.txt 1
```
Wait for `[Slave 2] Listening on port 5002...`

**PC4 — Slave 3:**
```bash
./lab05 8 5003 3 config_4pc.txt 1
```
Wait for `[Slave 3] Listening on port 5003...`

The trailing `1` pins each slave to core 1 (LRP05 spec item 4 — slave
processes core-affine).

### Step 2 — Run the master on PC1

```bash
cd ~
./lab05 8 5000 0 config_4pc.txt input8.txt
```

### Step 3 — What you'll see, and what to point at

#### On PC1 (master):

```
[Master] n=8, slaves=3, mode=file

--- Full Matrix X (8 x 8) ---
11 12 13 14 15 16 17 18
...
81 82 83 84 85 86 87 88

[Master] Slave 1 -> 3 cols (cols 0 to 2)
[Master] Slave 2 -> 3 cols (cols 3 to 5)
[Master] Slave 3 -> 2 cols (cols 6 to 7)

[Master] Tree: 2 direct children (O(log 3))

[-> Slave 2] SENT X + work [cols 3..7] (subtree of 2 slave(s))   ← POINT
[-> Slave 1] SENT X + work [cols 0..2] (subtree of 1 slave(s))

[Master] Received T-strip from Slave 2 (cols 3..7, subtree=2)    ← POINT
[Master] Received T-strip from Slave 1 (cols 0..2, subtree=1)
time elapsed: 0.00XXXX seconds                                    ← POINT

--- Full Matrix T (8 x 8) ---                                    ← POINT
0.0000 0.0000 0.0000 0.0000 0.0000 0.0000 0.0000 0.0000
0.1429 0.1429 0.1429 ...
...
1.0000 1.0000 1.0000 1.0000 1.0000 1.0000 1.0000 1.0000
```

#### On PC3 (Slave 2 — the forwarder):

```
[Slave 2] RECEIVED X (n=8), assignment cols [3..7] (subtree=2),
          my MMT cols [3..5]
[Slave 2] X received (8 x 8): ...

[Slave 2] Forwarding to Slave 3: cols [6..7], subtree=1          ← POINT
[-> Slave 3] SENT X + work [cols 6..7] (subtree of 1 slave(s))

time elapsed: 0.00XXXX seconds                                    ← POINT (compute timer)

[Slave 2] My MMT (cols [3..5]):                                  ← POINT
...

[Slave 2] Received T-strip from Slave 3 (cols [6..7])
[Slave 2] Subtree T-strip ready (cols [3..7]):                   ← POINT
...

[Slave 2] Sent T-strip to parent (cols [3..7], 2 slave(s) in subtree)
```

#### On PC4 (Slave 3 — the leaf):

```
[Slave 3] RECEIVED X (n=8), assignment cols [6..7] (subtree=1),
          my MMT cols [6..7]
[Slave 3] X received (8 x 8): ...

time elapsed: 0.00XXXX seconds

[Slave 3] My MMT (cols [6..7]):
...

[Slave 3] Sent T-strip to parent (cols [6..7], 1 slave(s) in subtree)
```

---

## What to Say While Pointing at the Screen

### Phrase 1 — Tree structure (point at master)
> "Here you see `Tree: 2 direct children (O(log 3))`. For t = 3 slaves,
> the master only initiates 2 parallel sends instead of 3 sequential
> ones. That's the same O(log t) tree from lab04, reused for lab05's
> distribute and reduce."

### Phrase 2 — Per-slave column work (point at master's "Slave X -> cols")
> "Notice each slave gets a *different* range of columns — Slave 1 gets
> 0–2, Slave 2 gets 3–5, Slave 3 gets 6–7. Lab 05 partitions the **work**
> by columns, but every slave receives the **full X** because the MMT of
> column k needs the full vector of column k's values."

### Phrase 3 — The forwarding hop (point at PC3's `Forwarding to Slave 3`)
> "This is the critical lab05 detail: Slave 2 spawns its forwarder
> thread for Slave 3 *before* starting its own compute timer. So while
> Slave 2 computes its MMT for columns 3–5, the forwarder is already
> streaming X to Slave 3. Communication overlaps with compute."

### Phrase 4 — Slave timer (point at PC3's `time elapsed`)
> "This is the slave's compute timer. It wraps **only** the call to
> `compute_mmt_strip`. Receiving X, forwarding, and sending the T-strip
> back are all excluded — that's exactly what LRP05 spec item 3 asks
> for. The maximum across all slaves per run is what feeds Table 2."

### Phrase 5 — M1PR (point at master's "Received T-strip from Slave 2 (cols 3..7, subtree=2)")
> "Slave 2 didn't just send back its own MMT — it sent the *full*
> subtree T-strip covering columns 3–7. Slave 2 collected Slave 3's
> strip via pthread_join, memcpy'd it into the right column slot of its
> subtree-wide buffer, and forwarded the assembled block. That's the
> M1PR — many-to-one personalized reduction up the same tree."

### Phrase 6 — Master timer + rebuilt T (point at master's `time elapsed` and `Full Matrix T`)
> "Master's timer wraps the full distribute → MMT → reduce → rebuild
> round trip. Once it finishes, the rebuilt T has every column
> normalized to [0, 1] — column min mapped to 0.0, column max mapped to
> 1.0. That's the LRP05 deliverable."

---

## Likely Questions + Canned Answers

**Q: Why did you choose to broadcast the full X instead of column-strips?**
A: "Three reasons. First, the LRP05 PDF says '1MPB distributed parts of
the matrix X' — broadcasting the full X via the same tree as lab04 maps
most naturally onto that wording. Second, sending the full X means each
slave has all the data needed to compute MMT for its assigned columns
independently, with no cross-slave reductions for min/max. Third, it
preserves lab04's tree topology byte-for-byte — same edges, same payload
shape, just a new compute step on each slave."

**Q: Where exactly does the slave timer start and stop?**
A: "Right before and right after the call to `compute_mmt_strip`. Lines
475 and 479 in lab05.c. Forwarder threads are spawned at line 464 —
*before* the timer — so their TCP setup and X transfer overlap with my
own compute and aren't billed to the slave timer."

**Q: Why report the max of slave times for Table 2 and not the average?**
A: "Because the master can't move on until **every** slave has
finished. The slowest slave bounds the parallel time. If 15 slaves take
1 second and 1 takes 5 seconds, averaging gives ~1.25 s, which is a
massive underestimate of the actual parallel runtime. The max captures
the critical path."

**Q: What does the master's timer include?**
A: "It includes everything from the first `pthread_create` for the
direct children, through all the slaves' compute (in parallel), through
the M1PR up the tree, until the last `memcpy` of a child's T-strip into
the master's full T. So it covers communication + compute + reduce."

**Q: How does the M1PR aggregate work?**
A: "Each forwarding slave allocates one subtree-wide buffer sized for
all the columns its subtree owns. Its own MMT writes into the leftmost
slot via the `strip_stride` parameter to `compute_mmt_strip`. Children's
T-strips get memcpy'd in at the right column offsets after
`pthread_join`. One contiguous block goes up to the parent — no extra
allocation, no extra copy."

**Q: What's the memory cost?**
A: "Every slave holds the full X (~n² ints) plus its subtree-wide T
buffer (~n × subtree_col_count floats). At n = 16000, that's about
1 GB X + up to 1 GB T-buffer per slave. Borderline for 2 GB swarm
drones, fine for the lab PCs."

**Q: Could you have used MPI?**
A: "Yes — MPI_Bcast for distribute, MPI_Gatherv for the M1PR. The lab
spec asked for raw sockets to teach the protocol layer, so I built the
tree explicitly. The shape would be the same with MPI."

---

## Emergency Backup Plan

### Plan B — Run on 1 PC with 4 terminals

Demonstrates everything except network distance:
```bash
./lab05 8 5001 1 config.txt 1     # config.txt = 1 master + 3 slaves on 127.0.0.1
./lab05 8 5002 2 config.txt 1
./lab05 8 5003 3 config.txt 1
./lab05 8 5000 0 config.txt input8.txt
```

Say: "My 4-PC setup hit a network issue; here it is on loopback — same
tree, same per-hop prints, same compute timer, same rebuilt T, just
faster because no Ethernet."

### Plan C — Pre-recorded screenshot

Have a screenshot of a successful run on your phone or USB.

---

## Post-Demo Cleanup

On every PC:
```bash
pkill -f lab05
```

So the next person can use the PCs without port conflicts.

---

## One-Page Cheat Sheet (print separately)

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
3. scp config_4pc.txt + lab05 + input8.txt to PC2, PC3, PC4
4. Start slaves (wait for "Listening..." each time):
     PC2:  ./lab05 8 5001 1 config_4pc.txt 1
     PC3:  ./lab05 8 5002 2 config_4pc.txt 1
     PC4:  ./lab05 8 5003 3 config_4pc.txt 1
5. Run master on PC1:
     ./lab05 8 5000 0 config_4pc.txt input8.txt
6. Point at:
     - "Tree: 2 direct children (O(log 3))"
     - "[-> Slave N] SENT X + work [cols A..B]"
     - "[Slave N] Forwarding to Slave M: cols [A..B]" ← BEFORE its timer
     - Slave's "time elapsed" (compute window only — Table 2 source)
     - "[Slave N] Received T-strip from Slave M" + Subtree T-strip print
     - Master's "Received T-strip from Slave N (cols A..B, subtree=k)"
     - Master's "time elapsed" (Table 1 source)
     - Rebuilt "Full Matrix T" with columns normalized to [0, 1]
==========================================
```
