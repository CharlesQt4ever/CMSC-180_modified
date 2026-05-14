# Lab 05 — Paper Content (Tables + Discussions Only)

> **Note:** The numeric cells below are TEMPLATES. Once `lab05_table3.csv`
> is collected from the swarm, fill in the time values and (for Table 3)
> compute the derived metrics in a spreadsheet using the formulas in
> Research Activity 4.

---

## Table 1. Time elapsed as reported by the master process.

Run on the ICS Swarm with **1 master drone + 16 slave drones** (LRP05
spec item 4: "different machines, slave processes core-affine"). Each
slave is pinned to a non-OS core via `sched_setaffinity`. Master's timer
wraps the full distribute → MMT (parallel on slaves) → reduce → rebuild
round trip.

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

All times in seconds. Source: `lab05_table3.csv` column 4.

---

## Table 2. Time elapsed as reported by the slave processes (max of run).

Slave timer wraps **only** the call to `compute_mmt_strip`. Receiving X,
spawning forwarder threads, joining children's strips, and sending the
subtree T-strip back are all excluded. Per LRP05 Research Activity 3,
the value reported per (n, t, run) is the **maximum** across the t
slaves: the master cannot finish until every slave has finished, so the
slowest slave bounds the parallel runtime.

| n      | t  | Max of Run 1 | Max of Run 2 | Max of Run 3 | Average |
|--------|----|--------------|--------------|--------------|---------|
| 4,000  | 2  |              |              |              |         |
| 4,000  | 4  |              |              |              |         |
| 4,000  | 8  |              |              |              |         |
| 4,000  | 16 |              |              |              |         |
| 8,000  | 2  |              |              |              |         |
| 8,000  | 4  |              |              |              |         |
| 8,000  | 8  |              |              |              |         |
| 8,000  | 16 |              |              |              |         |
| 16,000 | 2  |              |              |              |         |
| 16,000 | 4  |              |              |              |         |
| 16,000 | 8  |              |              |              |         |
| 16,000 | 16 |              |              |              |         |

Source: `lab05_table3.csv` column 5 (`slave_max`, computed by the sweep
script as the max across per-slave log files).

---

## Table 3. Performance metrics of the parallel, distributed MMT.

For each n, T_S is the lab01 serial runtime (constant per n), T_P is the
average master time from Table 1, and p = t.

- **T_O = p · T_P − T_S** (parallel overhead)
- **S = T_S / T_P** (parallel speedup)
- **E = S / p = T_S / (p · T_P)** (parallel efficiency)
- **pT_P = p · T_P** (parallel cost)

| n      | t  | Serial T_S | Parallel T_O | S | E | pT_P |
|--------|----|------------|--------------|---|---|------|
| 4,000  | 2  |            |              |   |   |      |
| 4,000  | 4  |            |              |   |   |      |
| 4,000  | 8  |            |              |   |   |      |
| 4,000  | 16 |            |              |   |   |      |
| 8,000  | 2  |            |              |   |   |      |
| 8,000  | 4  |            |              |   |   |      |
| 8,000  | 8  |            |              |   |   |      |
| 8,000  | 16 |            |              |   |   |      |
| 16,000 | 2  |            |              |   |   |      |
| 16,000 | 4  |            |              |   |   |      |
| 16,000 | 8  |            |              |   |   |      |
| 16,000 | 16 |            |              |   |   |      |

T_S is constant per n (not per (n, t)) because the serial run does not
depend on t.

---

## Discussion

### Research Activity 2 — Communication and Computation (Figure 1: n × t × master time)

**Pattern as n increases (fixed t):** The master's wall time grows
roughly with the size of X in bytes — distribute and reduce are
bandwidth-bound, and the per-slave compute is O(n²/t). At n = 16,000 the
matrix X is 1.024 GB, and Approach C ships the full X across every tree
edge, so cumulative bytes-on-wire across the tree dominates wall time on
the swarm.

**Pattern as t increases (fixed n):** Two competing effects.
Per-slave compute work shrinks linearly with t (each slave handles
n/t columns). Communication grows: more tree edges, more bytes total,
even though tree depth grows only as ⌈log₂ t⌉. The master time typically
**decreases** from t = 2 to a sweet spot (often t = 4 or 8) where the
compute savings dominate, then **plateaus or increases** as
communication and per-edge overhead take over.

[Insert observed shape once Table 1 is filled. Sample wording: "On the
swarm, the minimum master time at n = 16,000 occurs at t = X, after
which adding slaves stops helping because each new edge contributes more
communication time than it saves in compute."]

---

### Research Activity 3 — Computation Only (Figure 2: n × t × slave-max time)

**Why the maximum and not the average?** The master cannot terminate
until **every** slave's T-strip has arrived. The slowest slave's compute
sets the lower bound on the parallel compute window. Reporting the
average across t slaves would understate this: 15 fast slaves and 1 slow
straggler would average to a number smaller than the actual time the
master had to wait. The maximum captures the critical path correctly,
which is the standard convention in parallel performance analysis.

**Pattern as n increases (fixed t):** Slave compute grows as O(n²/t)
per slave — `compute_mmt_strip` does two passes over each column it
owns (one to find min/max, one to normalize), which is 2 · n · (n/t)
floating-point operations per slave. Doubling n quadruples per-slave
compute (for fixed t).

**Pattern as t increases (fixed n):** Slave compute falls roughly
as 1/t because each slave's column count drops linearly. At very high
t, the constant-time per-slave overhead (allocations, loop setup, cache
warm-up) starts to limit the falloff and the curve flattens.

---

### Research Activity 4 — Performance Metrics

**Communication vs. computation share.** The gap `T_P − T_2(slave-max)`
per (n, t) gives the wall-clock time spent outside slave compute:
master-side distribute + master-side reduce + idling. For Approach C, the
distribute alone is `t × n²` ints in cumulative bytes — ~1 GB per edge at
n = 16,000 — which dominates at large n on the swarm.

**Other overhead beyond communication and computation:**

1. **Idling.** A forwarding slave finishes its compute (timer stops) but
   must still `pthread_join` its child forwarders before it can M1PR up
   to the parent. That join wait is excluded from the slave timer but
   included in the master timer. It manifests as a gap when one path of
   the reduce tree completes faster than another.
2. **Excess computation.** `compute_mmt_strip` does two passes per
   column (one for min/max, one for normalization). A single-pass
   formulation is possible but loses numerical clarity and adds branches
   to a hot inner loop. The "excess" is one additional column read per
   assigned column per slave.
3. **Synchronization.** TCP `recv` blocks; `pthread_join` blocks. These
   are zero-CPU waits on the slave (covered by the master timer) and on
   forwarding slaves between own-MMT-done and reduce-up.

**Superlinearity.** Superlinear speedup (S > p) typically requires that
the parallel version benefit from cache effects unavailable to the
serial version — for example, the per-slave column slice fits in L2 or
L3 even though the full matrix did not. For Approach C, every slave
still holds the full X in memory, so cache benefits are limited to the
working set during the 2-pass MMT (a small number of columns at a
time). Expectation: speedup grows with t but stays sub-linear.

[Once Table 3 is filled: confirm whether any S > p was observed. If yes,
attribute to cache; if no, the expected sub-linear behavior is
consistent with an I/O-bound parallel workload.]

**Cost-optimality.** A parallel algorithm is cost-optimal in the
practical sense when pT_P does not grow much faster than T_S as t
increases. Comparing the pT_P column across t for each fixed n shows
whether adding processors continues to be worthwhile. On the swarm, the
expected behavior at large n is that pT_P grows with t because
communication overhead grows linearly with t while compute savings
diminish — i.e., the implementation is **not cost-optimal** at high t for
the swarm topology, but is approximately cost-optimal at moderate t.

[Fill in observed pT_P trend once Table 3 is populated.]

---

## Implementation Note (from `lab05.c`)

The 1MPB and M1PR are realized over the same TCP edges of the same
recursive-halving tree inherited from lab04. Each tree edge carries the
full matrix X downward (1MPB direction) and the subtree-wide T-strip
upward (M1PR direction). Forwarding slaves spawn their child threads
**before** starting their compute timer, so the downstream X transfer
overlaps with this slave's MMT compute and is correctly excluded from
the slave timing per LRP05 spec item 3. The slave's own MMT writes
directly into the leftmost columns of the subtree-wide T-strip buffer
(via the `strip_stride` parameter to `compute_mmt_strip`), so children's
strips can be `memcpy`'d into the same buffer at their column offsets
without an extra allocation or copy.
