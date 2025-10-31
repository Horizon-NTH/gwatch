#!/usr/bin/env bash

set -euo pipefail

RUNS=3
WARMUP=1
LIMIT=2.0
GWATCH=""
TARGET=""

usage() {
	cat <<'USAGE'
Usage: perf_check.sh --gwatch <path> --target <path> [--runs N] [--warmup N] [--ratio-limit X]

Measure the execution time of a stress debuggee directly and under gwatch, failing if
slowdown exceeds the permitted ratio (default 2.0x).
USAGE
}

require_high_res_time() {
	if ! date +%s%N >/dev/null 2>&1; then
		echo "ERROR: date +%s%N not supported on this platform." >&2
		exit 1
	fi
}

parse_args() {
	while [[ $# -gt 0 ]]; do
		case "$1" in
			--gwatch)
				GWATCH=$2
				shift 2
				;;
			--target)
				TARGET=$2
				shift 2
				;;
			--runs)
				RUNS=$2
				shift 2
				;;
			--warmup)
				WARMUP=$2
				shift 2
				;;
			--ratio-limit)
				LIMIT=$2
				shift 2
				;;
			-h|--help)
				usage
				exit 0
				;;
			*)
				echo "Unknown argument: $1" >&2
				usage >&2
				exit 2
				;;
		esac
	done
}

measure_ns() {
	local __result_var=$1
	shift
	local cmd=("$@")
	local i

	for ((i=0; i<WARMUP; ++i)); do
		"${cmd[@]}" >/dev/null || return 1
	done

	local total_ns=0
	for ((i=0; i<RUNS; ++i)); do
		local start end diff
		start=$(date +%s%N)
		"${cmd[@]}" >/dev/null || return 1
		end=$(date +%s%N)
		diff=$((end - start))
		total_ns=$((total_ns + diff))
	done

	printf -v "$__result_var" '%s' "$total_ns"
}

parse_args "$@"

if [[ -z $GWATCH || -z $TARGET ]]; then
	echo "Missing required arguments." >&2
	usage >&2
	exit 2
fi

if [[ ! -f $GWATCH ]]; then
	echo "gwatch not found: $GWATCH" >&2
	exit 2
fi

if [[ ! -f $TARGET ]]; then
	echo "target not found: $TARGET" >&2
	exit 2
fi

require_high_res_time

RUNS=${RUNS:-3}
WARMUP=${WARMUP:-1}
LIMIT=${LIMIT:-2.0}

if ! [[ $RUNS =~ ^[0-9]+$ ]] || ! [[ $WARMUP =~ ^[0-9]+$ ]]; then
	echo "RUNS and WARMUP must be integers." >&2
	exit 2
fi

direct_ns=0
measure_ns direct_ns "$TARGET"

wrapped_ns=0
measure_ns wrapped_ns "$GWATCH" --var g_counter --exec "$TARGET"

if [[ $direct_ns -le 0 ]]; then
	echo "ERROR: direct execution time measured as zero; increase workload or runs." >&2
	exit 1
fi

sec_from_ns() {
	awk -v ns="$1" 'BEGIN{printf "%.9f", ns / 1000000000.0}'
}

ratio_calc() {
	awk -v num="$1" -v den="$2" 'BEGIN{printf "%.6f", num / den}'
}

direct_sec=$(sec_from_ns "$direct_ns")
wrapped_sec=$(sec_from_ns "$wrapped_ns")
ratio=$(ratio_calc "$wrapped_ns" "$direct_ns")

printf 'Direct run avg:  %s s
' "$direct_sec"
printf 'gwatch run avg: %s s
' "$wrapped_sec"
printf 'Slowdown ratio: %.3fx (limit %.2fx)
' "$ratio" "$LIMIT"

if awk -v r="$ratio" -v limit="$LIMIT" 'BEGIN{exit !(r <= limit)}'; then
	exit 0
else
	echo "ERROR: gwatch exceeds allowed slowdown." >&2
	exit 1
fi
