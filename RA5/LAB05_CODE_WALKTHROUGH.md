# Lab 05 — Code Walkthrough of `lab05.c`

A function-by-function reading of the source, and the Q&A drill at the end.
All line numbers refer to `lab05.c` (577 lines total).

---

## 0. Big Picture (one paragraph)

`lab05.c` extends `lab04_v3.c` from a pure distribution program to a
**distribute → compute → reduce** program. The same single binary plays
master (`s = 0`) or slave (`s ≥ 1`). The master tree-broadcasts the **full
matrix X** to every slave (1MPB), each slave computes the **column-wise
Min-Max Transformation** for its assigned column range only, and the
slaves return their T-strips up the tree to the master in a many-to-one
personalized reduction (M1PR). The master rebuilds full T from the
returned strips. The master timer wraps the whole distribute → reduce
window; each slave timer wraps **only** the MMT compute window.

---

## 1. Lines 1–14 — Header Comment

```c
/*
Charles Andrei P. De los Reyes
2023-15797
B-3L

Lab 05: Distributed Min-Max Transformation (column-wise)
- Master 1MPB-broadcasts the full matrix X to slaves via tree (O(log t)).
- Each slave computes MMT on its assigned column range only.
- Slaves M1PR-reduce their T-strips back up the same tree.
- Master rebuilds the full T.

Master timer: full distribute -> rebuild round-trip (Table 1).
Slave  timer: own MMT compute window only (Table 2 — report max across slaves).
*/
```

The block comment doubles as documentation — it tells you exactly what
the timers measure, which is the most-asked clarifying question.

---

## 2. Lines 16–42 — Includes, platform shim, print lock

```c
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <float.h>          // FLT_MAX

#ifdef _WIN32
#include <winsock2.h>
...
#else
#include <sys/socket.h>
...
#define SOCKET int
...
#endif

static pthread_mutex_t print_lock = PTHREAD_MUTEX_INITIALIZER;
```

Same Winsock/POSIX shim as lab04. The new include is `<float.h>` for
`FLT_MAX` used by `compute_mmt_strip`. The `print_lock` serializes
diagnostic prints when multiple threads (master sends + master prints,
or slave-self compute + slave forwarder threads) want stdout at once.

---

## 3. Lines 44–62 — `send_all` / `recv_all`

Identical to lab04. Loop until exactly `len` bytes are moved; `-1` on
short-read or short-write.

**Why it matters at n = 16000:** the full X is 1.024 GB. No single send
or recv moves that much in one shot — the loop is required for
correctness, not optimization.

---

## 4. Lines 64–70 — `compute_size_for_range`

```c
int compute_size_for_range(int n, int t, int first, int count) {
    int base = n / t, rem = n % t, total = 0;
    for (int i = first; i < first + count; i++)
        total += base + (i < rem ? 1 : 0);
    return total;
}
```

For lab04 this counted **rows**; for lab05 it counts **columns**. The math
is the same (n / t with the first `rem` slaves getting one extra), only
the dimension being partitioned changed. Used during tree split to know
how many columns belong to each subtree.

---

## 5. Lines 72–96 — `compute_mmt_strip` (the kernel)

```c
void compute_mmt_strip(const int *X, int n, int col_first, int col_count,
                       float *T_strip, int strip_stride) {
    for (int j = 0; j < col_count; j++) {
        int abs_c = col_first + j;
        float vmin = FLT_MAX, vmax = -FLT_MAX;
        for (int r = 0; r < n; r++) {
            float v = (float)X[r * n + abs_c];
            if (v < vmin) vmin = v;
            if (v > vmax) vmax = v;
        }
        float denom = vmax - vmin;
        if (denom != 0.0f) {
            for (int r = 0; r < n; r++) {
                float v = (float)X[r * n + abs_c];
                T_strip[r * strip_stride + j] = (v - vmin) / denom;
            }
        } else {
            for (int r = 0; r < n; r++)
                T_strip[r * strip_stride + j] = 0.0f;
        }
    }
}
```

**The kernel that lab05 adds over lab04.** Two-pass per column:

1. Walk the column once to find `vmin` and `vmax`.
2. Walk again to write `(v − vmin) / (vmax − vmin)` into `T_strip`.

**`strip_stride` parameter — the trick that avoids double allocation.**
Rather than writing into a tight `n × my_col_count` buffer (and copying
later into the subtree-wide buffer), we let the caller pass any stride.
A forwarding slave writes its own MMT into the **leftmost** part of the
subtree-wide `recv_n × col_count` buffer by passing
`strip_stride = col_count`. Children's strips are later memcpy'd into the
remaining slots at the right column offsets. No extra allocation, no copy.

**Constant-column edge case (`denom == 0`):** if every value in the
column is identical, the MMT is undefined. We write 0.0 — same convention
lab01.c uses. With `rand() % 100 + 1` matrices this never triggers in
practice but it's defensive.

---

## 6. Lines 98–108 — `TArgs` struct

```c
typedef struct {
    int *X;                    // full matrix to send (n*n ints) — shared, not freed by worker
    int n;
    int col_first, col_count;  // subtree's column-range (work assignment)
    int first_slave, num_slaves;
    int t_id;                  // 1-based slave id of subtree root (display only)
    char ip[64];
    int port;
    int success;
    float *returned_strip;     // worker mallocs n*col_count floats; caller frees
} TArgs;
```

vs lab04's `TArgs`:
- `int *sub` (row pointer) → `int *X` (whole matrix pointer).
- `rows, start_row` → `col_first, col_count` (column-range work).
- New: `float *returned_strip` for the M1PR direction. The worker mallocs
  it after recv'ing from its child; the caller frees after consuming.

`X` is **shared** — both master and forwarding slaves point all their
`TArgs[i].X` at the same buffer. No copy, no per-thread duplication.

---

## 7. Lines 110–159 — `worker_func`

The thread body that runs on each parent → child connection. Used by the
master (master → direct child) and by intermediate slaves (forwarding to
sub-children). Same function for both roles, like lab04.

### 7a. Open socket, connect with retry

```c
SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
...
for (int i = 0; i < 30 && !connected; i++) {
    if (connect(sock, ...) == 0) connected = 1;
    else Sleep(500);
}
```

15 s of `connect` retries (30 × 500 ms). The child slave may not have
reached `accept()` yet when the parent tries to connect.

### 7b. Send 5-int metadata

```c
send_all(sock, (char *)&a->n, sizeof(int));
send_all(sock, (char *)&a->col_first, sizeof(int));
send_all(sock, (char *)&a->col_count, sizeof(int));
send_all(sock, (char *)&a->first_slave, sizeof(int));
send_all(sock, (char *)&a->num_slaves, sizeof(int));
```

Five ints describe the subtree's work. The receiving slave uses
`(col_first, col_count)` to know its **subtree's** column range, and
`(first_slave, num_slaves)` to derive its **own** column range and to
plan its own forwarding split.

### 7c. Send full X bytes

```c
send_all(sock, (char *)a->X, (size_t)a->n * a->n * sizeof(int));
```

Full X every hop (Approach C). At n = 16000 this is 1.024 GB per edge,
which is why the swarm runs are bandwidth-bound. The benefit: no
cross-slave reductions, every slave can compute its MMT independently
without coordinating with anyone.

### 7d. Per-hop print (`n ≤ 32` only)

```c
if (a->n <= 32) {
    pthread_mutex_lock(&print_lock);
    printf("[-> Slave %d] SENT X + work [cols %d..%d] (subtree of %d slave(s))\n", ...);
    fflush(stdout);
    pthread_mutex_unlock(&print_lock);
}
```

`n ≤ 32` keeps benchmark runs silent (otherwise we'd dump the full
matrix every hop). The lock prevents output from this thread interleaving
with output from sibling threads or from the master's own prints.

### 7e. Recv T-strip (the M1PR direction)

```c
size_t strip_bytes = (size_t)a->n * a->col_count * sizeof(float);
a->returned_strip = (float *)malloc(strip_bytes);
if (a->returned_strip && recv_all(sock, (char *)a->returned_strip, strip_bytes) >= 0)
    a->success = 1;
```

Wait for the subtree's combined T-strip to come back, allocate exactly
`n × col_count` floats, recv into it. The caller frees `returned_strip`
after copying its contents into the larger T (master) or `subtree_strip`
(forwarder).

This is what makes `worker_func` a full **distribute + reduce** edge:
each thread sends X downstream and receives T-strip upstream.

---

## 8. Lines 161–314 — `run_master`

The master role: build X, plan tree, distribute, wait for T-strips,
rebuild T.

### 8a. Parse config (lines 163–179)

Same as lab04 — ignore `master` lines, collect `slave ip port` triples.

### 8b. Build / read X (lines 182–203)

File mode reads `n` then `n²` integers separated by commas or whitespace
(matches the lab04 input format). Random mode allocates and fills with
`rand() % 100 + 1`.

`int *X = malloc(n*n*sizeof(int))` — single contiguous block, row major.
Required so a single `send_all(X, n*n*sizeof(int))` works.

### 8c. Print X and slave assignments (lines 205–219)

Verification output gated by `n ≤ 32 || file_mode`. Each slave's column
assignment is computed once for printing; the actual work split is done
by the tree loop next.

### 8d. Build the tree (lines 221–260)

Same recursive-halving loop as lab04, but partitioning **columns**:

```c
int rem_first = 0, rem_count = t;
int rem_col_first = 0, rem_col_count = n;

while (rem_count > 0) {
    if (rem_count == 1) {
        // leaf — push the remaining range to slave[rem_first]
        ...
        break;
    }
    int left_count = rem_count / 2;
    int right_count = rem_count - left_count;
    int right_first = rem_first + left_count;
    int left_cols = compute_size_for_range(n, t, rem_first, left_count);
    int right_cols = rem_col_count - left_cols;

    // delegate the right half to slave[right_first] with its full column range
    t_args[num_children].col_first = rem_col_first + left_cols;
    t_args[num_children].col_count = right_cols;
    t_args[num_children].first_slave = right_first;
    t_args[num_children].num_slaves = right_count;
    ...
    num_children++;

    // recurse on the left half (still master's responsibility)
    rem_count = left_count;
    rem_col_count = left_cols;
}
```

For t = 4: produces 3 direct children (Slave 1 leaf, Slave 2 leaf,
Slave 3 with subtree {3,4}). For t = 16: produces 4 direct children
(Slave 1, Slave 2, Slave 3, Slave 5 — and Slave 5 owns subtree {5..16}).

### 8e. Sort children by t_id (lines 265–269)

Cosmetic only — makes the SENT prints appear in slave-id order rather
than reverse-tree order.

### 8f. Allocate T (lines 271–272)

`float *T = malloc(n*n*sizeof(float))` — same row-major layout as X but
floats. The master will fill T column-by-column from each child's
returned strip.

### 8g. The timed window (lines 274–301)

```c
clock_gettime(CLOCK_MONOTONIC, &start);

pthread_t threads[64];
for (int i = 0; i < num_children; i++)
    pthread_create(&threads[i], NULL, worker_func, &t_args[i]);

for (int i = 0; i < num_children; i++) {
    pthread_join(threads[i], NULL);
    if (t_args[i].success && t_args[i].returned_strip) {
        int cf = t_args[i].col_first, cc = t_args[i].col_count;
        for (int r = 0; r < n; r++)
            memcpy(&T[r * n + cf], &t_args[i].returned_strip[r * cc],
                   (size_t)cc * sizeof(float));
        free(t_args[i].returned_strip);
        ...
    }
}

clock_gettime(CLOCK_MONOTONIC, &end);
double time_elapsed = ...;
printf("time elapsed: %.6f seconds\n", time_elapsed);
```

**What's inside the timer:**
- Spawning all child worker threads
- Each thread's connect + send-X + recv-T-strip
- Joining each thread
- `memcpy`ing each strip into T's correct column slice

**What's outside the timer:**
- Config parsing
- X generation / file read
- T allocation
- T print
- `free(X)`, `free(T)`

This corresponds exactly to LRP05 spec item 2: "the master process must
take note of the `time_before` before distributing the matrix X to the
different slave processes and then take note of the `time_after` after the
master has rebuilt the full matrix T."

### 8h. Print T and free (lines 303–313)

T is printed only when `n ≤ 32 || file_mode`. Always `free(X)` and `free(T)`.

---

## 9. Lines 316–547 — `run_slave`

The slave's role: optional core pin, listen for parent, recv X + work
descriptor, spawn forwarders, compute MMT (timed), reduce children's
strips, send subtree strip back up.

### 9a. Detect core count, validate, optionally pin (lines 319–347)

```c
num_cores = ...;
if (core_id >= num_cores) error("out of range");
if (core_id >= 0) {
    #ifdef _WIN32
        SetProcessAffinityMask(GetCurrentProcess(), (DWORD_PTR)1 << core_id);
    #else
        cpu_set_t cpuset; CPU_ZERO(&cpuset); CPU_SET(core_id, &cpuset);
        sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
    #endif
}
```

Detection prevents the common "I tried core 8 but my laptop only has 4
cores" error. Pinning happens before any other work.

### 9b. Read config, identify self (lines 349–366)

Same as lab04: walk the slave list, match port to find `slave_id`. Need
the full slave list because forwarding slaves connect to their children
by IP/port.

### 9c. Listen + accept (lines 368–382)

Standard TCP server pattern with `SO_REUSEADDR`. Block on `accept()` for
the parent.

### 9d. Recv metadata + full X (lines 384–396)

Five ints, then `n × n × sizeof(int)` bytes. After this returns, this
slave has a full copy of X in its address space.

### 9e. Derive own column range (lines 398–404)

```c
int my_col_first = col_first;
int my_col_count = recv_n / t + (first_slave < recv_n % t ? 1 : 0);
```

`my_col_first` is the leftmost column of the subtree (which is **this
slave's own** column range, because the tree split keeps the lowest IDs
on the left). `my_col_count` is computed via the n/t + remainder rule
using the slave's own ID (`first_slave`) — same formula the master uses.

### 9f. Print received X (lines 406–415)

Verification output, gated by `recv_n ≤ 32`.

### 9g. Build forwarders, **launch BEFORE timer** (lines 417–466)

```c
if (num_slaves > 1) {
    // recursive halving on (recv_n, col_first, col_count) for child subtrees
    while (rem_count > 1) {
        ...
        child_args[num_children].col_first = rem_col_first + left_cols;
        child_args[num_children].col_count = right_cols;
        ...
        num_children++;
        rem_count = left_count;
        rem_col_count = left_cols;
    }

    // sort cosmetically by t_id, then launch
    for (int i = 0; i < num_children; i++) {
        printf("[Slave %d] Forwarding to Slave %d: ...\n", ...);
        pthread_create(&child_threads[i], NULL, worker_func, &child_args[i]);
    }
}
```

**Critical timing decision: forwarders are spawned BEFORE
`clock_gettime(start)`.** LRP05 spec item 3 says the slave timer covers
*only* the MMT compute. Spawning the threads early lets their TCP
handshake and X bytes travel down the tree concurrently with this
slave's compute, and keeps that communication out of the timed window.

### 9h. Allocate `subtree_strip`, COMPUTE WINDOW (lines 469–482)

```c
size_t strip_bytes = (size_t)recv_n * col_count * sizeof(float);
float *subtree_strip = (float *)malloc(strip_bytes);

// === COMPUTE WINDOW (slave timer) ===
struct timespec start, end;
clock_gettime(CLOCK_MONOTONIC, &start);

compute_mmt_strip(X, recv_n, my_col_first, my_col_count, subtree_strip, col_count);

clock_gettime(CLOCK_MONOTONIC, &end);
double time_elapsed = ...;
printf("time elapsed: %.6f seconds\n", time_elapsed);
// === end timer ===
```

`subtree_strip` is sized for the **whole subtree** (`recv_n × col_count`
floats). `compute_mmt_strip` is called with `strip_stride = col_count`,
so my own MMT writes into the **leftmost** `my_col_count` columns of the
buffer. Children's strips will be `memcpy`'d in afterward at the right
column offsets — no extra allocation, no copy.

The timer brackets exactly one function call. Anything else (allocation,
forwarding, recv, send-back) is excluded.

### 9i. Print my MMT for verification (lines 484–495)

Gated by `recv_n ≤ 32`. Prints only the leftmost `my_col_count` columns
(my own work).

### 9j. Reduce — collect children's strips (lines 498–519)

```c
for (int i = 0; i < num_children; i++) {
    pthread_join(child_threads[i], NULL);
    if (child_args[i].success && child_args[i].returned_strip) {
        int offset = child_args[i].col_first - col_first;
        int cc = child_args[i].col_count;
        for (int r = 0; r < recv_n; r++)
            memcpy(&subtree_strip[r * col_count + offset],
                   &child_args[i].returned_strip[r * cc],
                   (size_t)cc * sizeof(float));
        free(child_args[i].returned_strip);
        ...
    }
}
```

Each child's strip is `recv_n × cc` floats. The destination offset within
`subtree_strip` is `child.col_first - col_first` (because subtree_strip's
column 0 corresponds to absolute column `col_first`). Row by row,
`memcpy` the strip into the right slot.

### 9k. Print subtree strip (lines 521–532)

Diagnostic for forwarding slaves only — leaves don't have anything to
combine.

### 9l. Send subtree strip up (line 535)

```c
send_all(csock, (char *)subtree_strip, strip_bytes);
```

The whole subtree-wide buffer travels back up to this slave's parent in
one block. The parent's `worker_func` receives it as the `returned_strip`
field of the matching `TArgs`.

### 9m. Free, close, return (lines 542–546)

`free(subtree_strip); free(X); closesocket(csock); closesocket(srv);`

---

## 10. Lines 549–577 — `main`

Argument dispatch identical to lab04:

```c
if (argc < 5) {
    printf("Usage: %s <n> <p> <s> <config_file> [input_file|core_id]\n", argv[0]);
    return 1;
}

int n = atoi(argv[1]);
int p = atoi(argv[2]);
int s = atoi(argv[3]);
char *config_file = argv[4];

#ifdef _WIN32
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

if (s == 0) {
    char *input_file = (argc >= 6) ? argv[5] : NULL;
    run_master(n, p, config_file, input_file);
} else {
    int core_id = (argc >= 6) ? atoi(argv[5]) : -1;
    run_slave(n, p, config_file, core_id);
}
```

The 5th positional argument is reused: master-mode → input file path,
slave-mode → core ID for affinity. `WSAStartup` / `WSACleanup` only on
Windows; no-op on Linux.

---

## 11. End-to-End Walkthrough (concrete example, t = 4, n = 8)

```
config:
  master 127.0.0.1 5000
  slave  127.0.0.1 5001
  slave  127.0.0.1 5002
  slave  127.0.0.1 5003
  slave  127.0.0.1 5004
```

1. Slaves 1–4 start; each binds + listens.
2. Master starts, allocates X (8×8 ints), prints it and the 4 column
   assignments (Slave 1 → cols 0–1, ..., Slave 4 → cols 6–7).
3. Master tree loop builds 3 direct children:
   - Slave 1, cols 0–1, num_slaves=1 (leaf)
   - Slave 2, cols 2–3, num_slaves=1 (leaf)
   - Slave 3, cols 4–7, num_slaves=2 (subtree {3,4})
4. Master allocates T, starts timer, spawns 3 worker threads.
5. Each thread sends metadata + full X to its slave.
6. Slaves 1, 2, 4 receive and (because num_slaves=1) skip forwarding
   entirely. They just compute MMT on their assigned column range and
   send the strip back. Slave 4 was reached via Slave 3's forwarder.
7. Slave 3 receives 4 cols, num_slaves=2 → enters forwarding block, splits
   to {3} and {4}, spawns one forwarder for Slave 4. Spawns BEFORE timer.
8. Slave 3's compute window: `compute_mmt_strip(X, 8, 4, 2, subtree_strip, 4)`
   — writes columns 4–5 into subtree_strip[0..7][0..1].
9. Slave 3 stops its timer, prints `time elapsed`, joins its forwarder
   thread, copies Slave 4's returned strip into `subtree_strip[r][2..3]`.
10. Slave 3 sends the assembled `8 × 4` float buffer up to the master.
11. Master joins each thread, copies each child's `returned_strip` into
    T at the right column offset:
    - Slave 1's strip → T[r][0..1]
    - Slave 2's strip → T[r][2..3]
    - Slave 3's strip (4 cols) → T[r][4..7]
12. Master stops timer, prints `time elapsed`. Master prints T.

**Slave 4 was never reached directly by the master** — it was reached via
Slave 3, exactly as in lab04. The compute does not happen on the master.

---

## 12. Anticipated Questions and Answers

### Approach choice

**Q: Why does every slave receive the FULL X, not just its column strip?**
A: MMT is a per-column normalization. If I gave slave k only its column
strip, it could compute MMT on its own columns just fine — that's
Approach A. I chose Approach C (full X to everyone) because the LRP05
PDF says "1MPB distributed parts of the matrix X to different slave
processes which can be processed concurrently by different machines."
The phrasing maps most naturally onto a tree broadcast of the full X
where each slave is responsible for a different column range. It also
keeps each slave's MMT fully self-contained — no cross-slave min/max
reduction at any point.

**Q: Why not row-strip (Approach B)?**
A: Row-strip means each slave only has a slice of every column. To
compute the MMT of column k, slaves would need to exchange partial
min/max results across the network — extra round trips. With the
column work-partition + full-X broadcast, every slave has the full
column k's data and computes its MMT in isolation.

**Q: Isn't replicating X wasteful?**
A: Yes, in memory. ~1 GB per drone at n = 16000. The trade is that the
compute side is embarrassingly parallel and needs zero coordination.
Given 2 GB drones, it fits with margin to spare for n ≤ 16000.

### Timer semantics

**Q: What exactly does the master timer measure?**
A: From right before `pthread_create` on the first child, to right after
`pthread_join` and `memcpy` of the last child's returned strip. So it
covers the full distribute → compute (in parallel on slaves) → reduce →
rebuild round trip.

**Q: What exactly does the slave timer measure?**
A: ONLY the call to `compute_mmt_strip`. Spawning forwarders, recv'ing X,
allocating buffers, joining children, send-back to parent — all of these
happen outside the timer. This matches LRP05 spec item 3 verbatim.

**Q: Why are forwarder threads spawned BEFORE the timer?**
A: Two reasons. (1) The spec excludes communication from the slave
timer; spawning forwarders is communication. (2) Latency hiding: while
this slave computes its own MMT, its forwarders are pushing X to the
sub-children in parallel. Both happen during what would otherwise be
the slave's idle compute window.

### Reduce / M1PR

**Q: Why does each slave send a SUBTREE-wide strip up, not just its own?**
A: Because the parent only opened one socket to me. If I sent only my
own strip, the parent would need separate connections to each
grandchild. Instead, I aggregate (own + children's strips) into one
contiguous block and send that on the existing edge. The tree shape of
the reduce mirrors the distribute, so the master's number of returned
strips equals its number of direct children, not t.

**Q: Where exactly does the assembly happen?**
A: In each forwarding slave, after its own MMT is done. The slave's own
MMT was already written into the leftmost columns of `subtree_strip`
during `compute_mmt_strip`. Children's strips are then memcpy'd into the
remaining slots based on their `col_first - col_first` offset.

### Tables

**Q: Why report the MAX of slave times for Table 2, not the average?**
A: Because the master cannot finish until **every** slave has finished.
The slowest slave bounds the parallel time. Averaging would
underestimate the true compute cost — if 15 slaves take 1 s and one
takes 5 s, the master is still waiting at second 4.999. The max captures
this critical-path behavior.

**Q: Where does Table 3 (T_O, S, E, pT_P) come from?**
A: Computed in a spreadsheet from Table 1 averages and the lab01.c
serial runtime. T_S = lab01 serial runtime for the same n. T_P =
average master_time. T_O = p · T_P − T_S. S = T_S / T_P. E = S / p =
T_S / (p · T_P). pT_P = p · T_P.

### Code specifics

**Q: What does `strip_stride` do in `compute_mmt_strip`?**
A: It tells the kernel how wide a row is in the destination buffer. A
forwarding slave passes `strip_stride = col_count` (the subtree width)
even though it only writes `my_col_count` columns. This means columns
1..my_col_count-1 are written, columns my_col_count..col_count-1 are
left untouched (will be filled by children's memcpy). One buffer, no
copy.

**Q: Why a `pthread_mutex_t print_lock`?**
A: Multiple threads (master sends + master prints, slave forwarder
threads + slave's own prints) can all want stdout at once. Without the
lock, lines from different threads can interleave mid-output. The
lock + `fflush` makes each block of prints atomic at the line level.

**Q: What's the type of T at the master?**
A: `float *T` (n × n floats, row-major). Strips received from children
are also floats. The conversion from `int` X to `float` happens inside
`compute_mmt_strip` on the slave side (`(float)X[r * n + abs_c]`).

**Q: What if n is not divisible by t?**
A: Same as lab04: `base = n / t`, `rem = n % t`, the first `rem` slaves
get `base + 1` columns, the rest get `base`. Sum to n exactly.

**Q: What if a slave dies mid-run?**
A: Its `returned_strip` never arrives, the parent's `recv_all` fails or
hangs. The `success` flag stays 0. The sweep scripts use `timeout 600s`
on the master, so a hung run is recorded as ERROR and the next
combination starts.

### Improvements

**Q: What would you change if you did this again?**
A: Two things. (1) For very large n, switch to Approach A
(column-strip) to avoid replicating X. The trade-off is more
complicated tree-routing because the master has to pre-split X. (2)
Overlap the M1PR with downstream MMT compute: as soon as a forwarder
slave finishes its own MMT, start streaming its strip up rather than
waiting for children. The current code does children-first, which is
correct but can leave the parent's NIC idle.

**Q: Could you have used MPI?**
A: Yes — `MPI_Bcast` for distribute, `MPI_Gather` or `MPI_Gatherv` for
reduce, `MPI_Comm_split` for subtree management. The lab spec called for
raw sockets to teach the protocol layer; MPI would hide most of what's
interesting here.
