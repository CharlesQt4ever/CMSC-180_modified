# Lab 05 — Manual Presentation Playbook

Charles Andrei P. De los Reyes | 2023-15797 | B-3L

Demo target (per professor's instructions):
- 4 slave PCs + 1 master PC
- `n = 4`, `t = 4` (slaves), 1 core per slave
- Master prints original random matrix X and transformed matrix T
- Each slave prints its received transformed MMT part

Assumes Linux PCs (same as the ICS swarm workflow). For Windows lab PCs, the only differences are: `gcc` → MSYS2/MinGW, `pkill` → `taskkill /F /IM lab05.exe`, no `chmod` step.

---

## Phase 0 — Bring with you

| Item | Where it lives |
|---|---|
| `lab05.c` | This directory |
| USB stick (or `scp` access between PCs) | — |
| Pen + paper for the 5 IPs | — |

No CSVs, no scripts.

---

## Phase 1 — Setup (15–20 minutes before the prof arrives)

### Step 1.1 — Assign roles to the 5 PCs

Put a sticky note on each monitor:

| Role | PC label | Slave ID | Port |
|---|---|---|---|
| MASTER | PC-M | — | 5000 |
| Slave 1 | PC-1 | 1 | 5001 |
| Slave 2 | PC-2 | 2 | 5001 |
| Slave 3 | PC-3 | 3 | 5001 |
| Slave 4 | PC-4 | 4 | 5001 |

> Separate physical PCs can share port 5001. Only loopback (single-PC rehearsal) forces different ports.

### Step 1.2 — Get each PC's IP

At each PC, open a terminal:

```bash
hostname -I
```

Pick the IP that looks like `10.0.x.x` or `192.168.x.x` (ignore `127.0.0.1`). Write it down:

```
MASTER  IP: ___________________
Slave 1 IP: ___________________
Slave 2 IP: ___________________
Slave 3 IP: ___________________
Slave 4 IP: ___________________
```

### Step 1.3 — Copy `lab05.c` to every PC

**Method A — USB stick** (simplest)

Plug USB into each PC, then on each:
```bash
cp /media/<your_user>/<usb_label>/lab05.c ~/
cd ~
```

**Method B — SCP from master**

After putting `lab05.c` on the master:
```bash
scp ~/lab05.c <user>@<slave1_ip>:~
scp ~/lab05.c <user>@<slave2_ip>:~
scp ~/lab05.c <user>@<slave3_ip>:~
scp ~/lab05.c <user>@<slave4_ip>:~
```

### Step 1.4 — Compile on every PC

```bash
cd ~
gcc -O2 -o lab05 lab05.c -lpthread -lm
ls -la lab05
```

You should see a green executable `lab05`. Do this on all 5 PCs.

### Step 1.5 — Create `config.txt` on every PC

Content must be **identical on all 5 PCs**.

```bash
nano config.txt
```

Type (substituting the IPs from Step 1.2):

```
slave 10.0.x.x1   5001
slave 10.0.x.x2   5001
slave 10.0.x.x3   5001
slave 10.0.x.x4   5001
```

Save: `Ctrl+O`, `Enter`, `Ctrl+X`. Verify:

```bash
cat config.txt
```

> **Order matters.** First `slave` line = Slave 1, second = Slave 2, etc. The master assigns columns in this order: Slave 1 → col 0, Slave 2 → col 1, Slave 3 → col 2, Slave 4 → col 3.

### Step 1.6 — Dry run before the prof comes

Do **one full Phase 2 rehearsal** before the prof arrives. Port-in-use and firewall issues are the #1 cause of failed demos. If anything breaks, you have time to fix it.

---

## Phase 2 — The Demo (prof watching)

Walk order: Slave 1 → 2 → 3 → 4 → Master.

### Step 2.1 — At Slave 1 PC

```bash
cd ~
./lab05 4 5001 1 config.txt 0
```

**Arg meaning** (memorize for prof questions):
| Arg | Meaning |
|---|---|
| `4` | `n` — matrix size |
| `5001` | port this slave listens on |
| `1` | role: 1 = slave, 0 = master |
| `config.txt` | config file |
| `0` | `core_id` — pin to CPU core 0 |

Expected output:
```
[Slave] Detected N CPU core(s) on this machine (valid core_id: 0 to N-1)
[Slave] Pinned to core 0
[Slave 1] Listening on port 5001...
```

It will then **wait silently** — that's correct. It's blocked on `accept()`.

### Step 2.2 — At Slave 2 PC

```bash
cd ~
./lab05 4 5001 1 config.txt 0
```

Expect `[Slave 2] Listening on port 5001...`.

### Step 2.3 — At Slave 3 PC

```bash
cd ~
./lab05 4 5001 1 config.txt 0
```

### Step 2.4 — At Slave 4 PC

```bash
cd ~
./lab05 4 5001 1 config.txt 0
```

> All 4 slaves are now blocked waiting for the master. They look frozen. **This is correct.**

### Step 2.5 — At MASTER PC

```bash
cd ~
./lab05 4 5000 0 config.txt
```

| Arg | Meaning |
|---|---|
| `4` | `n` |
| `5000` | master's own port |
| `0` | role: 0 = master |
| `config.txt` | config file |
| *no 5th arg* | random matrix mode (satisfies requirement 3.4 "generated randomized matrix") |

### Step 2.6 — Master output (requirement 3.4)

```
[Master] n=4, slaves=4, mode=random

--- Full Matrix X (4 x 4) ---            ← REQUIREMENT 3.4 (original matrix)
73   12   85    3
22   91   47   60
14   88   33   71
67    5   99   28

[Master] Slave 1 (10.0.x.x1:5001) -> 1 cols (cols 0 to 0)
[Master] Slave 2 (10.0.x.x2:5001) -> 1 cols (cols 1 to 1)
[Master] Slave 3 (10.0.x.x3:5001) -> 1 cols (cols 2 to 2)
[Master] Slave 4 (10.0.x.x4:5001) -> 1 cols (cols 3 to 3)

[Master] Tree: 3 direct children (O(log 4))
[Master] Received T-strip from Slave 1 (cols 0..0, subtree=1)
[Master] Received T-strip from Slave 2 (cols 1..1, subtree=1)
[Master] Received T-strip from Slave 3 (cols 2..3, subtree=2)
time elapsed: 0.0XX seconds

--- Full Matrix T (4 x 4) ---            ← REQUIREMENT 3.4 (transformed matrix)
1.0000   0.0814   0.7879   0.0000
0.1356   1.0000   0.1515   0.8382
0.0000   0.9651   0.0000   1.0000
0.8983   0.0000   1.0000   0.3676
```

### Step 2.7 — Slave output (requirement 3.5)

Walk the prof to each slave. Example for Slave 2:

```
[Slave 2] Connected by parent.
[Slave 2] RECEIVED X (n=4), assignment cols [1..1] (subtree=1), my MMT cols [1..1]
[Slave 2] X received (4 x 4):
73   12   85    3
22   91   47   60
14   88   33   71
67    5   99   28
time elapsed: 0.00000X seconds

[Slave 2] My MMT (cols [1..1]):          ← REQUIREMENT 3.5
0.0814
1.0000
0.9651
0.0000

[Slave 2] Sent T-strip to parent (cols [1..1], 1 slave(s) in subtree)
```

The `My MMT` block answers requirement 3.5. Point at it explicitly.

### Step 2.8 — Spot-verification (impresses the prof)

For each slave's column, find that column in the master's printed X. Confirm:

- Row with the **smallest int** → became `0.0000` in T
- Row with the **largest int** → became `1.0000` in T
- Other rows → values in `(0, 1)`

Sample script for the prof:

> "On Slave 2, column 1 of X is `[12, 91, 88, 5]`. Smallest is 5 (row 3), largest is 91 (row 1). In the slave's output, row 3 is `0.0000` and row 1 is `1.0000`. Min maps to 0, max maps to 1 — that's the min-max transformation working correctly."

### Step 2.9 — Special note about Slave 3

Slave 3 prints **two output blocks**:

1. `[Slave 3] My MMT (cols [2..2]):` — Slave 3's own column (this is the 3.5 answer)
2. `[Slave 3] Subtree T-strip ready (cols [2..3]):` — Slave 3's column plus Slave 4's column, combined, before forwarding to master

This is because Slave 3 is the **subtree root for slaves 3 and 4** in the recursive-halving tree.

If the prof asks why: **"Recursive-halving reduction tree, O(log t) communication. Slave 3 acts as both a worker (its own column) and a tree-internal node (forwarder for Slave 4's strip)."**

---

## Phase 3 — Cleanup between runs

Normally no cleanup is needed — slaves auto-exit after sending their T-strip, and the master exits after printing T.

If a previous run hung (e.g., you `Ctrl+C`'d a slave):

```bash
pkill -f lab05
```

If a port is still stuck:

```bash
fuser -k 5001/tcp
```

If you need a different random matrix, **wait 2 seconds** between runs (`srand(time(NULL))` has 1-second resolution).

---

## Phase 4 — Troubleshooting during the demo

| Symptom | Cause | Fix |
|---|---|---|
| Slave window doesn't print "Listening on port 5001" | Port already in use | `fuser -k 5001/tcp` on that slave, retry |
| Master prints `Slave 1 ...` then nothing | Master can't reach a slave's IP/port | Check IP in `config.txt`; check firewall: `sudo ufw status` (disable if active) |
| Master hangs on "connect" forever | Slave not running, or wrong IP in config | Check the slave's terminal — is it showing "Listening"? |
| Fewer T-strip receipts than expected | A slave crashed mid-run | Look at that slave's terminal for the error |
| Matrix X different every run | Correct — random mode | If the prof wants reproducibility, pass a 5th arg with a fixed input file |

---

## One-line cheat sheet

```
Slaves (4 separate PCs):   ./lab05 4 5001 1 config.txt 0
Master (run LAST):         ./lab05 4 5000 0 config.txt
```

Order: **all 4 slaves first**, then master.
