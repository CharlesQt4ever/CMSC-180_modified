#!/bin/bash
# ============================================================
# Lab 01 serial baseline — produces T_S for LRP05 Table 3
# ============================================================
# Runs lab01 (serial column-wise MMT) at each n in {4000, 8000, 16000},
# 3 runs per n, averages, and writes lab05_T_S.csv.
#
# AUTO-RESUME: each completed n is appended to the CSV
# IMMEDIATELY. Re-running the script skips n-values already
# present (with non-ERROR T_S).
#
# To start over from scratch: rm lab05_T_S.csv
#
# Usage:
#   gcc -O2 -o lab01 lab01.c -lm
#   chmod +x lab01_baseline.sh
#   ./lab01_baseline.sh
#
# Output:
#   - lab05_T_S.csv     (n, run1, run2, run3, T_S)
# ============================================================

EXE="./lab01"
CSV="lab05_T_S.csv"
N_VALUES=(4000 8000 16000)
RUNS=3

if [ ! -f "$EXE" ]; then
    echo "Error: $EXE not found."
    echo "Compile first: gcc -O2 -o lab01 lab01.c -lm"
    exit 1
fi

# ==============================
# AUTO-RESUME: load already-completed n-values from CSV
# ==============================
declare -A DONE
EXISTING=0
if [ -f "$CSV" ]; then
    while IFS=',' read -r n_ r1_ r2_ r3_ ts_; do
        [ "$n_" = "n" ] && continue
        if [ -n "$ts_" ] && [ "$ts_" != "ERROR" ]; then
            DONE["$n_"]=1
            EXISTING=$((EXISTING + 1))
        fi
    done < "$CSV"
else
    echo "n,run1,run2,run3,T_S" > "$CSV"
fi

TOTAL=${#N_VALUES[@]}
REMAINING=$((TOTAL - EXISTING))

echo "=========================================="
echo "Lab 01 Serial Baseline (T_S for LRP05)"
echo "=========================================="
if [ "$EXISTING" -gt 0 ]; then
    echo "Resume: $EXISTING/$TOTAL n-values already saved in $CSV"
    echo "        $REMAINING n-values remaining"
else
    echo "Fresh start: $TOTAL n-values to do"
fi
echo ""

for n in "${N_VALUES[@]}"; do
    if [ -n "${DONE[$n]}" ]; then
        printf "n=%-6s ... SKIP (already saved)\n" "$n"
        continue
    fi

    times=()
    printf "n=%-6s ... " "$n"
    for ((run=1; run<=RUNS; run++)); do
        # lab01 reads n from stdin if no file argument is given
        time_val=$(echo "$n" | $EXE | grep -oP 'time elapsed:\s+\K[\d.]+')
        if [ -z "$time_val" ]; then
            echo "ERROR (no time emitted on run $run)"
            echo "$n,${times[0]:-ERROR},${times[1]:-ERROR},${times[2]:-ERROR},ERROR" >> "$CSV"
            sync 2>/dev/null
            continue 2
        fi
        times+=("$time_val")
        printf "run%d=%s " "$run" "$time_val"
    done
    avg=$(echo "scale=6; (${times[0]} + ${times[1]} + ${times[2]}) / 3" | bc)
    printf "→ T_S = %s s\n" "$avg"

    # Save IMMEDIATELY — survives a crash on the NEXT n.
    echo "$n,${times[0]},${times[1]},${times[2]},$avg" >> "$CSV"
    sync 2>/dev/null
    DONE["$n"]=1
done

echo ""
echo "=========================================="
echo "Saved: $CSV"
echo "=========================================="
echo ""
echo "Summary:"
printf "%-8s %-12s %-12s %-12s %-12s\n" "n" "Run 1" "Run 2" "Run 3" "T_S (avg)"
printf -- "----------------------------------------------------------\n"
tail -n +2 "$CSV" | while IFS=',' read -r n r1 r2 r3 ts; do
    printf "%-8s %-12s %-12s %-12s %-12s\n" "$n" "$r1" "$r2" "$r3" "$ts"
done
echo ""
echo "If any row shows ERROR, just re-run ./lab01_baseline.sh — it will"
echo "skip the n-values that are already done and re-attempt the rest."
