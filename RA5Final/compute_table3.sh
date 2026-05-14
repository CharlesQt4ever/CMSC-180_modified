#!/bin/bash
# ============================================================
# Compute LRP05 Table 3 (performance metrics) from sweep CSVs
# ============================================================
# Reads:
#   lab05_T_S.csv          (from lab01_baseline.sh)
#   lab05_table3.csv       (from table3.sh — default; override with $1)
#
# Produces:
#   lab05_table3_metrics.csv   (n, t, T_S, T_P, T_O, S, E, pT_P)
#   formatted table printed to terminal
#
# Formulas (LRP05 Research Activity 4):
#   T_S  = serial runtime from lab01 at the same n (constant per n)
#   T_P  = average master time from sweep CSV (col 4 = master_time)
#   p    = t (number of slaves)
#   T_O  = p · T_P − T_S       (parallel overhead)
#   S    = T_S / T_P           (parallel speedup)
#   E    = S / p               (parallel efficiency)
#   pT_P = p · T_P             (parallel cost)
#
# Usage:
#   ./compute_table3.sh                          # uses lab05_table3.csv
#   ./compute_table3.sh lab05_table1.csv         # use a different sweep CSV
#   ./compute_table3.sh lab05_table2.csv         # core-affine single-PC variant
# ============================================================

TS_CSV="lab05_T_S.csv"
TP_CSV="${1:-lab05_table3.csv}"
OUT="lab05_table3_metrics.csv"
N_VALUES=(4000 8000 16000)
T_VALUES=(2 4 8 16)

if [ ! -f "$TS_CSV" ]; then
    echo "Error: $TS_CSV not found."
    echo "Run ./lab01_baseline.sh first to generate the serial baseline."
    exit 1
fi
if [ ! -f "$TP_CSV" ]; then
    echo "Error: $TP_CSV not found."
    echo "Run ./table3.sh (or pass a different sweep CSV as the first argument)."
    exit 1
fi

# Build T_S map: n -> T_S
declare -A TS_MAP
while IFS=',' read -r n r1 r2 r3 ts; do
    [ "$n" = "n" ] && continue
    [ "$ts" = "ERROR" ] && continue
    TS_MAP[$n]="$ts"
done < "$TS_CSV"

echo "=========================================="
echo "LRP05 Table 3 — Performance Metrics"
echo "=========================================="
echo "Source T_S:  $TS_CSV"
echo "Source T_P:  $TP_CSV (master_time column)"
echo ""

echo "n,t,T_S,T_P,T_O,S,E,pT_P" > "$OUT"

for n in "${N_VALUES[@]}"; do
    TS="${TS_MAP[$n]}"
    if [ -z "$TS" ]; then
        echo "WARNING: no T_S for n=$n — skipping all t for this n"
        for t in "${T_VALUES[@]}"; do
            echo "$n,$t,MISSING,MISSING,MISSING,MISSING,MISSING,MISSING" >> "$OUT"
        done
        continue
    fi
    for t in "${T_VALUES[@]}"; do
        # Pull master_time values for this (n, t); skip ERROR rows
        runs=$(grep "^${n},${t}," "$TP_CSV" | cut -d',' -f4 | grep -v -e ERROR -e '^$')
        nruns=$(echo "$runs" | grep -c .)
        if [ "$nruns" -eq 0 ]; then
            echo "$n,$t,$TS,NO_DATA,NO_DATA,NO_DATA,NO_DATA,NO_DATA" >> "$OUT"
            continue
        fi
        sum=$(echo "$runs" | awk '{s+=$1} END {printf "%.6f", s}')
        TP=$(echo "scale=6; $sum / $nruns" | bc)
        TO=$(echo "scale=6; $t * $TP - $TS" | bc)
        S=$(echo "scale=6; $TS / $TP" | bc)
        E=$(echo "scale=6; $S / $t" | bc)
        pTP=$(echo "scale=6; $t * $TP" | bc)
        echo "$n,$t,$TS,$TP,$TO,$S,$E,$pTP" >> "$OUT"
    done
done

echo "Saved: $OUT"
echo ""
echo "=========================================="
echo "Table 3 — Performance Metrics"
echo "=========================================="
printf "%-7s %-3s %-12s %-12s %-12s %-9s %-9s %-12s\n" "n" "t" "T_S" "T_P" "T_O" "S" "E" "pT_P"
printf -- "----------------------------------------------------------------------------------\n"
tail -n +2 "$OUT" | while IFS=',' read -r n t ts tp to s e ptp; do
    printf "%-7s %-3s %-12s %-12s %-12s %-9s %-9s %-12s\n" "$n" "$t" "$ts" "$tp" "$to" "$s" "$e" "$ptp"
done

echo ""
echo "Notes:"
echo "  - S > p indicates superlinearity (typically a cache effect)."
echo "  - pT_P growing much faster than T_S as t increases means the"
echo "    implementation is not cost-optimal at that t for this topology."
