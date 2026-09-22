#!/bin/sh
# Basic command-line and terminal smoke tests for cbonsai.

set -eu

binary=${1:-./cbonsai}
failures=0

fail() {
	printf '%s\n' "not ok: $1" >&2
	failures=$((failures + 1))
}

pass() {
	printf '%s\n' "ok: $1"
}

assert_contains() {
	name=$1
	text=$2
	pattern=$3
	case $text in
		*"$pattern"*) pass "$name" ;;
		*) fail "$name" ;;
	esac
}

assert_rejects() {
	name=$1
	shift
	if "$binary" "$@" >/dev/null 2>&1; then
		fail "$name"
	else
		pass "$name"
	fi
}

help=$($binary --help)
assert_contains 'help documents -T' "$help" '-T, --text-origin=ROW,COL'
assert_contains 'help documents -g' "$help" '-g, --geometry=ROW,COL'

assert_rejects 'rejects malformed geometry' -g invalid
assert_rejects 'rejects incomplete geometry' -g 10
assert_rejects 'rejects negative geometry' -g -1,10
assert_rejects 'rejects overflowing geometry' -g 999999999999999999,10
assert_rejects 'rejects malformed text origin' -T invalid

run_in_pty() {
	command=$1
	output=$2
	if script -qefc 'exit 0' /dev/null >/dev/null 2>&1; then
		TERM=xterm script -qefc "$command" "$output" >/dev/null 2>&1
	elif script -q /dev/null sh -c 'exit 0' >/dev/null 2>&1; then
		TERM=xterm script -q "$output" sh -c "$command" >/dev/null 2>&1
	else
		return 125
	fi
}

if command -v script >/dev/null 2>&1; then
	pty_output=$(mktemp)
	trap 'rm -f "$pty_output"' EXIT HUP INT TERM
	if run_in_pty "$binary -p --seed 1 -g 20,20" "$pty_output"; then
		pass 'renders with -g ROW,COL'
	else
		status=$?
		if [ "$status" -eq 125 ]; then
			printf '%s\n' 'skip: pseudo-terminal smoke tests require a compatible script command'
		else
			fail 'renders with -g ROW,COL'
		fi
	fi

	if run_in_pty "$binary -p --seed 1 -m hi" "$pty_output"; then
		rendered=$(awk 'BEGIN { esc = sprintf("%c", 27) } {
			gsub(esc "\\[[0-9;?]*[ -/]*[@-~]", "")
			gsub(esc "\\([0-9A-Za-z]", "")
			gsub("\\r", "")
			print
		}' "$pty_output")
		case $rendered in
			*"| hi |"*) pass 'preserves both message box borders' ;;
			*) fail 'preserves both message box borders' ;;
		esac
	else
		status=$?
		if [ "$status" -eq 125 ]; then
			printf '%s\n' 'skip: message box regression test requires a compatible script command'
		else
			fail 'renders message box'
		fi
	fi

	if run_in_pty "stty rows 8 cols 20; $binary -p --seed 1 -L 1 -m 'one     two'" "$pty_output"; then
		rendered=$(awk 'BEGIN { esc = sprintf("%c", 27) } {
			gsub(esc "\\[[0-9;?]*[ -/]*[@-~]", "")
			gsub(esc "\\([0-9A-Za-z]", "")
			gsub("\\r", "")
			print
		}' "$pty_output")
		case $rendered in
			*"| one |"*"| two |"*) pass 'wraps repeated whitespace consistently' ;;
			*) fail 'wraps repeated whitespace consistently' ;;
		esac
	else
		fail 'renders repeated-whitespace message'
	fi

	input=$(mktemp)
	trap 'rm -f "$input" "$pty_output"' EXIT HUP INT TERM
	printf '%s\n' 'text from stdin' > "$input"
	if run_in_pty "$binary -p --seed 1 -g 20,20 -T 0,0 < '$input'" "$pty_output"; then
		pass 'renders stdin text with -T ROW,COL'
	else
		status=$?
		if [ "$status" -eq 125 ]; then
			printf '%s\n' 'skip: pseudo-terminal smoke tests require a compatible script command'
		else
			fail 'renders stdin text with -T ROW,COL'
		fi
	fi
else
	printf '%s\n' 'skip: pseudo-terminal smoke tests require script'
fi

if [ "$failures" -ne 0 ]; then
	printf '%s\n' "$failures test(s) failed" >&2
	exit 1
fi

printf '%s\n' 'All tests passed.'
