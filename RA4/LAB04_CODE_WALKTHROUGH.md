# Lab 04 — Code Walkthrough of `lab04_v3.c`

A section-by-section reading of the source, plus anticipated questions and
answers. Read top-to-bottom the first time; use the Q&A section at the end
as a drill before the demo.

---

## 0. Big Picture (one paragraph)

`lab04_v3.c` is a single binary that plays two roles depending on the
`<s>` argument: `s = 0` makes it the **master**, `s >= 1` makes it a
**slave** with id s. The master reads a config file listing the master
and all slaves by IP and port, builds (or reads) an n×n integer matrix,
partitions it row-wise across the t slaves, and distributes each piece
using a **binomial-tree one-to-many personalized broadcast**. Each slave
that receives a block larger than it needs forwards the tail end to a
downstream slave, so the work of sending is parallelized and the total
number of sequential sends is only ⌈log₂ t⌉ instead of t. Acknowledgments
travel back up the same tree. The master's timer brackets exactly the
distribution-plus-ack phase.

---

## 1. Lines 1–6 — Header Comment

```c
/*
Charles Andrei P. De los Reyes
2023-15797
B-3L
*/
```

Identifies you as the author. Required by the lab spec.

---

## 2. Lines 7–30 — Includes and Platform Abstractions

```c
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
```

- `_GNU_SOURCE` exposes Linux-specific APIs, notably `CPU_SET`,
  `CPU_ZERO`, and `sched_setaffinity` used later for core pinning.
- `pthread.h` is the POSIX threads API, used to launch parallel sends.

```c
#ifdef _WIN32
#include <winsock2.h>
...
#else
#include <sys/socket.h>
...
#define SOCKET int
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define closesocket close
#define Sleep(ms) usleep((ms) * 1000)
#endif
```

This block lets the same source compile on both Windows and Linux.
Windows uses Winsock (`SOCKET` is a typedef, `closesocket` is its close
call), while Linux uses BSD sockets (a plain `int` file descriptor,
`close`). The `#define`s alias the Windows names so the rest of the code
can be written once using Winsock-style identifiers.

**Why it matters for the demo:** proves the code is portable. You
benchmarked Tables 1 and 2 on Windows using PowerShell scripts and Table
3 on Linux swarm drones using shell scripts — both used the same C
source.

---

## 3. Lines 32–50 — Reliable Send/Receive

```c
int send_all(SOCKET sock, const char *buf, int len) {
    int total = 0;
    while (total < len) {
        int sent = send(sock, buf + total, len - total, 0);
        if (sent <= 0) return -1;
        total += sent;
    }
    return total;
}

int recv_all(SOCKET sock, char *buf, int len) {
    int total = 0;
    while (total < len) {
        int got = recv(sock, buf + total, len - total, 0);
        if (got <= 0) return -1;
        total += got;
    }
    return total;
}
```

These are "send exactly N bytes" and "receive exactly N bytes" helpers.

**The problem they solve:** a single `send()` or `recv()` call may
transfer *fewer bytes than requested*. This is normal TCP behavior, not
an error: the kernel buffers fill up, the reader consumed only part, etc.
If you naively call `recv(sock, buf, 1024)` expecting 1024 bytes, you
often get less and the rest of your buffer is garbage.

**The fix:** loop until the whole payload has moved. Every subsequent
call picks up where the previous one left off (`buf + total`,
`len - total`).

Both return `-1` on error and otherwise return the total bytes moved.

**Why it matters:** at n = 16,000, the full matrix is 1.024 GB. No
single `send()` call will move that in one shot — the loop is
essential for correctness, not optimization.

---

## 4. Lines 52–58 — Row-Distribution Helper

```c
int compute_rows_for_range(int n, int t, int first, int count) {
    int base = n / t, rem = n % t, total = 0;
    for (int i = first; i < first + count; i++)
        total += base + (i < rem ? 1 : 0);
    return total;
}
```

Given n rows split across t slaves, compute how many rows the
contiguous block `[first, first+count)` adds up to.

**The row-distribution rule:** each slave gets either `base = n/t` or
`base + 1` rows. The first `rem = n%t` slaves get one extra row each so
every row is covered exactly once. Example: n = 10, t = 3 → base = 3,
rem = 1 → slaves get 4, 3, 3 rows respectively.

**Where it's used:** the master (and any slave that needs to forward)
calls this to compute how much matrix data to keep versus forward at
each tree level.

---

## 5. Lines 60–67 — The `TArgs` Struct

```c
typedef struct {
    int *sub;          // pointer into M for the submatrix this child gets
    int rows, n, start_row;
    int first_slave, num_slaves, t_id;
    char ip[64];
    int port;
    int success;       // set to 1 after ack is received
} TArgs;
```

One argument bundle per thread. Each entry in a `TArgs` array describes
one outgoing send (master → child, or slave → grandchild).

Important fields:
- `sub` — direct pointer into the matrix; no copy needed, the thread
  sends straight from M.
- `rows, n` — dimensions of the submatrix.
- `start_row` — absolute row index of the first row in `sub` (only used
  for logging/verification).
- `first_slave, num_slaves` — the subtree this send covers. The
  receiving slave will use these to do its own split-and-forward.
- `t_id` — 1-based slave id for printing.
- `ip, port` — where to connect.
- `success` — gets flipped to 1 when the ack comes back.

---

## 6. Lines 69–119 — `worker_func` (The Thread Body)

This is what each pthread runs. It is used by both the master (to send
to direct children) and any slave that has sub-children to forward to.

```c
void *worker_func(void *args) {
    TArgs *a = (TArgs *)args;
    a->success = 0;
```

Claim "not yet successful" up front.

### 6a. Create a TCP socket

```c
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) return NULL;
```

`AF_INET` = IPv4, `SOCK_STREAM` = TCP.

### 6b. Connect with retry

```c
    for (int i = 0; i < 30 && !connected; i++) {
        if (connect(sock, ...) == 0) connected = 1;
        else Sleep(500);
    }
```

The child slave might not have called `listen()` yet — processes start
in parallel, not in lock-step. The loop retries every 500 ms for up to
15 seconds, which is enough time for any slave to get its listening
socket up.

### 6c. Send metadata, then data

```c
    send_all(sock, (char *)&a->n, sizeof(int));
    send_all(sock, (char *)&a->rows, sizeof(int));
    send_all(sock, (char *)&a->start_row, sizeof(int));
    send_all(sock, (char *)&a->first_slave, sizeof(int));
    send_all(sock, (char *)&a->num_slaves, sizeof(int));
    send_all(sock, (char *)a->sub, a->rows * a->n * (int)sizeof(int));
```

Five 4-byte ints of header, then the raw submatrix bytes. The recipient
uses the header to know exactly how much data to read and what to do
with it (keep vs forward).

**Binary format vs text.** You're sending raw `int` bytes — four bytes
per matrix cell. This is why your runtimes are much faster than
classmates who used `printf`/`fscanf`: text serialization can add 3–5×
overhead for large integers, and parsing on the receiver is CPU-bound.

### 6d. Per-hop printing (the demo requirement)

```c
    if (a->n <= 32) {
        printf("[-> Slave %d] SENT %d rows (rows %d to %d):\n", ...);
        // prints the submatrix
    }
```

Matches the professor's "print the matrix after every send" requirement.
Gated by `n <= 32` so benchmark runs at n ≥ 4,000 stay silent (would
otherwise dump hundreds of MB to the terminal).

### 6e. Wait for subtree ack

```c
    char ack[4] = {0};
    if (recv_all(sock, ack, 3) >= 0)
        a->success = 1;
```

The three-byte string `"ack"`. Crucially, the child only acks *after*
its own subtree has finished. So blocking on this recv() makes the
master's ack-wait propagate through the whole tree.

---

## 7. Lines 121–253 — `run_master`

The master role: build the matrix, divide it, launch the tree, time the
whole thing.

### 7a. Parse config (lines 122–139)

```c
    while (fgets(line, 256, cfg)) {
        char role[16], ip[64]; int port;
        if (sscanf(line, "%15s %63s %d", role, ip, &port) != 3) continue;
        if (strcmp(role, "slave") == 0 && t < 64) {
            strncpy(slave_ips[t], ip, 63);
            slave_ports[t] = port;
            t++;
        }
    }
```

Reads lines like `slave 10.0.9.132 5001`. Silently ignores unparseable
lines and the `master` line. `t` ends up equal to the number of slaves.

### 7b. Build or read the matrix (lines 141–163)

- If an input file is provided (demo mode), read it: first integer is n,
  then n² integers separated by commas or whitespace.
- Otherwise (benchmark mode) allocate a fresh n×n array filled with
  random ints in [1, 100].

**The `int *M = malloc(n*n*sizeof(int))` layout is key.** The matrix is
**one contiguous block of memory** — row r column c is at index
`r*n + c`. This is why `send_all(sock, M, ...)` can transmit a whole
block of rows in a single call: they are adjacent in RAM. A 2D array of
pointers (`int **M`) would require a send per row, which would be far
slower.

### 7c. Print for verification (lines 166–172)

If `file_mode || n <= 32`, print the full matrix. Benchmark runs skip
this.

### 7d. Show slave assignments (lines 174–187)

For each slave, print which rows it will own and (for small n) the
actual rows. This is purely verification output — not used for any
computation.

### 7e. Build the tree (lines 189–227)

This is the heart of the algorithm. Walk through it with a concrete
example: t = 4 slaves (indices 0,1,2,3), n = 8.

**Iteration 1:** rem_first = 0, rem_count = 4.
- left_count = 4/2 = 2, right_count = 2, right_first = 2.
- left_rows = rows for slaves 0–1 = 4, right_rows = 8 − 4 = 4.
- Push a `TArgs` for Slave 3 (index 2), sending 4 rows covering slaves
  {2,3}.
- Set `rem_count = 2, rem_rows = 4`.

**Iteration 2:** rem_first = 0, rem_count = 2.
- left_count = 1, right_count = 1, right_first = 1.
- left_rows = 2, right_rows = 2.
- Push a `TArgs` for Slave 2 (index 1), sending 2 rows covering just
  slave {1}.
- Set `rem_count = 1, rem_rows = 2`.

**Iteration 3:** rem_count == 1, terminal case.
- Push a `TArgs` for Slave 1 (index 0), sending 2 rows for slave {0}.
- Break.

**Result:** `num_children = 3` but only the first two sends actually
fork a subtree; the third is a leaf. The master performs 3 parallel
sends (not 4 — that last slave is reached directly), and critically,
**the send to Slave 3 includes slaves 2 and 3's data**, meaning Slave 3
becomes responsible for forwarding Slave 4's half. That is what makes
it a tree.

For t = 16 slaves, the loop runs log₂(16) = 4 times, producing 4
direct children. Each of those recursively does the same thing on its
half.

### 7f. Launch threads, join, time (lines 231–249)

```c
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < num_children; i++)
        pthread_create(&threads[i], NULL, worker_func, &t_args[i]);
    for (int i = 0; i < num_children; i++)
        pthread_join(threads[i], NULL);
    clock_gettime(CLOCK_MONOTONIC, &end);
```

- `CLOCK_MONOTONIC` is a steady clock that is unaffected by wall-clock
  adjustments (NTP, DST) — the right choice for measuring durations.
- `pthread_create` launches one thread per direct child, and each
  thread runs `worker_func`. All outgoing sends happen in parallel.
- `pthread_join` waits for that thread to finish, which in turn only
  happens after its ack is received. Since `worker_func` blocks on
  `recv_all(..., "ack", 3)`, the join wait propagates through the
  whole subtree.

**What is and is not timed:**
- Timed: the full distribution phase, all forwarding hops, all ack
  returns.
- Not timed: matrix generation, config parsing, printing, cleanup.

### 7g. Report and free (lines 247–252)

Subtract the two timespecs to get elapsed seconds with nanosecond
precision, print, free the matrix.

---

## 8. Lines 255–402 — `run_slave`

Each slave runs this. It receives, maybe forwards, maybe pins itself to
a core, acks.

### 8a. Optional core pinning (lines 256–266)

```c
    if (core_id >= 0) {
    #ifdef _WIN32
        SetProcessAffinityMask(GetCurrentProcess(), (DWORD_PTR)1 << core_id);
    #else
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(core_id, &cpuset);
        sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
    #endif
```

Pins the whole slave process to exactly one CPU core. Only used for
Table 2 (core-affine). When invoked without the core argument, `core_id
= -1` and this block is skipped.

- Windows: `SetProcessAffinityMask` with a bitmask (bit `core_id` set).
- Linux: build a `cpu_set_t`, set one bit, call `sched_setaffinity`.

### 8b. Read config for the slave list (lines 269–286)

A slave also needs the full list of slave IPs and ports so it can
forward downstream. The slave identifies itself (`slave_id`) by matching
its port against the config's slave entries.

### 8c. Listen and accept (lines 288–302)

```c
    SOCKET srv = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, ...);
    bind(srv, ...);
    listen(srv, 1);
    SOCKET csock = accept(srv, NULL, NULL);
```

Standard TCP server pattern.

- `SO_REUSEADDR` lets the slave restart immediately on the same port
  without waiting for the OS's `TIME_WAIT` to expire — important if you
  re-run the binary multiple times in a row.
- `listen(srv, 1)` requests a backlog of 1 pending connection (only one
  parent ever connects, so that's enough).
- `accept()` blocks until the parent connects.

### 8d. Receive metadata and data (lines 304–321)

Five ints then the submatrix bytes. Mirror-image of what `worker_func`
sent. `sub = malloc(rows * recv_n * sizeof(int))` is a local contiguous
buffer.

### 8e. Per-hop receive print (lines 323–333)

Prints the submatrix just received, gated by `recv_n <= 32`. This is
the receive half of the professor's requirement.

### 8f. The forwarding half (lines 335–371)

```c
    if (num_slaves > 1) {
        int rem_first = first_slave, rem_count = num_slaves;
        ...
        while (rem_count > 1) {
            int left_count = rem_count / 2;
            int right_count = rem_count - left_count;
            ...
            child_args[num_children].sub = sub + ... ;
            pthread_create(&child_threads[num_children], NULL,
                           worker_func, &child_args[num_children]);
            num_children++;
```

Same split-and-delegate logic as the master, but starting from the
slave's own subset of the matrix. If `num_slaves == 1`, this block is
skipped entirely — you are a leaf.

**This is the key moment of "each slave becomes a source":** the slave
calls `pthread_create(..., worker_func, ...)` and the exact same
`worker_func` used by the master now fires off a subtree send to a
grandchild. The pattern is recursive in structure even though the code
is iterative.

### 8g. Print my final block (lines 373–381)

After forwarding off whatever I needed to forward, `sub` still holds
*my* rows at the front of the buffer. Print them for verification.

### 8h. Wait for children to ack, then ack parent (lines 383–391)

```c
    for (int i = 0; i < num_children; i++)
        pthread_join(child_threads[i], NULL);
    send_all(csock, "ack", 3);
```

**Ordering matters here.** Ack the parent only *after* all children
have acked back, because the parent is using these acks to infer that
the whole subtree is done. If you acked the parent first, the master
would stop its timer before the grandchildren had finished.

### 8i. Local timer, free, close (lines 393–401)

Each slave also measures and prints its own local time. This is purely
diagnostic — the authoritative time is the master's, which appears in
the CSV.

---

## 9. Lines 404–432 — `main`

```c
    if (argc < 5) {
        printf("Usage: %s <n> <p> <s> <config_file> [input_file|core_id]\n", ...);
        return 1;
    }
    int n = atoi(argv[1]);
    int p = atoi(argv[2]);
    int s = atoi(argv[3]);
    char *config_file = argv[4];
    ...
    if (s == 0) {
        char *input_file = (argc >= 6) ? argv[5] : NULL;
        run_master(n, p, config_file, input_file);
    } else {
        int core_id = (argc >= 6) ? atoi(argv[5]) : -1;
        run_slave(n, p, config_file, core_id);
    }
```

Argument dispatch:
- `<n>` — matrix dimension.
- `<p>` — port to bind (master listens/connects; slave listens here).
- `<s>` — 0 = master, anything else = slave (the exact number is
  informational only; the real slave id is recomputed from the port).
- `<config_file>` — e.g., `config_4pc.txt`.
- Optional 5th argument is an input filename for the master or a core
  id for the slave.

The `_WIN32` blocks call `WSAStartup`/`WSACleanup`, which Winsock
requires; a no-op on Linux.

---

## 10. End-to-End Walk-through (concrete example)

**Setup:** t = 3 slaves, n = 8, `config_4pc.txt`:
```
master 10.0.9.100 5000
slave  10.0.9.101 5001
slave  10.0.9.102 5002
slave  10.0.9.103 5003
```

1. Slaves 1, 2, 3 start first. Each binds and listens on its port.
2. Master starts. Reads config, allocates M (8×8 = 256 bytes), prints
   it, prints the row assignments.
3. Master's tree-building loop runs: for t = 3, it produces 2 direct
   children (Slave 2 covers slaves {1,2}, Slave 1 covers just itself).
   Wait, re-check: with rem_count = 3 → left_count = 1, right_count =
   2, right_first = 1 → push send to Slave 2 with num_slaves = 2.
   Next iteration rem_count = 1 → push leaf send to Slave 1. So two
   children total.
4. Master starts timer, creates 2 threads.
5. Thread A connects to Slave 1, sends metadata (num_slaves = 1,
   rows = 3) and 3 rows.
6. Thread B connects to Slave 2, sends metadata (num_slaves = 2,
   rows = 5) and 5 rows.
7. Slave 1 receives, sees num_slaves = 1, prints, acks back immediately.
8. Slave 2 receives 5 rows covering itself and Slave 3, sees
   num_slaves = 2, enters forwarding block.
9. Slave 2 spins up its own thread via `pthread_create(worker_func,
   ...)` to send the trailing 2 rows to Slave 3.
10. Slave 3 receives 2 rows, num_slaves = 1, prints, acks to Slave 2.
11. Slave 2's `pthread_join` returns. Slave 2 sends ack to master.
12. Master's `pthread_join` for Thread B returns (Thread A already
    returned). Master stops timer, prints "time elapsed: X.XXXXXX
    seconds".

Total sequential network hops: 2 (master → Slave 2, Slave 2 → Slave 3)
instead of 3 (master → 1, → 2, → 3). That is the O(log t) speedup
made visible.

---

## 11. Anticipated Questions and Answers

### Core concepts

**Q: Why one binary for both master and slave?**
A: Simpler to deploy — one compile, one scp target, fewer chances for
version drift between the master and slave executables. The `<s>`
argument selects the role at startup.

**Q: Why TCP and not UDP?**
A: TCP guarantees in-order, reliable delivery. A 1 GB matrix with one
lost packet in UDP would mean corrupted data that looks valid. Reliable
delivery is essential for correctness, and the lab isn't about
implementing custom retransmission.

**Q: Why POSIX threads instead of `fork`?**
A: Threads share memory with the caller, so each thread's `TArgs`
already points into the master's matrix M with no copying. `fork` would
duplicate the whole address space.

**Q: Why do you use `CLOCK_MONOTONIC`?**
A: It's a steady-rate clock guaranteed not to go backwards, even if
NTP adjusts wall time during the run. `CLOCK_REALTIME` or `time()`
could jump and corrupt the measurement.

### Algorithm

**Q: Explain the tree construction in your own words.**
A: At each step I split my range of slaves in half, keep the left half
for me, and send the right half's data to the slave at the boundary
along with instructions to repeat the procedure. That slave is now
responsible for its subtree. Recursion ends when a subtree has one
slave, which just keeps its data.

**Q: How many sends does the master make for t = 16 slaves?**
A: ⌈log₂ 16⌉ = 4 direct sends, in parallel. Each of those four slaves
then forwards to two grandchildren, which forward to four
great-grandchildren, and so on.

**Q: Why is a flat parallel scheme (master sends to every slave
simultaneously) not as good?**
A: Every byte still exits through the master's single NIC. The total
bandwidth is capped by the master's outgoing link. In a tree, once
slaves 2, 3, 4, 5 have received data, they can transmit to slaves
6–16 using their own independent NICs in parallel, multiplying the
effective throughput.

**Q: Why did you use ceiling-log, not floor-log?**
A: Because you always need at least `⌈log₂ t⌉` tree levels to cover t
nodes. For t = 5, log₂ 5 ≈ 2.32, so you need 3 levels.

**Q: What happens when t is not a power of two?**
A: The loop `rem_count / 2` handles any t. For t = 6 the first split
is 3+3, next is 1+2 and 1+2 inside each half, and so on. The tree just
gets slightly unbalanced, which is fine.

### Tables and benchmarks

**Q: Why is Table 3 so much slower than Tables 1 and 2?**
A: Bandwidth, not CPU. Loopback moves several GB/s because nothing
leaves RAM. Gigabit Ethernet caps at about 125 MB/s, and the shared
swarm fabric is often lower. The matrix at n = 16,000 is 1.024 GB, so
wire time alone accounts for most of the swarm runtimes.

**Q: Why did core affinity not help?**
A: The workload is I/O-bound. Each slave spends most of its time
blocked in `recv()`; there's almost no computation to benefit from
cache locality. Pinning the master plus 15 slaves (plus kernel
softirqs, plus other system processes) crowds the scheduler and
introduces a small contention penalty at large n.

**Q: Why does Table 3 scale better with t than Table 1?**
A: On loopback, the master's single RAM bus and kernel softirq path
still serialize part of the work even across threads. On the swarm,
each tree depth uses physically independent NICs and CPUs, so the
log-depth parallelism is expressed cleanly.

### Specifics the professor might drill

**Q: What does `SO_REUSEADDR` do?**
A: Lets the slave restart on the same port without waiting for the
kernel's TIME_WAIT interval to expire. Handy when re-running the demo.

**Q: What does `send_all` do differently from `send`?**
A: `send` may return after transferring fewer bytes than requested.
`send_all` loops until all `len` bytes have been handed to the kernel.

**Q: Why a 1D array instead of `int **M`?**
A: Contiguous memory. I can send any rectangle of rows with a single
`send_all` call because they are adjacent in RAM. An array of pointers
to rows would force me to send one row at a time or assemble a staging
buffer.

**Q: What are the five header ints you send before the data?**
A: `n` (matrix dimension), `rows` (how many rows in this submatrix),
`start_row` (absolute row index, for logging), `first_slave` (the id
of the first slave in this subtree), and `num_slaves` (how many slaves
this subtree covers). The recipient uses `num_slaves` to decide whether
to forward.

**Q: How does a slave know its own slave_id?**
A: During config parsing, it walks the slave list and picks the entry
where the port matches the `<p>` argument it was launched with.

**Q: Where exactly does the timer start and stop?**
A: Start is right before `pthread_create` in `run_master` (line 233),
end is right after all `pthread_join` calls complete (line 245). So
the timer covers all sends plus waiting for the last ack to bubble up.
Nothing else is inside the timer.

**Q: Why do you call `fflush(stdout)` after per-hop prints?**
A: Without it, terminal output from multiple machines (or threads) can
be buffered and interleave unpredictably. `fflush` forces each print
out immediately so the sequence on screen matches the actual order of
network events.

**Q: What limit does `TArgs t_args[32]` impose?**
A: At most 32 direct children, i.e., up to 32 slaves at the root of
the tree. For the lab, t maxes at 16, so this is comfortable.

**Q: What if a slave crashes mid-run?**
A: The master's `pthread_join` on the affected branch will never
return because the ack never comes back. The whole run hangs. The
benchmark scripts use a `timeout` wrapper on the master (e.g.,
`timeout 300s`) to catch this case and record an ERROR row.

### If asked about improvements

**Q: What would you change if you did this again?**
A: Two possible wins. First, use a more balanced (binomial) split that
accounts for variable row counts across slaves so all subtrees finish
at similar times. Second, move acks to UDP or even omit them for the
distribution phase, using a final all-slaves-done barrier via TCP
instead — saves the round-trip time per tree level. But for this lab,
the current scheme is well within the spec and fast enough.

**Q: Does this generalize to non-matrix data?**
A: Yes. The payload is just opaque bytes; only the metadata and the
row-distribution helper assume matrix semantics. Replace the "rows"
math with a more generic "chunks" concept and the tree works for any
partitionable dataset.
