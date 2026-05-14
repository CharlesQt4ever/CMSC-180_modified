# Lab 04 Implementation Analysis

## Professor's Instructions vs Current Implementation

Based on the lecture notes from the professor, this document maps each
requirement to its current status and outlines what still needs work.

---

### LEGEND

- **DONE** = Already implemented and working in `lab04.c`
- **PARTIAL** = Partially done, needs improvement
- **TODO** = Not yet implemented

---

## 1. Command-Line Interface & Input

| # | Requirement | Status | Notes |
|---|-------------|--------|-------|
| 1.1 | Provide own port number | **DONE** | `argv[2]` = port `p` |
| 1.2 | Preferably command line input | **DONE** | `lab04 <n> <p> <s> <config_file> [input_file\|core_id]` |
| 1.3 | `s = 0` for master, `s = 1` for slave | **DONE** | `argv[3]` determines role |
| 1.4 | 16 computers - use different ports | **DONE** | `config.txt` supports up to `MAX_SLAVES = 64` |
| 1.5 | Non-randomized mode (read from input file) | **DONE** | Master `argv[5]` = input file path |
| 1.6 | Random 1-100 integers (like previous labs) | **DONE** | `(rand() % 100) + 1` |
| 1.7 | `t` = number of slaves (from config file) | **DONE** | `t = config.num_slaves` |

---

## 2. Matrix Distribution (Master Side)

| # | Requirement | Status | Notes |
|---|-------------|--------|-------|
| 2.1 | Master creates matrix | **DONE** | Random or file-based |
| 2.2 | Row-wise division: `n/t x n` submatrices | **DONE** | `base_rows = n / t`, remainder distributed |
| 2.3 | Handle remainder when `n % t != 0` | **DONE** | First `rem` slaves get 1 extra row each |
| 2.4 | Master timing = basis for table | **DONE** | Timer wraps send + ack-receive |
| 2.5 | One-to-many personalized broadcast | **DONE** | Master sends unique submatrix to each slave |
| 2.6 | O(log t) tree-based distribution (slave propagation) | **TODO** | Currently O(t) sequential sends. Professor says O(n) gets deductions. See Section 7 below. |

---

## 3. Slave Side

| # | Requirement | Status | Notes |
|---|-------------|--------|-------|
| 3.1 | Slave reads config for master IP | **DONE** | `read_config()` parses master entry |
| 3.2 | Slave waits for master to initiate | **DONE** | `listen()` + `accept()` |
| 3.3 | Slave `time_before` starts when master initiates | **DONE** | Timer starts right after `accept()` |
| 3.4 | Slave receives submatrix | **DONE** | Receives `n`, `rows_count`, `start_row`, then data |
| 3.5 | Slave sends "ack" string back | **DONE** | `send_all(csock, "ack", 3)` |
| 3.6 | Slave `time_after` and output `time_elapsed` | **DONE** | Printed at end |
| 3.7 | Add slave computation (for next exercise) | **TODO** | Slaves currently only receive + ack. Professor says to add "functionality 4" (likely MMT computation on received submatrix) as groundwork for Lab 05. No timing table needed for this. |

---

## 4. Verification / Printing (CRITICAL - Grading Basis)

> **Professor emphasis:** Item 6 under "write the main program lab04" is the
> most important -- it is a primary basis for checking and grading. Each slave
> must print its received submatrix to verify correctness.

| # | Requirement | Status | Notes |
|---|-------------|--------|-------|
| 4.1 | Master prints full matrix before distribution | **DONE** | Always prints in file mode; prints when `n <= 32` in random mode |
| 4.2 | Master prints each submatrix as it sends | **DONE** | Shows slave ID, row range, and submatrix content |
| 4.3 | Each slave prints its received submatrix | **DONE** | Labeled `[Slave X] Received submatrix:` with full content |
| 4.4 | Verification: slave output matches master's sent data | **DONE** | Can compare master's "Sending" print vs slave's "Received" print side-by-side across terminals |

**Verified working output with `input.txt` (3x3 matrix, 2 slaves):**
```
MASTER:                                 SLAVE 1:                              SLAVE 2:
Slave 1 Submatrix (rows 0 to 1)        Received submatrix:                   Received submatrix:
11  12  13                              11  12  13                            17  18  19
14  15  16                              14  15  16

Slave 2 Submatrix (rows 2 to 2)
17  18  19
```

---

## 5. Core Affinity

| # | Requirement | Status | Notes |
|---|-------------|--------|-------|
| 5.1 | Pin slave to specific core | **DONE** | `SetProcessAffinityMask()` on Windows via `argv[5]` = core_id |
| 5.2 | Leave one core for OS/other processes | **PARTIAL** | Lab03 does this automatically (skips core 0). Lab04 requires manual core_id input -- user must avoid core 0 themselves. Should automate or document clearly. |
| 5.3 | Core-affine runs on same PC, separate terminals | **DONE** | Each slave instance gets a different port + core_id |
| 5.4 | Different core per slave | **DONE** | User specifies: `lab04 n 5001 1 config.txt 1` (core 1), `lab04 n 5002 1 config.txt 2` (core 2), etc. |

---

## 6. Required Tables & Report

The professor requires **three separate timing tables**, all from master's perspective.

| Table | Description | Status |
|-------|-------------|--------|
| **Table 1** | Single PC, **without** core affinity | **TODO** - Need benchmark script |
| **Table 2** | Single PC, **with** core affinity | **TODO** - Need benchmark script |
| **Table 3** | Different PCs (ICS Swarm), without core affinity | **TODO** - Need config + SSH setup |

**Table format (from lab instructions):**
```
n       t       Run 1       Run 2       Run 3       Average
4,000   2
4,000   4
4,000   8
4,000   16
8,000   2
8,000   4
8,000   8
8,000   16
16,000  2
16,000  4
16,000  8
16,000  16
```

**Additional report requirements:**
- Explain the results across all 3 tables and discuss differences
- For question 5: identify as one-to-many personalized broadcast
- Master specs can differ from slaves, but all slaves must have same specs

---

## 7. Distribution Efficiency (CRITICAL - Deduction Risk)

> **Professor warning:** "If the running time of the program is O(n) there
> will be deductions."

### Current: Sequential O(t) Distribution
```
Master ---> Slave 1  (send submatrix 1)
Master ---> Slave 2  (send submatrix 2)
Master ---> Slave 3  (send submatrix 3)
...
Master ---> Slave t  (send submatrix t)

Total sends from master: t (sequential)
```

The master currently connects and sends to each slave **one at a time**.
When `t` is large, this is O(t) communication steps from the master.

### Required: O(log t) Tree-Based Distribution
The professor mentions two allowed approaches:

**Approach A -- Master sends to all (current, one-to-many personalized):**
This is acceptable per "In number 5 use one-to-many personalized broadcast",
but should ideally use **concurrent sends** (threads or non-blocking I/O)
rather than sequential to avoid O(t) bottleneck.

**Approach B -- Slave-propagated tree broadcast:**
```
Step 1: Master ---> Slave 1 (full matrix or half)
Step 2: Master ---> Slave 2,  Slave 1 ---> Slave 3  (parallel)
Step 3: Master ---> Slave 4,  Slave 1 ---> Slave 5, Slave 2 ---> Slave 6, Slave 3 ---> Slave 7

Depth: O(log t)
```
Master sends the full matrix to one slave, that slave splits and forwards
to other slaves. This achieves O(log t) distribution depth.

### Recommendation
Implement **Approach A with concurrent sends** as the primary method:
- Use threads on the master side to send to all slaves in parallel
- Each thread handles one slave connection
- This makes the wall-clock time O(1) send rounds instead of O(t)
- Still qualifies as one-to-many personalized broadcast
- Simpler than tree-based and matches the professor's answer for Q5

---

## 8. Priority Action Items

Ordered by importance (grading impact):

### HIGH PRIORITY

1. **Threaded master sends** -- Parallelize the master's send loop so all
   slaves receive their submatrices concurrently. This avoids the O(t)
   sequential penalty the professor warned about.

2. **Benchmark scripts for 3 tables** -- Create PowerShell scripts that:
   - Launch t slaves (ports 5001..5000+t)
   - Launch master with timing
   - Collect results into CSV
   - Run for n=4000,8000,16000 x t=2,4,8,16 x 3 runs each

3. **Slave MMT computation** -- Add optional MMT computation on the slave's
   received submatrix. Professor says this prepares for Lab 05. No timing
   table needed, but the functionality should exist.

### MEDIUM PRIORITY

4. **Core affinity automation** -- Auto-assign cores to slaves (like lab03
   does) rather than requiring manual core_id. Reserve core 0 for OS.

5. **Config files for each table** -- Prepare separate config files:
   - `config_local.txt` -- all slaves on 127.0.0.1 with different ports
   - `config_swarm.txt` -- slaves on different ICS machines

### LOW PRIORITY

6. **Tree-based distribution** -- Implement Approach B as an alternative
   mode. While the professor says it's "allowed," the concurrent
   one-to-many personalized broadcast (Approach A) is the expected answer
   for Q5.

---

## 9. Summary Checklist

```
[x] Port from command line
[x] Master/slave role selection (s=0/s=1)
[x] Config file with IPs and ports
[x] Random matrix 1-100
[x] Non-randomized file input mode
[x] Row-wise n/t x n distribution
[x] Remainder handling (n % t != 0)
[x] "ack" acknowledgement from slaves
[x] Slave timing starts at master connection
[x] Submatrix printing for verification (GRADING BASIS)
[x] Core affinity on slave (manual core_id)
[x] Concurrent/threaded master sends (avoid O(t) deduction)  <-- IMPLEMENTED
[x] Benchmark scripts for 3 timing tables                    <-- IMPLEMENTED
[x] Slave MMT computation (prep for Lab 05)                  <-- IMPLEMENTED
[x] Auto core assignment for slaves (in benchmark script)    <-- IMPLEMENTED
[ ] Report: explain 3 tables + differences
[ ] Report: identify one-to-many personalized broadcast
```

---

## 10. Implementation Log

### Changes Made (2026-04-15)

**1. Threaded Master Sends (`lab04.c`)**
- Added `#include <pthread.h>`
- Added `MasterSendArgs` struct and `master_send_worker()` thread function
- Rewrote `run_master()` to launch **all slave sends concurrently** via pthreads
- Each thread independently: connects, sends metadata + submatrix, receives ack
- Submatrix printing happens **before** threads launch (sequential, for display)
- Timed section only covers the parallel send/ack phase
- Compile with: `gcc -o lab04.exe lab04.c -lws2_32 -lpthread`

**2. Slave Local MMT Computation (`lab04.c`)**
- Added `compute_local_mmt()` function
- Converts received `int` submatrix to `float`, computes column-wise MMT
- Uses **local** min/max only (not global -- that's Lab 05)
- Prints result for small matrices (`n <= 32`)
- Called after receiving submatrix, before sending "ack"

**3. Benchmark Scripts**
- `run_lab04.ps1` -- Table 1: Single PC, no core affinity
- `run_lab04_affinity.ps1` -- Table 2: Single PC, with core affinity
- `run_lab04_multipc.ps1` -- Table 3: Different PCs (interactive, requires SSH)
- Scripts auto-generate config files for each `t` value
- Core affinity script assigns cores round-robin (1 through N-1, skipping core 0)
- All produce CSV output + formatted summary table
