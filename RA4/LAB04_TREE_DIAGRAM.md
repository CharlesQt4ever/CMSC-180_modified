# Lab 04 — Binomial-Tree Personalized Broadcast (Visual Guide)

---

## 0. Quick Answer to Your Question

> *"How does the master pass the work to slaves, and does the master also do work?"*

**Short answer:** The master does **not** keep any rows for itself. All n
rows of the matrix are distributed to the t slaves. What the master
*does* do is:

1. **Build the matrix** (or read it from a file).
2. **Plan the tree** — decide who sends to whom.
3. **Launch parallel sends** to its direct children (a subset of the
   slaves, not all of them).
4. **Wait for acks** from each direct child. An ack from a child only
   arrives once that child's *entire subtree* has finished.
5. **Stop the timer.**

The real CPU/network "work" is the sending, and the master only does
⌈log₂ t⌉ of those; the rest is done by slaves forwarding to other
slaves. That is the entire point of the tree.

---

## 1. The Matrix Partitioning (t = 4 slaves, n = 8)

The master divides M row-wise among the slaves. Each slave owns an
equal share (plus one extra row each for the first `n mod t` slaves):

```
M (8 x 8)                      Owner after distribution
┌─────────────────────┐
│ row 0               │ ─┐
│ row 1               │  │── Slave 1's block  (2 rows)
├─────────────────────┤
│ row 2               │ ─┐
│ row 3               │  │── Slave 2's block  (2 rows)
├─────────────────────┤
│ row 4               │ ─┐
│ row 5               │  │── Slave 3's block  (2 rows)
├─────────────────────┤
│ row 6               │ ─┐
│ row 7               │  │── Slave 4's block  (2 rows)
└─────────────────────┘
```

**Master owns zero rows.** It starts with the whole matrix in memory
(for construction and for sending), but after distribution it free's M
and finishes.

---

## 2. How the Tree Is Built (t = 4 walkthrough)

The master runs this loop in `run_master` (lines 192–227 of
`lab04_v3.c`). Work through it once with t = 4:

### Iteration 1 — split {Slave 1, 2, 3, 4}

```
rem_count = 4 → left = 2, right = 2

      Slaves I still cover: [1, 2, 3, 4]
                 ▼
          split at middle
             /        \
       KEEP LEFT    DELEGATE RIGHT
       [1, 2]       [3, 4]        ← send to Slave 3 with num_slaves = 2
                                    (meaning: "Slave 3, you handle
                                    yourself AND Slave 4")
```

After iter 1: master still has to distribute to {1, 2}. Slave 3 will
handle {3, 4} on its own.

### Iteration 2 — split {Slave 1, 2}

```
rem_count = 2 → left = 1, right = 1

      Slaves I still cover: [1, 2]
                 ▼
          split at middle
             /        \
       KEEP LEFT    DELEGATE RIGHT
       [1]          [2]           ← send to Slave 2 with num_slaves = 1
                                    (leaf — Slave 2 just keeps it)
```

### Iteration 3 — rem_count == 1, terminal

```
Only Slave 1 left → send directly to Slave 1 with num_slaves = 1
                                    (leaf — Slave 1 just keeps it)
```

### Final tree

```
                    MASTER
                   ╱   │    ╲
          (2 rows)     │(2r)  \ (4 rows, covers Slaves 3+4)
          ╱            │        ╲
     SLAVE 1       SLAVE 2     SLAVE 3
     (leaf)        (leaf)     ╱       \
                              │         (2 rows, covers Slave 4)
                              │             ╲
                         keeps 2 rows      SLAVE 4
                                           (leaf)
```

**Reading the tree:** the master has **3 direct children**: Slaves 1, 2,
and 3. It sends to all three in parallel. Slaves 1 and 2 are leaves —
they do nothing more. Slave 3 is a subtree root — after receiving, it
forwards 2 rows to Slave 4.

---

## 3. What Happens on the Wire (timeline, t = 4)

Each column is one machine; time flows downward. `▼` means "sending",
`◆` means "ack". Indentation lines up with the thread responsible.

```
  Master               Slave 1          Slave 2          Slave 3           Slave 4
    │                    │                │                │                  │
    │─── connect ────────►│                │                │                  │
    │─── connect ──────────────────────────►│                │                  │
    │─── connect ────────────────────────────────────────────►│                  │
    │                    │                │                │                  │
    │(parallel sends — 3 threads)         │                │                  │
    │─── 2 rows ────────►│  recv          │                │                  │
    │                    │  (leaf)        │                │                  │
    │─── 2 rows ───────────────────────►│  recv           │                  │
    │                    │                │  (leaf)        │                  │
    │─── 4 rows ────────────────────────────────────────────►│  recv            │
    │                    │                │                │  (has to forward)│
    │                    │                │                │                  │
    │                    │                │                │── connect ──────►│
    │                    │                │                │── 2 rows ───────►│
    │                    │                │                │                  │ recv
    │                    │                │                │                  │ (leaf)
    │                    │                │                │◄──── ack ────────│
    │                    │                │                │                  │
    │◄──── ack ──────────│                │                │                  │
    │◄──── ack ─────────────────────────────│                │                  │
    │◄──── ack ────────────────────────────────────────────│                  │
    │                                                                         │
    ▼  STOP TIMER
```

**Key observations:**

- All three master sends happen in parallel (3 `pthread_create` calls).
- Slave 3's "recv + forward + recv_ack" happens on Slave 3's machine.
  The master is not involved in the Slave 3 ↔ Slave 4 hop.
- Slave 3 acks the master only **after** Slave 4 acks it back. So the
  master's "last ack" tells it the whole tree is done.
- Sequential network hops from the master's point of view: **2** (M→S3,
  S3→S4), not 4. This is the log-depth speedup.

---

## 4. Slave 3's Point of View (the Most Interesting Node)

Slave 3 is the only non-leaf slave in the t = 4 example. Its code path
goes through the `num_slaves > 1` branch in `run_slave` (lines 340–371).

```
┌──────────────────────────────────────────────────────────────┐
│ SLAVE 3 — code execution timeline                            │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  1. accept() returns  ← master has connected                 │
│  2. recv_all(metadata + 4 rows of data)                      │
│     ↳ metadata says num_slaves = 2, so I have to forward     │
│                                                              │
│  3. Enter forwarding loop (num_slaves > 1):                  │
│     ↳ split {3, 4} → keep {3}, delegate {4}                  │
│     ↳ pthread_create(worker_func, &arg_for_slave4)            │
│     ↳ worker_func opens TCP to Slave 4                       │
│     ↳ sends metadata + last 2 rows of my buffer              │
│                                                              │
│  4. Keep rows 4–5 for myself (first half of buffer)          │
│                                                              │
│  5. pthread_join(child thread)                               │
│     ↳ blocks until Slave 4 acks                              │
│                                                              │
│  6. send_all(csock, "ack", 3) to master                      │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

This is exactly the same `worker_func` the master uses — the code is
**one function**, but because Slave 3 also calls `pthread_create` on it,
Slave 3 effectively acts as a second master for its subtree.

---

## 5. Same Picture for t = 8 (just for scale)

```
                            MASTER
                          ╱  │  │   ╲
                        ╱    │  │     ╲
                      ╱      │  │       ╲
                    ╱        │  │         ╲
                S1          S2  S3           S5 (covers 5,6,7,8)
              (leaf)     (leaf) ╱  ╲         ╱     ╲
                               │    │      S6       S7 (covers 7,8)
                          (1 row?)  │     (leaf)    ╱   ╲
                         Actually   │              S7→?  S8
                         for t=8    │              Wait — let me redo
                         it's       │
                         cleaner    │
                         below      │
                                    │
```

Let me redo for t = 8 properly. The loop produces children by
repeatedly splitting the remaining range in half.

**Splits for t = 8:**

```
Step 1: {1..8}, split at 4 → keep {1..4}, delegate {5..8} to Slave 5
        num_slaves sent to Slave 5 = 4
Step 2: {1..4}, split at 2 → keep {1..2}, delegate {3..4} to Slave 3
        num_slaves sent to Slave 3 = 2
Step 3: {1..2}, split at 1 → keep {1},    delegate {2}    to Slave 2
        num_slaves sent to Slave 2 = 1   (leaf)
Step 4: {1}, terminal → send to Slave 1
        num_slaves = 1                    (leaf)
```

Master has 4 direct children for t = 8: Slave 1, Slave 2, Slave 3,
Slave 5. Resulting tree:

```
                       MASTER
                    ╱   │   │    ╲
                   ╱    │   │      ╲
                  S1    S2  S3      S5
                 (L)   (L)  ╱ ╲     ╱ │ ╲
                            │   S4  S6 S7  (no wait — let me trace S5)
                          (L) (L)
```

Tracing Slave 5 (which received covers_slaves = 4 for {5,6,7,8}):

```
Slave 5 runs the same split-and-forward logic on {5,6,7,8}:
  Step 1: {5..8} split 2+2 → keep {5..6}, delegate {7..8} to Slave 7
          Slave 7 receives num_slaves = 2
  Step 2: {5..6} split 1+1 → keep {5},    delegate {6}   to Slave 6
          Slave 6 receives num_slaves = 1 (leaf)
  Step 3: {5} terminal → keep for myself

Slave 7 runs the same split on {7,8}:
  Step 1: {7..8} split 1+1 → keep {7},    delegate {8}   to Slave 8
          Slave 8 receives num_slaves = 1 (leaf)
  Step 2: {7} terminal → keep for myself
```

Full tree for t = 8:

```
                              MASTER
                ┌──────────┬────┴────┬──────────────┐
                │          │         │              │
               S1         S2        S3             S5
              (leaf)   (leaf)      ╱  ╲          ╱  │  ╲
                                  S4   │        S6  S7  (and S7 forwards to S8)
                                (leaf) │      (leaf) ╱ ╲
                                     kept           │  S8
                                                    │ (leaf)
                                                  kept
```

**Levels of the tree for t = 8:**
- Level 0: Master only
- Level 1: Masters' 4 direct children (S1, S2, S3, S5) — 4 parallel
  sends from the master
- Level 2: S3 forwards to S4; S5 forwards to S6 and S7 in parallel
- Level 3: S7 forwards to S8

**Total tree depth = log₂(8) = 3.** The master's send time is dominated
by the deepest branch, which is 3 hops instead of 8.

---

## 6. "Does the Master Do Work?" — the precise answer

Here is exactly what the master does vs what the slaves do:

| Phase                      | Master | Each slave |
|----------------------------|--------|------------|
| Build/read matrix M        | ✅ yes | no        |
| Print M, slave assignments | ✅ yes | no        |
| Plan the tree              | ✅ yes | no — but each forwarding slave plans its own subtree |
| **Own any matrix rows**    | **❌ no** | **✅ yes — every slave ends up owning a unique block** |
| Open TCP to children       | ✅ (to ⌈log₂ t⌉ direct children) | forwarding slaves: yes, to their own children; leaf slaves: no |
| Send matrix bytes          | ✅ yes | forwarding slaves: yes; leaf slaves: no |
| Receive matrix bytes       | no | ✅ yes (every slave receives exactly once) |
| Forward a sub-block onward | no | forwarding slaves: yes; leaf slaves: no |
| Send "ack"                 | no | ✅ yes (after its subtree finishes) |
| Wait for "ack"             | ✅ yes (from every direct child) | forwarding slaves: yes (from each child); leaf slaves: no |
| Measure time elapsed       | ✅ yes | yes (diagnostic only; master's figure is the official one) |

**So the master's "work" is orchestration, not computation.** It never
holds a submatrix long-term (it free's M at the end), and it never
forwards data between slaves — it only fires the initial sends and
waits for the tree to complete.

---

## 7. Compare: Sequential vs Flat-Parallel vs Tree

For t = 8 slaves sending a 1 GB matrix over Gigabit Ethernet
(hypothetical, bandwidth-bound):

### Sequential ("send to slave 1, wait, send to slave 2, ..."): O(t) time

```
t0 ─── master → S1 (125 MB payload, 1 second) ──►
t1 ─── master → S2 (125 MB payload, 1 second) ──►
t2 ─── master → S3 (1 second)                  ──►
... (5 more)
t7 ─── master → S8                             ──►
     TOTAL: ~8 seconds
```

### Flat-parallel (master launches 8 threads): master NIC saturates

```
t0 ─── master's 1 Gbps NIC is the bottleneck. 1 GB total
       payload ÷ 125 MB/s = 8 seconds, just spread across
       all 8 slaves receiving slowly in parallel.
     TOTAL: ~8 seconds (same as sequential!)
```

### Tree (binomial): O(log t) time when the network has spare capacity

```
t0 ─── master sends to S1, S2, S3, S5 in parallel.
       Master's 1 Gbps NIC splits 1 GB across 4 sends.
       Effective per-receiver: 250 MB to each.
       Wait, that's worse per link — but S5 got 500 MB
       covering {5,6,7,8}, and the leaves got 125 MB each.
       Master's outgoing is still 1 Gbps total.
t1 ─── Now S5 has 500 MB and is free to forward.
       S5 sends 250 MB to S7 (covers {7,8}) using S5's
       NIC, and 125 MB to S6 using another thread on S5.
       S3 sends 125 MB to S4 using S3's NIC.
       ALL of these happen simultaneously — S5's NIC, S3's
       NIC, and whatever else is forwarding are independent.
t2 ─── S7 sends 125 MB to S8 using S7's NIC.
     TOTAL: ~3 time units (master send + S5 send + S7 send)
```

The key insight: at depth d there are 2^d independent NICs sending in
parallel, and they all finish at roughly the same wall-clock time
because each is carrying only its own leaf's share. The master's NIC
is only a bottleneck during depth 1.

---

## 8. Mapping Back to Your Code

When demoing, you can point at specific lines:

| Concept                                   | Line range in `lab04_v3.c` |
|-------------------------------------------|----------------------------|
| Master builds the tree                    | 189–227                    |
| Master launches parallel sends            | 235–237                    |
| "Start timer here"                        | 233                        |
| "Stop timer here"                         | 245                        |
| Slave receives metadata + data            | 309–318                    |
| Slave decides whether to forward          | 340 (`if (num_slaves > 1)`) |
| Slave forwards to its own children        | 344–368                    |
| Slave waits for grandchildren acks        | 383–387                    |
| Slave sends its own ack to parent         | 390                        |
| `worker_func` used by both master & slave | 70–119                     |

---

## 9. One-Line Takeaway

> **The master assigns every row to some slave, sends to only O(log t)
> direct children in parallel, and each recipient either keeps its
> block (leaf) or recursively forwards a tail portion to the next
> slave (subtree root) — just like the master does at the top.**
