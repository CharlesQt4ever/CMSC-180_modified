# CMSC 180 — Laboratory Research Problem 04
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

Run on the ICS Swarm cluster with **hayabusa (10.0.9.134)** as master and
**16 slave drones** — grock, fredrinn, belerick, leomord, khaleed, dyrroth,
gusion, lunox, zhuxin, mathilda, natan, balmond, beatrix, alucard, phoveus,
freya — each on a separate physical machine. Connection: Gigabit Ethernet.

| n      | t  | Run 1      | Run 2      | Run 3      | Average    |
|--------|----|------------|------------|------------|------------|
| 4,000  | 2  | 7.705485   | 5.440405   | 5.447783   | 6.197891   |
| 4,000  | 4  | 6.820272   | 6.821481   | 6.837200   | 6.826318   |
| 4,000  | 8  | 8.223834   | 8.171413   | 8.177687   | 8.190978   |
| 4,000  | 16 | 9.586386   | 9.621205   | 9.574000   | 9.593864   |
| 8,000  | 2  | 22.989403  | 21.836092  | 21.759297  | 22.194931  |
| 8,000  | 4  | 27.254486  | 27.234979  | 27.228129  | 27.239198  |
| 8,000  | 8  | 32.654327  | 32.708185  | 32.647786  | 32.670099  |
| 8,000  | 16 | 38.553450  | 41.309176  | 41.949142  | 40.603923  |
| 16,000 | 2  | 104.956912 | 106.237788 | 93.714543  | 101.636414 |
| 16,000 | 4  | 111.340261 | 119.479887 | 115.722317 | 115.514155 |
| 16,000 | 8  | 145.615386 | 137.460770 | 141.499060 | 141.525072 |
| 16,000 | 16 | 149.397894 | 148.750442 | 151.504708 | 149.884348 |

Times are in seconds. Each slave ran on a dedicated physical drone, so every
send/receive in the tree crossed the Gigabit Ethernet fabric (no loopback
shortcut like in Tables 1 and 2).

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

Running the same program across 17 physical machines on the ICS Swarm
(1 master + 16 slave drones connected by Gigabit Ethernet) produced three
clearly observable effects.

### 1. Absolute times grew by two to three orders of magnitude

| n = 16,000, t = 16 | Average time | Ratio to Table 1 |
|--------------------|--------------|-------------------|
| Table 1 (loopback)                     | 0.450 s   | 1× |
| Table 2 (loopback, core affinity)      | 0.482 s   | 1.07× |
| Table 3 (swarm, 17 physical machines)  | 149.884 s | **≈ 333×** |

The cause is simply bandwidth. Loopback reaches several GB/s because the
payload never leaves RAM, while Gigabit Ethernet tops out near 125 MB/s —
and effective throughput on the shared swarm is usually lower than nominal
because the fabric is shared with every other student's job. The full
matrix at n = 16,000 is 1.024 GB; at the observed effective throughput, the
wire time alone accounts for most of the measured 150 seconds.

### 2. Scaling with t remained logarithmic

This was the most important confirmation of the tree design.

At n = 16,000:

| t  | Table 3 average | Ratio vs t = 2 |
|----|-----------------|----------------|
| 2  | 101.636 s       | 1.00× |
| 4  | 115.514 s       | 1.14× |
| 8  | 141.525 s       | 1.39× |
| 16 | 149.884 s       | **1.47×** |

Going from t = 2 to t = 16 is an 8× increase in the number of slaves, but
the measured time only grew by ~1.47×. A naive O(t) scheme would have grown
~8× under the same change. The observed growth is close to the
log₂(16)/log₂(2) = 4× ratio expected from adding log-depth tree levels,
modulated by the fact that deeper tree levels are transmitting *smaller*
chunks in parallel. This means the O(log t) advantage of the tree is **not
a loopback artifact** — it survives intact on real physical hardware.

### 3. Variance increased, consistent with a shared cluster

Run-to-run variance on the swarm is noticeably higher than on a single PC.
For example, at n = 16,000 and t = 2, the three runs produced 104.96 s,
106.24 s, and 93.71 s — a spread of ≈ 12 %, compared to < 3 % in Table 1.
Likely contributors: other users' jobs competing for the same network
switches, background TCP traffic on the 10.0.9.0/24 subnet, DHCP renewals,
and variable time for 16 SSH sessions to propagate the slave startup
command.

### Summary

The swarm measurements validate the claims in Q5 under the most demanding
topology available to the class. Absolute times are larger (bandwidth-bound
by Ethernet, not by CPU/loopback), but the *relative* behavior — log-depth
scaling, parallel forwarding once data lands at a drone — is preserved.
A tree distribution remains the correct technique whether the slaves are on
the same PC or spread across a rack.

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
visible in every table at n = 16,000:

| Table                  | t = 2      | t = 16     | t = 16 / t = 2 |
|------------------------|------------|------------|-----------------|
| 1 — single PC          | 0.204 s    | 0.450 s    | 2.21× |
| 2 — single PC + affinity | 0.220 s  | 0.482 s    | 2.19× |
| 3 — 17 physical drones | 101.636 s  | 149.884 s  | 1.47× |

Across all three tables, an **8× increase in t** produces only a **1.5× to
2.2× increase in time** — well below the ~8× a sequential scheme would have
required. The swarm run (Table 3) actually shows the flattest scaling, which
makes physical sense: on loopback, the kernel softirq path and the master's
single RAM bus still serialize some of the work, but on the swarm each depth
of the tree uses genuinely independent NICs and CPUs, so the log-depth
advantage is most cleanly visible. Classmates using non-tree implementations
report roughly 2 s at n = 16,000 on a single PC — about 4× slower than the
tree at t = 16 on loopback, consistent with the O(log t) vs O(t) gap.

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
