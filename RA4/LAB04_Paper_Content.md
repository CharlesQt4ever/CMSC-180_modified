# CMSC 180 — Laboratory Research Problem 04
## Distributing Parts of a Matrix over Sockets

**Name:** Charles Andrei P. De los Reyes
**Student Number:** 2023-15797
**Section:** B-3L

---

## 1. Implementation Summary

`lab04_v3.c` is a master–slave TCP program that distributes an n×n integer
matrix across t slaves. The master partitions the matrix M into t
row-contiguous submatrices of approximately ⌈n/t⌉ rows each — with any
leftover rows spread across the first few slaves so that every row is
delivered exactly once — then dispatches each submatrix using a **tree-based
one-to-many personalized broadcast**.

Each slave listens on its configured port, receives its assigned submatrix
together with the metadata needed to forward data to any downstream children,
prints the received submatrix for verification, and sends an `ack` back up
the tree. The master's timer wraps exactly the distribution-plus-ack phase
(spec items 2d–2h) using `clock_gettime(CLOCK_MONOTONIC, ...)` for nanosecond
precision. Matrix generation, configuration parsing, and output printing
are deliberately excluded from the timed region so that the reported times
reflect only the communication cost.

---

## 2. Table 1 — Single PC, No Core Affinity

All slaves and the master ran as separate processes on a single PC,
communicating through distinct ports on the loopback interface.

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

All times are in seconds.

---

## 3. Table 2 — Single PC, With Core Affinity

Each slave process was pinned to a distinct CPU core (cores 1–15) via
`sched_setaffinity`; the master remained on core 0. All other conditions
matched Table 1.

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

## 4. Table 3 — Different PCs (ICS Swarm)

Executed on the ICS Swarm cluster with **hayabusa (10.0.9.134)** as the
master and **16 slave drones** — grock, fredrinn, belerick, leomord,
khaleed, dyrroth, gusion, lunox, zhuxin, mathilda, natan, balmond, beatrix,
alucard, phoveus, freya — each on a separate physical machine connected by
Gigabit Ethernet. Every send and receive in the tree therefore crossed the
Ethernet fabric; no loopback shortcut was available, unlike in Tables 1 and 2.

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

---

## 5. Question 3 — What happened when slaves were pinned to cores?

Comparing Table 1 to Table 2, **binding each slave to a distinct core
produced no speedup**; at the largest problem size it produced a small but
consistent slowdown:

| n      | t  | No affinity | With affinity | Δ       |
|--------|----|-------------|---------------|---------|
| 16,000 | 2  | 0.204 s     | 0.220 s       | +7.8 %  |
| 16,000 | 4  | 0.302 s     | 0.336 s       | +11.2 % |
| 16,000 | 8  | 0.402 s     | 0.405 s       | ≈ 0 %   |
| 16,000 | 16 | 0.450 s     | 0.482 s       | +7.0 %  |

The result is consistent with the nature of the workload. Socket
distribution is **I/O- and memory-bandwidth-bound**, not compute-bound:
each slave spends most of its time blocked in `recv()` waiting on the
kernel TCP stack, then performs a `memcpy` into its own buffer. Between
these two operations there is almost no work for a CPU core to do.

In this regime, the Linux scheduler's default freedom to migrate threads
onto whichever core is currently idle is *helpful*, because it keeps both
the kernel softirq handlers (which service loopback sockets) and the
user-space receivers on warm caches. Forcing affinity removes that
freedom. With cores 1–15 dedicated exclusively to the 15 slaves, core 0
becomes a contention point for the master thread, its spawned worker
threads, and the loopback service routines — the small but consistent
slowdown seen at n = 16,000 is the direct consequence.

Core affinity would matter more for workloads that perform heavy
computation *after* receiving their data (for example, the MMT kernel in
LRP05), because cache locality of the submatrix in L2/L3 would then
dominate the runtime. For pure distribution, the transfer completes
before meaningful caching can pay off.

---

## 6. Question 4 — What happened when slaves ran on different PCs?

Running the same program across 17 physical machines on the ICS Swarm
(1 master + 16 slave drones over Gigabit Ethernet) produced three
clearly observable effects.

### 6.1 Absolute times grew by two to three orders of magnitude

| n = 16,000, t = 16                    | Average time | Ratio to Table 1 |
|---------------------------------------|--------------|------------------|
| Table 1 (loopback)                    | 0.450 s      | 1×               |
| Table 2 (loopback, core affinity)     | 0.482 s      | 1.07×            |
| Table 3 (swarm, 17 physical machines) | 149.884 s    | **≈ 333×**       |

The cause is bandwidth, not CPU. Loopback reaches several GB/s because
the payload never leaves RAM, while Gigabit Ethernet caps near 125 MB/s
of nominal throughput — and the effective figure on a shared cluster is
usually lower because the fabric is contended with every other student's
job. The full matrix at n = 16,000 is 1.024 GB; at the observed
effective throughput, the wire time alone accounts for most of the
measured 150 seconds.

### 6.2 Scaling with t remained logarithmic

This is the most important confirmation of the tree design. At n = 16,000:

| t  | Table 3 average | Ratio vs t = 2 |
|----|-----------------|----------------|
| 2  | 101.636 s       | 1.00×          |
| 4  | 115.514 s       | 1.14×          |
| 8  | 141.525 s       | 1.39×          |
| 16 | 149.884 s       | **1.47×**      |

An 8× increase in the number of slaves produced only a 1.47× increase in
time. A naive O(t) scheme would have grown by roughly 8× under the same
change. The observed growth is consistent with the log₂(16)/log₂(2) = 4×
ratio expected from adding log-depth tree levels, further moderated by
the fact that deeper levels carry proportionally smaller chunks in
parallel. The O(log t) advantage of the tree is therefore **not a
loopback artifact**; it survives intact on real physical hardware.

### 6.3 Variance increased, consistent with a shared cluster

Run-to-run variance on the swarm was noticeably higher than on a single
PC. At n = 16,000, t = 2, the three runs produced 104.96 s, 106.24 s,
and 93.71 s — a spread of roughly 12 %, compared with under 3 % in
Table 1. Likely contributors include competing jobs on the same
switches, background TCP traffic on the 10.0.9.0/24 subnet, DHCP
renewals, and variable propagation time across the 16 parallel SSH
sessions used to start the slaves.

### 6.4 Summary

The swarm measurements validate the claims in Question 5 under the most
demanding topology available to the class. Absolute times scale with the
network bandwidth rather than with CPU, as expected, but the *relative*
behavior — logarithmic scaling with t and parallel forwarding once data
lands at each drone — is preserved. A tree distribution is therefore the
correct technique whether slaves share a PC or span a rack.

---

## 7. Question 5 — Is the implementation efficient, and which communication technique was used?

**The implementation is efficient.** It uses a **one-to-many personalized
broadcast** realized as a recursive **binomial (binary) tree** rooted at the
master. Every slave receives a *different* submatrix (personalized, not
broadcast), and the distribution requires only O(log t) sequential network
steps rather than O(t).

### 7.1 How the tree is constructed

At each recursion step, the master splits its t slaves into two halves. It
keeps the left half for itself and delegates the right half to the slave at
the boundary, sending that slave the entire right-half block of the matrix
together with enough metadata (`first_slave`, `num_slaves`) for that slave
to repeat the same split-and-forward procedure on its own subtree.
Recursion terminates when a subtree contains exactly one slave, which
retains its single assigned submatrix.

### 7.2 Why the tree is the efficient choice

A naive sequential scheme ("send to slave 1, wait for ack, send to slave 2,
...") is O(t) in serial sends from the master, and the last slave waits
while the first t − 1 transfers complete. A flat parallel scheme — one
thread per slave, all initiated by the master — shortens wall time but
still routes every byte through the master's single outgoing link, so the
throughput is bounded by the master alone.

The tree avoids both bottlenecks:

- The master performs only ⌈log₂ t⌉ = `num_children` outgoing transfers in
  parallel. For t = 16, that is **4 sends instead of 16**.
- Once a slave has received its block, it becomes a source in turn. At
  depth d, 2^d nodes transmit simultaneously, so the aggregate throughput
  at that depth is 2^d × the master's lone outgoing bandwidth.
- Acknowledgments travel up the tree in the same shape, so the master's
  ack-wait also terminates in O(log t) rather than O(t).

### 7.3 Correspondence to the lecture taxonomy

In the standard parallel-computing taxonomy (Kumar et al., *Introduction
to Parallel Computing*), the four collective primitives are one-to-many
broadcast, all-to-all broadcast, **one-to-many personalized**
communication, and all-to-all personalized. Because each slave receives a
distinct submatrix mᵢ, this implementation falls cleanly into the
one-to-many personalized category, realized with the log-depth tree
pattern presented in that chapter.

### 7.4 Quantitative evidence

The effect of the tree is visible in every table at n = 16,000:

| Table                    | t = 2     | t = 16    | t = 16 / t = 2 |
|--------------------------|-----------|-----------|----------------|
| 1 — single PC            | 0.204 s   | 0.450 s   | 2.21×          |
| 2 — single PC + affinity | 0.220 s   | 0.482 s   | 2.19×          |
| 3 — 17 physical drones   | 101.636 s | 149.884 s | **1.47×**      |

Across all three tables, an 8× increase in t produces only a 1.5×–2.2×
increase in time — well below the 8× that a sequential scheme would
demand. Table 3 exhibits the flattest scaling, which is physically
sensible: on loopback, the kernel softirq path and the master's single
RAM bus still serialize a portion of the work, whereas on the swarm each
tree depth uses genuinely independent NICs and CPUs, so the log-depth
advantage is expressed most cleanly. Classmates using non-tree
implementations report runtimes near 2 s at n = 16,000 on a single PC —
roughly 4× slower than the tree at t = 16 on loopback, consistent with
the O(log t) vs O(t) gap predicted by theory.

---

## 8. Verification

Correctness of the row assignment was confirmed by running the program at
n = 3, t = 2 with a fixed 3×3 input matrix:

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
item 2(6). Per-hop matrix printing at every send and receive (required by
the RA4 announcement) is implemented in `lab04_v3.c` and gated by
`n ≤ 32`, so small demonstration runs show the full matrix at each hop
while the benchmark runs at n ≥ 4,000 remain unaffected.

---

## 9. Conclusion

The tree-based one-to-many personalized broadcast implemented in
`lab04_v3.c` delivered the expected logarithmic scaling across all three
measurement environments. On a single PC, loopback bandwidth kept absolute
times well below one second even at n = 16,000 and t = 16. Pinning slaves
to cores produced no benefit, and a small penalty at large n — a result
consistent with an I/O-bound workload. On the 17-machine ICS Swarm, the
same program ran roughly 333× slower in absolute terms (as expected,
given the Ethernet bandwidth ceiling), but the relative scaling with t
stayed clearly sub-linear (1.47× for an 8× increase in slaves),
confirming that the O(log t) behavior is intrinsic to the algorithm and
not an artifact of loopback timing.
