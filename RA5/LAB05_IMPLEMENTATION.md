# Lab 05 Implementation Analysis

## LRP05 spec → `lab05.c` mapping

This document maps each requirement from the LRP05 PDF to the corresponding
construct in `lab05.c`, and notes the design choices made (most notably the
column-partition compute model on top of the lab04 tree).

---

### LEGEND

- **DONE** = implemented in `lab05.c`
- **PARTIAL** = present but with caveats
- **TODO** = not yet implemented

---

## 1. Distributed-broadcast inheritance from lab04

| # | Requirement | Status | Notes |
|---|-------------|--------|-------|
| 1.1 | O(log t) tree-based 1MPB of matrix X | **DONE** | Same recursive-halving tree as lab04, preserved in `run_master` (lines 213–260) and `run_slave` forwarding block (lines 417–466) |
| 1.2 | Same single-binary master/slave topology | **DONE** | `s = 0` master, `s ≥ 1` slave, `argv` shape unchanged from lab04 |
| 1.3 | Cross-platform (Windows/Linux) | **DONE** | `#ifdef _WIN32` Winsock + `pthread`/`SetProcessAffinityMask` on Windows; BSD sockets + `pthread`/`sched_setaffinity` on Linux |
| 1.4 | Config file format | **DONE** | `master ip port` + `slave ip port` lines, identical to lab04 |
| 1.5 | Up to 64 slaves | **DONE** | `TArgs t_args[64]`; `t = 16` is the largest LRP05 case |
| 1.6 | Random-fill or file-input matrix | **DONE** | Random `[1,100]` if no input file; comma- or whitespace-separated file mode otherwise |

---

## 2. Distribution model — broadcast vs. partition

LRP05 says "1MPB distributed parts of the matrix X to different slave
processes." Lab04 partitioned X by **rows**. LRP05's MMT compute is
**column-wise**, so a row partition would force every slave to do
cross-slave min/max reductions before normalizing.

Three plausible distribution schemes were considered:

| Approach | What master sends | Slave compute | Cost |
|----------|-------------------|---------------|------|
| **A** — strict column-strip 1MPB | Each slave gets only its own column strip (n × my_col_count ints) | MMT on its strip directly | Communication: O(n²) ints per slave; Compute: clean |
| **B** — row-strip + cross-slave reduction | lab04-style row partition | MMT requires gathering full-column min/max from all slaves | Communication: 2× phases (distribute + reduce minmax); Compute: simple but N round trips |
| **C** — full-X broadcast + column work-partition | Every slave receives the entire X | Each slave computes MMT on its assigned column range | Communication: t × n² ints (largest); Compute: fully self-contained |

**`lab05.c` implements Approach C.** Rationale:

- The LRP05 wording **"1MPB distributed parts of the matrix X to different
  slave processes which can be processed concurrently by different
  machines"** matches a tree-broadcast of the full X most literally — every
  slave receives the matrix; what differs is which *columns* each slave
  processes.
- Each slave's MMT is fully self-contained — no cross-slave min/max
  reduction is needed because column k's full vector is already on every
  slave.
- Mirrors lab04's tree topology byte-for-byte: each tree edge sends the
  full X (the same payload lab04 already shipped), so the existing
  worker_func and split logic carry over with one new column-range
  parameter.

The cost is replication (every slave holds a full copy of X). At
n = 16,000 that is ~1 GB on each drone; on the 2 GB swarm drones this is
borderline but workable for the n values in the spec.

| # | Requirement | Status | Notes |
|---|-------------|--------|-------|
| 2.1 | Master 1MPB-distributes X via tree | **DONE** | `run_master` builds tree of direct children (lines 213–260); each `worker_func` thread sends full X bytes to its child (lines 134–141) |
| 2.2 | Slaves receive parts of X | **DONE** | Approach C — every slave receives the **whole** X, and gets a `(col_first, col_count, first_slave, num_slaves)` work descriptor with it (`run_slave` lines 386–396) |
| 2.3 | Same n/t + remainder distribution rule | **DONE** | `compute_size_for_range` and `my_col_count = recv_n / t + (first_slave < recv_n % t ? 1 : 0)` |

---

## 3. Min-Max Transformation on the slave side

LRP05 spec items 1(1) and 3 require:
- Compute the MMT into a matrix T of the **respective columns** of received X.
- Slave timer wraps **only** the MMT compute (`time_before` after recv X,
  `time_after` after MMT done, **before** sending T back).

| # | Requirement | Status | Notes |
|---|-------------|--------|-------|
| 3.1 | Per-column MMT (`(x − min) / (max − min)`) | **DONE** | `compute_mmt_strip()` (lines 75–96) — column-wise, identical formula to lab01.c |
| 3.2 | Compute only the columns assigned to this slave | **DONE** | Slave's own range = `[my_col_first, my_col_first + my_col_count)`; pre-loop derives `my_col_count` per the n/t + remainder rule (line 400) |
| 3.3 | Slave timer wraps **only** MMT | **DONE** | `clock_gettime` brackets exactly `compute_mmt_strip(...)` (lines 474–482); no recv, no forwarding, no send-back inside the timer |
| 3.4 | Forwarder threads launched **before** the timer | **DONE** | Child workers spawned at lines 456–465 — these run concurrently while this slave's compute window happens (lines 477) |
| 3.5 | Subtree T-strip placement avoids double allocation | **DONE** | `subtree_strip` is sized for the whole subtree (`recv_n × col_count` floats); slave's own MMT writes into the leftmost `my_col_count` columns directly via the `strip_stride = col_count` parameter to `compute_mmt_strip` |
| 3.6 | Slave reports `time_elapsed` | **DONE** | `printf("time elapsed: %.6f seconds\n", time_elapsed)` (line 481) — same line shape as the master so the same `grep -oP` pattern catches both |

---

## 4. M1PR — many-to-one personalized reduction

LRP05 requires the slaves to send their parts of T back to the master in a
many-to-one personalized **reduction** so the master rebuilds full T.

Implementation: the same tree used for distribution is reversed for
reduction. Each forwarding slave concatenates its own MMT with the strips
returned by its children into one contiguous subtree-wide buffer, then
sends it up to its parent in one block.

| # | Requirement | Status | Notes |
|---|-------------|--------|-------|
| 4.1 | Slaves send parts of T back | **DONE** | After joining children, slave `send_all`s its `subtree_strip` (line 535) |
| 4.2 | Master rebuilds full T | **DONE** | Master `memcpy`s each child's returned strip into the correct column offset of T row-by-row (lines 285–286) |
| 4.3 | Personalized reduction (each slave returns a unique strip) | **DONE** | Each subtree owns a contiguous, disjoint column range; no strip overlap, no cross-slave aggregation |
| 4.4 | Tree-shaped reduction (O(log t)) | **DONE** | Reduction tree is the mirror of the distribution tree — each slave aggregates own + children before forwarding up |

---

## 5. Master timing — LRP05 Table 1

| # | Requirement | Status | Notes |
|---|-------------|--------|-------|
| 5.1 | `time_before` before distributing X | **DONE** | `clock_gettime(CLOCK_MONOTONIC, &start)` right before `pthread_create` loop (line 275) |
| 5.2 | `time_after` after rebuilding T | **DONE** | `clock_gettime(CLOCK_MONOTONIC, &end)` right after the join+memcpy loop completes (line 299) |
| 5.3 | Master prints `time_elapsed` | **DONE** | `printf("time elapsed: %.6f seconds\n", ...)` at line 301 |
| 5.4 | Timer excludes matrix gen, config parse, T print | **DONE** | These all happen outside the start/end bracket |

---

## 6. Slave timing — LRP05 Table 2

| # | Requirement | Status | Notes |
|---|-------------|--------|-------|
| 6.1 | `time_before` after recv X | **DONE** | Bracket starts after `recv_all(X)` returns and after children are spawned (line 475) |
| 6.2 | `time_after` after T-strip computed, before send | **DONE** | Bracket ends right after `compute_mmt_strip` returns (line 479); the `send_all(subtree_strip)` to parent happens *after* the timer ends |
| 6.3 | Each slave reports its own `time_elapsed` | **DONE** | Line 481 |
| 6.4 | Sweep records the **maximum** across slaves per run | **DONE** | `table1.sh`/`table2.sh`/`table3.sh` redirect each slave's stdout to a per-slave log, then `grep "time elapsed:"` and take the max via awk |

---

## 7. Verification / Printing

LRP05 inherits lab04's "print after every send/recv" requirement.

| # | Requirement | Status | Notes |
|---|-------------|--------|-------|
| 7.1 | Master prints full X | **DONE** | When `n ≤ 32` or file mode (lines 205–211) |
| 7.2 | Master prints slave assignments | **DONE** | Lines 214–219 (col-range per slave) |
| 7.3 | Each slave prints its received X | **DONE** | When `n ≤ 32` (lines 406–415) |
| 7.4 | Each slave prints its own MMT strip | **DONE** | When `n ≤ 32` (lines 484–495) |
| 7.5 | Intermediate slaves print combined subtree T-strip | **DONE** | When `n ≤ 32 && num_slaves > 1` (lines 521–532) |
| 7.6 | Master prints rebuilt full T | **DONE** | When `n ≤ 32` or file mode (lines 303–309) |

---

## 8. Core affinity

| # | Requirement | Status | Notes |
|---|-------------|--------|-------|
| 8.1 | Slave can be pinned to a specific core | **DONE** | Same as lab04: `SetProcessAffinityMask` on Windows, `sched_setaffinity` on Linux (lines 337–347) |
| 8.2 | "Slave processes in each machine are running in a core-affine manner" (LRP05 item 4) | **DONE** | The `table3.sh` sweep assigns `core_id = (round + 1)` per drone — round 0 → core 1, round 1 → core 2 — so each slave on a drone uses a distinct non-OS core |
| 8.3 | Auto-skip core 0 | **DONE** | All sweep scripts start cores at 1 |

---

## 9. Performance metrics — LRP05 Table 3

LRP05 Research Activity 4 requires Table 3:

| Field | Definition | Source |
|-------|------------|--------|
| T_S | Serial runtime | Lab 01 result for the same n (constant per n) |
| T_P | Parallel runtime | Master-time average from Table 1 (this lab) |
| T_O | Parallel overhead | T_O = p · T_P − T_S, where p = t |
| S | Speedup | S = T_S / T_P |
| E | Efficiency | E = S / p = T_S / (p · T_P) |
| pT_P | Parallel cost | p · T_P |

**Status:** Table 3 is computed *after* sweeps, by the user, in a
spreadsheet using lab01.c's serial runtimes as T_S and the master times
from `lab05_table3.csv` as T_P. The sweep scripts produce all the inputs
needed; `LAB05_ANSWERS_DRAFT.md` provides the discussion template.

---

## 10. Summary checklist

```
[x] Tree-based 1MPB of full X (from lab04 base)
[x] Slave receives full X + work-range descriptor
[x] compute_mmt_strip — column-wise MMT with custom row stride
[x] Slave timer wraps ONLY the MMT compute
[x] Forwarder threads spawned BEFORE slave timer starts
[x] M1PR: slaves return T-strips up the tree, master rebuilds full T
[x] Master timer wraps distribute + recv + rebuild
[x] Verbose prints at every send/recv when n ≤ 32 or file mode
[x] Cross-platform (Winsock vs POSIX socket shims)
[x] Core affinity for slave processes
[x] Sweep scripts capture both master time AND slave-max time per run
[ ] Lab 05 Table 1 / 2 / 3 filled in from real swarm runs
[ ] Figure 1 (3D plot of n, t, master time) for Research Activity 2
[ ] Figure 2 (3D plot of n, t, slave-max time) for Research Activity 3
[ ] Performance metrics paragraph (superlinearity, cost-optimality)
```

---

## 11. Implementation log

### Files created (lab05 deliverables)

- `lab05.c` (577 lines) — single source for master + slave
- `table1.sh` / `table2.sh` / `table3.sh` — sweep automations; each writes
  a CSV with `n,t,run,master_time,slave_max`
- `resume_table3.sh` / `resume_table3_v2.sh` — partial-failure resume
- `run_lab05.ps1` / `run_lab05_affinity.ps1` / `run_lab05_multipc.ps1` —
  Windows-side equivalents
- `LAB05_*.md` — documentation suite (this file plus run guide, code
  walkthrough, tree diagram, presentation, paper drafts)

### Key design decisions and why

1. **Approach C (full-X broadcast + column work-partition).** Removes
   cross-slave reductions; each slave's MMT is self-contained.
2. **Subtree-wide T-strip buffer.** A forwarding slave allocates one
   `recv_n × col_count` float buffer; its own MMT writes into the leftmost
   `my_col_count` columns via the `strip_stride` parameter, and child
   strips are `memcpy`'d in at the correct offset. No double allocation.
3. **Forwarder threads before timer.** LRP05 explicitly excludes
   communication from the slave timer; spawning child workers early lets
   their TCP setup overlap with the slave's own MMT compute window.
4. **M1PR reuses the same tree shape.** Distribute and reduce travel the
   same edges; each subtree returns one contiguous strip per join.
5. **Slave reports `time elapsed: %.6f seconds` in the same wording as the
   master.** A single regex (`grep -oP 'time elapsed:\s+\K[\d.]+'`)
   captures both, so sweep scripts share extraction code.
