# RA4 (`lab04_v3.c`) vs RA5Final (`lab05.c`) — Code Differences

This document lists every meaningful difference between `RA4/lab04_v3.c` (496 lines) and `RA5Final/lab05.c` (577 lines), and explains *why* each change was made.

The two programs share the same skeleton (TCP sockets, pthreads, log-tree distribution, core-affinity pinning), so this doc focuses on what RA5 **adds**, **changes**, or **removes**.

---

## High-level summary

| Aspect | RA4 (`lab04_v3.c`) | RA5 (`lab05.c`) |
|---|---|---|
| Goal | **1MPB** only — distribute matrix from master to t slaves over a log-tree | **1MPB + MMT + M1PR** — distribute → each slave computes column MMT → reduce results back |
| Partitioning | Splits matrix **by rows** (n/t rows per slave) | Splits matrix **by columns** (n/t cols per slave) |
| Slave does work? | No — slave only receives, forwards, and sends `"ack"` | **Yes** — slave runs MMT on its assigned columns |
| Return value from slave | 3-byte `"ack"` string | `n × col_count` floats (the slave's T-strip) |
| Master final state | Just times the distribution | Rebuilds full **T matrix** from received strips |
| Master timer measures | distribute + ack roundtrip | **distribute → compute → reduce** roundtrip (Table 1) |
| Slave timer measures | recv + forward + ack | **only the MMT compute window** (Table 2) |

---

## 1. File header / docstring

**RA4** (lines 1–5): Just author info.

**RA5** (lines 1–14): Adds a description of the algorithm:

```c
Lab 05: Distributed Min-Max Transformation (column-wise)
- Master 1MPB-broadcasts the full matrix X to slaves via tree (O(log t)).
- Each slave computes MMT on its assigned column range only.
- Slaves M1PR-reduce their T-strips back up the same tree.
- Master rebuilds the full T.

Master timer: full distribute -> rebuild round-trip (Table 1).
Slave  timer: own MMT compute window only (Table 2 — report max across slaves).
```

**Why:** Lab05 is no longer just a broadcast — the docstring documents the new round-trip pattern (1MPB out, M1PR back) and clarifies what each timer measures, which the lab spec asks for in Tables 1 and 2.

---

## 2. New include: `<float.h>`

**RA5** (line 22): adds `#include <float.h>`.

**Why:** Needed for `FLT_MAX`, used inside the new `compute_mmt_strip()` to seed the per-column min/max search.

---

## 3. `send_all` / `recv_all` signatures changed (`int` → `size_t`)

**RA4** (lines 35–53):
```c
int send_all(SOCKET sock, const char *buf, int len) { ... }
int recv_all(SOCKET sock,       char *buf, int len) { ... }
```

**RA5** (lines 44–62):
```c
int send_all(SOCKET sock, const char *buf, size_t len) { ... }
int recv_all(SOCKET sock,       char *buf, size_t len) { ... }
```

**Why:** RA5 transmits the **full n×n matrix X** to every direct child of the master (vs RA4 only sending each slave its own row-slice). For n = 16000 with `int` payload, that's 16000·16000·4 = **1.024 GB** — well past the `INT_MAX` signed-int byte counter ceiling. Changing to `size_t` keeps the byte counters safe for the larger transfers RA5 introduces.

---

## 4. Renamed helper: `compute_rows_for_range` → `compute_size_for_range`

**RA4** (lines 56–61):
```c
int compute_rows_for_range(int n, int t, int first, int count) { ... }
```

**RA5** (lines 65–70):
```c
int compute_size_for_range(int n, int t, int first, int count) { ... }
```

The body is identical — same `base = n/t, rem = n%t` partition math.

**Why:** In RA4 the function returned the number of **rows** assigned to a contiguous range of slaves. In RA5 the same arithmetic now describes **columns**. The rename matches the RA5 partitioning model so the reader doesn't get confused when the function is called with column meanings.

---

## 5. **NEW function:** `compute_mmt_strip()`

**RA5** (lines 72–96):
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

This function did not exist in RA4. **It is the heart of RA5.**

**What it does:**
- For each column in `[col_first, col_first + col_count)`:
  1. Scan all `n` rows to find that column's min and max.
  2. Apply the Min-Max normalization `T[r,c] = (X[r,c] − min_c) / (max_c − min_c)`.
  3. Write the result into `T_strip` at `[r * strip_stride + j]`.
- Handles the divide-by-zero edge case (constant column) by writing 0.0f.

**Why the `strip_stride` parameter exists:** A slave's `T_strip` buffer is sized for the entire **subtree** it owns (so it can also store T-strips returned by descendants). The slave's own MMT output goes into the leftmost `col_count` columns, but the row stride must equal the full subtree width so the descendants' strips can be `memcpy`'d in alongside without overlap.

**Why:** The lab spec (Activity 1–2) requires each slave to compute MMT on its assigned columns of `X`. RA4 had no compute step at all.

---

## 6. `TArgs` struct rewritten

**RA4** (lines 63–70):
```c
typedef struct {
    int *sub;                       // pointer into a row-slice of M
    int rows, n, start_row;         // row-partition geometry
    int first_slave, num_slaves, t_id;
    char ip[64];
    int port;
    int success;
} TArgs;
```

**RA5** (lines 98–108):
```c
typedef struct {
    int *X;                         // full matrix to send (shared, not freed by worker)
    int n;
    int col_first, col_count;       // column-partition geometry
    int first_slave, num_slaves;
    int t_id;
    char ip[64];
    int port;
    int success;
    float *returned_strip;          // worker mallocs n*col_count floats; caller frees
} TArgs;
```

**What changed:**
- **Removed:** `sub`, `rows`, `start_row` — RA4 needed these because each child got a row-slice carved out of `M`.
- **Added:** `X` — pointer to the **full matrix**; everyone gets the whole thing (1MPB).
- **Added:** `col_first`, `col_count` — describes the column range this subtree is responsible for computing.
- **Added:** `returned_strip` — receive buffer for the T-strip the worker thread will collect from its child during M1PR. The parent thread reads it after `pthread_join` and `free`s it.

**Why:** The data-flow direction reversed. In RA4, the worker only **pushes** rows down. In RA5, the worker **pushes the whole X down and pulls a T-strip back up**. The struct has to hold both directions.

---

## 7. `worker_func` rewritten

**RA4** (lines 73–124):
- Sends 5 ints of metadata: `n, rows, start_row, first_slave, num_slaves`.
- Sends `rows × n` ints of matrix data (only the slave's row-slice).
- Receives a 3-byte `"ack"` string back.

**RA5** (lines 111–159):
- Sends 5 ints of metadata: `n, col_first, col_count, first_slave, num_slaves`. (`rows`/`start_row` replaced with `col_first`/`col_count`.)
- Sends `n × n` ints of matrix data (the **entire** matrix).
- **Allocates** `n × col_count` floats and **receives the T-strip** into it.

Key code added in RA5:
```c
size_t strip_bytes = (size_t)a->n * a->col_count * sizeof(float);
a->returned_strip = (float *)malloc(strip_bytes);
if (a->returned_strip && recv_all(sock, (char *)a->returned_strip, strip_bytes) >= 0)
    a->success = 1;
```

**Why:**
- **Sending full X:** Each slave needs every row of its assigned columns to compute per-column min/max. There is no way to do MMT correctly with only a row-slice — you'd be normalizing against a *partial* column. So RA5 broadcasts the entire matrix.
- **Receiving a T-strip:** The slave's job is no longer just "I got it" — it has to return its computed MMT result. The 3-byte `"ack"` is replaced with the actual data payload (the M1PR step).

Also: RA4 printed a per-thread submatrix dump after sending (lines 104–115). RA5 replaces it with a single concise log line — fewer prints because the data being sent is now identical for every child (the full X), so dumping it n times would be noisy.

---

## 8. `run_master` differences

### 8a. Tree split now by columns

**RA4** (lines 197–232): tracks `rem_start_row` and `rem_rows`, halving the row count.

**RA5** (lines 224–260): tracks `rem_col_first` and `rem_col_count`, halving the column count. The `t_args[].sub = &M[...]` row-pointer arithmetic is gone — every child just gets `t_args[].X = X`.

**Why:** Whole-matrix broadcast means there is no per-child slice pointer. The "geometry" passed to each child is now (col_first, col_count) — work assignment, not data pointer.

### 8b. Allocates the result matrix `T`

**RA5** (lines 271–272):
```c
float *T = (float *)malloc((size_t)n * n * sizeof(float));
if (!T) { fprintf(stderr, "[Master] alloc T failed\n"); free(X); return 1; }
```

**Why:** The master now needs to assemble the final n×n MMT result. Allocated before the timer starts so allocation cost is excluded from the measurement.

### 8c. Reassembles `T` from returned strips

**RA4** (lines 253–260): just prints "Received 'ack' from Slave X".

**RA5** (lines 281–297):
```c
if (t_args[i].success && t_args[i].returned_strip) {
    int cf = t_args[i].col_first, cc = t_args[i].col_count;
    for (int r = 0; r < n; r++)
        memcpy(&T[r * n + cf], &t_args[i].returned_strip[r * cc], (size_t)cc * sizeof(float));
    free(t_args[i].returned_strip);
    ...
}
```

**Why:** Each direct child returns a strip covering its **whole subtree** of slaves. The master copies that strip into the correct column band of `T`, row by row. After all children are joined, `T` is fully populated.

### 8d. Prints `T` matrix for verification

**RA5** (lines 303–309): on small / file-mode runs, prints the full T matrix in `%.4f` format.

**Why:** Sanity check that distributed MMT matches the serial baseline (`lab01.c`). Compare the printed T to a serial-MMT reference run on the same input to confirm correctness.

### 8e. Thread arrays grew from `[32]` to `[64]`

**RA4** uses `pthread_t threads[32]; TArgs t_args[32];`.
**RA5** uses `[64]` for both, plus `slave_ips[64]` (already 64 in RA4).

**Why:** `64` matches the slave-array bound. The lab spec only goes up to t=16, so 32 was already enough — but bumping to 64 leaves headroom for future scaling without an off-by-one.

---

## 9. `run_slave` differences

### 9a. Receives the full X (not a row-slice)

**RA4** (lines 353–355):
```c
int sub_bytes = rows * recv_n * (int)sizeof(int);
int *sub = (int *)malloc(sub_bytes);
recv_all(csock, (char *)sub, sub_bytes);
```

**RA5** (lines 393–396):
```c
size_t X_bytes = (size_t)recv_n * recv_n * sizeof(int);
int *X = (int *)malloc(X_bytes);
recv_all(csock, (char *)X, X_bytes);
```

**Why:** Slave needs full columns to compute per-column min/max — see §7. Buffer is now n*n ints, not rows*n.

### 9b. Computes its own column range

**RA5** (lines 399–400):
```c
int my_col_first = col_first;
int my_col_count = recv_n / t + (first_slave < recv_n % t ? 1 : 0);
```

**Why:** Within the subtree it received, the slave itself owns the **leftmost slave's worth of columns**. The remainder belongs to descendants and gets forwarded.

### 9c. Allocates a "subtree strip" buffer

**RA5** (lines 469–471):
```c
size_t strip_bytes = (size_t)recv_n * col_count * sizeof(float);
float *subtree_strip = (float *)malloc(strip_bytes);
```

**Why:** This single buffer holds the **whole subtree's** T result — own MMT goes into the leftmost `my_col_count` columns; children's returned strips get `memcpy`'d into the offsets to the right. After everything is collected, the slave sends the entire `subtree_strip` up to its parent in one M1PR send.

### 9d. Slave timer wraps **only** `compute_mmt_strip()`

**RA4** (lines 341–342, 457–460): timer brackets the entire slave run — accept, recv, forward, ack.

**RA5** (lines 474–482):
```c
struct timespec start, end;
clock_gettime(CLOCK_MONOTONIC, &start);

compute_mmt_strip(X, recv_n, my_col_first, my_col_count, subtree_strip, col_count);

clock_gettime(CLOCK_MONOTONIC, &end);
double time_elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
printf("time elapsed: %.6f seconds\n", time_elapsed);
```

**Why:** The lab spec asks for **Table 2: max compute time across slaves**. The slave's measurement must therefore exclude communication. Putting `clock_gettime` immediately around the MMT call gives a clean compute-only window.

### 9e. Forwarders launched **before** the timer

**RA5** (line 455 comment + line 456):
```c
// launch forwarders BEFORE timer — communication overlap, not part of compute
for (int i = 0; i < num_children; i++) { ... pthread_create(&child_threads[i], ...); }
```

**Why:** Children's `recv` of X overlaps with this slave's compute. Starting forwarder threads first lets that overlap actually happen (children can be downloading X while we compute), and keeps the comm work out of this slave's compute timer.

### 9f. Joins children, splices their strips into `subtree_strip`

**RA5** (lines 498–519):
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

**Why:** This is the M1PR reduction. Each direct child returns the T-strip for *its* whole subtree. We compute the column offset relative to our own subtree (`child.col_first − our.col_first`) and `memcpy` row-by-row. After this loop, `subtree_strip` covers our own MMT cols + every descendant's cols.

### 9g. Sends T-strip up instead of `"ack"`

**RA4** (line 451):
```c
send_all(csock, "ack", 3);
```

**RA5** (line 535):
```c
send_all(csock, (char *)subtree_strip, strip_bytes);
```

**Why:** Same M1PR pattern — the slave's "I'm done" signal *is* the data payload. The parent uses receipt of the full strip as both the completion signal and the result.

---

## 10. Removed: detailed per-thread row-dump prints

RA4 had several `pthread_mutex_lock(&print_lock); ... print every row of sub ...` blocks for verifying which rows landed at which slave (lines 104–115, 360–372, 428–439). RA5 keeps the per-X dump and per-T dump but tightens the per-thread chatter to a single line per send/recv.

**Why:** Less noise. In RA5 every direct child receives the same X, so dumping it once per child is redundant. The interesting data is now the T-strips, which are still printed for `n ≤ 32`.

---

## 11. Summary of "what was added and why" (TL;DR)

| Change | What it is | Why it's there |
|---|---|---|
| `compute_mmt_strip()` | New function that runs MMT on a column range | Activity 1: each slave normalizes its assigned columns |
| `<float.h>` include | New header | Provides `FLT_MAX` for the min seed in `compute_mmt_strip` |
| Column partitioning | Tree splits by `col_first/col_count` instead of rows | MMT requires a full column to compute min/max |
| Full-X broadcast | Each slave receives the entire n×n matrix | Slaves need every row of their cols — partial rows can't produce correct min/max |
| `returned_strip` field in `TArgs` | Receive buffer in worker | Stores the M1PR reply from the child before parent thread joins |
| `subtree_strip` buffer in slave | Combined T-strip for whole subtree | Lets the slave place own MMT + children's strips into one contiguous send |
| `T` matrix on master | n×n float result | Master rebuilds the full transformed matrix from received strips |
| Slave timer narrowed to `compute_mmt_strip()` | New timer placement | Table 2 must report **compute time**, not comm |
| `send_all`/`recv_all` use `size_t` | Type widening | n=16000 ⇒ 1 GB transfers exceed `INT_MAX` |
| Renamed `compute_rows_for_range` → `compute_size_for_range` | Cosmetic rename | Reflects column semantics |
| Thread arrays bumped to `[64]` | Larger fixed bound | Headroom; consistency with `slave_ips[64]` |
| Removed per-thread row dumps | Less verbose logging | Same X is sent to every child — printing it many times adds nothing |

---

## What stayed the same

For completeness, these pieces of RA4 are reused **unchanged or near-unchanged** in RA5:

- TCP socket setup (`socket`, `bind`, `listen`, `accept`, `connect`).
- Cross-platform shims (`_WIN32` block, `closesocket`, `Sleep`).
- Connection retry loop (30 attempts × 500ms) in `worker_func`.
- Config file parsing (`master`/`slave` lines, building `slave_ips[]` / `slave_ports[]`).
- Slave-id detection by matching listening port against config entries.
- Core-affinity pinning (`SetProcessAffinityMask` / `sched_setaffinity`) and the core-count detection / range check.
- File-mode vs random-mode matrix initialization.
- Tree topology itself (recursive halving by `rem_count / 2`).
- Cosmetic `t_id` bubble sort before launching threads.
- `print_lock` mutex pattern for clean multi-threaded console output.
- `main()` argument parsing (`<n> <p> <s> <config> [input_file|core_id]`).

These layers are stable infrastructure; RA5 only changes what flows through them.
