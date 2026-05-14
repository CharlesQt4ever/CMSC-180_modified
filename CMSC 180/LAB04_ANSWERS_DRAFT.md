+# CMSC 180 — Laboratory Research Problem 04
## Distributing Parts of a Matrix over Sockets

**Name:** Charles Andrei P. De los Reyes
**Student Number:** 2023-15797
**Section:** B-3L

---

## Implementation Summary

`lab04_v3.c` implements a master–slave program that distributes an n×n
integer matrix over TCP sockets. The master divides M into t submatrices of
approximately n/t rows × n columns each (the remainder rows are spread across
the first few slaves so every row is accounted for), opens port p, and uses a
**tree-based one-to-many personalized broadcast** to deliver the submatrices.

Each slave listens on its configured port, receives its submatrix together
with the metadata needed to forward data to its own children (if any), prints
the submatrix for verification, and sends an `ack` up the tree. The master's
timer wraps exactly the distribution and ack phase (spec items 2d–2h).

---

## Table 1 — Single PC, No Core Affinity

| n      | t  | Run 1    | Run 2    | Run 3    | Average  |
|--------|----|----------|----------|----------|----------|
| 4,000  | 2  | 0.014540 | 0.018172 | 0.018506 | 0.017073 |
| 4,000  | 4  | 0.022927 | 0.022501 | 0.023492 | 0.022973 |
| 4,000  | 8  | 0.027694 | 0.027366 | 0.033009 | 0.029356 |
| 4,000  | 16 | 0.038022 | 0.033686 | 0.033495 | 0.035068 |
| 8,000  | 2  | 0.056518 | 0.060112 | 0.055731 | 0.057454 |
| 8,000  | 4  | 0.085382 | 0.087451 | 0.086881 | 0.086571 |
| 8,000  | 8  | 0.102653 | 0.099831 | 0.111726 | 0.104737 |
| 8,000  | 16 | 0.124436 | 0.122716 | 0.126551 | 0.124568 |
| 16,000 | 2  | 0.204243 | 0.199779 | 0.208155 | 0.204059 |
| 16,000 | 4  | 0.307341 | 0.296739 | 0.303263 | 0.302448 |
| 16,000 | 8  | 0.414202 | 0.394432 | 0.398659 | 0.402431 |
| 16,000 | 16 | 0.433494 | 0.459253 | 0.458044 | 0.450264 |

Times are in seconds. All slaves and the master ran on the same PC as separate
processes bound to different ports on the loopback interface.

---

## Table 2 — Single PC, With Core Affinity

Each slave process was pinned to a distinct CPU core (cores 1…15) via
`sched_setaffinity`, while the master remained on core 0.

| n      | t  | Run 1    | Run 2    | Run 3    | Average  |
|--------|----|----------|----------|----------|----------|
| 4,000  | 2  | 0.016576 | 0.015553 | 0.015187 | 0.015772 |
| 4,000  | 4  | 0.023021 | 0.024071 | 0.024345 | 0.023812 |
| 4,000  | 8  | 0.029421 | 0.030629 | 0.029055 | 0.029702 |
| 4,000  | 16 | 0.035423 | 0.034414 | 0.035923 | 0.035253 |
| 8,000  | 2  | 0.052038 | 0.056003 | 0.056865 | 0.054969 |
| 8,000  | 4  | 0.088928 | 0.086846 | 0.085981 | 0.087252 |
| 8,000  | 8  | 0.103423 | 0.101003 | 0.109725 | 0.104717 |
| 8,000  | 16 | 0.130102 | 0.122868 | 0.128082 | 0.127017 |
| 16,000 | 2  | 0.219621 | 0.215163 | 0.225091 | 0.219958 |
| 16,000 | 4  | 0.343269 | 0.333158 | 0.332469 | 0.336299 |
| 16,000 | 8  | 0.414651 | 0.385119 | 0.414849 | 0.404873 |
| 16,000 | 16 | 0.461068 | 0.494783 | 0.489821 | 0.481891 |

---

## Table 3 — Different PCs (ICS Swarm)

*Pending: runs were attempted on the ICS Swarm cluster with fanny
(10.0.9.125) as master and 16 drones as slaves. SSH key propagation to all
slave drones and the final `./table3.sh` execution could not be completed
within the remaining laboratory time. Data will be added once the run is
finished.*

| n      | t  | Run 1 | Run 2 | Run 3 | Average |
|--------|----|-------|-------|-------|---------|
| 4,000  | 2  | TBD   | TBD   | TBD   | TBD     |
| 4,000  | 4  | TBD   | TBD   | TBD   | TBD     |
| 4,000  | 8  | TBD   | TBD   | TBD   | TBD     |
| 4,000  | 16 | TBD   | TBD   | TBD   | TBD     |
| 8,000  | 2  | TBD   | TBD   | TBD   | TBD     |
| 8,000  | 4  | TBD   | TBD   | TBD   | TBD     |
| 8,000  | 8  | TBD   | TBD   | TBD   | TBD     |
| 8,000  | 16 | TBD   | TBD   | TBD   | TBD     |
| 16,000 | 2  | TBD   | TBD   | TBD   | TBD     |
| 16,000 | 4  | TBD   | TBD   | TBD   | TBD     |
| 16,000 | 8  | TBD   | TBD   | TBD   | TBD     |
| 16,000 | 16 | TBD   | TBD   | TBD   | TBD     |

---

## Question 3 — What happened when slaves were pinned to cores (core-affine)?

Comparing Table 1 and Table 2, **pinning each slave to a distinct core
produced no meaningful speedup** and in several cases produced a small
slowdown, particularly at n = 16,000:

| n      | t  | No affinity | With affinity | Δ          |
|--------|----|-------------|---------------|------------|
| 16,000 | 2  | 0.204059 s  | 0.219958 s    | +7.8 %     |
| 16,000 | 4  | 0.302448 s  | 0.336299 s    | +11.2 %    |
| 16,000 | 8  | 0.402431 s  | 0.404873 s    | ≈ 0 %      |
| 16,000 | 16 | 0.450264 s  | 0.481891 s    | +7.0 %     |

The result is consistent with the nature of the workload. Distribution over
sockets is **I/O- and memory-bandwidth-bound**, not compute-bound. The slaves
spend most of their time inside `recv()` waiting on the kernel TCP stack and
then `memcpy`-ing bytes into their own buffer; there is almost no CPU work
between these operations. In such a workload, the Linux scheduler's normal
ability to migrate threads onto whichever core is currently idle is *helpful*,
because it keeps both the kernel softirq handlers (which service loopback
sockets) and the user-space receiver on warm caches.

Forcing affinity removes that freedom. With cores 1–15 reserved exclusively
for 15 slaves (and the master sharing core 0 with the kernel, softirqs, and
all background processes), core 0 becomes a contention point for the master
thread, its spawned worker threads, and the loopback service routines. The net
effect at large n is a small but consistent slowdown.

Core affinity would matter more if the slaves did heavy computation after
receiving the submatrix (for example, MMT in LRP05), because then cache
locality of the submatrix in L2/L3 would dominate. For pure distribution, the
transfer itself is over before any meaningful amount of caching could pay off.

---

## Question 4 — What happened when slaves ran on different PCs?

*To be completed after Table 3 runs finish. Expected behavior, based on the
implementation and the ICS Swarm's 1 Gbps Ethernet fabric:*

- **Absolute times will increase** relative to Table 1 and Table 2, because
  the effective bandwidth of 1 Gbps Ethernet (≈ 125 MB/s) is roughly 20–40×
  slower than loopback memory transfers (several GB/s). For n = 16,000 the
  full matrix is 1.024 GB, so the wire-time floor for delivering the whole
  matrix over a single link is about 8 s — the tree distributes over several
  links in parallel, bringing this down proportionally.
- **Scaling with t will invert.** On a single PC, larger t meant more
  process/thread context-switching and loopback contention on the same
  memory, so time grew with t. With true physical parallelism across drones,
  each additional slave adds its own NIC, its own memory, and its own CPU.
  After the fixed O(log t) tree overhead, adding slaves *reduces* the serial
  portion of the transfer, so time should *decrease* (or at least stay flat)
  as t grows, until the tree depth itself becomes the bottleneck.
- **Variance will increase** because the swarm is a shared cluster (other
  students' jobs, other TCP traffic, DHCP behaviour).

The specific numbers depend on the drones selected, so the narrative above
will be revised once Table 3 is collected.

---

## Question 5 — Is the implementation efficient? Which communication technique was used?

**Yes.** The implementation uses a **one-to-many personalized broadcast**,
realised as a recursive **binomial (binary) tree** rooted at the master.
Every slave receives a *different* submatrix (personalized, not broadcast),
and the distribution is organized so that the number of sequential network
steps is O(log t) rather than O(t).

**How the tree is built.** The master splits its t slaves into two halves at
each recursion step. It keeps the left half for itself and delegates the
right half to the slave at the boundary, sending that slave the entire
right-half block of the matrix together with enough metadata (`first_slave`,
`num_slaves`) for that slave to repeat the same split-and-forward procedure
on its own subtree. The recursion continues until every subtree contains
exactly one slave, which keeps its single assigned submatrix.

**Why this is the efficient technique.** A naive sequential scheme ("send to
slave 1, wait ack, send to slave 2, …") is O(t) in the number of serial sends
from the master, and the last slave waits while the first t − 1 transfers
complete. A flat parallel scheme (one thread per slave, all from the master)
shortens wall time, but every byte still passes through the master's single
outgoing pipe, so it is bandwidth-bounded by the master alone.

The tree solves both problems:

- The root (master) performs only ⌈log₂ t⌉ = `num_children` transfers in
  parallel — for t = 16, that is **4 outgoing transfers instead of 16**.
- Once a slave has received its block, it becomes a source itself. At depth
  d, 2^d nodes are transmitting simultaneously, so the aggregate network
  throughput at that depth is 2^d times the master's lone outgoing
  bandwidth.
- Acknowledgments travel up the tree in the same shape, so the master
  finishes waiting in O(log t) time rather than O(t).

**Correspondence to the lecture's taxonomy.** In the standard taxonomy
(Kumar et al., *Introduction to Parallel Computing*), the four primitives are
one-to-many broadcast (same data to everyone), all-to-all broadcast,
one-to-many personalized communication (different data to each node), and
all-to-all personalized. Because each slave receives a distinct submatrix
mᵢ, this is one-to-many **personalized** communication, implemented with the
log-depth tree pattern shown in that chapter.

**Quantitative evidence from the measurements.** The effect of the tree is
visible in Table 1: going from t = 2 to t = 16 at n = 16,000 only increases
the master's observed time from 0.204 s to 0.450 s — a 2.2× increase for an
8× increase in t, which matches the expected log-depth growth rather than
linear growth. A naive sequential scheme at the same data volume would have
produced roughly 4× the time at t = 16, which is consistent with what
classmates using non-tree implementations have reported (≈ 2 s at
n = 16,000).

---

## Verification

Correctness of the row assignment was verified by running the program at
n = 3, t = 2 with a fixed input matrix:

```
--- Full Matrix M (3 x 3) ---
11 12 13
14 15 16
17 18 19
[Master] Slave 1 (127.0.0.1:5001) -> 2 rows (rows 0 to 1)
11 12 13
14 15 16
[Master] Slave 2 (127.0.0.1:5002) -> 1 rows (rows 2 to 2)
17 18 19
```

Each slave printed back exactly the rows assigned to it, confirming spec
item 2(6).
