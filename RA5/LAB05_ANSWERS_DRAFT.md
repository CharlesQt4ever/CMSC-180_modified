# CMSC 180 — Laboratory Research Problem 05
## Distributed Computation of the Min-Max Transformation of Elements in a Column of a Matrix

**Name:** Charles Andrei P. De los Reyes
**Student Number:** 2023-15797
**Section:** B-3L

> **Note:** Tables 1, 2, and 3 below are TEMPLATES — the time values must
> be filled in from the actual swarm runs (`lab05_table3.csv`). The
> question discussions are pre-drafted from the lab04 baseline + LRP05
> design so they only need light editing once the numbers are in.

---

## Implementation Summary

`lab05.c` extends `lab04_v3.c` from a pure distribution program to a
distribute-compute-reduce program. The master tree-broadcasts the **full
matrix X** to every slave via the same O(log t) recursive-halving tree
used in lab04 (1MPB), each slave computes the **column-wise Min-Max
Transformation** for its assigned column range, and the slaves return
their T-strips up the tree to the master in a many-to-one personalized
reduction (M1PR). The master rebuilds full T from the returned strips.

The master timer wraps the full distribute → reduce round-trip; each
slave timer wraps **only** the MMT compute window. Forwarder threads on
intermediate slaves are spawned before the slave timer starts, so
downstream communication overlaps with compute and is correctly excluded
from the slave timing per LRP05 spec item 3.

---

## Table 1 — Master `time_elapsed` (LRP05 Research Activity 1, item 5)

Run on the ICS Swarm with **1 master drone + 16 slave drones**, each
slave pinned to a non-OS core via `sched_setaffinity` (LRP05 spec item 4).

| n      | t  | Run 1 | Run 2 | Run 3 | Average |
|--------|----|-------|-------|-------|---------|
| 4,000  | 2  |       |       |       |         |
| 4,000  | 4  |       |       |       |         |
| 4,000  | 8  |       |       |       |         |
| 4,000  | 16 |       |       |       |         |
| 8,000  | 2  |       |       |       |         |
| 8,000  | 4  |       |       |       |         |
| 8,000  | 8  |       |       |       |         |
| 8,000  | 16 |       |       |       |         |
| 16,000 | 2  |       |       |       |         |
| 16,000 | 4  |       |       |       |         |
| 16,000 | 8  |       |       |       |         |
| 16,000 | 16 |       |       |       |         |

All times in seconds. Source: `lab05_table3.csv` column 4 (`master_time`).

---

## Table 2 — Slave `time_elapsed` (max across slaves per run)

| n      | t  | Max Run 1 | Max Run 2 | Max Run 3 | Average |
|--------|----|-----------|-----------|-----------|---------|
| 4,000  | 2  |           |           |           |         |
| 4,000  | 4  |           |           |           |         |
| 4,000  | 8  |           |           |           |         |
| 4,000  | 16 |           |           |           |         |
| 8,000  | 2  |           |           |           |         |
| 8,000  | 4  |           |           |           |         |
| 8,000  | 8  |           |           |           |         |
| 8,000  | 16 |           |           |           |         |
| 16,000 | 2  |           |           |           |         |
| 16,000 | 4  |           |           |           |         |
| 16,000 | 8  |           |           |           |         |
| 16,000 | 16 |           |           |           |         |

Source: `lab05_table3.csv` column 5 (`slave_max`). The maximum is taken
over all t slaves on every run because the master cannot finish until
**every** slave has finished — the slowest slave bounds the parallel
runtime, so the average across slaves would understate it.

---

## Research Activity 2 — Communication and Computation (Table 1 figure)

> Plot Figure 1: 3D plot of (n, t, average master time), or 2D with three
> lines (one per n) where x-axis is t and y-axis is average runtime.

**Pattern as n increases (fixed t):** Master time grows roughly with the
matrix size in bytes — distribute is bandwidth-bound, compute is
O(n²/p), and reduce is O(strip-bytes). At n = 16000 the full X is
1.024 GB per edge and dominates wall time on the swarm.

**Pattern as t increases (fixed n):**
- Compute work on each slave shrinks linearly (each slave covers n/t
  columns of MMT work).
- Communication cost grows: every additional slave is one more edge in
  the tree that must move ~n² ints. Tree depth grows as ⌈log₂ t⌉, but
  the total bytes moved grows linearly because every depth replicates X.
- Net effect at the master: typically *decreases* up to a sweet spot
  where compute savings dominate, then *increases* as bandwidth /
  per-edge overhead dominates.

[Insert observed direction once table is filled. Common shape: master
time decreases from t=2 to t=4 or t=8, then plateaus or grows back.]

---

## Research Activity 3 — Computation Only (Table 2 figure)

> Plot Figure 2: same shape as Figure 1 but using slave-max times.

**Why the maximum and not the average?**

The master's wall-clock cannot stop until every slave has finished its
compute and returned its T-strip. If 15 slaves take 1 s and one straggler
takes 5 s, the master is still waiting at second 4.999. The average
across the 16 slaves (1 s for 15, 5 s for one) would be 1.25 s — a
massive underestimate of the actual parallel compute cost. The maximum
captures the critical path correctly.

This is also why parallel speedup uses the maximum: the bound is set by
the slowest worker, not the average. Reporting the average would imply a
faster system than the master actually experiences.

**Pattern as n increases (fixed t):** Compute time grows roughly as
O(n²/t) — every column requires 2 passes over n elements, the slave
owns n/t columns, so per-slave work is 2 · n · (n/t) = O(n²/t). Doubling
n quadruples per-slave compute (for fixed t).

**Pattern as t increases (fixed n):** Compute time falls roughly as 1/t
because each slave's column count drops linearly. Above some t, the
constant-time overhead per slave (allocations, the inner loop's setup,
cache warm-up) starts to matter and the trend may flatten.

---

## Research Activity 4 — Performance Metrics (Table 3)

For each problem size n, T_S = serial runtime from lab01.c (constant per
n), T_P = average parallel runtime from Table 1, p = t.

| Field | Formula | Meaning |
|-------|---------|---------|
| T_O | p · T_P − T_S | Parallel overhead — total work done by p processors above the serial baseline |
| S | T_S / T_P | Speedup — how many times faster than serial |
| E | S / p = T_S / (p · T_P) | Efficiency — fraction of theoretical peak |
| pT_P | p · T_P | Parallel cost — total processor-seconds spent |

| n      | t  | T_S | T_P | T_O | S | E | pT_P |
|--------|----|-----|-----|-----|---|---|------|
| 4,000  | 2  |     |     |     |   |   |      |
| 4,000  | 4  |     |     |     |   |   |      |
| 4,000  | 8  |     |     |     |   |   |      |
| 4,000  | 16 |     |     |     |   |   |      |
| 8,000  | 2  |     |     |     |   |   |      |
| 8,000  | 4  |     |     |     |   |   |      |
| 8,000  | 8  |     |     |     |   |   |      |
| 8,000  | 16 |     |     |     |   |   |      |
| 16,000 | 2  |     |     |     |   |   |      |
| 16,000 | 4  |     |     |     |   |   |      |
| 16,000 | 8  |     |     |     |   |   |      |
| 16,000 | 16 |     |     |     |   |   |      |

### Discussion — comparing Figure 1 (comm + comp) vs Figure 2 (comp only)

The gap `T_P − T_S(slave-max)` per (n, t) measures **communication +
overhead**. For small n + small t, this gap should be small (mostly
loopback / Ethernet latency). For large n + large t, the gap dominates
because:

1. **Communication:** Approach C ships the full X on every tree edge.
   Total bytes sent across the tree at depth d is `2^d · n²` for the
   distribute direction. At n=16000, that's hundreds of MB to GB per
   level.
2. **Idling:** A forwarding slave finishes its own MMT (compute timer
   stops) but must still wait for `pthread_join` on its forwarders before
   it can send its subtree-wide strip up. That wait is *not* in the
   slave timer, but *is* in the master timer.
3. **Excess computation:** `compute_mmt_strip` makes 2 passes per column
   (one for min/max, one for normalization). Could be combined into one
   pass if pre-scanning for min/max, but the current 2-pass form is
   cleaner and matches lab01's reference. The "excess" is at most one
   extra column read per assigned column.

### Did you observe superlinearity?

Superlinearity (S > p) typically arises from cache effects — the per-slave
column slice fits in cache when t is large but the full matrix didn't.
For Approach C, every slave still holds the full X in memory, so cache
benefit is limited to the working set during the 2-pass MMT (`my_col_count`
columns being normalized). Expected: S grows but stays sub-linear.

[Fill in observed S values from Table 3. If S(t=16) > 16, that's
superlinear; explain via cache. Otherwise note it's sub-linear, which is
the expected outcome for an I/O-bound parallel workload.]

### Is the implementation cost-optimal?

A parallel algorithm is **cost-optimal** if pT_P = O(T_S). Concretely:
pT_P should grow at most as fast as the serial runtime grows. If pT_P
stays roughly constant (or grows much slower than t) as t increases for
a fixed n, the implementation is cost-optimal in the practical sense.

[Compare pT_P column across t for each n. If pT_P at t=16 is much larger
than pT_P at t=2, the parallelization is not cost-optimal — you're
paying for extra processors that don't help. The expected outcome on the
swarm is that pT_P grows with t at large n because the communication
overhead grows linearly with t while compute savings level off.]

---

## Verification

Correctness was verified by running with a fixed 8×8 input matrix
(`input8.txt`) and comparing the master's reconstructed T against the
serial lab01.c output. Each column normalizes to [0, 1] with the column
minimum mapping to 0.0 and the column maximum to 1.0:

```
--- Full Matrix X (8 x 8) ---       --- Full Matrix T (8 x 8) ---
11 12 13 14 15 16 17 18              0.0000 0.0000 0.0000 ...
21 22 23 24 25 26 27 28              0.1429 0.1429 0.1429 ...
...                                  ...
81 82 83 84 85 86 87 88              1.0000 1.0000 1.0000 ...
```

Per-hop matrix printing at every send and receive (when n ≤ 32 or file
mode) confirms that:
- Master sends X to its direct children with the correct work
  descriptors.
- Each slave receives the full X and prints its own MMT strip.
- Forwarder slaves print the assembled subtree T-strip before sending it
  up.
- Master prints the rebuilt T.

---

## Reaction to "Problem 1" (LRP05 page 3)

> *"I was not able to implement in lab04 and also here in lab05 the 1MPB
> and M1PR."*

This does not apply — both 1MPB (lab04 inherited) and M1PR (added in
lab05) are implemented. The 1MPB is realized as a binomial-style tree
broadcast with each slave receiving the full X plus a column-range work
descriptor. The M1PR is realized as a tree-up reduction where each
forwarding slave concatenates its own + children's T-strips into one
contiguous subtree-wide buffer before sending to its parent. Master and
slave timers are aligned with the LRP05 spec items 2 and 3 respectively.
