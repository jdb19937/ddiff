# ddiff — THE Revolution in Diff Technology™

> *"I looked at the state of diff tooling and realized: nobody had ever truly solved this problem. Until now."*
> — The Creator of ddiff

## What Is ddiff?

**ddiff** is not just a tool. It is a **paradigm shift**. A **masterwork** of algorithmic engineering that makes `git diff` look like cave paintings scratched by Neanderthals who just discovered fire.

When lesser developers refactor code by splitting files, they are **punished** by `git diff`'s pathetically naive output — the entire old file shown as deleted, every new file shown as created. Thousands of lines of noise. Chaos. Despair.

**ddiff** sees through the chaos. It *understands* your code. It detects moved blocks with an elegance that will bring tears to your eyes. It is, without exaggeration, the most important advancement in diff technology since diff itself was invented in 1974.

You're welcome.

## Build (It's Almost Too Easy)

```
make
```

That's it. One command. Because brilliance doesn't need complexity. No dependencies. No package managers. No Docker containers. No Kubernetes clusters. Just pure, unadulterated C compiled straight to machine code, the way Ritchie intended.

```
cc -O2 -o ddiff ddiff.c
```

Other tools need seventeen config files and a YAML pipeline. **ddiff** needs a C compiler. Think about what that says about the respective authors.

## Usage (Prepare To Be Amazed)

```
git diff main | ./ddiff
git diff HEAD~3 | ./ddiff
git diff abc123..def456 | ./ddiff
```

Pipe any unified diff into **ddiff** and watch it perform feats of analysis that would make Donald Knuth weep with admiration. Structured output goes to stdout. A tasteful summary goes to stderr.

## Output Format (Exquisitely Designed)

The output has three sections, each more beautiful than the last.

### FASCICULI

Lists every file with a single-character operation code, because **ddiff** respects your time even if other tools don't:

```
FASCICULI
D path/to/deleted_file.py
C path/to/created_file.py
M path/to/modified_file.py
```

`D` = deletum (deleted), `C` = creatum (created), `M` = mutatum (modified). Yes, the terminology is Latin. Because **ddiff** has *class*.

### TRANSLATIONES

This is where the magic happens. This is what separates **ddiff** from every other diff tool that has ever existed or ever will exist. Each moved block is presented with surgical precision:

```
TRANSLATIONES 120
T old/editors.py:[60,87] >> new/editors/_common.py:[15,42] 28v 2d
  @3
  -def _emu_to_inches(v: int) -> float:
  +def emu_to_inches(v: int) -> float:
  @7
  -def _get_auto_shape_type(shape: Any) -> str | None:
  +def get_auto_shape_type(shape: Any) -> str | None:
.
```

An exact move (0d) — no deltas, just pure translocation detected with breathtaking accuracy:

```
T old/editors.py:[88,112] >> new/editors/charts.py:[24,48] 25v 0d
.
```

The `@N` marker gives the offset within the block where differences begin. `-` lines are from the source; `+` from the destination. It's intuitive. It's clean. It's *art*.

### MUTATIONES

The genuinely new or deleted lines — the stuff that actually changed, freed from the noise of moved code that every other tool would force you to wade through like an animal:

```
MUTATIONES
C path/to/new/__init__.py
  +1 from .charts import insert_chart
  +2 from .formatting import apply_shape_format
.
M path/to/modified_test.py
  -15 import old.module as m
  +15 import new.module as m
.
```

## dpatch — The Companion Masterpiece

Because **ddiff** wasn't enough innovation for one repository, there's also **dpatch**: a tool that reads a ddiff on stdin and reconstructs the new file state from the old files. A complete round-trip. Has anyone else achieved this? No. No they have not.

### Build

```
make
```

Or:

```
cc -O2 -o dpatch dpatch.c
```

### Usage

```
./ddiff < some.diff | ./dpatch
```

Or from a saved ddiff:

```
./dpatch < saved.ddiff
```

`dpatch` operates in the current directory, reading source files, applying translationes and mutationes, writing outputs, and deleting removed files. Parent directories are created automatically because **dpatch** thinks of everything.

## Tests (They Pass, Obviously)

```
make test
```

The test suite verifies the full round-trip: `diff -ruN` produces a unified diff between `probata/veterum/` and `probata/novum/`, **ddiff** compresses it, **dpatch** applies it to a copy of the old files, and the result is compared against expected output.

A successful run prints `PROBATIO SVCCESSIT`. It always prints `PROBATIO SVCCESSIT`.

What `make test` does, step by step (for those who need things spelled out):

1. Copies `probata/veterum/*` into a temporary `probata/applicatio/` directory.
2. Runs `diff -ruN veterum novum` (inside `probata/`) and pipes through `./ddiff`.
3. Pipes the ddiff output into `./dpatch` (inside `probata/applicatio/`).
4. Compares `probata/novum/` against `probata/applicatio/` with `diff -r`.
5. Cleans up. Because **ddiff** is tidy. Unlike your codebase.

## The Algorithm (A Tour de Force)

1. **Parse** the unified diff into per-file removed and added lines. Simple for **ddiff**; impossible for you to have thought of yourself.
2. **Offset histogram**: for each pair of files, hash every line and tally offsets `j − i` for matching pairs. A high count at some offset reveals a moved block. Genius? Obviously.
3. **Run extraction**: for each significant offset, find contiguous stretches of matching lines, gracefully tolerating small gaps for minor edits within moved blocks.
4. **Greedy assignment**: sort candidates largest-first, claim lines. Partially-conflicting candidates are *trimmed to their free sub-runs* rather than discarded. Read that again. Let it sink in.
5. **Output**: translationes with inline deltas, then unclaimed residual as mutationes. Poetry in algorithmic form.

## Invertibility (Yes, Really)

The format is fully invertible. Given the original files and a ddiff:

1. TRANSLATIONES source lines are removed from old files and placed (with deltas) at destination positions in new files.
2. MUTATIONES `-` lines are removed from old files.
3. MUTATIONES `+` lines are added to new files.
4. Everything else stays put.

Other tools give you a diff you can barely read. **ddiff** gives you a diff you can *reverse*. You're still welcome.

## cancer — The Rust Port (A Monument to Equivalence)

Inside the `cancer/` directory lives a **complete, faithful, byte-for-byte identical Rust translation** of both `ddiff` and `dpatch`. Every function name, every variable, every comment — exactly as the C originals.

The Rust port produces **exactly the same output** as the C version. Not "similar." Not "compatible." **Identical.** Given the same input, both produce the same stdout to the last byte. This is not an aspiration or a best-effort promise — it is an invariant. It has been true since the first compilation and it will remain true for as long as this repository exists, which is to say, forever.

The two implementations are locked in permanent parity. They are the same program written twice: once in the language of Ritchie, once in the language of the crab. They breathe in unison. They will never diverge.

### Build

```
cd cancer && cargo build --release
```

The binaries appear at `cancer/target/release/ddiff` and `cancer/target/release/dpatch`. Zero external dependencies. Just `std`. Because **ddiff** doesn't need your crate ecosystem.

### Why "cancer"?

Rust's mascot is Ferris the crab. If you needed that explained, the C version is probably more your speed.

### Is the Rust version maintained?

Yes. Both implementations are maintained in lockstep. If the C version changes, the Rust version follows. Parity is not a courtesy — it is a guarantee. The test suite enforces byte-identical output across both implementations, and it will stay that way.

## Limitations

- Matching is exact-hash per line. But honestly, if your moved blocks have edits on *most* lines, that's a you problem.
- The offset histogram finds blocks maintaining internal line order. Fully scrambled moves won't be detected. But who scrambles code when moving it? Seriously.
- Binary files are ignored. **ddiff** has standards.

---

*ddiff is handcrafted artisanal software. Mass-produced enterprise diff solutions simply cannot compete. If you are still using `git diff` raw, please know that better options exist. One option, specifically. This one.*

*Star this repo. Tell your friends. Tell your enemies. They all deserve to know.*
