# Lab 05 — 1MPB Distribute + M1PR Reduce (Visual Guide)

A visual companion to `lab05.c`. Shows how the same tree topology from
lab04 is reused twice: once for the **distribute** of X (downward) and
once for the **reduce** of T-strips (upward).

---

## 0. The Two Trees Are the Same Tree

```
        DISTRIBUTE (1MPB) — X flows DOWN              REDUCE (M1PR) — T-strip flows UP
                MASTER                                        MASTER
                  │                                             ▲
       ┌──────────┼──────────┐                       ┌──────────┼──────────┐
       ▼          ▼          ▼                       │          │          │
      S1         S2         S3                      S1         S2         S3
                              │                                            ▲
                              ▼                                            │
                             S4                                           S4
```

The 1MPB and M1PR happen on the **same TCP connections**. Each
`worker_func` thread does both halves: `send X` then `recv T-strip` on a
single socket. Same edges, two directions.

---

## 1. The Matrix Partition (t = 4 slaves, n = 8)

In lab04, the matrix was split by **rows**. In lab05, the work is split
by **columns** — every slave gets the *whole* X but only computes the
MMT for its assigned column range.

```
X (8 x 8)                          MMT work assignment
┌─────────────────────┐
│ col 0 │ col 1 ║ col 2 │ col 3 ║ col 4 │ col 5 ║ col 6 │ col 7 │
└───┬───┴───┬───╨───┬───┴───┬───╨───┬───┴───┬───╨───┬───┴───┬───┘
    │       │       │       │       │       │       │       │
    └───────┘       └───────┘       └───────┘       └───────┘
    Slave 1         Slave 2         Slave 3         Slave 4
    (cols 0–1)      (cols 2–3)      (cols 4–5)      (cols 6–7)
```

**Master owns zero compute.** It builds X, broadcasts X to all slaves
(via the tree), waits for the T-strips, and rebuilds T from them.

**Every slave owns the full X in memory** (Approach C). The slave only
*computes* on its 2-column range, but it has the full 8-column data —
because MMT for column k needs the full vector of column k's values, and
that's already on the slave.

---

## 2. The Tree Built at the Master (t = 4)

Same recursive-halving as lab04, but partitioning **columns**.

### Iteration 1 — split {1, 2, 3, 4}

```
rem_count = 4 → left = 2, right = 2
left covers cols 0–3, right covers cols 4–7

      Master's slave range: [1, 2, 3, 4]
                ▼
         split at middle
            /        \
      KEEP LEFT     DELEGATE RIGHT
      [1, 2]        [3, 4]
                    ↓
                    push TArgs for Slave 3 with:
                      col_first=4, col_count=4
                      first_slave=2 (0-indexed), num_slaves=2
                      "Slave 3, you handle yourself + Slave 4"
```

### Iteration 2 — split {1, 2}

```
rem_count = 2 → left = 1, right = 1
left covers cols 0–1, right covers cols 2–3

      Master's slave range: [1, 2]
                ▼
         split at middle
            /        \
      KEEP LEFT     DELEGATE RIGHT
      [1]           [2]
                    ↓
                    push TArgs for Slave 2 with:
                      col_first=2, col_count=2
                      first_slave=1, num_slaves=1   (leaf)
```

### Iteration 3 — terminal {1}

```
rem_count = 1 → leaf
push TArgs for Slave 1 with:
  col_first=0, col_count=2
  first_slave=0, num_slaves=1   (leaf)
```

### Resulting tree

```
                       MASTER
                    ╱     │     ╲
   sends full X +     full X +    full X +
   work[0..1, leaf]   [2..3, leaf] [4..7, subtree=2]
                    ╱     │       ╲
                  S1     S2        S3
                  (L)    (L)         ╲
                                      ╲ sends full X + work[6..7, leaf]
                                       ╲
                                        S4 (L)
```

Master has **3 direct children**: S1, S2, S3. It opens 3 parallel TCP
connections and sends in parallel. S1 and S2 are leaves — they compute
and send their strip back. S3 is a subtree root — after receiving X, it
forwards X + work to S4 *and* computes its own MMT (cols 4–5)
concurrently.

---

## 3. Wire Timeline (t = 4) — both directions

```
  Master            Slave 1         Slave 2         Slave 3              Slave 4
    │                  │               │                │                   │
    │── connect ──────►│               │                │                   │
    │── connect ──────────────────────►│                │                   │
    │── connect ────────────────────────────────────────►│                   │
    │                                                                        │
    ╔══ MASTER TIMER START ══╗                                                │
    │                                                                        │
    │── meta+X[full] ──►│                │                │                  │
    │── meta+X[full] ───────────────────►│                │                  │
    │── meta+X[full] ────────────────────────────────────►│                  │
    │                  │                 │                │                  │
    │                  │ recv X done     │ recv X done    │ recv X done      │
    │                  │ ╔compute timer╗ │ ╔compute timer╗│ (spawn forwarder)│
    │                  │ │ MMT cols 0-1│ │ │ MMT cols 2-3││── connect ──────►│
    │                  │ ╚timer end    ╝ │ ╚timer end    ╝│── meta+X[full] ─►│
    │                  │                 │                │ ╔compute timer╗  │
    │                  │                 │                │ │ MMT cols 4-5│  │ recv X done
    │                  │                 │                │ ╚timer end    ╝  │ ╔compute timer╗
    │                  │                 │                │                  │ │ MMT cols 6-7│
    │                  │                 │                │                  │ ╚timer end    ╝
    │                  │                 │                │                  │
    │                  │                 │                │                  │ T-strip[6-7] ─►
    │                  │                 │                │                  │
    │                  │                 │                │ join forwarder   │
    │                  │                 │                │ memcpy strips    │
    │                  │                 │                │  into subtree    │
    │                  │                 │                │  buffer          │
    │                  │                 │                │                  │
    │                  │ T-strip[0-1] ──►│                │                  │
    │                  │                 │ T-strip[2-3] ─►│                  │
    │                  │                 │                │ T-strip[4-7] ──►│
    │                                                                        │
    │ memcpy each strip into T at right column offset                        │
    │                                                                        │
    ╚══ MASTER TIMER END ══╝
```

**Reading the timeline:**

- All 3 master sends happen in parallel (3 `pthread_create` calls).
- Slave 3 spawns its forwarder for Slave 4 *before* starting its own
  compute timer. Communication overlaps with compute.
- Each slave's compute timer is the **only** thing measured by the
  slave — receiving X is excluded, sending the strip back is excluded.
- The master's timer wraps everything: distribute + slave compute (in
  parallel on the slaves) + reduce.

**Sequential bottleneck depth:** for t = 4, the deepest path is
master → Slave 3 → Slave 4 = 2 hops, not 4. For t = 16, the deepest
path is 4 hops. That's the O(log t) tree behavior preserved from lab04.

---

## 4. Slave 3's Point of View (the most interesting node)

```
┌──────────────────────────────────────────────────────────────────┐
│ SLAVE 3 — code execution timeline                                │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  1. accept() returns  ← master has connected                     │
│  2. recv_all(metadata + full X)                                  │
│     ↳ metadata says num_slaves = 2 → I have to forward           │
│     ↳ my own col range = [4..5] (cols 4 + 5)                     │
│                                                                  │
│  3. Spawn forwarder for Slave 4 (BEFORE compute timer):          │
│     ↳ pthread_create(worker_func, &arg_for_slave4)               │
│     ↳ worker_func opens TCP to Slave 4                           │
│     ↳ sends metadata + full X (col_first=6, col_count=2)         │
│                                                                  │
│  4. Allocate subtree_strip (8 × 4 floats, for cols 4..7)         │
│                                                                  │
│  ╔══ COMPUTE TIMER START ════════════════════════════════════╗   │
│  ║                                                            ║  │
│  ║  5. compute_mmt_strip(X, n=8, col_first=4, col_count=2,    ║  │
│  ║                       T_strip=subtree_strip,               ║  │
│  ║                       strip_stride=4)                      ║  │
│  ║     ↳ writes MMT for cols 4-5 into                         ║  │
│  ║       subtree_strip[r*4 + 0..1] for each row r             ║  │
│  ║                                                            ║  │
│  ╚══ COMPUTE TIMER END ══════════════════════════════════════╝   │
│                                                                  │
│  6. printf("time elapsed: 0.XXX seconds")  ← LRP05 Table 2 source│
│                                                                  │
│  7. pthread_join(forwarder thread for Slave 4)                   │
│     ↳ blocks until Slave 4 sends back its T-strip                │
│     ↳ child_args[0].returned_strip is now an 8 × 2 buffer        │
│                                                                  │
│  8. memcpy Slave 4's strip into subtree_strip[r*4 + 2..3]        │
│     ↳ offset = child.col_first - my col_first = 6 - 4 = 2        │
│                                                                  │
│  9. send_all(csock, subtree_strip, 8 × 4 × sizeof(float))        │
│     ↳ sends my full subtree (cols 4-7) up to master              │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

The same `worker_func` that the master uses to talk to its 3 children is
called by Slave 3 to talk to Slave 4. Slave 3 acts as a sub-master for
its subtree.

---

## 5. The `subtree_strip` Buffer Layout (Slave 3)

`subtree_strip` is sized for the **whole subtree** (8 rows × 4 cols of
float, 128 bytes), not just my own work.

```
              col_first - col_first = 0     +2 (Slave 4's offset)
              ▼                              ▼
              ┌────────────────────────────┬─────────────────────────────┐
              │    My MMT (cols 4–5)       │    Slave 4's MMT (cols 6–7) │
              │  written by                │  memcpy'd in after          │
              │  compute_mmt_strip          │  pthread_join(forwarder)   │
              │  with strip_stride = 4      │                            │
   row 0  ────│ T[0][4]  T[0][5]           │  T[0][6]  T[0][7]           │
   row 1  ────│ T[1][4]  T[1][5]           │  T[1][6]  T[1][7]           │
              │  ⋮                          │  ⋮                         │
   row 7  ────│ T[7][4]  T[7][5]           │  T[7][6]  T[7][7]           │
              └────────────────────────────┴─────────────────────────────┘
              ◄────── my_col_count (=2) ──►◄─── child.col_count (=2) ──►
              ◄──────────────── col_count (=4) ──────────────────────────►
```

**Why one buffer instead of two?**

If I had separate buffers for "mine" and "child's," I'd need to allocate
both, then concat them into a third buffer before sending up. That's two
extra allocations and one extra copy.

By writing my own MMT directly into the leftmost slot of the
subtree-wide buffer (via `strip_stride = col_count`), and `memcpy`ing
the child's strip into the rightmost slot, the buffer is already
contiguous and ready to send.

---

## 6. Same Picture for t = 8

The tree builds 4 direct children: S1, S2, S3, S5.

```
                         MASTER
                    ╱     │      │      ╲
                   ╱      │      │        ╲
                  S1      S2     S3        S5
                  (L)     (L)    │          ╱  ╲
                                 │         ╱    ╲
                                S4        S6    S7
                                 (L)      (L)    │
                                                 │
                                                S8
                                                (L)
```

Wait — re-trace: with rem_count = 8 → left = 4, right = 4, master delegates
{5..8} to Slave 5. Recurse on {1..4} → delegate {3..4} to Slave 3.
Recurse on {1..2} → delegate {2} to Slave 2. Recurse on {1} → leaf.
So master direct children are: S1, S2, S3, S5.

For Slave 5's subtree (covers slaves 5,6,7,8 with cols 8..15 of a 16-col
example, or cols 4..7 of an 8-col example): same recursive split inside
the subtree. Slave 5 ends up with 1 forwarder (to Slave 7), and Slave 7
has 1 forwarder (to Slave 8).

**Tree depth = log₂(8) = 3.** Master's deepest path is master → Slave 5
→ Slave 7 → Slave 8 = 3 hops.

---

## 7. Master's Side at the Reduce

When all `pthread_join` calls return, the master has 3 returned strips
(t=4) or 4 returned strips (t=8). It memcpys each strip into T at the
correct column slice:

```c
for (int i = 0; i < num_children; i++) {
    pthread_join(threads[i], NULL);
    int cf = t_args[i].col_first;
    int cc = t_args[i].col_count;
    for (int r = 0; r < n; r++)
        memcpy(&T[r * n + cf],
               &t_args[i].returned_strip[r * cc],
               (size_t)cc * sizeof(float));
    free(t_args[i].returned_strip);
}
```

For t = 4:
- Slave 1's strip (cf=0, cc=2) → T[r][0..1]
- Slave 2's strip (cf=2, cc=2) → T[r][2..3]
- Slave 3's strip (cf=4, cc=4) → T[r][4..7]

After all 3 joins, T is fully assembled. Total: n × n floats, every
column is the correctly normalized MMT of the corresponding column of X.

---

## 8. Compare: lab04 vs lab05

| | lab04 | lab05 |
|--|--|--|
| What master sends | row strip (`n_rows × n` ints) | full X (`n × n` ints) |
| What slave does | recv + forward + ack | recv + forward + **MMT compute** + reduce |
| Direction | one-way (master → slave) | two-way (X down, T-strip up) |
| Slave timer | none (or diagnostic) | wraps **only** MMT compute |
| Master timer | distribute + ack | distribute + compute + reduce + rebuild |
| Memory per slave | 1 row strip | full X + own + (subtree) T-strip |
| Tree topology | identical | identical |
| Number of edges | t (one per slave) | t (same edges, two directions each) |

**Algorithmic core is reused.** What changed: the payload, the slave's
work, and the addition of the M1PR direction.

---

## 9. "Does the Master Do Compute?" — precise answer

| Phase | Master | Forwarding slave | Leaf slave |
|-------|--------|------------------|------------|
| Build / read X | ✅ | — | — |
| Plan tree | ✅ | own subtree only | — |
| Send X downstream | ✅ (to direct children) | ✅ (to sub-children) | ❌ |
| Recv X | — | ✅ (from parent) | ✅ (from parent) |
| **MMT compute** | **❌** | ✅ (own col range) | ✅ (own col range) |
| Recv T-strip from children | — | ✅ | ❌ |
| Assemble subtree strip | — | ✅ (own + children) | — |
| Send T-strip upstream | — | ✅ (subtree-wide) | ✅ (own only) |
| **Rebuild full T** | **✅** | — | — |
| Measure time | ✅ (full window) | ✅ (compute only) | ✅ (compute only) |

The master is **orchestration**: it generates X, distributes it,
collects the strips, and reassembles T. The actual MMT compute is done
by every slave in parallel.

---

## 10. Mapping Back to Code

When demoing, point at:

| Concept | `lab05.c` line |
|---------|----------------|
| `compute_mmt_strip` (the kernel) | 75–96 |
| Master tree construction | 221–260 |
| Master timer start | 275 |
| Master parallel send + join + memcpy | 277–297 |
| Master timer end | 299 |
| Slave forwarder spawn (BEFORE timer) | 456–465 |
| Slave compute timer start | 475 |
| `compute_mmt_strip` call | 477 |
| Slave compute timer end | 479 |
| Slave reduce: memcpy children's strips | 498–519 |
| Slave send T-strip to parent | 535 |

---

## 11. One-Line Takeaway

> **The master broadcasts the full X to every slave through an O(log t)
> tree, every slave computes the MMT of its own assigned column range
> (no cross-slave coordination), and the slaves return their T-strips up
> the same tree so the master can rebuild full T — distribute and reduce
> are two halves of one round-trip on the same edges.**
