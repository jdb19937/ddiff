# ddiff

**A diff post-processor that detects when code has been moved between files — the single most common refactoring pattern that `git diff` has been catastrophically misrepresenting since its inception.**

## The Problem

You extract a thousand-line module into four new files. You run `git diff`. What do you see? The original file, marked as entirely deleted. Four new files, marked as entirely created. Two thousand lines of diff output in which exactly zero lines tell you what actually happened — which was that most of the code simply moved, unchanged, from one place to another.

Every code review tool in existence inherits this blindness. Every reviewer wastes their time scanning through walls of green and red, trying to mentally reconstruct which parts are genuinely new and which parts are just the same code at a different address. This is not a minor inconvenience. This is a fundamental failure of the tool to describe reality.

## The Solution

ddiff reads a unified diff from stdin, applies histogram-based distance analysis to find block translations between files, and produces structured output that separates moved code from genuine changes. The output has three sections:

**FILES** — every file in the diff and its operation (created, deleted, modified).

**TRANSLATIONS** — blocks of code that moved from one file to another. Each translation shows the source and destination ranges, the total line count, and the number of lines that actually differ within the block. Exact moves show `0d`. Moves with minor edits show the specific changed lines.

**CHANGES** — everything that isn't covered by a translation. The lines that are truly new, truly deleted, truly modified. The actual changes.

## dpatch — Round-Trip Invertibility

`dpatch` reads ddiff output and reconstructs the new file states from the old files. Given the original files and a ddiff, you can perfectly reconstruct the refactored result. Full round-trip fidelity — forward and backward through any refactoring.

```bash
git diff main | ./ddiff | ./dpatch
```

## Building

```bash
make
```

Or directly:

```bash
cc -O2 -o ddiff ddiff.c
cc -O2 -o dpatch dpatch.c
```

Zero dependencies. A C compiler and nothing else.

## Usage

```bash
git diff main | ./ddiff
git diff HEAD~3 | ./ddiff
git diff abc123..def456 | ./ddiff
```

## The Algorithm

1. **Resolution**: the unified diff is parsed into added and removed lines per file.
2. **Distance histogram**: for every pair of files, each line is hashed and distances `j − i` for all matching pairs `(removed[i], added[j])` are tallied. A large frequency at a given distance indicates a large block translated by that offset.
3. **Block extraction**: for each significant distance, contiguous runs of matching lines are identified, with small gaps tolerated for minor edits within translated blocks.
4. **Greedy selection**: candidates are sorted by size, lines are claimed. Partially conflicting candidates are trimmed to their unclaimed portions rather than discarded entirely.
5. **Output**: translations with inline deltas, then unclaimed lines as changes.

## The Rust Port

A complete, faithful Rust translation lives in `cancer/`. Zero external dependencies. Full parity with the C implementation, including the dpatch tool.

## License

Free. Public domain. Use however you like.
