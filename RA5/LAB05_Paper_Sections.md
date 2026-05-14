# CMSC 180 — Laboratory Research Problem 05
## Distributed Computation of the Min-Max Transformation of Elements in a Column of a Matrix

**Name:** Charles Andrei P. De los Reyes
**Student Number:** 2023-15797
**Section:** B-3L

---

## 1. Implementation Summary

`lab05.c` is a master-slave TCP program that computes the column-wise
**Min-Max Transformation** of an n × n integer matrix X across t slaves
on different machines. It extends `lab04_v3.c` by adding (a) a
**column-wise compute step** on each slave and (b) a **many-to-one
personalized reduction (M1PR)** of the resulting T-strips back to the
master, both layered on top of lab04's existing **one-to-many
personalized broadcast (1MPB)** of the matrix.

The master partitions the t slaves into a recursive-halving tree. Every
tree edge carries the **full** matrix X downward (1MPB direction) — each
slave receives the entire X regardless of its position in the tree. Each
slave is assigned a contiguous **column range**: it computes the MMT
only for its own columns, but it has the full per-column data needed to
do so independently (no cross-slave min/max reductions). After
computing, the slaves return their T-strips up the same tree (M1PR
direction); intermediate slaves concatenate their own and their
children's T-strips into one contiguous subtree-wide block before
forwarding upward. The master receives one strip per direct child and
`memcpy`'s each into the rebuilt full T at the correct column offset.

The master's `clock_gettime(CLOCK_MONOTONIC)` timer wraps the entire
distribute → MMT (in parallel on slaves) → reduce → rebuild round trip
(LRP05 spec item 2). Each slave's timer wraps **only** the
`compute_mmt_strip` call (LRP05 spec item 3); forwarder threads on
intermediate slaves are spawned **before** the slave timer starts, so
downstream communication overlaps with this slave's compute and is
correctly excluded from the slave timing. Slaves are pinned to non-OS
cores via `sched_setaffinity` (LRP05 spec item 4). Matrix generation,
configuration parsing, and output printing are deliberately excluded
from both timers.

---

## 2. Table 1 — Time elapsed as reported by the master process.

Run on the ICS Swarm with 1 master drone + 16 slave drones, slave
processes core-affine.

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

All times in seconds.

---

## 3. Table 2 — Time elapsed as reported by the slave processes (max).

Per (n, t, run), the entry is the **maximum** of the per-slave compute
times. The maximum is reported because the master cannot finish until
every slave has finished — the slowest slave bounds the parallel
runtime, so the average across slaves would understate the true
parallel compute window.

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

---

## 4. Table 3 — Performance metrics.

T_S is the lab01 serial runtime (constant per n), T_P is the average
master time from Table 1, p = t. T_O = p · T_P − T_S, S = T_S / T_P,
E = S / p, pT_P = p · T_P.

| n      | t  | T_S | T_O | S | E | pT_P |
|--------|----|-----|-----|---|---|------|
| 4,000  | 2  |     |     |   |   |      |
| 4,000  | 4  |     |     |   |   |      |
| 4,000  | 8  |     |     |   |   |      |
| 4,000  | 16 |     |     |   |   |      |
| 8,000  | 2  |     |     |   |   |      |
| 8,000  | 4  |     |     |   |   |      |
| 8,000  | 8  |     |     |   |   |      |
| 8,000  | 16 |     |     |   |   |      |
| 16,000 | 2  |     |     |   |   |      |
| 16,000 | 4  |     |     |   |   |      |
| 16,000 | 8  |     |     |   |   |      |
| 16,000 | 16 |     |     |   |   |      |

---

## 5. Research Activity 2 — Communication and Computation (Figure 1)

Plot Figure 1: 3D plot of (n, t, average master time), or 2D with three
lines (one per n).

[Discussion to be filled with observed pattern after Table 1 is
populated. Expected shape: master time decreases from t = 2 to a sweet
spot (often t = 4 or 8) as compute parallelism dominates, then
plateaus or increases at higher t as the cumulative tree-edge
communication cost (every edge ships full X under Approach C) catches
up. Across n, the curves shift upward roughly with n² because both
distribute size and per-slave compute scale with n².]

---

## 6. Research Activity 3 — Computation Only (Figure 2)

Plot Figure 2: same shape as Figure 1, using slave-max times.

**Why max and not average?** The master's wall-clock cannot stop until
every slave has finished. The slowest slave bounds the parallel time:
if 15 slaves take 1 s and 1 takes 5 s, the master is still waiting at
second 4.999. Averaging would yield ~1.25 s, a large underestimate of
the actual parallel runtime. The maximum captures the critical path,
which is the standard convention in parallel-systems performance
analysis.

[Discussion to be filled. Expected pattern: slave-max compute falls
roughly as 1/t for fixed n because each slave's column count is n/t,
and grows as n² for fixed t because per-column work is O(n) and per-
slave column count is O(n).]

---

## 7. Research Activity 4 — Performance Metrics

For each n, compare Figure 1 vs Figure 2. The gap between them is
**communication + idling overhead**:

- **Communication.** Approach C ships the full X across every tree edge.
  At n = 16,000, that is ~1 GB per edge × O(t) edges across the tree.
  This dominates the master-time-minus-slave-max-time gap on the swarm.
- **Idling.** A forwarding slave's compute timer stops as soon as
  `compute_mmt_strip` returns, but the slave still must `pthread_join`
  its forwarder threads before sending its subtree-wide T-strip up. The
  wait between own-compute-done and reduce-send is excluded from the
  slave timer but included in the master timer.
- **Excess computation.** `compute_mmt_strip` does two passes per column
  (one for min/max, one for normalization). A single-pass form is
  possible but loses clarity; the excess is one extra column read per
  assigned column.

**Superlinearity (S > p).** Superlinearity typically arises when the
per-slave working set fits in cache while the serial working set did
not. Under Approach C every slave holds the full X (~1 GB at n = 16,000),
so cache benefits are limited to the small working set during the 2-pass
MMT. Expectation: S grows with t but stays sub-linear. [Confirm or
correct after Table 3 is filled.]

**Cost-optimality.** An algorithm is cost-optimal in the practical sense
when pT_P does not grow much faster than T_S as t increases — i.e., the
parallel cost stays bounded by a constant factor of the serial cost. On
the swarm the expected outcome at large n is that pT_P grows with t
because communication grows linearly with t while compute savings level
off. The implementation is therefore approximately cost-optimal at
moderate t (where compute savings still dominate added communication)
but loses cost-optimality as t approaches the largest values for which
communication overhead exceeds compute reduction. [Confirm with
observed pT_P trend.]

---

## 8. Verification

Correctness was confirmed by running with a fixed 8 × 8 input
(`input8.txt`) and comparing the master's reconstructed T against the
serial lab01.c output. Each column of T normalizes to [0, 1] with the
column minimum mapping to 0.0 and the column maximum to 1.0, with
intermediate values placed proportionally — exactly the column-wise MMT
specification from LRP01.

Per-hop matrix printing at every send and receive (gated by `n ≤ 32` or
file mode) confirms the four required behaviors:

1. The master sends the full X to its direct children with the correct
   column-range work descriptors.
2. Each slave receives the full X and computes the MMT for its own
   column range only.
3. Intermediate slaves spawn forwarders before their compute timer
   starts and assemble their subtree-wide T-strip from own + children's
   strips.
4. The master receives one T-strip per direct child and rebuilds full T
   by `memcpy` at the correct column offsets.

---

## 9. Conclusion

The lab05 distribute-compute-reduce pipeline preserves lab04's O(log t)
tree topology while adding the column-wise MMT compute step and the M1PR
direction. The master's timing measures the full parallel round trip
(LRP05 Table 1), each slave's timing measures only its own compute
window (LRP05 Table 2), and the spread between these two values is what
the performance metrics in Table 3 quantify as parallel overhead.
Approach C (broadcast full X, partition compute work by columns) was
chosen because it maps the LRP05 PDF's wording most literally and keeps
each slave's MMT fully self-contained. The cost is X-replication on
every slave, which is borderline on the swarm at n = 16,000 but
acceptable for the problem sizes specified by the lab.
