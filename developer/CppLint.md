# C++ lint

First-party C++ files under `apps/`, `components/`, and `tests/` are formatted
with a pinned clang-format. The tracked pre-commit hook runs the same check as
CI.

## Getting started

Install [uv](https://docs.astral.sh/uv/) and enable the tracked hooks once per
clone:

```bash
git config core.hooksPath .githooks
```

That is all. A commit that stages first-party C++ is formatted with
`clang-format==23.1.1` before it is recorded, and the hook stages its own
changes.

## Commands

```bash
# Check the files changed from HEAD (tracked and untracked)
uv run --script scripts/lint-cpp.py

# Fix the files changed from HEAD
uv run --script scripts/lint-cpp.py --fix

# Check or fix only the staged files
uv run --script scripts/lint-cpp.py --staged
uv run --script scripts/lint-cpp.py --staged --fix

# Check or fix the complete first-party scope
uv run --script scripts/lint-cpp.py --all
uv run --script scripts/lint-cpp.py --all --fix
```

## Notes

- The hook reports the files it rewrote. Because it stages those edits, a commit
  can differ from the staged diff; check `git show` when that matters. A commit
  that staged only a formatting fix ends up empty.
- Partial staging does not work for first-party C++: the hook formats and
  re-stages a staged file, so if that file also has unstaged changes it aborts
  with exit 2. Stage whole files and split a commit by file, not by hunk.
- `uv` must be on `PATH`. Where the uv cache is read-only (sandboxes, agents),
  point `UV_CACHE_DIR` at a writable directory.
- CI runs `--all` on pull requests, so the local hook does not excuse
  pre-existing formatting errors.
